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

bool cMyRemote::Put(uint64 Code, bool Repeat, bool Release)
{
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
    while (!cOSDWorker::ClientIsReady())
	usleep(100000);
	
    while (cOsd::IsOpen() > 0)
	usleep(100000);

    sleep(5);
    dsyslog("[ffnetdev] start learning keys");
    Interface->LearnKeys();
    delete this;
}
