/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "memmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <sys/pstat.h>
#include <unistd.h>

static void checkres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "memTextColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "memUsedColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "memOtherColor"));
  fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "memFreeColor"));
  m->priority = atoi(xwin_getresource(m->xw, "memPriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "memDecay");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "memUsedFormat"));
}

static void getmeminfo(MemMeter *mm) {
  FieldMeter *fm = &mm->f;
  struct pst_dynamic stats;
  struct pst_vminfo vmstats;

  pstat_getdynamic(&stats, sizeof(stats), 1, 0);
  pstat_getvminfo(&vmstats, sizeof(vmstats), 1, 0);

  fm->fields[0] = stats.psd_rmtxt + stats.psd_arm;
  fm->fields[1] = stats.psd_rm - stats.psd_rmtxt;
  fm->fields[2] = fm->total - fm->fields[0] - fm->fields[1] - stats.psd_free;
  fm->fields[3] = stats.psd_free;

  fieldmeter_setused(fm, (fm->total - fm->fields[3]) * mm->pageSize,
                     fm->total * mm->pageSize);
}

static void checkevent(Meter *m) {
  MemMeter *mm = (MemMeter *)m;

  mm->pass = (mm->pass + 1) % 5;
  if (mm->pass != 0)
    return;

  getmeminfo(mm);
  fieldmeter_drawfields(&mm->f, 0);
}

Meter *memmeter_new(XOSView *parent) {
  MemMeter *mm = (MemMeter *)meter_alloc(sizeof *mm);
  struct pst_static pststatic;

  fieldmeter_init(&mm->f, parent, 4, "MemMeter", "MEM",
                  "TEXT/USED/OTHER/FREE", 0, 0, 0);
  mm->f.m.checkres = checkres;
  mm->f.m.checkevent = checkevent;

  pstat_getstatic(&pststatic, sizeof(struct pst_static), 1, 0);
  mm->f.total = pststatic.physical_memory;
  mm->pageSize = (int)pststatic.page_size;
  mm->pass = 0;

  return &mm->f.m;
}
