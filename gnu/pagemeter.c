/*
 *  Copyright (c) 1996, 2004 by Massimiliano Ghilardi ( ghilardi@cibs.sns.it )
 *  2007 by Samuel Thibault ( samuel.thibault@ens-lyon.org )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "pagemeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <error.h>

#include <mach/mach_traps.h>
#include <mach/mach_interface.h>

static void checkres(Meter *m) {
  PageMeter *pm = (PageMeter *)m;
  FieldMeter *fm = &pm->f;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "pageInColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "pageOutColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "pageIdleColor"));
  m->priority = atoi(xwin_getresource(m->xw, "pagePriority"));
  pm->maxspeed *= m->priority / 10.0;
  fm->dodecay = xwin_isresourcetrue(m->xw, "pageDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "pageGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "pageUsedFormat"));
}

static void updateinfo(PageMeter *pm) {
  FieldMeter *fm = &pm->f;
  int i, oldindex = (pm->pageindex + 1) % 2;

  for (i = 0; i < 2; i++) {
    if (pm->pageinfo[oldindex][i] == 0)
      pm->pageinfo[oldindex][i] = pm->pageinfo[pm->pageindex][i];

    fm->fields[i] = pm->pageinfo[pm->pageindex][i]
                    - pm->pageinfo[oldindex][i];
    fm->total += fm->fields[i];
  }

  if (fm->total > pm->maxspeed) {
    fm->fields[2] = 0.0;
  } else {
    fm->fields[2] = pm->maxspeed - fm->total;
    fm->total = pm->maxspeed;
  }

  fieldmeter_setused(fm, fm->total - fm->fields[2], pm->maxspeed);
  pm->pageindex = (pm->pageindex + 1) % 2;
}

static void getpageinfo(PageMeter *pm) {
  FieldMeter *fm = &pm->f;
  struct vm_statistics vmstats;
  kern_return_t err;

  fm->total = 0;

  err = vm_statistics(mach_task_self(), &vmstats);
  if (err) {
    error(0, err, "vm_statistics");
    xwin_setdone(fm->m.xw, 1);
    return;
  }

  pm->pageinfo[pm->pageindex][0] = vmstats.pageins;
  pm->pageinfo[pm->pageindex][1] = vmstats.pageouts;

  updateinfo(pm);
}

static void checkevent(Meter *m) {
  getpageinfo((PageMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *pagemeter_new(XOSView *parent, float max) {
  PageMeter *pm = (PageMeter *)meter_alloc(sizeof *pm);
  int i, j;

  fieldmeter_init(&pm->f, parent, 3, "PageMeter", "PAGE", "IN/OUT/IDLE",
                  0, 0, 0);
  pm->f.m.checkres = checkres;
  pm->f.m.checkevent = checkevent;

  for (i = 0; i < 2; i++)
    for (j = 0; j < 2; j++)
      pm->pageinfo[j][i] = 0;

  pm->maxspeed = max;
  pm->pageindex = 0;

  return &pm->f.m;
}
