/*
 *  Copyright (c) 1996, 2004 by Massimiliano Ghilardi ( ghilardi@cibs.sns.it )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "pagemeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PROCSTAT_LENGTH 4096

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

static FILE *openstats(PageMeter *pm) {
  FILE *f = fopen(pm->statFileName, "r");

  if (!f) {
    fprintf(stderr, "Cannot open file : %s\n", pm->statFileName);
    exit(1);
  }
  return f;
}

static void getvmpageinfo(PageMeter *pm) {
  FILE *f = openstats(pm);
  char buf[MAX_PROCSTAT_LENGTH];
  int found_in = 0, found_out = 0;

  pm->f.total = 0;

  while (!(found_in && found_out) && fgets(buf, sizeof buf, f)) {
    if (!strncmp(buf, "pswpin", 6)) {
      pm->pageinfo[pm->pageindex][0] = strtoul(buf + 7, NULL, 10);
      found_in = 1;
    }
    if (!strncmp(buf, "pswpout", 7)) {
      pm->pageinfo[pm->pageindex][1] = strtoul(buf + 8, NULL, 10);
      found_out = 1;
    }
  }
  fclose(f);

  updateinfo(pm);
}

static void getpageinfo(PageMeter *pm) {
  FILE *f = openstats(pm);
  char buf[MAX_PROCSTAT_LENGTH];

  pm->f.total = 0;

  /*  Walk the whitespace separated words of /proc/stat until the "swap"
   *  one; the two counts follow it.  */
  do {
    if (fscanf(f, "%4095s", buf) != 1) {
      fclose(f);
      updateinfo(pm);
      return;
    }
  } while (strncasecmp(buf, "swap", 5));

  if (fscanf(f, "%lu %lu", &pm->pageinfo[pm->pageindex][0],
             &pm->pageinfo[pm->pageindex][1]) != 2) {
    fclose(f);
    updateinfo(pm);
    return;
  }
  fclose(f);

  updateinfo(pm);
}

static void checkevent(Meter *m) {
  PageMeter *pm = (PageMeter *)m;

  if (pm->vmstat)
    getvmpageinfo(pm);
  else
    getpageinfo(pm);
  fieldmeter_drawfields(&pm->f, 0);
}

Meter *pagemeter_new(XOSView *parent, float max) {
  PageMeter *pm = (PageMeter *)meter_alloc(sizeof *pm);
  struct stat buf;
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
  pm->vmstat = 0;
  pm->statFileName = "/proc/stat";

  if (stat("/proc/vmstat", &buf) == 0 && buf.st_mode & S_IFREG) {
    pm->vmstat = 1;
    pm->statFileName = "/proc/vmstat";
  }

  return &pm->f.m;
}
