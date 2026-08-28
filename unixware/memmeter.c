/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "memmeter.h"
#include "unixwarestats.h"
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
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "memFreeColor"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "memDecay");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "memUsedFormat"));
}

static void getmeminfo(MemMeter *mm) {
  /*  The kernel keeps freefilemem identical to freemem on this release, so
   *  the file cache can not be shown as a field of its own.  */
  FieldMeter *fm = &mm->f;

  if (!unixwarestats_memory(&fm->total, &fm->fields[1]))
    return;

  fm->fields[0] = fm->total - fm->fields[1];

  fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  getmeminfo((MemMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *memmeter_new(XOSView *parent) {
  MemMeter *mm = (MemMeter *)meter_alloc(sizeof *mm);
  double total, freemem;

  fieldmeter_init(&mm->f, parent, 2, "MemMeter", "MEM", "USED/FREE",
                  0, 0, 0);
  mm->f.m.checkres = checkres;
  mm->f.m.checkevent = checkevent;

  mm->ok = unixwarestats_memory(&total, &freemem);

  if (!mm->ok)
    fieldmeter_disable(&mm->f);

  return &mm->f.m;
}
