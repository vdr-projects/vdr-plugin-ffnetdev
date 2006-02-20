/* 
 * osdworker.c: osd worker thread
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/tools.h>
#include <sys/time.h>

#include "tools/socket.h"
#include "tools/select.h"

#include "osdworker.h"


#include "vncEncodeRRE.h"
#include "vncEncodeCoRRE.h"
#include "vncEncodeHexT.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

cOSDWorker *cOSDWorker::m_Instance = NULL;

cOSDWorker::cOSDWorker(void)
		: cThread("[ffnetdev] OSD via VNC")
{
	m_Active 		= false;
	m_OSDClient 		= NULL;
	m_pSendBuffer 		= NULL;
	m_SendBufferSize	= 0;
	state 			= NO_CLIENT;
	
	UseAlpha 		= false;
	
	m_pEncoder 		= NULL;
	
	ServerFormat.trueColour		= false;
	ServerFormat.bitsPerPixel 	= 8;
	ServerFormat.depth 		= 8;
	ServerFormat.bigEndian 		= false;
	ServerFormat.redShift	 	= 16;
	ServerFormat.redMax	 	= 255;
	ServerFormat.greenShift	 	= 8;
	ServerFormat.greenMax	 	= 255;
	ServerFormat.blueShift	 	= 0;
	ServerFormat.blueMax	 	= 255;
	
	memset(&m_OSDBuffer, 0, sizeof(m_OSDBuffer));
	
	numOSDColors		= 1;
	memset(&OSDColors, 0, sizeof(OSDColors));
	
	m_notupdatedLeft   = -1;
	m_notupdatedTop    = -1;
	m_notupdatedRight  = -1;
	m_notupdatedBottom = -1;
	memset(&m_lasttime, 0, sizeof(m_lasttime));
	memset(&m_lastupdate, 0, sizeof(m_lastupdate));
	
	memset(&ClientFormat, 0, sizeof(ClientFormat));
	ClientFormat.trueColour = 1;
}

cOSDWorker::~cOSDWorker() {
	if (m_Active) Stop();
	
	if (m_pEncoder != NULL) 
	{
	    m_pEncoder->LogStats();
	    delete m_pEncoder;
	    m_pEncoder = NULL;
	}
	
	if (m_pSendBuffer != NULL)
	    delete [] m_pSendBuffer;
}





void cOSDWorker::Init(int osdport, cPluginFFNetDev *pPlugin) {
    if (m_Instance == NULL)
    {
    	m_Instance = new cOSDWorker;
	m_Instance->OSDPort = osdport;
	m_Instance->Start();
	m_Instance->m_pPlugin = pPlugin;
    }
}


void cOSDWorker::Exit(void) {
	if (m_Instance != NULL) {
		m_Instance->Stop();
		DELETENULL(m_Instance);
	}
}

void cOSDWorker::Stop(void) {
	m_Active = false;
	Cancel(3);
}

void cOSDWorker::CloseOSDClient(void) {
	if (m_Instance == NULL)
	    return;
	    
	m_Instance->state = NO_CLIENT;
	m_Instance->UseAlpha = false;
	
	delete m_Instance->m_pEncoder;
	m_Instance->m_pEncoder = NULL;
	
	m_Instance->m_pPlugin->RestorePrimaryDevice();
	
	if (m_Instance->m_OSDClient != NULL) {
	    if ( m_Instance->m_OSDClient->Close() ) {
#ifdef DEBUG
		fprintf(stderr, "[ffnetdev] VNC: Client socket closed successfully.\n");
#endif	
	        isyslog("[ffnetdev] VNC: Connection closed.");
	    }
	    else {
#ifdef DEBUG
		fprintf(stderr, "[ffnetdev] VNC: Error closing client socket.\n");
#endif
		esyslog("[ffnetdev] VNC: Error closing connection.");
		m_Instance->m_Active=false;
	    }
	}
}

bool cOSDWorker::SendPlayMode(ePlayMode PlayMode) {

	// not yet implemented
	return true;
}


void cOSDWorker::CreateSendBuffer(int SendBufferSize)
{
    if (SendBufferSize != m_SendBufferSize)
    {
	if (m_pSendBuffer != NULL)
	    delete [] m_pSendBuffer;
	
	m_pSendBuffer = new BYTE[SendBufferSize];
	memset(m_pSendBuffer, 0, SendBufferSize);
	
	m_SendBufferSize = SendBufferSize;
    }
}

bool cOSDWorker::ClearScreen(void)
{
	if (m_Instance == NULL)
	    return false;
	
	// this should be improved;
	// 1) maybe we should send a our very "special" pseudo encoding[CLEAR_SCREEN] to our "special" VNC client to get an empty screen really fast

	m_Instance->numOSDColors = 1;
	memset(&m_Instance->OSDColors, 0, sizeof(m_Instance->OSDColors));
	SendCMAP(m_Instance->numOSDColors, m_Instance->OSDColors);
		
	bool bRetVal = false;
	memset(&(m_Instance->m_OSDBuffer), 0, 720*576); 
	memset(&(m_Instance->m_lasttime), 0, sizeof(m_Instance->m_lasttime));
	bRetVal = SendScreen(720,0,0,720,576,&(m_Instance->m_OSDBuffer));
	
	return bRetVal;
}


bool cOSDWorker::SendScreen(unsigned int stride, unsigned int x1, unsigned int y1, unsigned int w, unsigned int h, const void *data)
{
   if (m_Instance == NULL)
	return false;
	    
   rfbFramebufferUpdateMsg fu;
   rfbFramebufferUpdateRectHeader furh;
   
   unsigned int x;
   unsigned int y;
   for (y=0; y<h; y++)
      for (x=0; x<w; x++)
   	 m_Instance->m_OSDBuffer[(720*(y+y1))+x+x1] = *((unsigned char *)data+(y*stride)+x);

    x = x1;
    y = y1;
    x1 = (m_Instance->m_notupdatedLeft   != -1 && m_Instance->m_notupdatedLeft  < (int)x1)
    	 ? m_Instance->m_notupdatedLeft : x1;
    y1 = (m_Instance->m_notupdatedTop    != -1 && m_Instance->m_notupdatedTop   < (int)y1)
	 ? m_Instance->m_notupdatedTop  : y1;
	
    w  = (m_Instance->m_notupdatedRight  != -1 && m_Instance->m_notupdatedRight > (int)(x1 + w))
	 ? m_Instance->m_notupdatedRight  - x1 : w + (x - x1);
    h  = (m_Instance->m_notupdatedBottom != -1 && m_Instance->m_notupdatedBottom > (int)(y1 + h)) 
	 ? m_Instance->m_notupdatedBottom - y1 : h + (y - y1);

   struct timeval curtime;
   gettimeofday(&curtime, 0);
   curtime.tv_sec = curtime.tv_sec - (((int)curtime.tv_sec / 1000000) * 1000000);
   if ((curtime.tv_sec * 1000 + (curtime.tv_usec / 1000) < m_Instance->m_lasttime.tv_sec * 1000 + (m_Instance->m_lasttime.tv_usec / 1000) + 100) ||
      (m_Instance->m_pEncoder == NULL))
   {
        m_Instance->m_notupdatedLeft   = (int)x1;
	m_Instance->m_notupdatedTop    = (int)y1;
	m_Instance->m_notupdatedRight  = (int)(x1 + w);
	m_Instance->m_notupdatedBottom = (int)(y1 + h);
	m_Instance->m_lasttime = curtime;
	
	return false;
   }
   else
   {
	m_Instance->m_notupdatedLeft   = -1;
	m_Instance->m_notupdatedTop    = -1;
	m_Instance->m_notupdatedRight  = -1;
	m_Instance->m_notupdatedBottom = -1;
	m_Instance->m_lasttime = curtime;
	m_Instance->m_lastupdate = curtime;
   }
    
   if ((m_Instance->state==HANDSHAKE_OK) && (m_Instance->m_pEncoder != NULL)) {
	RECT rect = {x1, y1, x1 + w, y1 + h};
   	fu.type=rfbFramebufferUpdate;
   	fu.nRects=Swap16IfLE(1);
   	OSDWrite((unsigned char*)&fu, sz_rfbFramebufferUpdateMsg);
	
	int BufferSize = m_Instance->m_pEncoder->RequiredBuffSize(w, h);
	m_Instance->CreateSendBuffer(BufferSize);
	BufferSize = m_Instance->m_pEncoder->EncodeRect( &(m_Instance->m_OSDBuffer[0]), m_Instance->m_pSendBuffer, rect);
#ifdef DEBUG
	fprintf(stderr, "[ffnetdev] VNC: Send OSD Data %d Bytes\n", BufferSize);
#endif
	dsyslog("[ffnetdev] VNC: Send OSD Data %d Bytes\n", BufferSize);
       	OSDWrite((unsigned char*)m_Instance->m_pSendBuffer, BufferSize);
	
	if ((m_Instance->numOSDColors == 0) || ((m_Instance->numOSDColors == 1) && (m_Instance->OSDColors[0] == 0)))
	{
	    rfbBellMsg fu;
	    fu.type=rfbBell;
	    OSDWrite((unsigned char*)&fu, sz_rfbBellMsg);
	}

   	return true;
   }
   else {
	return false;
   }
}


bool cOSDWorker::SendCMAP(int NumColors, const tColor *Colors)
{
	if (m_Instance == NULL)
	    return false;
	
	rfbSetColourMapEntriesMsg scme;
	CARD16 red;
	CARD16 green;
	CARD16 blue;
	CARD16 alpha;

       	int i;
	
	if ((m_Instance->state==HANDSHAKE_OK) && !(m_Instance->ClientFormat.trueColour)) { 
		dsyslog("[ffnetdev] VNC: SendColourMapEntries");
		scme.type=rfbSetColourMapEntries;
		scme.firstColour = Swap16IfLE(0);
		scme.nColours = Swap16IfLE((CARD16)NumColors);

		OSDWrite((unsigned char*)&scme, sz_rfbSetColourMapEntriesMsg);

        	for(i=0; i<NumColors; i++)
        	{
           	red   = ((Colors[i]&0x00FF0000) >> 16);
           	green = ((Colors[i]&0x0000FF00) >>  8);
           	blue  = ((Colors[i]&0x000000FF) );
           	
           	if (m_Instance->UseAlpha) {
           		alpha = ((Colors[i]&0xFF000000) >> 24);
               		OSDWrite( (unsigned char*) &alpha, 2);
           	}
           	
	        OSDWrite( (unsigned char*) &red, 2);
      	   	OSDWrite( (unsigned char*) &green, 2);
	  	OSDWrite( (unsigned char*) &blue, 2);
	   	
	   	m_Instance->OSDColors[i] = Colors[i];
		}
		m_Instance->numOSDColors = NumColors;
		if (m_Instance->m_pEncoder != NULL)
		    m_Instance->m_pEncoder->SetLocalFormat(m_Instance->ServerFormat, 720, 576);
		
		return true;
	} 
	else {
		for (i=0; i<NumColors; i++)
			m_Instance->OSDColors[i] = Colors[i];
		m_Instance->numOSDColors = NumColors;
		if (m_Instance->m_pEncoder != NULL)
		    m_Instance->m_pEncoder->SetLocalFormat(m_Instance->ServerFormat, 720, 576);
		
		return false;
	}
	
	
}

bool cOSDWorker::OSDWrite(unsigned char *data, unsigned int data_length)
{
  if (m_Instance == NULL)
    return false;
    
  if (m_Instance->m_OSDClient!=NULL) {
   if (m_Instance->m_OSDClient->IsOpen()) 
   {
	//fprintf(stderr, "[ffnetdev] VNC: Trying to send...\n");
	if (!m_Instance->m_OSDClient->SafeWrite(data, data_length))
	{   
	    CloseOSDClient();
	    return false;
	}
   }  //else fprintf(stderr, "[ffnetdev] VNC: Cannot send...\n"); 
    //for (unsigned int i=0; i<data_length; i++) fprintf(stderr,"%02X ",*(data+i));
    //fprintf(stderr,"\n");
    
    return true;
  }
  return false;
}

bool cOSDWorker::RFBRead(char *buffer, int len)
{
	if (m_Instance == NULL)
	    return false;
	    
	if ( m_OSDClient->Read(buffer, len)==0 ) {
	/*
#ifdef DEBUG
			fprintf(stderr, "[ffnetdev] VNC: Client closed connection.\n");
#endif
			isyslog("[ffnetdev] VNC: Connection closed: client %s:%d",
			m_OSDClient->RemoteIp().c_str(), m_OSDClient->RemotePort());
			state = NO_CLIENT;
			m_Instance->m_OSDClient->Close();
			delete m_pEncoder;
			m_pEncoder = NULL;
	*/
			CloseOSDClient();
	
			return false;
	} 
	else
		return true;
}

