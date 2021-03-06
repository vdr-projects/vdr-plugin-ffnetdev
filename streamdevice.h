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

enum ePlayState {psBufferReset,psBufferReseted,psPlay};

class cStreamDevice: public cDevice {
private:
   cPESRemux *m_Remux;
   bool       m_Playing;
   bool       m_Forward;
   int       m_Speed;
   ePlayState m_PlayState;
   ePlayMode  m_PlayMode;
   
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
#if VDRVERSNUM < 10342   
   virtual int PlayAudio(const uchar *Data, int Length);
#else
   virtual int PlayAudio(const uchar *Data, int Length, uchar Id);
#endif  
   virtual int ProvidesCa(const cChannel *Channel) const;
   virtual void MakePrimaryDevice(bool On);
   uchar *Get(int &Count) { return m_Remux->Get(Count); }
   void LockOutput()   { m_Remux->LockOutput(); }
   void UnlockOutput() { m_Remux->UnlockOutput(); }
   void Del(int Count) { m_Remux->DelOutput(Count); }
   void ClearOutput() { m_Remux->ClearOutput(); }
   int Available(void) { return m_Remux->Available(); }
   int Free(void) { return m_Remux->Free(); }
   void SetPlayState(ePlayState PlayState) { m_PlayState = PlayState;}
   ePlayState GetPlayState(void)           { return m_PlayState; }
};

#endif
