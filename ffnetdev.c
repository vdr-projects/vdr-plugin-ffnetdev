/* 
 * ffnetdev.c: Full Featured Network Device VDR-Plugin for Streaming 
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
 
#include <getopt.h>
#include <stdlib.h>

#include <vdr/tools.h>

//#include "i18n.h"
#include "tsworker.h"
#include "netosd.h"
#include "ffnetdev.h"
#include "streamdevice.h"
#include "osdworker.h"
#include "clientcontrol.h"
#include "config.h"
#include "ffnetdevsetup.h"


static const char *VERSION = "0.1.3";
const char *cPluginFFNetDev::VERSION = VERSION;
const char *cPluginFFNetDev::DESCRIPTION 		= trNOOP("Full Featured Network Device for Streaming");
//const char *cOSDWorker::MAINMENUENTRY 		= "FFNetDev";
 
// --- cNetOSDProvider -----------------------------------------------

cNetOSDProvider::cNetOSDProvider(void)
{
	
}

cOsd * cNetOSDProvider::CreateOsd(int Left, int Top, uint Level)
{
    
    osd = new cNetOSD(Left, Top, Level);
    return osd;
}


// --- cPluginFFNetDev ----------------------------------------------------------

const char *cPluginFFNetDev::Version(void) {
		return tr(VERSION);
}

const char *cPluginFFNetDev::Description(void) {
		return tr(DESCRIPTION);
}

cPluginFFNetDev::cPluginFFNetDev(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
  TSPort = STREAMPORT;
  OSDPort = OSDPORT;
  ControlPort = CONTROLPORT;
  EnableRemote = false;
  m_origPrimaryDevice = -1;
  m_Remote = NULL;
  m_LearningThread = NULL;
  
  config.iAutoSetPrimaryDVB = 0;
}

cPluginFFNetDev::~cPluginFFNetDev()
{
  dsyslog("[ffnetdev] Destructor cPluginFFNetDev\n");

  cOSDWorker::Exit();
  cTSWorker::Exit();
  cClientControl::Exit();
  
  delete m_Remote;
}

const char *cPluginFFNetDev::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return 
  "  -t PORT, --tsport PORT      port number for sending TS to.\n"
  "  -o PORT, --osdport PORT     listen on this port for OSD connect.\n"
  "  -c PORT, --controlport PORT listen on this port for ClientControl connect.\n"
  "  -e 			 enable remote control over OSD connection.\n"
  "\n";
}

bool cPluginFFNetDev::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  dsyslog("[ffnetdev] processing args.\n");
  static struct option long_options[] = {
      { "tsport",    		required_argument	, NULL, 't' },
      { "osdport",   		required_argument	, NULL, 'o' },
      { "controlport",   	required_argument	, NULL, 'c' },
      { "enable-remote",   	no_argument		, NULL, 'e' },
      { NULL, 			0			, NULL, 0 }
  };

  int c;
  while ((c = getopt_long(argc, argv, "t:o:c:e", long_options, NULL)) != -1) {
        switch (c) {
          case 'e': EnableRemote = true;
		    dsyslog("[ffnetdev] Remote enabled\n");
          	    break;
          case 't': if (isnumber(optarg)) {
                       int n = atoi(optarg);
                       if (0 < n && n < 65536) {
                          TSPort = n;
                          dsyslog("[ffnetdev] TS Port: %d\n", n);
                          break;
                       }
                    }
                    esyslog("[ffnetdev] invalid port number: %s\n", optarg);
                    return 2;
                    break;
          case 'o': if (isnumber(optarg)) {
                       int n = atoi(optarg);
                       if (0 < n && n < 65536) {
                          OSDPort = n;
                          dsyslog("[ffnetdev] OSD Port: %d\n", n);
                          break;
                       }
                    }
                    esyslog("[ffnetdev] invalid port number: %s\n", optarg);
                    return 2;
                    break;
          case 'c': if (isnumber(optarg)) {
                       int n = atoi(optarg);
                       if (0 < n && n < 65536) {
                          ControlPort = n;
                          dsyslog("[ffnetdev] ClientControl Port: %d\n", n);
                          break;
                       }
                    }
                    esyslog("[ffnetdev] invalid port number: %s\n", optarg);
                    return 2;
                    break;
	  default : return 2;
	}
  }
  dsyslog("[ffnetdev] finished processing args.\n");
  return true;
}

#if VDRVERSNUM >= 10347
cString cPluginFFNetDev::Active(void) {

    if(cOSDWorker::Active() || cTSWorker::Active() || cClientControl::Active())
           return tr("ffnetdev is running");
 
    return NULL;
}
#else
bool cPluginFFNetDev::Active(void) {
    return (cOSDWorker::Active() || cTSWorker::Active() || cClientControl::Active());
}
#endif

bool cPluginFFNetDev::Start(void)
{
  // Start any background activities the plugin shall perform.
//  RegisterI18n(Phrases);
  
  	  
  cOSDWorker::Init(OSDPort, this);
  cTSWorker::Init(m_StreamDevice, TSPort, this);
  cClientControl::Init(ControlPort, this);
 
  return true;
}

bool cPluginFFNetDev::Initialize(void)
{
  // Start any background activities the plugin shall perform.
  dsyslog("[ffnetdev] initializing plugin.\n");
  m_StreamDevice = new cStreamDevice();
  return true;
}

void cPluginFFNetDev::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cOsdObject *cPluginFFNetDev::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  dsyslog("[ffnetdev] MainMenuAction called.\n");
//  return new cMenuSetupSoftdevice;
  return NULL;
}

cMenuSetupPage *cPluginFFNetDev::SetupMenu(void)
{
  // Return our setup menu
//  return new cMenuSetupFFNetDev;
  return new cFFNetDevSetup;
}

bool cPluginFFNetDev::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  if (!strcasecmp(Name, "AutoSetPrimaryDVB"))     config.iAutoSetPrimaryDVB = atoi(Value);
  else
    return false;
    
  return true;
}

void cPluginFFNetDev::SetPrimaryDevice()
{	
   int i = 0;
   while ((cOsd::IsOpen() > 0) && (i-- > 0))
      cRemote::Put(kBack);
      
   i = 0;
   while ((strlen(m_ClientName) == 0) && (i++ < 2))
      sleep(1); 
   
   if ((config.iAutoSetPrimaryDVB == 1) && (m_origPrimaryDevice == -1))
   {
      cDevice *PrimaryDevice;
      if ((PrimaryDevice = cDevice::PrimaryDevice()) != NULL)
         m_origPrimaryDevice = PrimaryDevice->DeviceNumber() + 1;
      else
         m_origPrimaryDevice = -1;
      
      if (m_StreamDevice->DeviceNumber() + 1 != m_origPrimaryDevice)
      {
         cDevice::SetPrimaryDevice(m_StreamDevice->DeviceNumber() + 1);
         isyslog("[ffnetdev] set Primary Device to %d", m_StreamDevice->DeviceNumber() + 1);
      }
      else
      {
         m_origPrimaryDevice = -1;
      }
   }
   
   if(EnableRemote) 
   {
      if (m_Remote == NULL)
      {            
         char str[30];
         if (strlen(m_ClientName) > 0)
            sprintf(str, "ffnetdev-%s", m_ClientName);
         else
            strcpy(str, "ffnetdev");
         
         m_Remote = new cMyRemote(str);
      }

      if (!cRemote::HasKeys())
      {
         m_LearningThread = new cLearningThread();
      }
      dsyslog("[ffnetdev] remote control enabled.\n");
   }
   else 
   {
      dsyslog("[ffnetdev] remote control disabled.\n");
   }
}


void cPluginFFNetDev::RestorePrimaryDevice()
{
   int i = 10;
   while ((cOsd::IsOpen() > 0) && (i-- > 0))
      cRemote::Put(kBack);
   
   dsyslog("[ffnetdev] remote control disabled.\n");
   
   if (m_origPrimaryDevice != -1)
   {
      cDevice::SetPrimaryDevice(m_origPrimaryDevice);
      isyslog("[ffnetdev] restore Primary Device to %d", m_origPrimaryDevice);
      m_origPrimaryDevice = -1;
      sleep(5);
   }					
   
   m_ClientName[0] = '\0';
   if (m_Remote)
   {
      Remotes.Del(m_Remote);
      m_Remote = NULL;
   }
   
   if (m_LearningThread)
   {
      delete m_LearningThread;
      m_LearningThread = NULL;
   }
}

void cPluginFFNetDev::SetClientName(char* ClientName)
{
   strcpy(m_ClientName, ClientName);
   dsyslog("[ffnetdev] SetClientname %s", m_ClientName);
}


sFFNetDevConfig config;

VDRPLUGINCREATOR(cPluginFFNetDev); // Don't touch this!
