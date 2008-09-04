/* 
 * netosd.c: OSD over network
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "netosd.h"
#include "osdworker.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
cNetOSD::cNetOSD(int XOfs, int YOfs, uint Level) : cOsd(XOfs, YOfs, Level)
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
   dsyslog("[ffnetdev] Destructor cNetOSD\n");
   
   cOSDWorker::ClearScreen();    
}

eOsdError cNetOSD::CanHandleAreas(const tArea *Areas, int NumAreas)
{
   eOsdError Result = cOsd::CanHandleAreas(Areas, NumAreas);
   if (Result == oeOk) {
      if (NumAreas > 7)			
    	 return oeTooManyAreas;
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
    int x1=0, x2=0, y1=0, y2=0;

    for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++)
    {   
       if (Bitmap->Dirty(x1, y1, x2, y2))   
       {  
#ifdef DEBUG         
	        //fprintf(stderr, "[ffnetdev] NetOSD: Left: %d, Top: %d, X0: %d, Y0: %d, Width: %d, Height: %d\n", 
               	//		 Left(), Top(), Bitmap->X0(), Bitmap->Y0(), Bitmap->Width(), Bitmap->Height()); 
       		//fprintf(stderr, "[ffnetdev] NetOSD: Dirty area x1: %d, y1: %d, x2: %d, y2: %d\n",x1,y1,x2,y2);
#endif
		cOSDWorker::DrawBitmap(Left() + Bitmap->X0(), Top() + Bitmap->Y0(), *Bitmap);
       		
	}
        Bitmap->Clean();
    }
     
    cOSDWorker::SendScreen(); 
}


