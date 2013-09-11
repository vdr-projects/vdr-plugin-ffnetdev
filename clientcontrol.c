/* 
 * tsworker.c: ts streaming worker thread
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <sys/time.h>

#include <vdr/tools.h>

#include "tool_socket.h"
#include "tool_select.h"

#include "clientcontrol.h"
#include "config.h"


//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

cClientControl *cClientControl::m_Instance = NULL;

cClientControl::cClientControl(void)
		: cThread("[ffnetdev] ClientControl")
{
	m_Active = false;	
	m_ClientSocket = NULL;
}

cClientControl::~cClientControl() 
{
	dsyslog("[ffnetdev] Destructor cClientControl\n");

	if (m_Active) 
	   Stop();
	delete m_ClientSocket;
}

void cClientControl::Init(int iPort, cPluginFFNetDev *pPlugin) 
{
	if (m_Instance == NULL) 
	{
		m_Instance = new cClientControl;
		m_Instance->m_pPlugin = pPlugin;
		m_Instance->m_iPort = iPort;
		m_Instance->Start();
		m_Instance->m_bCloseClientRequest = false;
		m_Instance->m_bPlayStateReq = false;
	}
}

void cClientControl::Exit(void) 
{
	if (m_Instance != NULL) 
	{
		m_Instance->Stop();
		DELETENULL(m_Instance);
	}
}

void cClientControl::Stop(void)
{
	m_Active = false;
	Cancel(3);
}

void cClientControl::CloseStreamClient(void) 
{
	m_Instance->m_bCloseClientRequest = true;
#ifdef DEBUG
	fprintf(stderr, "[ffnetdev] Streamer: Closing of ClientControl client socket requested.\r\n");
#endif
}

void cClientControl::Action(void) 
{
	cTBSelect select;
	cTBSocket m_StreamListen;
	int ret;
   
	const char* m_ListenIp = "0.0.0.0";
	uint iPort = m_iPort;

	m_ClientSocket 	= new cTBSocket;

	m_Active		= true;
	m_bHaveClient	= false;

   if (!m_StreamListen.Listen(m_ListenIp, iPort, 1)) { // ToDo JN place to allow more connections/clients!
		esyslog("[ffnetdev] ClientControl: Couldn't listen %s:%d: %s", m_ListenIp, iPort, strerror(errno));
		m_Active = false;
	} 
	else
      isyslog("[ffnetdev] ClientControl: Listening on port %d", iPort);

	while (m_Active) {
		select.Clear();
		
		if (m_bHaveClient==false)
			select.Add(m_StreamListen, false);
		else {
			select.Add(*m_ClientSocket, true);  //select for writing fd
			select.Add(*m_ClientSocket, false);	//select for reading fd
		}

		int numfd;
		/* React on status change of any of the above file descriptor */
		if ((numfd=select.Select(1000)) < 0) {
			if (!m_Active) // Exit was requested while polling
				continue;
			esyslog("[ffnetdev] ClientControl: Fatal error, ffnetdev exiting: %s", strerror(errno));
			m_Active = false;
			continue;
		}
		
		
		/* Accept connecting	*/
		if ( (m_bHaveClient==false)&&select.CanRead(m_StreamListen) ) {
			if (m_ClientSocket->Accept(m_StreamListen)) {
				isyslog("[ffnetdev] ClientControl: Accepted client %s:%d",
					m_ClientSocket->RemoteIp().c_str(), m_ClientSocket->RemotePort());
				m_bHaveClient = true;
			} 
			else {
				esyslog("[ffnetdev] Streamer: Couldn't accept : %s", strerror(errno));
				m_bHaveClient = false;
				m_Active = false;
				continue;
			}
		}

		
		/* Check for closed client connection */		
		if (m_bHaveClient==true) { 
			if (m_bCloseClientRequest==true) {
				m_bCloseClientRequest = false;
				m_bHaveClient = false;
				
				if ( m_ClientSocket->Close() ) {
#ifdef DEBUG
					fprintf(stderr, "[ffnetdev] ClientControl: Client socket closed successfully.\n");
#endif
					isyslog("[ffnetdev] ClientControl: Connection closed: client %s:%d",
						 m_ClientSocket->RemoteIp().c_str(), m_ClientSocket->RemotePort());
				}
				else
					{
#ifdef DEBUG
					   fprintf(stderr, "[ffnetdev] ClientControl: Error closing client socket.\n");
#endif
					   esyslog("[ffnetdev] ClientControl: Error closing connection.");
					   m_Active=false;
					   continue;
					} 
					  
			}
			
			if ( select.CanWrite(*m_ClientSocket) ) 
			{
			
			}
			
			if ( select.CanRead(*m_ClientSocket) )
			{
			   SClientControl data;
			   SClientControlInfo info;
			   
			   if ( (ret = m_ClientSocket->Read(&data, sizeof(data))) == sizeof(data)) 
			   {
			      dsyslog("pakType %d, dataLen %d", data.pakType, data.dataLen);
			      switch (data.pakType)
			      {
			      case ptInfo: 
			         if (m_ClientSocket->Read(&info, data.dataLen) == data.dataLen)
                     m_pPlugin->SetClientName(info.clientName);			         
                  dsyslog("clientName %s, data.dataLen %d %d", info.clientName, data.dataLen, sizeof(data));
			         break;
			      
			      case ptPlayStateReq: 
			         m_bPlayStateReq = true;
			         break;
			         
			      default:
			         break;
			      }
			   }
			   else if (ret == 0)
   			{
   			   CloseStreamClient();
   			}
		   }
		} 

		cCondWait::SleepMs(3);

	} // while(m_Active)

}


