/* 
 * pes2ts.c: PES2TS remux
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
#include <vdr/tools.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "tsworker.h"

#include "pes2ts.h"

//////////////////////////////////////////////////////////////////////////////
cPESRemux::cPESRemux(int inputBufferSize): 
   m_InputBuffer(new cRingBufferLinear(inputBufferSize, IPACKS))
{
   OutputLocked = false;
   m_InputBuffer->SetTimeouts(0, 1000);  // IMPORTANT to avoid busy wait in threads main loop and thus a high CPU load
}

cPESRemux::~cPESRemux()
{
   delete m_InputBuffer;
}

int cPESRemux::Put(const uchar *Data, int Count)
{
  InputMutex.Lock();
  int result = m_InputBuffer->Put(Data, Count);
  InputMutex.Unlock();
  return ( result);
}


//////////////////////////////////////////////////////////////////////////////
cPES2TSRemux::cPES2TSRemux(int VPid, int APid): cPESRemux(INPUTBUFSIZE),
    cThread("[ffnetdev] PES2TS remux"),
    m_OutputBuffer(new cRingBufferLinear(OUTPUTBUFSIZE, TS_SIZE * 2)),
    m_Active(false)
{
  vpid = VPid;
  apid = APid;
  Start();
}

cPES2TSRemux::~cPES2TSRemux()
{
  m_Active = false;
  delete m_OutputBuffer;
}

void cPES2TSRemux::Action(void)
{
  unsigned int i;
  uchar acc=0;    // continutiy counter for audio packets
  uchar vcc=0;      // continutiy counter for video packets
  uchar *cc;        // either cc=&vcc; or cc=&acc;
  unsigned short pid=0;
  unsigned int packetlen;
  uchar ts[188];
  uchar pes[IPACKS];

  unsigned int minNeededPacketlen = 10; // needed for read packet len: 6 Should be enought ... but makes no sense

  
  m_Active = true;
  while (m_Active) {
    int count=0;
//    fprintf(stderr, "[ffnetdev] Remuxer: Inputbuffersize: %d, Outputbuffersize: %d\n", 
//	    m_InputBuffer->Available(), m_OutputBuffer->Available());

    if (m_InputBuffer->Available() < (int)IPACKS*10) {	
	cCondWait::SleepMs(5);
	continue;
    }

    if (!cTSWorker::HaveStreamClient()) {
	ClearOutput();
	cCondWait::SleepMs(10);
	continue;
    }
    

    InputMutex.Lock();
    uchar *data = m_InputBuffer->Get(count);
    if (data==NULL) {
       InputMutex.Unlock();
       cCondWait::SleepMs(3);
       continue;
    }

//    fprintf(stderr, "[ffnetdev] count: %07d Free: %07d AvailO %07d AvailI: %07d\n", count, m_InputBuffer->Free(),
//	    m_OutputBuffer->Available(), m_InputBuffer->Available());

    if ( count < (int)minNeededPacketlen )	{
       dsyslog("[ffnetdev] Remuxer: not enought bytes for PacketLen-Analysis, have only: %d\n", count);
       InputMutex.Unlock();
       cCondWait::SleepMs(2);
     continue;
    }

    //DEBUG
    //fprintf(stderr, "Data ready to read: %d\n", count);
    //for (i=0; i<20; i++)
    //  fprintf(stderr, "%02X ", data[i]);
    // END DEBUG  
      

       // check for valid PES signature in PES header
    if ( (data[0]==0x00) && (data[1]==0x00) && (data[2]==0x01) ) {
          
          packetlen = ((data[4]<<8) | data[5]) + 6 ;
    
          if ( packetlen>IPACKS) {
              dsyslog("[ffnetdev] Remuxer: IPACKS changed? packet length was %d, maximum: %d\n"
                      "This should not happen! Please report!\n", packetlen, IPACKS);
          }

	  if ( count < (int)packetlen) {
	       dsyslog("[ffnetdev] Remuxer: not enought bytes for whole packet, have only: %d but LenShoud be %d\n", count, packetlen);
	       InputMutex.Unlock();
    	       cCondWait::SleepMs(1);
	       continue;
	  }


          // check for valid stream id type: is it video or audio or unknown?
          if ( (data[3]>=0xC0) && (data[3]<=0xDF) || data[3] == 0xBD ) {
              pid=apid;
              cc=&acc;
          } 
          else {
              if ( (data[3]>=0xE0) && (data[3]<=0xEF) ) {
                  pid=vpid;
                  cc=&vcc;
              } 
              else {
                  dsyslog("[ffnetdev] Remuxer: Unknown stream id: neither video nor audio type.\n");
                  // throw away whole PES packet
                  m_InputBuffer->Del(packetlen);
                  InputMutex.Unlock();
                  continue;
              }
          }
          
          memcpy( pes, data, packetlen);
          // we are now finished with the PES packet, delete it from ring buffer
          m_InputBuffer->Del(packetlen);    
	  
          InputMutex.Unlock();
    }
    else {
      // no valid PES signature was found, so delete this stuff from ring buffer
      // normally we should always receive a whole PES packet, since VDR always gives us a whole packet and not less or more
      // with each call in streamdevice.c (PlayVideo, PlayAudio)
      dsyslog("[ffnetdev] Remuxer: No valid PES signature found. This should not happen.\n");

      m_InputBuffer->Del(1); // Probably it is better to delete 1 byte only to get in sync again!?
      InputMutex.Unlock();
      continue;
    } 
    
    int tspacketlen = (packetlen/184) * 188  + ((packetlen % 184 > 0) ? 188 : 0); 
    while (m_OutputBuffer->Free() < tspacketlen) 
    {	
      if (!m_Active)
         continue;
   	cCondWait::SleepMs(10);
   	//dsyslog("[ffnetdev] Remuxer: sleep %d %d\n", m_OutputBuffer->Free(), tspacketlen);
    }

    LockOutput();
    bool first = true;    
  //--------------------------------------divide PES packet into small TS packets-----------------------    
    for (i=0; i< packetlen/184; i++) {
      ts[0] = 0x47; //SYNC Byte
      if (first) ts[1] = 0x40;        // Set PUSI or
      else       ts[1] = 0x00;        // clear PUSI,  TODO: PID (high) is missing
      ts[2] = pid & 0xFF;             // PID (low)  
      ts[3] = 0x10 | ((*cc)&0x0F);    // No adaptation field, payload only, continuity counter 
      memcpy(ts + 4, pes + i * 184, 184);
      ++(*cc);
      m_OutputBuffer->Put(ts, 188);
      first = false;
    }
    uchar rest = packetlen % 184;
    if (rest>0) {
      ts[0] = 0x47; //SYNC Byte
      if (first) ts[1] = 0x40;        // Set PUSI or
      else       ts[1] = 0x00;        // clear PUSI,  TODO: PID (high) is missing
      ts[2] = pid & 0xFF;             // PID (low)  
      ts[3] = 0x30 | ((*cc)&0x0F);    // adaptation field, payload, continuity counter 
      ++(*cc);
      ts[4] = 183-rest;
      if (ts[4]>0) {
        ts[5] = 0x00;
        memset(ts + 6, 0xFF, ts[4] - 1);
      }
      memcpy(ts + 5 + ts[4], pes + i * 184, rest);
      m_OutputBuffer->Put(ts, 188);
      first = false;
    }
    
    UnlockOutput();
	
  }
  m_Active = false;
}


//////////////////////////////////////////////////////////////////////////////
cPES2PESRemux::cPES2PESRemux(): cPESRemux(INPUTBUFSIZE),
    cThread("[ffnetdev] PES2PES remux"),
    m_OutputBuffer(new cRingBufferLinear(OUTPUTBUFSIZE, IPACKS)),
    m_Active(false)
{
   m_OutputBuffer->SetTimeouts(0, 1000);
   Start();
}

cPES2PESRemux::~cPES2PESRemux()
{
  m_Active = false;
  delete m_OutputBuffer;
}

void cPES2PESRemux::Action(void)
{
   unsigned int packetlen;
   unsigned int minNeededPacketlen = 10; // needed for read packet len: 6 Should be enought ... but makes no sense
   
   m_Active = true;
   
   while (m_Active) 
   {
      int count=0;
      //    fprintf(stderr, "[ffnetdev] Remuxer: Inputbuffersize: %d, Outputbuffersize: %d\n", 
      //	    m_InputBuffer->Available(), m_OutputBuffer->Available());
      
      if (m_InputBuffer->Available() < (int)IPACKS*10) 
      {	
         cCondWait::SleepMs(5);
         continue;
      }
      
      if (!cTSWorker::HaveStreamClient()) 
      {
         ClearOutput();
         cCondWait::SleepMs(10);
         continue;
      }
      
      
      InputMutex.Lock();
      uchar *data = m_InputBuffer->Get(count);
      if (data==NULL) 
      {
         InputMutex.Unlock();
         cCondWait::SleepMs(3);
         continue;
      }
      
      if ( count < (int)minNeededPacketlen )	
      {
         dsyslog("[ffnetdev] Remuxer: not enought bytes for PacketLen-Analysis, have only: %d\n", count);
         InputMutex.Unlock();
         cCondWait::SleepMs(2);
         continue;
      }
      
      // check for valid PES signature in PES header
      if ( (data[0]==0x00) && (data[1]==0x00) && (data[2]==0x01) ) 
      {    
         packetlen = ((data[4]<<8) | data[5]) + 6 ;
         
         if ( packetlen>IPACKS) {
           dsyslog("[ffnetdev] Remuxer: IPACKS changed? packet length was %d, maximum: %d\n"
                   "This should not happen! Please report!\n", packetlen, IPACKS);
         }
   
         if ( count < (int)packetlen) 
         {
            dsyslog("[ffnetdev] Remuxer: not enought bytes for whole packet, have only: %d but LenShoud be %d\n", count, packetlen);
            InputMutex.Unlock();
            cCondWait::SleepMs(1);
            continue;
         }
      
      
         // check for valid stream id type: is it video or audio or unknown?
         if ( ((data[3]>=0xC0) && (data[3]<=0xDF)) || 
              (data[3] == 0xBD) || 
              ((data[3]>=0xE0) && (data[3]<=0xEF)))
         {
            while (m_OutputBuffer->Free() < (int)packetlen) 
            {	
               if (!m_Active)
               continue;
               cCondWait::SleepMs(10);
               //dsyslog("[ffnetdev] Remuxer: sleep %d %d\n", m_OutputBuffer->Free(), tspacketlen);
            }
    
            LockOutput();
            m_OutputBuffer->Put(data, packetlen);
            // we are now finished with the PES packet, delete it from ring buffer
            m_InputBuffer->Del(packetlen); 
            UnlockOutput();  
         } 
         else if (data[3]>=0xBE)
         {
            //dsyslog("[ffnetdev] Remuxer: Padding stream removed.\n");
            m_InputBuffer->Del(packetlen);
            InputMutex.Unlock();
            continue;
         }
         else 
         {
            dsyslog("[ffnetdev] Remuxer: Unknown stream id: neither video nor audio type.\n");
            // throw away whole PES packet
            m_InputBuffer->Del(packetlen);
            InputMutex.Unlock();
            continue;
         }
         
         InputMutex.Unlock();
      }
      else 
      {
         // no valid PES signature was found, so delete this stuff from ring buffer
         // normally we should always receive a whole PES packet, since VDR always gives us a whole packet and not less or more
         // with each call in streamdevice.c (PlayVideo, PlayAudio)
         dsyslog("[ffnetdev] Remuxer: No valid PES signature found. This should not happen.\n");
         
         m_InputBuffer->Del(1); // Probably it is better to delete 1 byte only to get in sync again!?
         InputMutex.Unlock();
         continue;
      } 
   }
   m_Active = false;
}
