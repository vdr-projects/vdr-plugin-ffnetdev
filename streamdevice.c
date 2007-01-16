/* 
 * streamdevice.c: streaming network device
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
 
#include "streamdevice.h"
#include "osdworker.h"
#include "tsworker.h"
#include "netosd.h"

cStreamDevice::cStreamDevice(void)
{
    dsyslog("[ffnetdev] Device: Constructor cStreamDevice \n");
    //m_Remux = new cPES2TSRemux(TS_VPID, TS_APID);   
    m_Remux = new cPES2PESRemux();   
}

cStreamDevice::~cStreamDevice(void)
{
   dsyslog("[ffnetdev] Device: Destructor cStreamDevice \n");
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
   m_Remux->ClearInput();
   m_Remux->ClearOutput();
   m_Remux->PlayModeChange();
   return true;
}

void cStreamDevice::TrickSpeed(int Speed)
{
   dsyslog("[ffnetdev] Device: Trickspeed(not implemented). Speed: %d\n", Speed);
   m_Remux->ClearInput();
   m_Remux->ClearOutput();
   m_Remux->PlayModeChange();
}

void cStreamDevice::Clear(void)
{
   dsyslog("[ffnetdev] Device: Clear(not implemented).\n");
   m_Remux->ClearInput();
   m_Remux->ClearOutput();
   m_Remux->PlayModeChange();
//    cDevice::Clear();
}
void cStreamDevice::Play(void)
{
   dsyslog("[ffnetdev] Device: Play(not implemented).\n");
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
   dsyslog("[ffnetdev] Device: StillPicture(not implemented).\n");
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
       while ((m_Remux->InputFree() < Length) && cTSWorker::HaveStreamClient())
           cCondWait::SleepMs(1);
       int result=m_Remux->Put(Data, Length);
       if (result!=Length) {
         fprintf(stderr,"[ffnetdev] Device: Did not put all in input buffer(audio). result:%d Length: %d Skipping Audio PES packet...\n", result, Length );
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

       while ((m_Remux->InputFree() < Length) && cTSWorker::HaveStreamClient())
           cCondWait::SleepMs(1);
       int result=m_Remux->Put(Data, Length);
       if (result!=Length) {
         fprintf(stderr,"[ffnetdev] Device: Did not put all in input buffer(video). result:%d Length: %d Skipping Video PES packet...\n", result, Length );
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
