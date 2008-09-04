/* 
 * remote.c: remote control 
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
 

#include <vdr/interface.h>

#include "osdworker.h"
#include "remote.h"
 
cMyRemote::cMyRemote(const char *Name)
:cRemote(Name)
{

}

bool cMyRemote::Ready(void)
{
	return true;
}

bool cMyRemote::Initialize(void)
{
	return true;
}

bool cMyRemote::Put(uint64_t Code, bool Repeat, bool Release)
{
#if VDRVERSNUM >= 10347
    if ((cRemote::IsLearning()) && (Release))
	return false;
    else
#endif
	return cRemote::Put(Code, Repeat ,Release);
}



cLearningThread::cLearningThread(void)
{
    Start();
}

cLearningThread::~cLearningThread(void)
{
}

void cLearningThread::Action(void)
{
    int i = 10;
    while ((!cOSDWorker::ClientIsReady()) && (i-- > 0))
	sleep(1);
	
    i = 10;
    while ((cOsd::IsOpen() > 0) && (i-- > 0))
	cRemote::Put(kBack);

    if (!cOSDWorker::ClientIsReady() || (cOsd::IsOpen() > 0))
    {	
	dsyslog("[ffnetdev] ClientIsReady=%d OsdIsOpen=%d SkinsIsOpen=%d", 
	    cOSDWorker::ClientIsReady(), cOsd::IsOpen(), Skins.IsOpen());
	delete this;
	return;
    }

    dsyslog("[ffnetdev] start learning keys");
    Interface->LearnKeys();
    
    delete this;
}
