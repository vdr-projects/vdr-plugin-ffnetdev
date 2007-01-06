/*
 * tsworker.h: TS worker thread 
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _TSWORKER__H
#define _TSWORKER__H

#include <vdr/thread.h>

#include "tools/socket.h"
#include "streamdevice.h"
#include "remote.h"
#include "netosd.h"
#include "ffnetdev.h"

typedef unsigned long long u64;


// --- cTSWorker -------------------------------------------------------------

class cTSWorker : public cThread {
private:
	bool m_Active;
	bool have_Streamclient;
	bool close_Streamclient_request;
	static cTSWorker *m_Instance;
	cStreamDevice *m_StreamDevice;
	cTBSocket *m_StreamClient;
	int TSPort;
	int origPrimaryDevice;
	cPluginFFNetDev *m_pPlugin;	
	u64 get_time(void);

protected:
	virtual void Action(void);
	virtual void ActionTCP(void);
	virtual void ActionUDP(void);
	void Stop(void);
public:
	cTSWorker(void);
	virtual ~cTSWorker();

	static void Init(cStreamDevice*, int, cPluginFFNetDev*);
	static void Exit(void);
	static bool Active(void);
	
	static bool HaveStreamClient(void);
	static void CloseStreamClient(void);
};

inline bool cTSWorker::Active(void) {
    return m_Instance && (m_Instance->have_Streamclient==true);
}

inline bool cTSWorker::HaveStreamClient(void) {
	return m_Instance->have_Streamclient;
}
#endif
