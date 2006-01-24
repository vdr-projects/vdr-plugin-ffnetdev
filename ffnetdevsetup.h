#ifndef __FFNETDEVSETUP_H
#define __FFNETDEVSETUP_H

#include <vdr/plugin.h>

class cFFNetDevSetup : public cMenuSetupPage 
{
private:
  int m_iAutoSetPrimaryDVB;
protected:
  virtual void Store(void);
public:
  cFFNetDevSetup(void);
};

#endif //__FFNETDEVSETUP_H

