/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "cpumeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <sys/pstat.h>

static void checkres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "cpuUserColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "cpuNiceColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "cpuSystemColor"));
  fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "cpuInterruptColor"));
  fieldmeter_setcolorname(fm, 4, xwin_getresource(m->xw, "cpuFreeColor"));
  m->priority = atoi(xwin_getresource(m->xw, "cpuPriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "cpuDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "cpuGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "cpuUsedFormat"));
}

static void getcputime(CPUMeter *cm) {
  FieldMeter *fm = &cm->f;
  struct pst_dynamic stats;
  int i, oldindex;

  fm->total = 0;

  pstat_getdynamic(&stats, sizeof(struct pst_dynamic), 1, 0);

  cm->cputime[cm->cpuindex][0] = stats.psd_cpu_time[0];
  cm->cputime[cm->cpuindex][1] = stats.psd_cpu_time[1];
  cm->cputime[cm->cpuindex][2] = stats.psd_cpu_time[2];
  cm->cputime[cm->cpuindex][3] = stats.psd_cpu_time[4];
  cm->cputime[cm->cpuindex][4] = stats.psd_cpu_time[3];

  oldindex = (cm->cpuindex + 1) % 2;
  for (i = 0; i < 5; i++) {
    fm->fields[i] = cm->cputime[cm->cpuindex][i] - cm->cputime[oldindex][i];
    fm->total += fm->fields[i];
  }
  cm->cpuindex = (cm->cpuindex + 1) % 2;

  if (fm->total)
    fieldmeter_setused(fm, fm->total - fm->fields[4], fm->total);
}

static void checkevent(Meter *m) {
  getcputime((CPUMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *cpumeter_new(XOSView *parent) {
  CPUMeter *cm = (CPUMeter *)meter_alloc(sizeof *cm);
  int i, j;

  fieldmeter_init(&cm->f, parent, 5, "CPUMeter", "CPU",
                  "USR/NICE/SYS/INT/FREE", 0, 0, 0);
  cm->f.m.checkres = checkres;
  cm->f.m.checkevent = checkevent;

  for (i = 0; i < 2; i++)
    for (j = 0; j < 5; j++)
      cm->cputime[i][j] = 0;
  cm->cpuindex = 0;

  return &cm->f.m;
}
