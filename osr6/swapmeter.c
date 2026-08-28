/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "swapmeter.h"
#include "osr6stats.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

/*  swapctl(2) lists every swap area, so the totals here cover all of them. */

static void checkres(Meter *m) {
  SwapMeter *sm = (SwapMeter *)m;
  FieldMeter *fm = &sm->f;

  fieldmeter_checkresources(m);

  m->priority = atoi(xwin_getresource(m->xw, "swapPriority"));

  /*  fieldmeter_disable() collapsed this meter to a single field, so setting
   *  the per field colours below would run off the end of the array.  */
  if (!sm->ok)
    return;

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "swapUsedColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "swapFreeColor"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "swapDecay");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "swapUsedFormat"));
}

static void getswapinfo(SwapMeter *sm) {
  FieldMeter *fm = &sm->f;

  if (!osr6stats_swap(&fm->total, &fm->fields[1]))
    return;

  fm->fields[0] = fm->total - fm->fields[1];

  fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  getswapinfo((SwapMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *swapmeter_new(XOSView *parent) {
  SwapMeter *sm = (SwapMeter *)meter_alloc(sizeof *sm);
  double total, freeswap;

  fieldmeter_init(&sm->f, parent, 2, "SwapMeter", "SWAP", "USED/FREE",
                  0, 0, 0);
  sm->f.m.checkres = checkres;
  sm->f.m.checkevent = checkevent;

  sm->ok = osr6stats_swap(&total, &freeswap);

  if (!sm->ok)
    fieldmeter_disable(&sm->f);

  return &sm->f.m;
}