void cOSDWorker::HandleClientRequests(cTBSelect *select) 
{
	if ( select->CanRead(*m_OSDClient) ) {
		rfbClientToServerMsg msg;
		
		if (!RFBRead((char*)&msg, 1))
			return;
		
		switch (msg.type) {
			
		case rfbSetPixelFormat:		if (!RFBRead( ((char*)&msg.spf)+1, sz_rfbSetPixelFormatMsg-1))
							return;
						dsyslog("[ffnetdev] VNC: SetPixelFormat\n");
						dsyslog("[ffnetdev] VNC: ->bitsPerPixel: %d\n",msg.spf.format.bitsPerPixel);
						dsyslog("[ffnetdev] VNC: ->depth: %d\n",msg.spf.format.depth);
						dsyslog("[ffnetdev] VNC: ->bigEndian: %d\n",msg.spf.format.bigEndian);
						dsyslog("[ffnetdev] VNC: ->trueColour: %d\n",msg.spf.format.trueColour);
						dsyslog("[ffnetdev] VNC: ->redMax: %d\n",Swap16IfLE(msg.spf.format.redMax));
						dsyslog("[ffnetdev] VNC: ->greenMax: %d\n",Swap16IfLE(msg.spf.format.greenMax));
						dsyslog("[ffnetdev] VNC: ->blueMax: %d\n",Swap16IfLE(msg.spf.format.blueMax));
						dsyslog("[ffnetdev] VNC: ->redShift: %d\n",msg.spf.format.redShift);
						dsyslog("[ffnetdev] VNC: ->greenShift: %d\n",msg.spf.format.greenShift);
						dsyslog("[ffnetdev] VNC: ->blueShift: %d\n",msg.spf.format.blueShift);
						
						if ( (msg.spf.format.bitsPerPixel!=8 ) 
						   &&(msg.spf.format.bitsPerPixel!=16)
						   &&(msg.spf.format.bitsPerPixel!=32)) {
							esyslog("[ffnetdev] SORRY!!! VNC client requested unsupported pixel format! Closing connection...\n");
							CloseOSDClient();
							return;
						}
						else
							ClientFormat = msg.spf.format;
						dsyslog("[ffnetdev] VNC: RGB %d %d %d %d %d %d\n", 
							ClientFormat.redShift, ClientFormat.redMax, ClientFormat.greenShift, ClientFormat.greenMax, ClientFormat.blueShift, ClientFormat.blueMax);
						break;
		case rfbFixColourMapEntries:	if (!RFBRead( ((char*)&msg.fcme)+1, sz_rfbFixColourMapEntriesMsg-1))
							return;
						isyslog("[ffnetdev] VNC: FixColourMapEntries (ignored).\n");
						break;
		case rfbSetEncodings:		if (!RFBRead( ((char*)&msg.se)+1, sz_rfbSetEncodingsMsg-1))
							return;
					
						dsyslog("[ffnetdev] VNC: SetEncodings: Client offers %d encodings:\n", Swap16IfLE(msg.se.nEncodings));

						CARD32 encodings[16];
						if (!RFBRead( ((char*)&encodings), Swap16IfLE(msg.se.nEncodings)*sizeof(CARD32) ))
							return;

						for (int i=0;i<Swap16IfLE(msg.se.nEncodings);i++) {
							switch (Swap32IfLE(encodings[i])) {
								case rfbEncodingRaw: 		
									if (m_pEncoder == NULL)
									{
									    dsyslog("[ffnetdev] VNC: ->RAW encoding.\n");
									    m_pEncoder = new vncEncoder();
									}
									break;
								case rfbEncodingCopyRect:	
									isyslog("[ffnetdev] VNC: ->CopyRect encoding(not supported).\n");
									break;
								case rfbEncodingRRE: 		
									if (m_pEncoder == NULL)
									{
									    dsyslog("[ffnetdev] VNC: ->RRE encoding.\n");
									    m_pEncoder = new vncEncodeRRE();
									}
									break;
								case rfbEncodingCoRRE: 		
									if (m_pEncoder == NULL)
									{
									    isyslog("[ffnetdev] VNC: ->CoRRE encoding(hav a bug).\n");
									    m_pEncoder = new vncEncodeCoRRE();
									}
									break;
								case rfbEncodingHextile:	
									if (m_pEncoder == NULL)
									{
									    isyslog("[ffnetdev] VNC: ->Hextile encoding.\n");
									    m_pEncoder = new vncEncodeHexT();
									}
									break;
								case rfbEncodingZlib: 		
									if (m_pEncoder == NULL)
									{
									    isyslog("[ffnetdev] VNC: ->Zlib encoding(not supported).\n");
									    //m_pEncoder = new vncEncodeZlib();
									}
									break;
								case rfbEncodingTight: 		
									if (m_pEncoder == NULL)
									{
									    isyslog("[ffnetdev] VNC: ->Tight encoding(not supported).\n");
									    //m_pEncoder = new vncEncodeTight();
									}
									break;
								case rfbEncodingZlibHex:	
									if (m_pEncoder == NULL)
									{
									    isyslog("[ffnetdev] VNC: ->ZlibHex encoding(not supported).\n");
									    //m_pEncoder = new vncEncodeZlibHex();
									}
									break;
								case rfbEncodingZRLE:		
									if (m_pEncoder == NULL)
									{
									    isyslog("[ffnetdev] VNC: ->ZRLE encoding(not supported).\n");
									}
									break;
								case rfbEncSpecialUseAlpha:	
									if (m_pEncoder == NULL)
									{
									    isyslog("[ffnetdev] VNC: ->Special FFnetDev Encoding: client wants alpha channel.\n");
									}
									UseAlpha = true;
									break;
								default:			
									esyslog("[ffnetdev] VNC: ->Unknown encoding or unknown pseudo encoding.\n");
							}
							
							if (m_pEncoder != NULL)
							{
							    m_pEncoder->Init();
							    m_pEncoder->SetLocalFormat(ServerFormat, 720, 576);
							    m_pEncoder->SetCompressLevel(6);
							    m_pEncoder->SetQualityLevel(9);
							    m_pEncoder->SetRemoteFormat(ClientFormat);
							}
						}
						break;
		case rfbFramebufferUpdateRequest:
						if (!RFBRead( ((char*)&msg.fur)+1, sz_rfbFramebufferUpdateRequestMsg-1))
							return;
#ifdef DEBUG
						fprintf(stderr, "[ffnetdev] VNC: FramebufferUpdateRequest: Client wants %s:\n",
										msg.fur.incremental? "incremental update":"complete update");
						fprintf(stderr, "[ffnetdev] VNC: x: %d, y: %d, w: %d, h: %d\n", 
								Swap16IfLE(msg.fur.x),
								Swap16IfLE(msg.fur.y),
								Swap16IfLE(msg.fur.w),
								Swap16IfLE(msg.fur.h)
							);
#endif
						dsyslog("[ffnetdev] VNC: FramebufferUpdateRequest: Client wants %s:\n",
										msg.fur.incremental? "incremental update":"complete update");
						dsyslog("[ffnetdev] VNC: x: %d, y: %d, w: %d, h: %d\n", 
								Swap16IfLE(msg.fur.x),
								Swap16IfLE(msg.fur.y),
								Swap16IfLE(msg.fur.w),
								Swap16IfLE(msg.fur.h)
							);
							
						if (numOSDColors > 0)
						{
						    SendCMAP(numOSDColors, OSDColors);
						    SendScreen(720, //stride
							Swap16IfLE(msg.fur.x), Swap16IfLE(msg.fur.y),
						        Swap16IfLE(msg.fur.w), Swap16IfLE(msg.fur.h),
						  	&m_OSDBuffer);
						}
						else
						{
						    ClearScreen();
						}
						break;
		case rfbKeyEvent:		if (!RFBRead( ((char*)&msg.ke)+1, sz_rfbKeyEventMsg-1))
							return;
#ifdef DEBUG
						fprintf(stderr, "[ffnetdev] VNC: KeyEvent\n");
#endif
						dsyslog("[ffnetdev] VNC: KeyEvent\n");
						cMyRemote *pRemote;
						if ((pRemote = m_pPlugin->GetRemote()) != NULL) {	
							if (msg.ke.down) 
								pRemote->Put(Swap32IfLE(msg.ke.key), false, false);
							else 
								pRemote->Put(Swap32IfLE(msg.ke.key), false, true);
						}
#ifdef DEBUG
						fprintf(stderr, "[ffnetdev] VNC: Remote: %04X\n", msg.ke.key);
#endif									
						dsyslog("[ffnetdev] VNC: Remote: %04X\n", msg.ke.key);
						break;
		case rfbPointerEvent:		if (!RFBRead( ((char*)&msg.pe)+1, sz_rfbPointerEventMsg-1))
							return;
						//fprintf(stderr, "[ffnetdev] VNC: PointerEvent\n");
						break;
 		case rfbClientCutText:		if (!RFBRead( ((char*)&msg.cct)+1, sz_rfbClientCutTextMsg-1))
							return;
#ifdef DEBUG
						fprintf(stderr, "[ffnetdev] VNC: ClientCutText\n");
#endif
						dsyslog("[ffnetdev] VNC: ClientCutText\n");
						break;
		default:
#ifdef DEBUG
						fprintf(stderr, "[ffnetdev] VNC: Unknown RFB message.\n");
#endif
						dsyslog("[ffnetdev] VNC: Unknown RFB message.\n");
		}
	}
}