bool cClientControl::SendPlayState(ePlayMode PlayMode, bool bPlay, bool bForward, int iSpeed) 
{
   SClientControl data;
   SClientControlPlayState state;
       
   if ((m_Instance == NULL) || (m_Instance->m_ClientSocket == NULL) || (!m_Instance->m_bHaveClient))
      return false;

   m_Instance->m_bPlayStateReq = false;

   state.PlayMode = PlayMode;
   state.Play = bPlay;
   state.Forward = bForward;
   state.Speed = iSpeed;
   
   data.pakType = ptPlayState;
   data.dataLen = sizeof(state);
   dsyslog("[ffnetdev] SendPlayState: PlayMode: %d, bPlay: %d, bForward: %d, iSpeed: %d", PlayMode, bPlay, bForward, iSpeed);
   if (m_Instance->m_ClientSocket->Write(&data, sizeof(data)) == sizeof(data))
   {
      if (m_Instance->m_ClientSocket->Write(&state, sizeof(state)) == sizeof(state))
         return true;
      else
         return false;
   }
   else
      return false;
}


bool cClientControl::SendStillPicture(const uchar *Data, int Length) 
{
   SClientControl data;
   int written, available, done;
  
   if ((m_Instance == NULL) || (m_Instance->m_ClientSocket == NULL) || (!m_Instance->m_bHaveClient))
      return false;
  
   data.pakType = ptStillPicture;
   data.dataLen = Length;
   if (m_Instance->m_ClientSocket->Write(&data, sizeof(data)) == sizeof(data))
   {
      available = Length;
      done = 0;
      while ((available > 0) && (m_Instance->m_bHaveClient) &&
			    (!m_Instance->m_bCloseClientRequest))
      {
      
         if (((written=m_Instance->m_ClientSocket->Write(Data + done, available)) < 0) && 
             (errno != EAGAIN)) 
         {
            CloseStreamClient();
            return false;
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
      
      return true;
   }
   else
      return false;
}

bool cClientControl::SendSFreeze() 
{
   SClientControl data;
   int written, available, done;
  
   if ((m_Instance == NULL) || (m_Instance->m_ClientSocket == NULL) || (!m_Instance->m_bHaveClient))
      return false;
  
   data.pakType = ptFreeze;
   data.dataLen = 0;
   if (m_Instance->m_ClientSocket->Write(&data, sizeof(data)) == sizeof(data))
   {     
      return true;
   }
   else
      return false;
}
