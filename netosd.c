/* 
 * netosd.c: OSD over network
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "netosd.h"
#include "osdworker.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
cNetOSD::cNetOSD(int Left, int Top) : cOsd(Left, Top)
{
#ifdef DEBUG   
   fprintf(stderr,"[ffnetdev] NetOSD: Constructor cNetOSD.\n");
#endif
}

cNetOSD::~cNetOSD()
{
#ifdef DEBUG
   fprintf(stderr,"[ffnetdev] NetOSD: Destructor cNetOSD.\n");
#endif
   
   cOSDWorker::ClearScreen();    
}

eOsdError cNetOSD::CanHandleAreas(const tArea *Areas, int NumAreas)
{
   eOsdError Result = cOsd::CanHandleAreas(Areas, NumAreas);
   if (Result == oeOk) {
      if (NumAreas > 1) 		// Handle only one big area (because we support VNC colour map mode)
         return oeTooManyAreas;		// We cannot handle multiple areas having different colour maps. We need a single colourmap. Thus, only one area.
      int TotalMemory = 0;
      for (int i = 0; i < NumAreas; i++) {
         if (Areas[i].bpp != 1 && Areas[i].bpp != 2 && Areas[i].bpp != 4 && Areas[i].bpp != 8)
            return oeBppNotSupported;
         if ((Areas[i].Width() & (8 / Areas[i].bpp - 1)) != 0)
            return oeWrongAlignment;
         TotalMemory += Areas[i].Width() * Areas[i].Height() / (8 / Areas[i].bpp);
     }
#ifdef DEBUG
     fprintf(stderr, "[ffnetdev] NetOSD: CanHandleAreas: OSD area size: %d bytes.\r\n", TotalMemory); 
#endif
     if (TotalMemory > MAXOSDMEMORY)
        return oeOutOfMemory;
     }
     return Result;
}

void cNetOSD::Flush(void)
{
    cBitmap *Bitmap;

    for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++)
    {
       int x1=0, x2=0, y1=0, y2=0;
       if (Bitmap->Dirty(x1, y1, x2, y2))   
       {  
  		// commit colors:
       		int NumColors;
       		const tColor *Colors = Bitmap->Colors(NumColors);
       		if (Colors) {
       			cOSDWorker::SendCMAP(NumColors , Colors);
       		}
#ifdef DEBUG         
	        fprintf(stderr, "[ffnetdev] NetOSD: Left: %d, Top: %d, X0: %d, Y0: %d, Width: %d, Height: %d\n", 
               			 Left(), Top(), Bitmap->X0(), Bitmap->Y0(), Bitmap->Width(), Bitmap->Height()); 
       		fprintf(stderr, "[ffnetdev] NetOSD: Dirty area x1: %d, y1: %d, x2: %d, y2: %d\n",x1,y1,x2,y2);
#endif
       		// commit modified data:
       		cOSDWorker::SendScreen(Bitmap->Width(), Left()+x1 +Bitmap->X0(), Top()+y1+Bitmap->Y0(), x2-x1+1, y2-y1+1, Bitmap->Data(x1, y1)); 
	}
        Bitmap->Clean();
     }
}


