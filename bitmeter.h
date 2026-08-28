/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _BITMETER_H_
#define _BITMETER_H_

#include "meter.h"

struct BitMeter {
  Meter m;

  unsigned long oncolor, offcolor;
  char *bits, *lastbits;
  int numbits;
  int disabled;
};

void bitmeter_init(BitMeter *bm, XOSView *parent, const char *name,
                   const char *title, const char *legend, int numbits,
                   int docaptions, int dolegends, int dousedlegends);

/*  Meter hooks; a concrete meter installs its own over these.  */
void bitmeter_fini(Meter *m);
void bitmeter_draw(Meter *m);
void bitmeter_checkevent(Meter *m);

void bitmeter_drawbits(BitMeter *bm, int mandatory);
void bitmeter_setbits(BitMeter *bm, int startbit, unsigned char values);
void bitmeter_setnumbits(BitMeter *bm, int n);
void bitmeter_disable(BitMeter *bm);

#endif
