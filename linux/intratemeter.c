/*
 *  Copyright (c) 2014 by Tomi Tapper (tomi.o.tapper@jyu.fi)
 *
 *  Based on bsd/intratemeter.* by
 *    Copyright (c) 1999 by Brian Grayson (bgrayson@netbsd.org)
 *  and on linux/intmeter.* by
 *    Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "intratemeter.h"
#include "cpumeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char INTFILE[] = "/proc/interrupts";

static void checkres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);
  fieldmeter_setcolorname(fm, 0,
                          xwin_getresource(m->xw, "irqrateUsedColor"));
  fieldmeter_setcolorname(fm, 1,
                          xwin_getresource(m->xw, "irqrateIdleColor"));
  m->priority = atoi(xwin_getresource(m->xw, "irqratePriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "irqrateDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "irqrateGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "irqrateUsedFormat"));
  fm->total = 2000;
}

static void getinfo(IrqRateMeter *im) {
  FieldMeter *fm = &im->f;
  FILE *f = fopen(INTFILE, "r");
  char line[1024];
  unsigned long long count = 0;

  if (!f) {
    fprintf(stderr, "Can not open file : %s\n", INTFILE);
    xwin_setdone(fm->m.xw, 1);
    return;
  }

  fieldmeter_timerstop(fm);
  if (!fgets(line, sizeof line, f)) {  /*  header  */
    fclose(f);
    return;
  }

  /*  sum all interrupts on all cpus  */
  while (fgets(line, sizeof line, f)) {
    unsigned int i = 0;
    char *cur, *end;
    size_t digit = strcspn(line, "0123456789");
    size_t colon = strcspn(line, ":");

    if (digit > colon)
      break;  /*  reached non-numeric interrupts  */

    strtoul(line, &end, 10);
    cur = end + 1;
    while (*cur && i++ < im->cpucount) {
      unsigned long tmp = strtoul(cur, &end, 10);
      if (end == cur)
        break;
      count += tmp;
      cur = end;
    }
  }
  fclose(f);

  if (im->lastirqs == 0)  /*  first run  */
    im->lastirqs = count;
  fm->fields[0] = (count - im->lastirqs) / fieldmeter_secs(fm);
  fieldmeter_timerstart(fm);
  im->lastirqs = count;

  /*  Bump total, if needed.  */
  if (fm->fields[0] > fm->total)
    fm->total = fm->fields[0];

  fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  getinfo((IrqRateMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *irqratemeter_new(XOSView *parent) {
  IrqRateMeter *im = (IrqRateMeter *)meter_alloc(sizeof *im);

  fieldmeter_init(&im->f, parent, 2, "IrqRateMeter", "IRQs",
                  "IRQs per sec/IDLE", 1, 1, 0);
  im->f.m.checkres = checkres;
  im->f.m.checkevent = checkevent;

  im->lastirqs = 0;
  im->cpucount = cpumeter_countcpus();

  return &im->f.m;
}
