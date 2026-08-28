/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "swapmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <sys/sysinfo.h>

static void checkres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "swapUsedColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "swapFreeColor"));
  m->priority = atoi(xwin_getresource(m->xw, "swapPriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "swapDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "swapGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "swapUsedFormat"));
}

static void getswapinfo(SwapMeter *sm) {
  FieldMeter *fm = &sm->f;
  struct sysinfo sinfo;
  double unit;

  sysinfo(&sinfo);
  unit = sinfo.mem_unit ? sinfo.mem_unit : 1;
  fm->total = (double)sinfo.totalswap * unit;
  fm->fields[0] = (double)(sinfo.totalswap - sinfo.freeswap) * unit;

  if (fm->total == 0) {
    fm->total = 1;
    fm->fields[0] = 0;
  }

  if (fm->total)
    fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  getswapinfo((SwapMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *swapmeter_new(XOSView *parent) {
  SwapMeter *sm = (SwapMeter *)meter_alloc(sizeof *sm);

  fieldmeter_init(&sm->f, parent, 2, "SwapMeter", "SWAP", "USED/FREE",
                  0, 0, 0);
  sm->f.m.checkres = checkres;
  sm->f.m.checkevent = checkevent;

  return &sm->f.m;
}
