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
#define IPACKS          65536

class cPESRemux 
{
private:
   bool  	   	   OutputLocked;
   
protected:
   cRingBufferLinear *m_InputBuffer;
   cMutex  	         InputMutex;

public:
   cPESRemux(int inputBufferSize);
	virtual ~cPESRemux();
	
   int Put(const uchar *Data, int Count);
   void DelInput (int Count) { InputMutex.Lock(); m_InputBuffer ->Del(Count); InputMutex.Unlock(); }
   void ClearInput () { InputMutex.Lock(); m_InputBuffer ->Clear(); InputMutex.Unlock(); }
   
   virtual int Available(void) = 0;
   virtual int Fill(void) = 0;
   virtual int Free(void) = 0;
   virtual int InputFree(void) = 0;
   virtual uchar *Get(int &Count) = 0;
   virtual void DelOutput(int Count) = 0;
   virtual void ClearOutput() = 0;
   void LockOutput()   { while (OutputLocked) cCondWait::SleepMs(1); OutputLocked = true; }
   void UnlockOutput() { OutputLocked = false; }
};

class cPES2TSRemux: public cPESRemux, cThread 
{
private:
	cRingBufferLinear *m_OutputBuffer;
	bool               m_Active;
	bool               m_Ended;
	unsigned short	   vpid;
	unsigned short	   apid;
  
protected:
	virtual void Action(void);

public:
	cPES2TSRemux(int VPid, int APid);
	virtual ~cPES2TSRemux();

   int Available(void) { return m_OutputBuffer->Available(); }
   int Fill(void) { return m_InputBuffer->Available() + m_OutputBuffer->Available(); }
	int Free(void) { return m_OutputBuffer->Free(); }
	int InputFree(void) { return m_InputBuffer->Free(); }
	uchar *Get(int &Count) { return  m_OutputBuffer->Get(Count); }
	void DelOutput(int Count) { m_OutputBuffer->Del(Count); }
	void ClearOutput() { LockOutput(); m_OutputBuffer->Clear(); UnlockOutput(); }
};

class cPES2PESRemux: public cPESRemux, cThread 
{
private:
	cRingBufferLinear *m_OutputBuffer;
	bool               m_Active;
	bool               m_Ended;
	
protected:
	virtual void Action(void);
	
public:
	cPES2PESRemux();
	virtual ~cPES2PESRemux();

   int Available(void) { return m_OutputBuffer->Available(); }
   int Fill(void) { return m_InputBuffer->Available() + m_OutputBuffer->Available(); }
	int Free(void) { return m_OutputBuffer->Free(); }
	int InputFree(void) { return m_InputBuffer->Free(); }
	uchar *Get(int &Count) { return  m_OutputBuffer->Get(Count); }
	void DelOutput(int Count) { m_OutputBuffer->Del(Count); }
	void ClearOutput() { LockOutput(); m_OutputBuffer->Clear(); UnlockOutput(); }
};

#endif // PES2TSREMUX_H
