/* 
 * streamdevice.h: streaming network device
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
 
#ifndef _STREAMDEVICE__H
#define _STREAMDEVICE__H
 
#define TS_VPID 99
#define TS_APID 100

#include <vdr/device.h>

#include "pes2ts.h"

class cStreamDevice: public cDevice {
private:
	  cPES2TSRemux *m_Remux;
protected:
public:
  cStreamDevice(void);
  ~cStreamDevice(void);
  virtual bool HasDecoder(void) const;
  virtual bool CanReplay(void) const;
  virtual bool SetPlayMode(ePlayMode PlayMode);
  virtual void TrickSpeed(int Speed);
  virtual void Clear(void);
  virtual void Play(void);
  virtual void Freeze(void);
  virtual void Mute(void);
  virtual void SetVolumeDevice (int Volume);
  virtual void StillPicture(const uchar *Data, int Length);
  virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
  virtual int PlayVideo(const uchar *Data, int Length);
  virtual int PlayAudio(const uchar *Data, int Length, uchar Id);
  virtual int ProvidesCa(const cChannel *Channel) const;
  virtual void MakePrimaryDevice(bool On);
  uchar *Get(int &Count) { return m_Remux->Get(Count); }
  void LockOutput()   { m_Remux->LockOutput(); }
  void UnlockOutput() { m_Remux->UnlockOutput(); }
  void Del(int Count) { m_Remux->DelOutput(Count); }
  void ClearOutput() { m_Remux->ClearOutput(); }
  int Available(void) { return m_Remux->Available(); }
};

#endif
