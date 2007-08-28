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

#define TS_PACKET_SIZE (188)
#define UDP_PACKET_SIZE (TS_PACKET_SIZE * 7)
#define UDP_MAX_BITRATE 8112832
#define UDP_SEND_INTERVALL 1000

// 8388608 = 8MBit
struct TSData
{
	char packNr;
    char packsCount;
    char tsHeaderCRC;
    char data[UDP_PACKET_SIZE];
};

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

cTSWorker::~cTSWorker() 
{
	dsyslog("[ffnetdev] Destructor cTSWorker\n");
	if (m_Active) Stop();
}

void cTSWorker::Init(cStreamDevice *StreamDevice, int tsport, cPluginFFNetDev *pPlugin ) {
	if (m_Instance == NULL) {
		m_Instance = new cTSWorker;
		m_Instance->m_StreamDevice = StreamDevice;
		m_Instance->TSPort = tsport;		
		m_Instance->Start();
		m_Instance->m_pPlugin = pPlugin;
		m_Instance->close_Streamclient_request = false;
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
    ActionTCP();
    //ActionUDP();
}


void cTSWorker::ActionTCP(void) {
	cTBSelect select;
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
			
			if ( select.CanWrite(*m_StreamClient) ) 
			{
			   if (m_StreamDevice->GetPlayState() == psBufferReset)
			   {
			      cCondWait::SleepMs(10);
			      m_StreamDevice->SetPlayState(psBufferReseted);
			   }
			   
			   if (m_StreamDevice->GetPlayState() == psPlay)
			   {
   		      int count=0;
   				
   				m_StreamDevice->LockOutput();
         				uchar *buffer = m_StreamDevice->Get(count);
   				if (buffer!=NULL) {
   				   count = (count > TCP_SEND_SIZE) ? TCP_SEND_SIZE : count;
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
   						double secs = (curtime.tv_sec * 1000 + (curtime.tv_usec / 1000.0)) / 1000 
   							- (oldtime.tv_sec * 1000 + (oldtime.tv_usec / 1000.0)) / 1000;
   						double rate = (double)((bytessend - oldbytessend) / secs) * 8 / 1024 / 1024;
   						int bufstat = m_StreamDevice->Available() * 100 / (m_StreamDevice->Available() + m_StreamDevice->Free()); 
   #ifdef DEBUG
   						fprintf(stderr, "[ffnetdev] Streamer: current TransferRate %2.3f MBit/Sec, %d Bytes send, %d%% Buffer used\n",
   							rate, bytessend - oldbytessend, bufstat);
   #endif						
   						dsyslog("[ffnetdev] Streamer: Rate %2.3f MBit/Sec, %d Bytes send, %d%% Buffer used\n",
   							rate, bytessend - oldbytessend, bufstat);
   						
   						oldbytessend = bytessend;
   						oldtime = curtime;
   					}
   				}
   				m_StreamDevice->UnlockOutput();
   			}
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


void cTSWorker::ActionUDP(void) 
{
	cTBSocket m_StreamClient(SOCK_DGRAM);
	struct timeval oldtime, curtime;
	u64 oldPacketTime = 0;
	long bytessend = 0;
	long oldbytessend = 0;
	long toSend = 0;
	int restData = 0;
	TSData tsData;

	const char* StreamIp = "192.168.0.61";
	uint StreamPort = TSPort;

	m_Active		= true;
	have_Streamclient	= true;
	
	if (!m_StreamClient.OpenUDP(StreamIp, StreamPort)) 
	{
		isyslog("[ffnetdev] Streamer: Couldn't create UDP-Socket: %s", strerror(errno));
		m_Active = false;
	} 
	else
	    isyslog("[ffnetdev] Streamer: UDP-Socket create successful");

	gettimeofday(&oldtime, 0);
	tsData.packNr = 0;
	tsData.packsCount = 0;
	tsData.tsHeaderCRC = 0;

	while (m_Active) 
	{
		/* Check for closed streaming client connection */		
		if (have_Streamclient==true) 
		{ 
			if (close_Streamclient_request==true) 
			{
				close_Streamclient_request = false;
				have_Streamclient = false;
				
				m_pPlugin->RestorePrimaryDevice();
				
				if ( m_StreamClient.Close() ) 
				{
#ifdef DEBUG
					fprintf(stderr, "[ffnetdev] Streamer: Client socket closed successfully.\n");
#endif
					isyslog("[ffnetdev] Streamer: Connection closed: client %s:%d",
						 m_StreamClient.RemoteIp().c_str(), m_StreamClient.RemotePort());
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
			
			
			int count=0;
			
			m_StreamDevice->LockOutput();
			uchar *buffer = m_StreamDevice->Get(count);
			if (buffer!=NULL) 
			{	
				int available = count;
				int done      = 0;
				int written   = 0;
				char data[100];
				int  rcvCount;
				int sleepTime;

			/*	rcvCount=m_StreamClient.Read(data, 10);
				if (rcvCount > 0)
				{
					isyslog("[ffnetdev] Streamer: empfangen:%d Bytes\n", rcvCount);
				}*/

				if (oldPacketTime == 0)
					oldPacketTime = get_time()- UDP_SEND_INTERVALL;

				while ((available > 0) && (have_Streamclient == true) &&
					   (!close_Streamclient_request))
				{
					while ((sleepTime = oldPacketTime + UDP_SEND_INTERVALL - get_time()) > 0)
						usleep(sleepTime);

					if (toSend == 0)
						toSend = (long)(UDP_MAX_BITRATE * (((double)get_time() - oldPacketTime) / 1000000) / 8);
					
					int sendcount = (available > toSend) ? toSend : available;
					sendcount = (sendcount > UDP_PACKET_SIZE) ? UDP_PACKET_SIZE : sendcount;

					available -= sendcount;

					while ((sendcount > 0) && (have_Streamclient == true) &&
					   (!close_Streamclient_request))
					{
						if (((written=m_StreamClient.Write(&buffer[done], sendcount)) < 0) && 
							(errno != EAGAIN)) 
						{
							isyslog("[ffnetdev] Streamer: Couldn't send data: %d %s Len:%d\n", errno, strerror(errno), sendcount);
							CloseStreamClient();
						}
						
						if (written > 0)
						{
							done += written;
							sendcount -= written;
							toSend	  -= written;
							if (toSend == 0)
								oldPacketTime = get_time();
						}
						else
						{
							cCondWait::SleepMs(5);
						}
					}
				}
				m_StreamDevice->Del(count);

				gettimeofday(&curtime, 0);
				if (oldtime.tv_sec == 0)
				{
					oldtime = curtime;
					bytessend    = 0;
					oldbytessend = 0;
				}
					
				bytessend += count;
				if (curtime.tv_sec > oldtime.tv_sec + 3)
				{
					double secs = (curtime.tv_sec * 1000 + ((double)curtime.tv_usec / 1000.0)) / 1000 
						- (oldtime.tv_sec * 1000 + (oldtime.tv_usec / 1000.0)) / 1000;
					double rate = (double)((bytessend - oldbytessend) / secs) * 8 / 1024 / 1024;
#ifdef DEBUG
					fprintf(stderr, "[ffnetdev] Streamer: current TransferRate %2.3f MBit/Sec, %d Bytes send\n",
						rate, bytessend - oldbytessend);
#endif						
					dsyslog("[ffnetdev] Streamer: current TransferRate %2.3f MBit/Sec, %d Bytes send\n",
						rate, bytessend - oldbytessend);
					
					oldbytessend = bytessend;
					oldtime = curtime;
				}
			}
			m_StreamDevice->UnlockOutput();

		} 
		else 
		{  
			/* simply discard all data in ringbuffer */
			int count=0;
			if ( (m_StreamDevice->Get(count)) !=NULL ) 
			{
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

/* Returns time since 1970 in microseconds */
u64 cTSWorker::get_time(void)
{
	struct timeval tv;
	struct timezone tz={0,0};

	gettimeofday(&tv,&tz);
	return ((u64)tv.tv_sec)*1000000+((u64)tv.tv_usec);
}
