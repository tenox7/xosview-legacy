/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _METER_H_
#define _METER_H_

#include "fwd.h"
#include <stddef.h>

/*
 *  The base of every meter.  Concrete meters embed this as their first
 *  member, so a pointer to one is also a pointer to the other, and the
 *  four hooks below take the place of the C++ virtual functions.
 */
struct Meter {
  XOSView *parent;
  XWin *xw;             /*  == (XWin *)parent; saves a cast at every use  */
  const char *name;     /*  static string, named in warning messages  */
  char *title;
  char *legend;
  int x, y, width, height;
  int docaptions, dolegends, dousedlegends;
  int priority, counter;
  unsigned long textcolor;

  void (*checkevent)(Meter *m);
  void (*checkres)(Meter *m);
  void (*draw)(Meter *m);
  void (*destroy)(Meter *m);   /*  frees the meter's own memory, not itself  */
};

void meter_init(Meter *m, XOSView *parent, const char *name,
                const char *title, const char *legend,
                int docaptions, int dolegends, int dousedlegends);
void meter_fini(Meter *m);

void meter_settitle(Meter *m, const char *title);
void meter_setlegend(Meter *m, const char *legend);
void meter_resize(Meter *m, int x, int y, int width, int height);
void meter_checkresources(Meter *m);
int meter_requestevent(Meter *m);

double meter_samplespersecond(const Meter *m);
double meter_secondspersample(const Meter *m);

double meter_scalevalue(double value, char *scale, int metric);

/*  How the "used" label is formatted.  Shared by FieldMeter and
 *  BitFieldMeter, which format it identically.  */
enum UsedType { UT_FLOAT, UT_PERCENT, UT_AUTOSCALE };

void *meter_alloc(size_t n);   /*  malloc that exits on failure  */

int meter_parseusedformat(const char *fmt);
void meter_formatused(char *buf, size_t bufsize, int print, double used,
                      int metric);

#endif
