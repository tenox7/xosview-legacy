/*
 *  Copyright (c) 1999, 2006 by Thomas Waldmann ( ThomasWaldmann@gmx.de )
 *  based on work of Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "raidmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char RAIDFILE[] = "/proc/mdstat";

/*  parser for /proc/mdstat  */

static int find1(const char *key, const char *findwhat, int num1) {
  char buf[80];

  snprintf(buf, sizeof buf, "%s.%d", findwhat, num1);
  return !strncmp(buf, key, 80);
}

static int raidparse(RAIDMeter *rm, char *cp) {
  char *key, *val;

  key = strtok(cp, " \n");
  val = strtok(NULL, " \n");
  if (key == NULL)
    return 1;

  if (find1(key, "md_state", rm->raiddev)) {
    if (val)
      snprintf(rm->state, sizeof rm->state, "%s", val);
  } else if (find1(key, "md_type", rm->raiddev)) {
    if (val)
      snprintf(rm->type, sizeof rm->type, "%s", val);
  } else if (find1(key, "md_disk_count", rm->raiddev)) {
    if (val)
      rm->disknum = atoi(val);
  } else if (find1(key, "md_working_disk_map", rm->raiddev)) {
    if (val)
      snprintf(rm->working_map, sizeof rm->working_map, "%s", val);
  } else if (find1(key, "md_resync_status", rm->raiddev)) {
    if (val)
      snprintf(rm->resync_state, sizeof rm->resync_state, "%s", val);
  }
  return 0;
}

static void getRAIDstate(RAIDMeter *rm) {
  FILE *f = fopen(RAIDFILE, "r");
  char l[256];

  if (!f) {
    fprintf(stderr, "Can not open file : %s\n", RAIDFILE);
    exit(1);
  }

  while (fgets(l, sizeof l, f))
    if (raidparse(rm, l) != 0)
      break;

  fclose(f);
}

static void checkres(Meter *m) {
  RAIDMeter *rm = (RAIDMeter *)m;
  BitFieldMeter *bm = &rm->b;

  bitfieldmeter_checkresources(m);
  bm->oncolor = xwin_alloccolor(m->xw,
                                xwin_getresource(m->xw,
                                                 "RAIDdiskOnlineColor"));
  bm->offcolor = xwin_alloccolor(m->xw,
                                 xwin_getresource(m->xw,
                                                  "RAIDdiskFailureColor"));
  rm->doneColor = xwin_alloccolor(m->xw,
                                  xwin_getresource(m->xw,
                                                   "RAIDresyncdoneColor"));
  rm->todoColor = xwin_alloccolor(m->xw,
                                  xwin_getresource(m->xw,
                                                   "RAIDresynctodoColor"));
  rm->completeColor =
      xwin_alloccolor(m->xw,
                      xwin_getresource(m->xw, "RAIDresynccompleteColor"));
  m->priority = atoi(xwin_getresource(m->xw, "RAIDPriority"));
  bitfieldmeter_setcolor(bm, 0, rm->doneColor);
  bitfieldmeter_setcolor(bm, 1, rm->todoColor);
  bitfieldmeter_setusedformat(bm, xwin_getresource(m->xw, "RAIDUsedFormat"));
}

static void checkevent(Meter *m) {
  RAIDMeter *rm = (RAIDMeter *)m;
  BitFieldMeter *bm = &rm->b;
  int i;

  getRAIDstate(rm);

  for (i = 0; i < rm->disknum; i++)
    bm->bits[i] = (rm->working_map[i] == '+');

  bm->fields[0] = 100.0;
  sscanf(rm->resync_state, "resync=%lf", &bm->fields[0]);
  bm->fields[1] = bm->total - bm->fields[1];
  if (bm->fields[0] < 100.0) {
    bitfieldmeter_setcolor(bm, 0, rm->doneColor);
    bitfieldmeter_setcolor(bm, 1, rm->todoColor);
  } else {
    bitfieldmeter_setcolor(bm, 0, rm->completeColor);
  }
  bitfieldmeter_setused(bm, bm->fields[0], bm->total);
  bitfieldmeter_checkevent(m);
}

Meter *raidmeter_new(XOSView *parent, int raiddev) {
  RAIDMeter *rm = (RAIDMeter *)meter_alloc(sizeof *rm);
  char legend[16];

  bitfieldmeter_init(&rm->b, parent, 1, 2, "RAIDMeter", "RAID", "", "",
                     0, 0, 0);
  rm->b.m.checkres = checkres;
  rm->b.m.checkevent = checkevent;

  rm->raiddev = raiddev;
  rm->disknum = 0;
  rm->state[0] = rm->type[0] = rm->working_map[0] = rm->resync_state[0] = '\0';
  rm->doneColor = rm->todoColor = rm->completeColor = 0;

  getRAIDstate(rm);
  if (rm->disknum < 1)
    bitfieldmeter_disable(&rm->b);

  snprintf(legend, sizeof legend, "MD%d", raiddev);
  meter_setlegend(&rm->b.m, legend);

  if (rm->disknum >= 1) {
    bitfieldmeter_setfieldlegend(&rm->b, "Done/ToDo");
    bitfieldmeter_setnumbits(&rm->b, rm->disknum);
  }
  rm->b.total = 100.0;

  return &rm->b.m;
}
