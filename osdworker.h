/*
 * osdworker.h: OSD worker thread 
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _OSDWORKER__H
#define _OSDWORKER__H

#include <vdr/thread.h>

#include "tools/socket.h"
#include "tools/select.h"
#include "streamdevice.h"
#include "remote.h"
#include "netosd.h"
#include "ffnetdev.h"

#include "vncEncoder.h"

#define MAXOSDMEMORY 2048000

#define rfbEncSpecialUseAlpha 0xFFFFF000	// server shall send alpha values in addition to RGB values

/*
 * Macros for endian swapping.
 */
/*
#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))

#define Swap32(l) (((l) >> 24) | \
		   (((l) & 0x00ff0000) >> 8)  | \
		   (((l) & 0x0000ff00) << 8)  | \
		   ((l) << 24))

static const int rfbEndianTest = 1;	// true means swap TODO: check machine endianess - for now -> hardcoded

#define Swap16IfLE(s) (*(const char *)&rfbEndianTest ? Swap16(s) : (s))

#define Swap32IfLE(l) (*(const char *)&rfbEndianTest ? Swap32(l) : (l))
*/

// --- cOSDWorker -------------------------------------------------------------
enum states {
		NO_CLIENT,
		CLIENT_CONNECTED,
	        PROTOCOL_OK,
	        AUTHENTICATED,
	        HANDSHAKE_OK
};
		
class cOSDWorker : public cThread {
private:
	bool		 m_Active;
	
	static cOSDWorker *m_Instance;
	
	cTBSocket 	*m_OSDClient;
	
	bool 		close_OSDclient_request;
	
	int 		OSDPort;
	int 		state;
	bool 		UseAlpha;
	bool 		FirstUpdateRequest;
	int 		numOSDColors;
	rfbPixelFormat  ClientFormat;
	rfbPixelFormat  ServerFormat;
	
	BYTE		m_OSDBuffer[720*576];
//	BYTE		m_oldOSDData[720*576];
	BYTE		*m_pSendBuffer;
	int		m_SendBufferSize;
	tColor 		OSDColors[256];

	void HandleClientRequests(cTBSelect *select);
	bool RFBRead(char *buffer, int len);
	void CreateSendBuffer(int OSDBufferSize);
	
	vncEncoder      *m_pEncoder;
	
	struct timeval  m_lasttime;
	struct timeval  m_lastupdate;
	int		m_notupdatedLeft;
	int		m_notupdatedTop;
	int		m_notupdatedRight;
	int		m_notupdatedBottom;
	
	cPluginFFNetDev *m_pPlugin;
	
protected:
	virtual void Action(void);
	void Stop(void);
public:
	cOSDWorker(void);
	virtual ~cOSDWorker();

	static void Init(int, cPluginFFNetDev*);
	static void Exit(void);
	static bool Active(void);
	static bool ClientIsReady(void);
	
	static void CloseOSDClient(void);

	static bool ClearScreen(void);
        static bool SendScreen(unsigned int stride, unsigned int x1, unsigned int y1, unsigned int w, unsigned int h, const void *data);
    	static bool SendCMAP(int NumColors, const tColor *Colors);
    	static bool OSDWrite(unsigned char *data, unsigned int data_length);
    	static bool SendPlayMode(ePlayMode PlayMode);
	static void GetOSDColors(tColor **OSDColors, int *numOSDColors) { *OSDColors = &m_Instance->OSDColors[0]; (*numOSDColors) = m_Instance->numOSDColors; };
};

inline bool cOSDWorker::Active(void) {
    return m_Instance && (m_Instance->state!=NO_CLIENT);
}

inline bool cOSDWorker::ClientIsReady(void) {
    return m_Instance && (m_Instance->state==HANDSHAKE_OK);
}

#endif
