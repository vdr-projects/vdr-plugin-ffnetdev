/*
 * netosd.h: OSD over network
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _NETOSD__H
#define _NETOSD__H

#include <vdr/osd.h>
#include "tools/socket.h"

// --- cNetOSD -----------------------------------------------
class cNetOSD : public cOsd {
private:
	bool truecolor;
protected:
public:
    cNetOSD(int XOfs, int XOfs);
    virtual ~cNetOSD();
    virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
    virtual void Flush(void);
};

// --- cNetOSDProvider ----------------------------------------

class cNetOSDProvider : public cOsdProvider {
private:
    cOsd *osd;
    cNetOSD **NetOSD;
public:
    cNetOSDProvider(void);
    virtual cOsd *CreateOsd(int Left, int Top);
};

#endif //_NETOSD__H

