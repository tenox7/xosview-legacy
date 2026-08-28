/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "pagemeter.h"
#include "aixstats.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

/*  The counters used here cover traffic to and from paging space only, which
 *  is what vmstat reports in its pi and po columns.  Ordinary file paging is
 *  deliberately left out because the disk meter already covers it.  */

static void checkres(Meter *m) {
  PageMeter *pm = (PageMeter *)m;
  FieldMeter *fm = &pm->f;

  fieldmeter_checkresources(m);

  m->priority = atoi(xwin_getresource(m->xw, "pagePriority"));
  pm->maxspeed *= m->priority / 10.0;

  /*  fieldmeter_disable() collapsed this meter to a single field, so setting
   *  the per field colours below would run off the end of the array.  */
  if (!pm->ok)
    return;

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "pageInColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "pageOutColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "pageIdleColor"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "pageDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "pageGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "pageUsedFormat"));
}

static void getpageinfo(PageMeter *pm) {
  FieldMeter *fm = &pm->f;
  int i, oldindex;

  if (!aixstats_paging(&pm->pageinfo[pm->pageindex][0],
                        &pm->pageinfo[pm->pageindex][1]))
    return;

  fm->total = 0;

  oldindex = (pm->pageindex + 1) % 2;
  for (i = 0; i < 2; i++) {
    if (pm->pageinfo[oldindex][i] == 0)
      pm->pageinfo[oldindex][i] = pm->pageinfo[pm->pageindex][i];

    fm->fields[i] = pm->pageinfo[pm->pageindex][i] - pm->pageinfo[oldindex][i];
    if (fm->fields[i] < 0)
      fm->fields[i] = 0;
    fm->total += fm->fields[i];
  }

  if (fm->total > pm->maxspeed) {
    fm->fields[2] = 0.0;
  } else {
    fm->fields[2] = pm->maxspeed - fm->total;
    fm->total = pm->maxspeed;
  }

  fieldmeter_setused(fm, fm->total - fm->fields[2], pm->maxspeed);
  pm->pageindex = oldindex;
}

static void checkevent(Meter *m) {
  getpageinfo((PageMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *pagemeter_new(XOSView *parent, float max) {
  PageMeter *pm = (PageMeter *)meter_alloc(sizeof *pm);
  double in, out;
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
  pm->ok = aixstats_paging(&in, &out);

  if (!pm->ok)
    fieldmeter_disable(&pm->f);

  return &pm->f.m;
}
