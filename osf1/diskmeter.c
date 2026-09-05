/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "diskmeter.h"
#include "osf1stats.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

/*  These are the counters iostat(1) reports.  They count each transfer
 *  without recording its direction, so unlike the other ports this meter
 *  has a single traffic field instead of separate reads and writes, and
 *  takes its colour from diskUsedColor rather than the read and write
 *  pair.  */

static void checkres(Meter *m) {
  DiskMeter *dm = (DiskMeter *)m;
  FieldMeter *fm = &dm->f;

  fieldmeter_checkresources(m);

  m->priority = atoi(xwin_getresource(m->xw, "diskPriority"));

  /*  fieldmeter_disable() collapsed this meter to a single field, so setting
   *  the per field colours below would run off the end of the array.  */
  if (!dm->ok)
    return;

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "diskUsedColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "diskIdleColor"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "diskDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "diskGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "diskUsedFormat"));
}

static void getdiskinfo(DiskMeter *dm) {
  FieldMeter *fm = &dm->f;
  double now;

  fm->total = dm->maxspeed;

  if (!osf1stats_disk(&now))
    return;

  fieldmeter_timerstop(fm);

  if (dm->first) {
    dm->prev = now;
    dm->first = 0;
  }

  fm->fields[0] = (now - dm->prev) / fieldmeter_secs(fm);

  fieldmeter_timerstart(fm);
  dm->prev = now;

  if (fm->fields[0] > fm->total)
    fm->total = fm->fields[0];
  fm->fields[1] = fm->total - fm->fields[0];

  fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  getdiskinfo((DiskMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *diskmeter_new(XOSView *parent, float max) {
  DiskMeter *dm = (DiskMeter *)meter_alloc(sizeof *dm);
  double bytes;

  fieldmeter_init(&dm->f, parent, 2, "DiskMeter", "DISK", "XFER/IDLE",
                  0, 0, 0);
  dm->f.m.checkres = checkres;
  dm->f.m.checkevent = checkevent;

  dm->maxspeed = max;
  dm->prev = 0;
  dm->first = 1;
  dm->ok = osf1stats_disk(&bytes);

  if (!dm->ok)
    fieldmeter_disable(&dm->f);

  return &dm->f.m;
}
