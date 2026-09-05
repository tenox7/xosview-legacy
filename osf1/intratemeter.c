/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "intratemeter.h"
#include "osf1stats.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

/*  The kernel keeps one interrupt counter for the whole machine and does
 *  not break it down per vector, so there is a rate meter here but no
 *  interrupt meter.  The clock is not counted.  */

static void checkres(Meter *m) {
  IrqRateMeter *im = (IrqRateMeter *)m;
  FieldMeter *fm = &im->f;

  fieldmeter_checkresources(m);

  m->priority = atoi(xwin_getresource(m->xw, "irqratePriority"));

  /*  fieldmeter_disable() collapsed this meter to a single field, so setting
   *  the per field colours below would run off the end of the array.  */
  if (!im->ok)
    return;

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "irqrateUsedColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "irqrateIdleColor"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "irqrateDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "irqrateGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "irqrateUsedFormat"));
  fm->total = 2000;
}

static void getinfo(IrqRateMeter *im) {
  FieldMeter *fm = &im->f;
  double now;

  if (!osf1stats_intr(&now))
    return;

  fieldmeter_timerstop(fm);

  if (im->first) {
    im->lastirqcount = now;
    im->first = 0;
  }

  fm->fields[0] = (now - im->lastirqcount) / fieldmeter_secs(fm);

  fieldmeter_timerstart(fm);
  im->lastirqcount = now;

  if (fm->fields[0] > fm->total)
    fm->total = fm->fields[0];
  fm->fields[1] = fm->total - fm->fields[0];

  fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  getinfo((IrqRateMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *irqratemeter_new(XOSView *parent) {
  IrqRateMeter *im = (IrqRateMeter *)meter_alloc(sizeof *im);
  double count;

  fieldmeter_init(&im->f, parent, 2, "IrqRateMeter", "IRQs",
                  "IRQs per sec/IDLE", 1, 1, 0);
  im->f.m.checkres = checkres;
  im->f.m.checkevent = checkevent;

  im->lastirqcount = 0;
  im->first = 1;
  im->ok = osf1stats_intr(&count);

  if (!im->ok)
    fieldmeter_disable(&im->f);

  return &im->f.m;
}
