/* 
 * pes2ts.h: PES2TS remux
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef PES2TSREMUX_H
#define PES2TSREMUX_H

#include <vdr/ringbuffer.h>
#include <vdr/tools.h>

#define INPUTBUFSIZE	KILOBYTE(2048)
#define OUTPUTBUFSIZE	KILOBYTE(2048)
#define TS_SIZE         188
#define IPACKS		2048

class cPES2TSRemux: public cThread {
private:
	cRingBufferLinear *m_OutputBuffer;
	cRingBufferLinear *m_InputBuffer;
	bool               m_Active;
	unsigned short	   vpid;
	unsigned short	   apid;
        bool  	   	   OutputLocked;
	cMutex  	   InputMutex;
	bool 		   m_PlayModeChanged;
  
protected:
	virtual void Action(void);

public:
	cPES2TSRemux(int VPid, int APid);
	virtual ~cPES2TSRemux();

	int Free(void) { return m_InputBuffer->Free(); }
	int Available(void) { return m_OutputBuffer->Available(); }
	int Put(const uchar *Data, int Count);
	uchar *Get(int &Count) { return  m_OutputBuffer->Get(Count); }
	void DelOutput(int Count) { m_OutputBuffer->Del(Count); }
	void DelInput (int Count) { InputMutex.Lock(); m_InputBuffer ->Del(Count); InputMutex.Unlock(); }
	void ClearOutput() { LockOutput(); m_OutputBuffer->Clear(); UnlockOutput(); }
	void ClearInput () { InputMutex.Lock(); m_InputBuffer ->Clear(); InputMutex.Unlock(); }
	void LockOutput()   { while (OutputLocked) cCondWait::SleepMs(1); OutputLocked = true; }
	void UnlockOutput() { OutputLocked = false; }
	void PlayModeChange() { m_PlayModeChanged = true; } 
};

#endif // PES2TSREMUX_H