void cOSDWorker::Action(void) {
	cTBSelect select;
	cTBSocket m_OSDListen;
	        
	const char* m_ListenIp = "0.0.0.0";
	uint m_OSDListenPort = OSDPort;
	
	m_OSDClient	= new cTBSocket;
	
	m_Active = true;

        if (!m_OSDListen.Listen(m_ListenIp, m_OSDListenPort, 1)) {
		esyslog("[ffnetdev] VNC: Couldn't listen %s:%d: %s", m_ListenIp, m_OSDListenPort, strerror(errno));
		m_Active = false;
	} 
	else
	        isyslog("[ffnetdev] VNC: Listening on port %d", m_OSDListenPort);
	        
            
	while (m_Active) {
		select.Clear();
		
		if (state==NO_CLIENT)
			select.Add(m_OSDListen, false);
		else
			select.Add(*m_OSDClient, false);
		
		int numfd;
		/* React on status change of any of the above file descriptor */
		if ((numfd=select.Select(1000)) < 0) {
			if (!m_Active) // Exit was requested while polling
				continue;
			esyslog("FFNetDev: Fatal error, FFNetDev exiting: %s", strerror(errno));
			m_Active = false;
			continue;
		}
		
		//DEBUG
		/*
		fprintf(stderr, "Num_FD OSD: %d\n", numfd);
		
		if (select.CanRead(m_OSDListen) || select.CanWrite(m_OSDListen)) 
			fprintf (stderr, "m_OSDListen can act.\n");
		if (select.CanRead(*m_OSDClient) || select.CanWrite(*m_OSDClient)) 
			fprintf (stderr, "m_OSDClient can act.\n");
		*/
		//fprintf(stderr, "State: %d\n",state);	
		
		int res;
		switch (state) {
		
			case NO_CLIENT: /* Accept connecting OSD clients  */		
					if (select.CanRead(m_OSDListen) ) {
		    				if (m_OSDClient->Accept(m_OSDListen)) {
							isyslog("[ffnetdev] VNC: Accepted client %s:%d",
								m_OSDClient->RemoteIp().c_str(), m_OSDClient->RemotePort());
							state = CLIENT_CONNECTED;
							/* Sending RFB protocol version */
							rfbProtocolVersionMsg msg;
							sprintf(msg, rfbProtocolVersionFormat, rfbProtocolMajorVersion, rfbProtocolMinorVersion);
							if (!m_OSDClient->SafeWrite(&msg, sz_rfbProtocolVersionMsg))
							{   
							    state = NO_CLIENT;
							}
							else
							{
							    m_pPlugin->SetPrimaryDevice();
							}
							break;					
							
		    				} 
		    				else {
							esyslog("[ffnetdev] VNC: Couldn't accept : %s", strerror(errno));
							state = NO_CLIENT;
							m_Active = false;
							continue;
		    				}
					}
					break;

			case CLIENT_CONNECTED: 	
						rfbProtocolVersionMsg pvmsg;
						res = m_OSDClient->Read(&pvmsg, sz_rfbProtocolVersionMsg);
#ifdef DEBUG
		 			        fprintf(stderr, "[ffnetdev] VNC: Client wants RFB protocol version %s\n", pvmsg);
#endif
						dsyslog("[ffnetdev] VNC: Client wants RFB protocol version %s\n", pvmsg);
		 			        if (1) {//FIXME:protocol ok
		 			        	state = PROTOCOL_OK;
		 			        	/* Sending security type */
		 			        	CARD32 secmsg;
		 			        	secmsg = Swap32IfLE(rfbNoAuth);
		 			        	if (!m_OSDClient->SafeWrite(&secmsg, sizeof(secmsg)))
							{   
							    state = NO_CLIENT;
							    break;
							}
		 			        }
		 			        else {
							state = NO_CLIENT;
#ifdef DEBUG
							fprintf(stderr, "[ffnetdev] VNC: Client wants unsupported protocol version.\n");
#endif
							dsyslog("[ffnetdev] VNC: Client wants unsupported protocol version.\n");
							if ( m_OSDClient->Close() ) {
#ifdef DEBUG
								fprintf(stderr, "[ffnetdev] VNC: Client socket closed successfully.\n");
#endif				
								isyslog("[ffnetdev] VNC: Connection closed.");
							}
							else {
#ifdef DEBUG
					   			fprintf(stderr, "[ffnetdev] VNC: Error closing client socket.\n");
#endif
					   			esyslog("[ffnetdev] VNC: Error closing connection.");
					   			m_Active=false;
					   			continue;
							} 		 			    
		 			        }	
						break;
			case PROTOCOL_OK:	/* Since we do not need authentication challenge(rfbNoAuth), proceed to next state */
						state = AUTHENTICATED;
						break;
			case AUTHENTICATED:	
						rfbClientInitMsg cimsg;
						res = m_OSDClient->Read(&cimsg, sz_rfbClientInitMsg);
#ifdef DEBUG
						fprintf(stderr, "[ffnetdev] VNC: Client wants %s desktop(ignored).\n", 
									cimsg.shared ? "shared":"non-shared");
#endif
						dsyslog("[ffnetdev] VNC: Client wants %s desktop(ignored).\n", 
									cimsg.shared ? "shared":"non-shared");
											
						rfbPixelFormat pxfmt;
						pxfmt.bitsPerPixel = 8;
						pxfmt.depth = 8;
						pxfmt.bigEndian = 0;
						pxfmt.trueColour = 0;
						pxfmt.redMax = 0;
						pxfmt.greenMax = 0;
						pxfmt.blueMax = 0;
						pxfmt.redShift = 0;
						pxfmt.greenShift = 0;
						pxfmt.blueShift = 0;
						pxfmt.pad1 = 0;
						pxfmt.pad2 = 0;
						
						rfbServerInitMsg simsg;
						simsg.framebufferWidth = Swap16IfLE(720);
						simsg.framebufferHeight = Swap16IfLE(576);
						simsg.format = pxfmt;
						simsg.nameLength = Swap32IfLE(7);
						if (!m_OSDClient->SafeWrite(&simsg, sizeof(simsg)))
						{   
						    state = NO_CLIENT;
						    break;
						}
						
					 	CARD8 name[7];
					 	strcpy((char*)&name, "VDR-OSD");
					 	if (!m_OSDClient->SafeWrite(&name, 7))
						{   
						    state = NO_CLIENT;
						    break;
						}
					 	
						state = HANDSHAKE_OK;
						break;

			case HANDSHAKE_OK:	
						
						HandleClientRequests(&select);

						break;		
			default: fprintf(stderr, "[ffnetdev] VNC: Undefined state! This is a bug! Please report.\n");
				 esyslog("[ffnetdev] VNC: Fatal error, FFNetDev exiting: undefined state");
				 m_Active = false;
				 continue;
		}
		
		
		
		struct timeval curtime;
		gettimeofday(&curtime, 0);
		curtime.tv_sec = curtime.tv_sec - (((int)curtime.tv_sec / 1000000) * 1000000);
		if ((curtime.tv_sec * 1000 + (curtime.tv_usec / 1000) > m_lastupdate.tv_sec * 1000 + (m_lastupdate.tv_usec / 1000) + 500) &&
		    (m_notupdatedLeft != -1) && (m_notupdatedTop != -1) && (m_notupdatedRight != -1) && 
		    (m_notupdatedBottom != -1))
		{
		    memset(&m_lasttime, 0, sizeof(m_lasttime));
		    SendScreen(720, m_notupdatedLeft, m_notupdatedTop, 
			       m_notupdatedRight  - m_notupdatedLeft, 
			       m_notupdatedBottom - m_notupdatedTop, 
			       &(m_Instance->m_OSDBuffer[m_notupdatedTop * 720 + m_notupdatedLeft]));
		}
	} // while(m_Active)

}
