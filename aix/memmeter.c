/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "memmeter.h"
#include "aixstats.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

static void checkres(Meter *m) {
  MemMeter *mm = (MemMeter *)m;
  FieldMeter *fm = &mm->f;

  fieldmeter_checkresources(m);

  m->priority = atoi(xwin_getresource(m->xw, "memPriority"));

  /*  fieldmeter_disable() collapsed this meter to a single field, so setting
   *  the per field colours below would run off the end of the array.  */
  if (!mm->ok)
    return;

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "memUsedColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "memCacheColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "memFreeColor"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "memDecay");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "memUsedFormat"));
}

static void getmeminfo(MemMeter *mm) {
  /*  The cache is made up of file pages, which the kernel keeps resident
   *  but will release under pressure, so show them apart from used
   *  memory.  */
  FieldMeter *fm = &mm->f;
  double cache, freemem;

  if (!aixstats_memory(&fm->total, &cache, &freemem))
    return;

  fm->fields[0] = fm->total - cache - freemem;
  fm->fields[1] = cache;
  fm->fields[2] = freemem;

  if (fm->fields[0] < 0)
    fm->fields[0] = 0;

  fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  getmeminfo((MemMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *memmeter_new(XOSView *parent) {
  MemMeter *mm = (MemMeter *)meter_alloc(sizeof *mm);
  double total, cache, freemem;

  fieldmeter_init(&mm->f, parent, 3, "MemMeter", "MEM", "USED/CACHE/FREE",
                  0, 0, 0);
  mm->f.m.checkres = checkres;
  mm->f.m.checkevent = checkevent;

  mm->ok = aixstats_memory(&total, &cache, &freemem);

  if (!mm->ok)
    fieldmeter_disable(&mm->f);

  return &mm->f.m;
}
