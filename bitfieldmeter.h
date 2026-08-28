/*
 *  Copyright (c) 1999, 2006 Thomas Waldmann (ThomasWaldmann@gmx.de)
 *  based on work of Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _BITFIELDMETER_H_
#define _BITFIELDMETER_H_

#include "meter.h"
#include <sys/time.h>

/*  A row of bits on the left half of the meter and a field bar on the
 *  right.  The field half repeats FieldMeter's machinery rather than
 *  reusing it, because its geometry is half-width throughout.  */
struct BitFieldMeter {
  Meter m;

  int numfields;
  double *fields;
  double total, used, lastused;
  int *lastvals, *lastx;
  unsigned long *colors;
  unsigned long usedcolor;
  int print;                    /*  enum UsedType  */
  int printedZeroTotalMesg;
  int numWarnings;
  int metric;
  int usedoffset;

  unsigned long oncolor, offcolor;
  char *bits, *lastbits;
  int numbits;
  char *fieldlegend;

  struct timeval tstart, tstop;
};

void bitfieldmeter_init(BitFieldMeter *bfm, XOSView *parent, int numbits,
                        int numfields, const char *name, const char *title,
                        const char *bitlegend, const char *fieldlegend,
                        int docaptions, int dolegends, int dousedlegends);

/*  Meter hooks; a concrete meter installs its own over these.  */
void bitfieldmeter_fini(Meter *m);
void bitfieldmeter_checkresources(Meter *m);
void bitfieldmeter_draw(Meter *m);
void bitfieldmeter_checkevent(Meter *m);

void bitfieldmeter_drawfields(BitFieldMeter *bfm, int mandatory);
void bitfieldmeter_drawbits(BitFieldMeter *bfm, int mandatory);
void bitfieldmeter_drawfieldlegend(BitFieldMeter *bfm);
void bitfieldmeter_drawused(BitFieldMeter *bfm, int mandatory);

void bitfieldmeter_setused(BitFieldMeter *bfm, double val, double total);
void bitfieldmeter_setusedformat(BitFieldMeter *bfm, const char *fmt);
void bitfieldmeter_setcolorname(BitFieldMeter *bfm, int field,
                                const char *color);
void bitfieldmeter_setcolor(BitFieldMeter *bfm, int field,
                            unsigned long color);
void bitfieldmeter_setfieldlegend(BitFieldMeter *bfm, const char *legend);
void bitfieldmeter_setbits(BitFieldMeter *bfm, int startbit,
                           unsigned char values);
void bitfieldmeter_setnumfields(BitFieldMeter *bfm, int n);
void bitfieldmeter_setnumbits(BitFieldMeter *bfm, int n);
void bitfieldmeter_reset(BitFieldMeter *bfm);
void bitfieldmeter_disable(BitFieldMeter *bfm);
int bitfieldmeter_checkx(const BitFieldMeter *bfm, int x, int width);

void bitfieldmeter_timerstart(BitFieldMeter *bfm);
void bitfieldmeter_timerstop(BitFieldMeter *bfm);
double bitfieldmeter_usecs(const BitFieldMeter *bfm);
double bitfieldmeter_secs(const BitFieldMeter *bfm);

#endif
