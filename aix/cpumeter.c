/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "cpumeter.h"
#include "aixstats.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <sys/systemcfg.h>

/*  The cpu ticks are accumulated system wide, so a multiprocessor machine
 *  gets a single aggregate meter and the cpuFormat resource has no effect. */

int cpumeter_countcpus(void) {
  int n = _system_configuration.ncpus;

  return n > 0 ? n : 1;
}

static void checkres(Meter *m) {
  CPUMeter *cm = (CPUMeter *)m;
  FieldMeter *fm = &cm->f;

  fieldmeter_checkresources(m);

  m->priority = atoi(xwin_getresource(m->xw, "cpuPriority"));

  /*  fieldmeter_disable() collapsed this meter to a single field, so setting
   *  the per field colours below would run off the end of the array.  */
  if (!cm->ok)
    return;

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "cpuUserColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "cpuSystemColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "cpuWaitColor"));
  fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "cpuFreeColor"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "cpuDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "cpuGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "cpuUsedFormat"));
}

static void getcputime(CPUMeter *cm) {
  FieldMeter *fm = &cm->f;
  int i, oldindex;

  if (!aixstats_cpu(cm->cputime[cm->cpuindex]))
    return;

  fm->total = 0;

  oldindex = (cm->cpuindex + 1) % 2;
  for (i = 0; i < 4; i++) {
    fm->fields[i] = cm->cputime[cm->cpuindex][i] - cm->cputime[oldindex][i];
    if (fm->fields[i] < 0)
      fm->fields[i] = 0;
    fm->total += fm->fields[i];
  }
  cm->cpuindex = oldindex;

  if (fm->total)
    fieldmeter_setused(fm, fm->total - fm->fields[3], fm->total);
}

static void checkevent(Meter *m) {
  getcputime((CPUMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *cpumeter_new(XOSView *parent) {
  CPUMeter *cm = (CPUMeter *)meter_alloc(sizeof *cm);
  double probe[4];
  int i, j;

  fieldmeter_init(&cm->f, parent, 4, "CPUMeter", "CPU", "USR/SYS/WIO/IDLE",
                  0, 0, 0);
  cm->f.m.checkres = checkres;
  cm->f.m.checkevent = checkevent;

  for (i = 0; i < 2; i++)
    for (j = 0; j < 4; j++)
      cm->cputime[i][j] = 0;

  cm->cpuindex = 0;
  cm->ok = aixstats_cpu(probe);

  if (!cm->ok)
    fieldmeter_disable(&cm->f);

  return &cm->f.m;
}
