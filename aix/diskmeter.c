/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "diskmeter.h"
#include "aixstats.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

/*  These are the same counters iostat(1) reports, so they only move when the
 *  kernel is maintaining disk history: see chdev -l sys0 -a iostat=true.  */

static void checkres(Meter *m) {
  DiskMeter *dm = (DiskMeter *)m;
  FieldMeter *fm = &dm->f;

  fieldmeter_checkresources(m);

  m->priority = atoi(xwin_getresource(m->xw, "diskPriority"));

  /*  fieldmeter_disable() collapsed this meter to a single field, so setting
   *  the per field colours below would run off the end of the array.  */
  if (!dm->ok)
    return;

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "diskReadColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "diskWriteColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "diskIdleColor"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "diskDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "diskGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "diskUsedFormat"));
}

static void getdiskinfo(DiskMeter *dm) {
  FieldMeter *fm = &dm->f;
  double readNow, writeNow, t;

  fm->total = dm->maxspeed;

  if (!aixstats_disk(&readNow, &writeNow))
    return;

  fieldmeter_timerstop(fm);

  if (dm->first) {
    dm->readPrev = readNow;
    dm->writePrev = writeNow;
    dm->first = 0;
  }

  /*  On AIX 4.x these are 32 bit kernel counters and they wrap on a busy
   *  disk, which shows up as the total moving backwards.  Skip that sample
   *  rather than plotting an enormous spike.  */
  t = fieldmeter_secs(fm);
  fm->fields[0] = readNow >= dm->readPrev ? (readNow - dm->readPrev) / t : 0.0;
  fm->fields[1] = writeNow >= dm->writePrev
                  ? (writeNow - dm->writePrev) / t : 0.0;

  fieldmeter_timerstart(fm);
  dm->readPrev = readNow;
  dm->writePrev = writeNow;

  if (fm->fields[0] + fm->fields[1] > fm->total)
    fm->total = fm->fields[0] + fm->fields[1];
  fm->fields[2] = fm->total - fm->fields[0] - fm->fields[1];

  fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
}

static void checkevent(Meter *m) {
  getdiskinfo((DiskMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *diskmeter_new(XOSView *parent, float max) {
  DiskMeter *dm = (DiskMeter *)meter_alloc(sizeof *dm);
  double readb, written;

  fieldmeter_init(&dm->f, parent, 3, "DiskMeter", "DISK", "READ/WRITE/IDLE",
                  0, 0, 0);
  dm->f.m.checkres = checkres;
  dm->f.m.checkevent = checkevent;

  dm->maxspeed = max;
  dm->readPrev = dm->writePrev = 0;
  dm->first = 1;
  dm->ok = aixstats_disk(&readb, &written);

  if (!dm->ok)
    fieldmeter_disable(&dm->f);

  return &dm->f.m;
}
