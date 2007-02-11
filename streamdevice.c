/* 
 * streamdevice.c: streaming network device
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
 
#include "vdr/player.h"
#include "streamdevice.h"
#include "osdworker.h"
#include "tsworker.h"
#include "netosd.h"
#include "clientcontrol.h"

cStreamDevice::cStreamDevice(void)
{
    dsyslog("[ffnetdev] Device: Constructor cStreamDevice \n");
    //m_Remux = new cPES2TSRemux(TS_VPID, TS_APID);   
    m_Remux = new cPES2PESRemux();  
    m_Playing = false; 
    m_PlayState = psPlay;
    m_PlayMode  = pmNone;
}

cStreamDevice::~cStreamDevice(void)
{
   dsyslog("[ffnetdev] Device: Destructor cStreamDevice \n");
   m_PlayState = psPlay;
   DELETENULL(m_Remux);
}


void cStreamDevice::MakePrimaryDevice(bool On)
{
  dsyslog("[ffnetdev] Device: ffnetdev becomes primary device. Registering our OSD provider...\n");
  new cNetOSDProvider();
}

int cStreamDevice::ProvidesCa(const cChannel *Channel) const
{
   return 0;
}

bool cStreamDevice::HasDecoder(void) const
{
   return true; // We can decode MPEG2
}

bool cStreamDevice::CanReplay(void) const
{
   return true;  // We can replay
}

bool cStreamDevice::SetPlayMode(ePlayMode PlayMode)
{
   dsyslog("[ffnetdev] Device: Setting playmode. Mode: %d\n",PlayMode);
   cOSDWorker::SendPlayMode(PlayMode);
      
   m_PlayMode = PlayMode;
   if (PlayMode == pmNone)
   {
      m_PlayState = psBufferReset;
      m_Remux->ClearInput();
      m_Remux->ClearOutput();
   }
   else
   {
      while (((m_PlayState == psBufferReset) || (m_PlayState == psBufferReseted)) && 
             (!cClientControl::PlayStateReq()) && (cTSWorker::HaveStreamClient()))
         cCondWait::SleepMs(10);
      dsyslog("[ffnetdev] PlayStateReq\n");   
      m_PlayState = psPlay;
   }
   
   cControl *pControl = cControl::Control();
   if (pControl)
   {
      bool Forward;
      int Speed;
      pControl->GetReplayMode(m_Playing, Forward, Speed);
      if (!cClientControl::SendPlayState(PlayMode, m_Playing, Forward, Speed))
         m_PlayState = psPlay;
   }
   else
   {
      m_Playing = false;
      if (!cClientControl::SendPlayState(PlayMode, false, false, 0))
         m_PlayState = psPlay;
   }
   return true;
}

void cStreamDevice::TrickSpeed(int Speed)
{
   dsyslog("[ffnetdev] Device: Trickspeed. Speed: %d\n", Speed);
   cControl *pControl = cControl::Control();
   if (pControl)
   {
      bool Forward;
      int Speed;
      pControl->GetReplayMode(m_Playing, Forward, Speed);
      if (!cClientControl::SendPlayState(m_PlayMode, m_Playing, Forward, Speed))
         m_PlayState = psPlay;
   }
}

void cStreamDevice::Clear(void)
{
   dsyslog("[ffnetdev] Device: Clear(not implemented).\n");
//    cDevice::Clear();
}
void cStreamDevice::Play(void)
{
   dsyslog("[ffnetdev] Device: Play.\n");
   
   cControl *pControl = cControl::Control();
   if (pControl)
   {
      bool Forward;
      int Speed;
      pControl->GetReplayMode(m_Playing, Forward, Speed);
      if (!cClientControl::SendPlayState(m_PlayMode, m_Playing, Forward, Speed))
         m_PlayState = psPlay;
   }
   
//    cDevice::Play();
}

void cStreamDevice::Freeze(void)
{
    dsyslog("[ffnetdev] Device: Freeze(not implemented).\n");
//    cDevice::Freeze();
}

void cStreamDevice::Mute(void)
{
   dsyslog("[ffnetdev] Device: Mute(not implemented).\n");
//    cDevice::Mute();
}

void cStreamDevice::SetVolumeDevice(int Volume)
{
  dsyslog("[ffnetdev] Device: Setting volume to %d (not implemented).\n", Volume);
}

void cStreamDevice::StillPicture(const uchar *Data, int Length)
{
   dsyslog("[ffnetdev] Device: StillPicture %d Bytes.\n", Length);
   cClientControl::SendStillPicture(Data, Length);
}

bool cStreamDevice::Poll(cPoller &Poller, int TimeoutMs)
{
   //dsyslog("[ffnetdev] Device: Poll TimeoutMs: %d ....\n",TimeoutMs);
   return true;
}
/* ----------------------------------------------------------------------------
 */
#if VDRVERSNUM < 10342
int cStreamDevice::PlayAudio(const uchar *Data, int Length)
#else
int cStreamDevice::PlayAudio(const uchar *Data, int Length, uchar Id)
#endif
{
   if (cTSWorker::HaveStreamClient()) 
   {
       while ((((!m_Playing) && (m_Remux->InputFree() < Length)) || 
               ((m_Playing) && ((m_Remux->InputFree() < Length) || 
                                (m_Remux->Fill() > TCP_SEND_SIZE * 10)))) && 
               (cTSWorker::HaveStreamClient()))
           cCondWait::SleepMs(1);
       int result=m_Remux->Put(Data, Length);
       if (result!=Length) {
         dsyslog("[ffnetdev] Device: Did not put all in input buffer(audio). result:%d Length: %d Skipping Audio PES packet...\n", result, Length );
         // Delete part of data already written to buffer 
         m_Remux->DelInput(result);
         return (0);
        }
        else
	{
            return ( Length );
	}
   }
   else
   {
      m_Remux->ClearInput();
//      usleep(10000);
      return ( Length );
   }
  
}

/* ----------------------------------------------------------------------------
 */
int cStreamDevice::PlayVideo(const uchar *Data, int Length)
{
   if (cTSWorker::HaveStreamClient()) 
   {
       while ((((!m_Playing) && (m_Remux->InputFree() < Length)) || 
               ((m_Playing) && ((m_Remux->InputFree() < Length) || 
                                (m_Remux->Fill() > TCP_SEND_SIZE * 10)))) && 
               (cTSWorker::HaveStreamClient()))
           cCondWait::SleepMs(1);
       int result=m_Remux->Put(Data, Length);
       if (result!=Length) {
         dsyslog("[ffnetdev] Device: Did not put all in input buffer(video). result:%d Length: %d Skipping Video PES packet...\n", result, Length );
         // Delete part of data already written to buffer 
         m_Remux->DelInput(result);
         return (0);
       }
       else
       {
           return ( Length );
       }
   }
   else
   {
      m_Remux->ClearInput();
//      usleep(10000);
      return ( Length );
   }
}
