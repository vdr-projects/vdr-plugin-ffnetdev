/* 
 * remote.h: remote control
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
 
#ifndef REMOTE_H
#define REMOTE_H

#include <vdr/remote.h>
#include <vdr/thread.h>

class cMyRemote : public cRemote {
private:
public:
  cMyRemote(const char *Name);
  virtual bool Initialize(void);
  virtual bool Ready(void);
  virtual bool Put(uint64 Code, bool Repeat = false, bool Release = false);
};


class cLearningThread : public cThread {
private:

public:
    cLearningThread(void);
    virtual  ~cLearningThread(void);
    
protected:
    virtual void Action(void);
};

#endif

