/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  Decay drawing (Oct. 1995) by Brian Grayson ( bgrayson@netbsd.org ),
 *  graph drawing (Oct. 1998) by Scott McNab ( jedi@tartarus.uwa.edu.au ).
 *
 *  This file may be distributed under terms of the GPL or of the BSD
 *  license, whichever you choose.  The full license notices are
 *  contained in the files COPYING.GPL and COPYING.BSD.
 */

#ifndef _FIELDMETER_H_
#define _FIELDMETER_H_

#include "meter.h"
#include <sys/time.h>

/*
 *  The C++ version split this over FieldMeter, FieldMeterDecay and
 *  FieldMeterGraph, each overriding drawfields() and falling back to its
 *  base when its own flag was off.  The three are one struct here, and
 *  fieldmeter_drawfields() picks the style from usegraph/dodecay.
 */
struct FieldMeter {
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

  /*  decay style  */
  int dodecay;
  int firsttime;                /*  set up decaying fields right the first
                                    time  */
  double *decay, *lastdecayval;

  /*  graph style  */
  int usegraph;
  int graphnumcols, graphpos;
  double *heightfield;
  int lastwinstate;

  struct timeval tstart, tstop;
};

void fieldmeter_init(FieldMeter *fm, XOSView *parent, int numfields,
                     const char *name, const char *title, const char *legend,
                     int docaptions, int dolegends, int dousedlegends);

/*  Meter hooks; a concrete meter installs its own over these.  */
void fieldmeter_fini(Meter *m);
void fieldmeter_checkresources(Meter *m);
void fieldmeter_draw(Meter *m);
void fieldmeter_checkevent(Meter *m);

void fieldmeter_drawfields(FieldMeter *fm, int mandatory);
void fieldmeter_drawlegend(FieldMeter *fm);
void fieldmeter_drawused(FieldMeter *fm, int mandatory);

void fieldmeter_setused(FieldMeter *fm, double val, double total);
void fieldmeter_setusedformat(FieldMeter *fm, const char *fmt);
void fieldmeter_setcolorname(FieldMeter *fm, int field, const char *color);
void fieldmeter_setcolor(FieldMeter *fm, int field, unsigned long color);
void fieldmeter_setnumfields(FieldMeter *fm, int n);
void fieldmeter_setnumcols(FieldMeter *fm, int n);
void fieldmeter_reset(FieldMeter *fm);
void fieldmeter_disable(FieldMeter *fm);
int fieldmeter_checkx(const FieldMeter *fm, int x, int width);

void fieldmeter_timerstart(FieldMeter *fm);
void fieldmeter_timerstop(FieldMeter *fm);
double fieldmeter_usecs(const FieldMeter *fm);
double fieldmeter_secs(const FieldMeter *fm);

#endif
