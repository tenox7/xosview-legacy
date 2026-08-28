/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _XOSVIEW_H_
#define _XOSVIEW_H_

#include "fwd.h"
#include "Xrm.h"
#include "xwin.h"

/*  Take at most n samples per second (default of 10)  */
extern double MAX_SAMPLES_PER_SECOND;

enum WindowVisibility { WV_FULLY_VISIBLE, WV_PARTIALLY_VISIBLE, WV_OBSCURED };

struct XOSView {
  XWin xw;              /*  must come first: an XOSView is an XWin  */
  Xrm xrm;

  Meter **meters;
  int nummeters, metercap;

  int caption, legend, xoff, yoff, usedlabels;
  int hmargin, vmargin, vspacing;
  unsigned long sleeptime, usleeptime;

  int deferred_resize, deferred_redraw;
  int visibility;       /*  enum WindowVisibility  */
};

void xosview_init(XOSView *xv, const char *instName, int argc, char **argv);
void xosview_fini(XOSView *xv);
void xosview_run(XOSView *xv);

/*  Used by the meter makers.  */
const char *xosview_getresource(XOSView *xv, const char *name);
int xosview_isresourcetrue(XOSView *xv, const char *name);
const char *xosview_getresource_default(XOSView *xv, const char *name,
                                        const char *defaultVal);
void xosview_setdone(XOSView *xv, int val);
void xosview_addmeter(XOSView *xv, Meter *m);
int xosview_xoff(const XOSView *xv);
int xosview_newypos(const XOSView *xv);
int xosview_visibility(const XOSView *xv);

/*  Prints only in a DEBUG build; a plain function rather than a variadic
 *  macro, which neither C89 nor the vendor compilers have.  */
void xosdebug(const char *fmt, ...);
#define XOSDEBUG xosdebug

#endif
