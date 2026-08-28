/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *  2007 by Samuel Thibault ( samuel.thibault@ens-lyon.org )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "memmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <error.h>

#include <mach/mach_traps.h>
#include <mach/mach_interface.h>

static void checkres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "memActiveColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "memInactiveColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "memCacheColor"));
  fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "memFreeColor"));
  m->priority = atoi(xwin_getresource(m->xw, "memPriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "memDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "memGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "memUsedFormat"));
}

static void getmeminfo(MemMeter *mm) {
  FieldMeter *fm = &mm->f;
  kern_return_t err;

  err = vm_statistics(mach_task_self(), &mm->vmstats);
  if (err) {
    error(0, err, "vm_statistics");
    xwin_setdone(fm->m.xw, 1);
    return;
  }

  fm->fields[0] = (double)mm->vmstats.active_count * mm->vmstats.pagesize;
  fm->fields[1] = (double)mm->vmstats.inactive_count * mm->vmstats.pagesize;
  fm->fields[2] = (double)mm->vmstats.wire_count * mm->vmstats.pagesize;
  fm->fields[3] = (double)mm->vmstats.free_count * mm->vmstats.pagesize;
  fm->total = fm->fields[0] + fm->fields[1] + fm->fields[2] + fm->fields[3];

  fieldmeter_setused(fm, fm->total - fm->fields[3], fm->total);
}

static void checkevent(Meter *m) {
  getmeminfo((MemMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *memmeter_new(XOSView *parent) {
  MemMeter *mm = (MemMeter *)meter_alloc(sizeof *mm);

  fieldmeter_init(&mm->f, parent, 4, "MemMeter", "MEM", "ACT/INACT/WIRE/FREE",
                  0, 0, 0);
  mm->f.m.checkres = checkres;
  mm->f.m.checkevent = checkevent;

  return &mm->f.m;
}
