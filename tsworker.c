/* 
 * tsworker.c: ts streaming worker thread
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
 
#include <sys/time.h>

#include <vdr/tools.h>

#include "tools/socket.h"
#include "tools/select.h"

#include "tsworker.h"
#include "config.h"

#define MINSENDBYTES KILOBYTE(500)

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

cTSWorker *cTSWorker::m_Instance = NULL;

cTSWorker::cTSWorker(void)
		: cThread("[ffnetdev] TS streamer")
{
	m_Active = false;
	
	m_StreamClient = NULL;
	origPrimaryDevice = -1;
}

cTSWorker::~cTSWorker() {
	if (m_Active) Stop();
}

void cTSWorker::Init(cStreamDevice *StreamDevice, int tsport, cPluginFFNetDev *pPlugin ) {
	if (m_Instance == NULL) {
		m_Instance = new cTSWorker;
		m_Instance->m_StreamDevice = StreamDevice;
		m_Instance->TSPort = tsport;		
		m_Instance->Start();
		m_Instance->m_pPlugin = pPlugin;
	}
}

void cTSWorker::Exit(void) {
	if (m_Instance != NULL) {
		m_Instance->Stop();
		DELETENULL(m_Instance);
	}
}

void cTSWorker::Stop(void) {
	m_Active = false;
	Cancel(3);
}

void cTSWorker::CloseStreamClient(void) {
	m_Instance->close_Streamclient_request = true;
#ifdef DEBUG
	fprintf(stderr, "[ffnetdev] Streamer: Closing of TS-streaming client socket requested.\r\n");
#endif
}

void cTSWorker::Action(void) {
	cTBSelect select;
	//cTBSocket m_StreamListen(SOCK_DGRAM);
	cTBSocket m_StreamListen;
	struct timeval oldtime;
	long bytessend = 0;
	long oldbytessend = 0;
	
	memset(&oldtime, 0, sizeof(oldtime));

	const char* m_ListenIp = "0.0.0.0";
	uint m_StreamListenPort = TSPort;

	m_StreamClient 	= new cTBSocket;

	m_Active		= true;
	have_Streamclient	= false;

        if (!m_StreamListen.Listen(m_ListenIp, m_StreamListenPort, 1)) { // ToDo JN place to allow more connections/clients!
		esyslog("[ffnetdev] Streamer: Couldn't listen %s:%d: %s", m_ListenIp, m_StreamListenPort, strerror(errno));
		m_Active = false;
	} 
	else
	        isyslog("[ffnetdev] Streamer: Listening on port %d", m_StreamListenPort);

	while (m_Active) {
		select.Clear();
		
		if (have_Streamclient==false)
			select.Add(m_StreamListen, false);
		else {
			select.Add(*m_StreamClient, true);      //select for writing fd
			select.Add(*m_StreamClient, false);	//select for reading fd
		}

		int numfd;
		/* React on status change of any of the above file descriptor */
		if ((numfd=select.Select(1000)) < 0) {
			if (!m_Active) // Exit was requested while polling
				continue;
			esyslog("[ffnetdev] Streamer: Fatal error, ffnetdev exiting: %s", strerror(errno));
			m_Active = false;
			continue;
		}
		
		
		//DEBUG
		/*		
		fprintf(stderr, "[ffnetdev] Streamer: Num_FD TS: %d", numfd);
		
		if (select.CanRead(m_StreamListen) || select.CanWrite(m_StreamListen)) 
			fprintf (stderr, "m_StreamListen can act.\n");
		if (select.CanRead(*m_StreamClient) || select.CanWrite(*m_StreamClient)) 
			fprintf (stderr, "m_StreamClient can act.\n");
		*/	
		
		/* Accept connecting streaming clients	*/
		if ( (have_Streamclient==false)&&select.CanRead(m_StreamListen) ) {
			if (m_StreamClient->Accept(m_StreamListen)) {
				isyslog("[ffnetdev] Streamer: Accepted client %s:%d",
					m_StreamClient->RemoteIp().c_str(), m_StreamClient->RemotePort());
				have_Streamclient = true;

				m_pPlugin->SetPrimaryDevice();
			} 
			else {
				esyslog("[ffnetdev] Streamer: Couldn't accept : %s", strerror(errno));
				have_Streamclient = false;
				m_Active = false;
				continue;
			}
		}

		
		/* Check for closed streaming client connection */		
		if (have_Streamclient==true) { 
			if (close_Streamclient_request==true) {
				close_Streamclient_request = false;
				have_Streamclient = false;
				
				m_pPlugin->RestorePrimaryDevice();
				
				if ( m_StreamClient->Close() ) {
#ifdef DEBUG
					fprintf(stderr, "[ffnetdev] Streamer: Client socket closed successfully.\n");
#endif
					isyslog("[ffnetdev] Streamer: Connection closed: client %s:%d",
						 m_StreamClient->RemoteIp().c_str(), m_StreamClient->RemotePort());
				}
				else
					{
#ifdef DEBUG
					   fprintf(stderr, "[ffnetdev] Streamer: Error closing client socket.\n");
#endif
					   esyslog("[ffnetdev] Streamer: Error closing connection.");
					   m_Active=false;
					   continue;
					} 
					  
			}
			
			if ( select.CanWrite(*m_StreamClient) ) {
				int count=0;
				
				m_StreamDevice->LockOutput();
      				uchar *buffer = m_StreamDevice->Get(count);
				if (buffer!=NULL) {
					int available = count;
					int done      = 0;
					int written   = 0;
					while ((available > 0) && (have_Streamclient == true) &&
					       (!close_Streamclient_request))
					{
					
					    if (((written=m_StreamClient->Write(&buffer[done], available)) < 0) && 
						(errno != EAGAIN)) 
					    {
						CloseStreamClient();
					    }
					    
					    if (written > 0)
					    {
						
						available -= written;
						done      += written;
					    }
					    else
					    {
						cCondWait::SleepMs(5);
					    }
					}
					m_StreamDevice->Del(count);

					struct timeval curtime;
					gettimeofday(&curtime, 0);
					if (oldtime.tv_sec == 0)
					{
					    oldtime = curtime;
					    bytessend    = 0;
					    oldbytessend = 0;
					}
					
					bytessend += count;
					if (curtime.tv_sec > oldtime.tv_sec + 10)
					{
					    double secs = (curtime.tv_sec * 1000 + (curtime.tv_usec / 1000)) / 1000 
							- (oldtime.tv_sec * 1000 + (oldtime.tv_usec / 1000)) / 1000;
#ifdef DEBUG
					    fprintf(stderr, "[ffnetdev] Streamer: current TransferRate %d Byte/Sec, %d Bytes send\n",
						(int)((bytessend - oldbytessend) / secs), bytessend - oldbytessend);
#endif						
					    dsyslog("[ffnetdev] Streamer: current TransferRate %d Byte/Sec, %d Bytes send\n",
						(int)((bytessend - oldbytessend) / secs), bytessend - oldbytessend);
					    
					    oldbytessend = bytessend;
					    oldtime = curtime;
					}
				}
				m_StreamDevice->UnlockOutput();
			}
			
			if ( select.CanRead(*m_StreamClient) )
			    if ( m_StreamClient->Read(NULL, 1)==0 ) 
				CloseStreamClient();
		} 
		else {  
			/* simply discard all data in ringbuffer */
			int count=0;
			if ( (m_StreamDevice->Get(count)) !=NULL ) {
				m_StreamDevice->Del(count);
#ifdef DEBUG
				fprintf (stderr, "[ffnetdev] Streamer: Bytes not sent, but deleted from ringbuffer: %d\n",count);
#endif
				dsyslog("[ffnetdev] Streamer: Bytes not sent, but deleted from ringbuffer: %d\n",count);
			}
		}
		cCondWait::SleepMs(3);

	} // while(m_Active)

}

