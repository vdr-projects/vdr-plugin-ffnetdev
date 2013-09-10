/*
 * tableinitcmtemplate.c - template for initialising lookup tables for
 * translation from a colour map to true colour.
 *
 * This file shouldn't be compiled.  It is included multiple times by
 * translate.c, each time with a different definition of the macro OUTBPP.
 * For each value of OUTBPP, this file defines a function which allocates an
 * appropriately sized lookup table and initialises it.
 *
 * I know this code isn't nice to read because of all the macros, but
 * efficiency is important here.
 */

#if !defined(OUTBPP)
#error "This file shouldn't be compiled."
#error "It is included as part of translate.c"
#endif

#define OUT_T CONCAT2E(CARD,OUTBPP)
#define SwapOUT(x) CONCAT2E(Swap,OUTBPP) (x)
#define rfbInitColourMapSingleTableOUT \
				CONCAT2E(rfbInitColourMapSingleTable,OUTBPP)
				
#include <vdr/plugin.h>
#include "osdworker.h"

// THIS CODE HAS BEEN MODIFIED FROM THE ORIGINAL UNIX SOURCE
// TO WORK FOR WINVNC.  THE PALETTE SHOULD REALLY BE RETRIEVED
// FROM THE VNCDESKTOP OBJECT, RATHER THAN FROM THE OS DIRECTLY

static void
rfbInitColourMapSingleTableOUT (char **table,
								rfbPixelFormat *in,
								rfbPixelFormat *out)
{
	fprintf(stderr, "[ffnetdev] VNC: rfbInitColourMapSingleTable called\n");

	// ALLOCATE SPACE FOR COLOUR TABLE

    int nEntries = 1 << in->bitsPerPixel;

	// Allocate the table
    if (*table) free(*table);
    *table = (char *)malloc(nEntries * sizeof(OUT_T));
	if (*table == NULL)
	{
		fprintf(stderr, "[ffnetdev] VNC: failed to allocate translation table\n");
		return;
	}

	// Obtain the system palette
	/*
	HDC hDC = GetDC(NULL);
	PALETTEENTRY palette[256];
	if (GetSystemPaletteEntries(hDC,
		0, 256, palette) == 0)
	{
		fprintf(stderr, "[ffnetdev] VNC: failed to get system palette, error=%d\n",
					 GetLastError());
		ReleaseDC(NULL, hDC);
		return;
	}
	ReleaseDC(NULL, hDC);
	*/
	
	const tColor *pColors;
	int    NumColors;	
	cOSDWorker::GetOSDColors(&pColors, &NumColors);

	// COLOUR TRANSLATION

	// We now have the colour table intact.  Map it into a translation table
    int i, r, g, b;
    OUT_T outRed, outGreen, outBlue;
    OUT_T *t = (OUT_T *)*table;
    
    const tColor *pColor = pColors;
    for (i = 0; i < NumColors; i++)
	{
		// Split down the RGB data
		r = (((tColor)*pColor) >> in->redShift)   & in->redMax;
		g = (((tColor)*pColor) >> in->greenShift) & in->greenMax;
		b = (((tColor)*pColor) >> in->blueShift)  & in->blueMax;

		
		outRed	 = (r / (256 / (1 + Swap16IfLE(out->redMax  )))) & Swap16IfLE(out->redMax);
		outGreen = (g / (256 / (1 + Swap16IfLE(out->greenMax)))) & Swap16IfLE(out->greenMax);
		outBlue	 = (b / (256 / (1 + Swap16IfLE(out->blueMax )))) & Swap16IfLE(out->blueMax);
		
		// Now translate it
		t[i] = ((outRed   << out->redShift) |
			(outGreen << out->greenShift) |
			(outBlue  << out->blueShift));
#if (OUTBPP != 8)
		if (out->bigEndian != in->bigEndian)
		{
			t[i] = SwapOUT(t[i]);
		}
#endif
	    pColor++;
	}

	fprintf(stderr, "[ffnetdev] VNC: rfbInitColourMapSingleTable done\n");
}

#undef OUT_T
#undef SwapOUT
#undef rfbInitColourMapSingleTableOUT
