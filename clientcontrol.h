/*
 * clientcontrol.h: ClientControl thread 
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _CLIENTCONTROL__H
#define _CLIENTCONTROL__H

#include <vdr/thread.h>

#include "tools/socket.h"
#include "ffnetdev.h"

enum CCPakType{ ptInfo=0, ptPlayState, ptPlayStateReq, ptStillPicture };

struct SClientControl
{
   char pakType;
   int dataLen;
   char data[0];
};

struct SClientControlInfo
{
   char clientName[20];
};

struct SClientControlPlayState
{
   char PlayMode;
   bool Play;
   bool Forward;
   char Speed;
};

struct SClientControlStillPicture
{
   uchar *Data;
   int Length;
};


// --- cClientControl -------------------------------------------------------------

class cClientControl : public cThread {
private:
	bool m_Active;
	bool m_bHaveClient;
	bool m_bCloseClientRequest;
	static cClientControl *m_Instance;
	cTBSocket *m_ClientSocket;
	int m_iPort;
	cPluginFFNetDev *m_pPlugin;
	bool m_bPlayStateReq;

protected:
	virtual void Action(void);
	void Stop(void);
public:
	cClientControl(void);
	virtual ~cClientControl();

	static void Init(int, cPluginFFNetDev*);
	static void Exit(void);
	static bool Active(void);
	
	static bool HaveClient(void);
	static void CloseStreamClient(void);
	
	static bool SendPlayState(ePlayMode PlayMode, bool bPlay, bool bForward, int iSpeed);
	static bool PlayStateReq(void) { return m_Instance->m_bPlayStateReq; };
	static bool SendStillPicture(const uchar *Data, int Length);
};

inline bool cClientControl::Active(void) {
    return m_Instance && (m_Instance->m_bHaveClient==true);
}

inline bool cClientControl::HaveClient(void) {
	return m_Instance->m_bHaveClient;
}
#endif
