/* 
 * ffnetdev.h: Full Featured Network Device VDR-Plugin for Streaming
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
#ifndef _FFNETDEV__H
#define _FFNETDEV__H

#include <vdr/plugin.h>
#include "streamdevice.h"
#include "remote.h"

#define OSDPORT 20001
#define STREAMPORT 20002
#define CONTROLPORT 20005

class cPluginFFNetDev : public cPlugin {
private:
	static const char *DESCRIPTION;
	static const char *VERSION;
	//static const char *MAINMENUENTRY;
	cStreamDevice *m_StreamDevice;
	cMyRemote *m_Remote;
	int OSDPort;
	int TSPort;
	int ControlPort;
	bool EnableRemote;
	int  m_origPrimaryDevice;	
	char m_ClientName[20];
	cLearningThread *m_LearningThread;
	
public:
  cPluginFFNetDev(void);
  virtual ~cPluginFFNetDev();
  virtual const char *Description(void); 
  virtual const char *Version(void);
  virtual const char *CommandLineHelp(void);
  virtual bool Initialize(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Start(void);
  virtual void Housekeeping(void);
  //virtual const char *MainMenuEntry(void) { return tr(MAINMENUENTRY); }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  
#ifndef VDRVERSNUM
#define VDRVERSNUM 10346
#endif

#if VDRVERSNUM >= 10347
  virtual cString Active(void);
#else
  virtual bool Active(void);
#endif
 

  void SetPrimaryDevice();
  void RestorePrimaryDevice();
  cMyRemote *GetRemote() { return m_Remote; }
  void SetClientName(char* ClientName);
};

#endif
