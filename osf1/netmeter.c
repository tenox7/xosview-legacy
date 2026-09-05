/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "netmeter.h"
#include "osf1stats.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void checkres(Meter *m) {
  NetMeter *nm = (NetMeter *)m;
  FieldMeter *fm = &nm->f;
  const char *iface;

  fieldmeter_checkresources(m);

  m->priority = atoi(xwin_getresource(m->xw, "netPriority"));

  /*  fieldmeter_disable() collapsed this meter to a single field, so setting
   *  the per field colours below would run off the end of the array.  */
  if (!nm->ok)
    return;

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "netInColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "netOutColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "netBackground"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "netDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "netGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "netUsedFormat"));

  iface = xwin_getresource(m->xw, "netIface");
  if (iface[0] == '-') {
    nm->ignored = 1;
    /*  A leading '-' means "every interface but this one".  */
    while (*iface == '-' || *iface == ' ')
      iface++;
  }
  snprintf(nm->netIface, sizeof nm->netIface, "%s", iface);
}

static void getnetstats(NetMeter *nm) {
  FieldMeter *fm = &nm->f;
  double nowBytesIn, nowBytesOut, t;
  int filtering = strcmp(nm->netIface, "False") != 0;

  fm->total = nm->maxpackets;

  if (!osf1stats_net(filtering ? nm->netIface : NULL, nm->ignored,
                     &nowBytesIn, &nowBytesOut))
    return;

  fieldmeter_timerstop(fm);

  if (nm->first) {
    nm->lastBytesIn = nowBytesIn;
    nm->lastBytesOut = nowBytesOut;
    nm->first = 0;
  }

  /*  The kernel keeps these counters in 32 bits and they wrap on a busy
   *  link, which shows up as the total going backwards.  Skip that sample
   *  rather than plotting an enormous spike.  */
  t = fieldmeter_secs(fm);
  fm->fields[0] = nowBytesIn >= nm->lastBytesIn
                  ? (nowBytesIn - nm->lastBytesIn) / t : 0.0;
  fm->fields[1] = nowBytesOut >= nm->lastBytesOut
                  ? (nowBytesOut - nm->lastBytesOut) / t : 0.0;

  fieldmeter_timerstart(fm);
  nm->lastBytesIn = nowBytesIn;
  nm->lastBytesOut = nowBytesOut;

  if (fm->total < fm->fields[0] + fm->fields[1])
    fm->total = fm->fields[0] + fm->fields[1];
  fm->fields[2] = fm->total - fm->fields[0] - fm->fields[1];

  fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
}

static void checkevent(Meter *m) {
  getnetstats((NetMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *netmeter_new(XOSView *parent, float max) {
  NetMeter *nm = (NetMeter *)meter_alloc(sizeof *nm);
  double in, out;

  fieldmeter_init(&nm->f, parent, 3, "NetMeter", "NET", "IN/OUT/IDLE",
                  0, 0, 0);
  nm->f.m.checkres = checkres;
  nm->f.m.checkevent = checkevent;

  nm->maxpackets = max;
  nm->lastBytesIn = nm->lastBytesOut = 0;
  nm->first = 1;
  nm->ignored = 0;
  nm->netIface[0] = '\0';
  nm->ok = osf1stats_net(NULL, 0, &in, &out);

  if (!nm->ok)
    fieldmeter_disable(&nm->f);

  return &nm->f.m;
}
