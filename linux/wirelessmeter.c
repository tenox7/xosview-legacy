/*
 *  Copyright (c) 2001 by Tim Ehlers ( tehlers@gwdg.de )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "wirelessmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char WLFILENAME[] = "/proc/net/wireless";

static void checkres(Meter *m) {
  WirelessMeter *wm = (WirelessMeter *)m;
  FieldMeter *fm = &wm->f;

  fieldmeter_checkresources(m);

  wm->poorqualcol = xwin_alloccolor(m->xw,
                                    xwin_getresource(m->xw,
                                                     "PoorQualityColor"));
  wm->fairqualcol = xwin_alloccolor(m->xw,
                                    xwin_getresource(m->xw,
                                                     "FairQualityColor"));
  wm->goodqualcol = xwin_alloccolor(m->xw,
                                    xwin_getresource(m->xw,
                                                     "GoodQualityColor"));
  fieldmeter_setcolorname(fm, 1,
                          xwin_getresource(m->xw, "wirelessUsedColor"));

  m->priority = atoi(xwin_getresource(m->xw, "wirelessPriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "wirelessDecay");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "wirelessUsedFormat"));
}

static void getpwrinfo(WirelessMeter *wm) {
  FieldMeter *fm = &wm->f;
  FILE *f = fopen(WLFILENAME, "r");
  char line[256], buff[32];
  int linkq = 0, quality = 0;
  int link = 0;
  int i;

  if (!f) {
    fprintf(stderr, "Can not open file : %s\n", WLFILENAME);
    xwin_setdone(fm->m.xw, 1);
    return;
  }

  /*  skip the two header rows  */
  if (!fgets(line, sizeof line, f) || !fgets(line, sizeof line, f)) {
    fclose(f);
    return;
  }

  if (wm->devname[0] == '\0') {  /*  find devname on first run  */
    for (i = 0; i < wm->number; i++)
      if (!fgets(line, sizeof line, f)) {
        fclose(f);
        return;
      }
    if (fgets(line, sizeof line, f))
      sscanf(line, "%31s %31s %d", wm->devname, buff, &linkq);
  } else {
    while (fgets(line, sizeof line, f)) {
      if (sscanf(line, "%31s", buff) != 1)
        continue;
      if (strcmp(wm->devname, buff) == 0) {
        sscanf(line, "%31s %31s %d", buff, buff, &linkq);
        link = 1;
        break;
      }
    }
  }
  fclose(f);

  if (linkq >= 250)
    linkq = 0;

  fm->fields[0] = linkq;
  if (fm->fields[0] >= 15)
    quality = 2;
  else if (fm->fields[0] >= 7)
    quality = 1;
  else
    quality = 0;

  if (link && !wm->lastlink) {
    meter_setlegend(&fm->m, "LINK/LEVEL");
    fieldmeter_drawlegend(fm);
    wm->lastlink = link;
  } else if (!link && wm->lastlink) {
    meter_setlegend(&fm->m, "NONE/LEVEL");
    fieldmeter_drawlegend(fm);
    wm->lastlink = link;
  }

  if (quality != wm->lastquality) {
    if (quality == 0)
      fieldmeter_setcolor(fm, 0, wm->poorqualcol);
    else if (quality == 1)
      fieldmeter_setcolor(fm, 0, wm->fairqualcol);
    else
      fieldmeter_setcolor(fm, 0, wm->goodqualcol);

    fieldmeter_drawlegend(fm);
    wm->lastquality = quality;
  }

  if (fm->fields[0] >= fm->total)
    fm->total = 30 * (int)(fm->fields[0] / 30 + 1);
  fm->fields[1] = fm->total - fm->fields[0];
  fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  getpwrinfo((WirelessMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

int wirelessmeter_countdevices(void) {
  glob_t gbuf;
  int count;

  glob("/sys/class/net/*/wireless", 0, NULL, &gbuf);
  count = gbuf.gl_pathc;
  globfree(&gbuf);
  return count;
}

const char *wirelessmeter_str(int num) {
  static char buffer[8] = "WL";

  snprintf(buffer + 2, 5, "%d", num);
  buffer[7] = '\0';
  return buffer;
}

Meter *wirelessmeter_new(XOSView *parent, int ID, const char *wlID) {
  WirelessMeter *wm = (WirelessMeter *)meter_alloc(sizeof *wm);

  fieldmeter_init(&wm->f, parent, 2, "WirelessMeter", wlID, "LINK/LEVEL",
                  1, 1, 0);
  wm->f.m.checkres = checkres;
  wm->f.m.checkevent = checkevent;

  wm->number = ID;
  wm->lastquality = -1;
  wm->lastlink = 1;
  wm->devname[0] = '\0';
  wm->poorqualcol = wm->fairqualcol = wm->goodqualcol = 0;
  wm->f.total = 0;

  return &wm->f.m;
}
