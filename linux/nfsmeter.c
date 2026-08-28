/*
 *  Copyright (c) 1994, 1995, 2002, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  Modifications to support dynamic addresses by:
 *    Michael N. Lipp (mnl@dtro.e-technik.th-darmstadt.de)
 *
 *  This file may be distributed under terms of the GPL
 */

#include "nfsmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX
#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#endif

static const char NFSSVCSTAT[] = "/proc/net/rpc/nfsd";
static const char NFSCLTSTAT[] = "/proc/net/rpc/nfs";

static void nfsmeter_init(NFSMeter *nm, XOSView *parent, const char *name,
                          int nfields, const char *fields,
                          const char *statfile) {
  fieldmeter_init(&nm->f, parent, nfields, name, name, fields, 0, 0, 0);
  nm->statfile = statfile;
  fieldmeter_timerstart(&nm->f);
}

/*---------------------------------------------------------------------------*/

static void nfsdcheckres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0,
                          xwin_getresource(m->xw, "NFSDStatBadCallsColor"));
  fieldmeter_setcolorname(fm, 1,
                          xwin_getresource(m->xw, "NFSDStatUDPColor"));
  fieldmeter_setcolorname(fm, 2,
                          xwin_getresource(m->xw, "NFSDStatTCPColor"));
  fieldmeter_setcolorname(fm, 3,
                          xwin_getresource(m->xw, "NFSDStatIdleColor"));

  fm->usegraph = xwin_isresourcetrue(m->xw, "NFSDStatGraph");
  fm->dodecay = xwin_isresourcetrue(m->xw, "NFSDStatDecay");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "NFSDStatUsedFormat"));
}

static void nfsdcheckevent(Meter *m) {
  NFSDStats *ds = (NFSDStats *)m;
  FieldMeter *fm = &ds->n.f;
  FILE *f = fopen(ds->n.statfile, "r");
  char buf[4096], name[64];
  unsigned long netcnt = 0, netudpcnt = 0, nettcpcnt = 0, nettcpconn = 0;
  unsigned long calls = 0, badcalls = 0;
  int found = 0;
  float t;

  if (!f)
    return;

  fm->fields[0] = fm->fields[1] = fm->fields[2] = 0;  /*  network activity  */
  fieldmeter_timerstop(fm);

  name[0] = '\0';
  while (found != 2 && fgets(buf, sizeof buf, f)) {
    if (strncmp("net", buf, 3) == 0) {
      sscanf(buf, "%63s %lu %lu %lu %lu", name, &netcnt, &netudpcnt,
             &nettcpcnt, &nettcpconn);
      found++;
    }
    if (strncmp("rpc", buf, 3) == 0) {
      sscanf(buf, "%63s %lu %lu", name, &calls, &badcalls);
      found++;
    }
  }
  fclose(f);

  t = 1000000.0 / fieldmeter_usecs(fm);
  if (t < 0)
    t = 0.1;

  ds->maxpackets = MAX(netcnt, calls) - ds->lastNetCnt;
  if (ds->maxpackets == 0) {
    ds->maxpackets = netcnt;
  } else {
    fm->fields[0] = (badcalls - ds->lastBad) * t;
    fm->fields[1] = (netudpcnt - ds->lastUdp) * t;
    fm->fields[2] = (nettcpcnt - ds->lastTcp) * t;
  }

  fm->total = fm->fields[0] + fm->fields[1] + fm->fields[2];
  if (fm->total > ds->maxpackets) {
    fm->fields[3] = 0;
  } else {
    fm->total = ds->maxpackets;
    fm->fields[3] = fm->total - fm->fields[0] - fm->fields[1] - fm->fields[2];
  }

  if (fm->total)
    fieldmeter_setused(fm, fm->fields[0] + fm->fields[1] + fm->fields[2],
                       fm->total);

  fieldmeter_timerstart(fm);
  fieldmeter_drawfields(fm, 0);

  ds->lastNetCnt = MAX(netcnt, calls);
  ds->lastTcp = nettcpcnt;
  ds->lastUdp = netudpcnt;
  ds->lastBad = badcalls;
}

Meter *nfsdstats_new(XOSView *parent) {
  NFSDStats *ds = (NFSDStats *)meter_alloc(sizeof *ds);

  nfsmeter_init(&ds->n, parent, "NFSD", 4, "BAD/UDP/TCP/IDLE", NFSSVCSTAT);
  ds->n.f.m.checkres = nfsdcheckres;
  ds->n.f.m.checkevent = nfsdcheckevent;

  /*  The C++ version left these uninitialised until the first sample.  */
  ds->maxpackets = 0;
  ds->lastTcp = ds->lastUdp = ds->lastNetCnt = ds->lastBad = 0;

  return &ds->n.f.m;
}

/*---------------------------------------------------------------------------*/

static void nfscheckres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0,
                          xwin_getresource(m->xw, "NFSStatReTransColor"));
  fieldmeter_setcolorname(fm, 1,
                          xwin_getresource(m->xw, "NFSStatAuthRefrshColor"));
  fieldmeter_setcolorname(fm, 2,
                          xwin_getresource(m->xw, "NFSStatCallsColor"));
  fieldmeter_setcolorname(fm, 3,
                          xwin_getresource(m->xw, "NFSStatIdleColor"));

  fm->usegraph = xwin_isresourcetrue(m->xw, "NFSStatGraph");
  fm->dodecay = xwin_isresourcetrue(m->xw, "NFSStatDecay");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "NFSStatUsedFormat"));
}

static void nfscheckevent(Meter *m) {
  NFSStats *ns = (NFSStats *)m;
  FieldMeter *fm = &ns->n.f;
  FILE *f = fopen(ns->n.statfile, "r");
  char buf[4096], name[64];
  unsigned long calls = 0, retrns = 0, authrefresh = 0, maxpackets;
  float t;

  if (!f)
    return;

  fm->fields[0] = fm->fields[1] = fm->fields[2] = 0;
  fieldmeter_timerstop(fm);

  name[0] = '\0';
  while (fgets(buf, sizeof buf, f)) {
    if (strncmp("rpc", buf, 3))
      continue;
    sscanf(buf, "%63s %lu %lu %lu", name, &calls, &retrns, &authrefresh);
    break;
  }
  fclose(f);

  t = 1000000.0 / fieldmeter_usecs(fm);
  if (t < 0)
    t = 0.1;

  maxpackets = calls - ns->lastcalls;
  if (maxpackets == 0) {
    maxpackets = calls;
  } else {
    fm->fields[2] = (calls - ns->lastcalls) * t;
    fm->fields[1] = (authrefresh - ns->lastauthrefresh) * t;
    fm->fields[0] = (retrns - ns->lastretrns) * t;
  }

  fm->total = fm->fields[0] + fm->fields[1] + fm->fields[2];
  if (fm->total > maxpackets) {
    fm->fields[3] = 0;
  } else {
    fm->total = maxpackets;
    fm->fields[3] = fm->total - fm->fields[2] - fm->fields[1] - fm->fields[0];
  }

  if (fm->total)
    fieldmeter_setused(fm, fm->fields[0] + fm->fields[1] + fm->fields[2],
                       fm->total);

  fieldmeter_timerstart(fm);
  fieldmeter_drawfields(fm, 0);

  ns->lastcalls = calls;
  ns->lastretrns = retrns;
  ns->lastauthrefresh = authrefresh;
}

Meter *nfsstats_new(XOSView *parent) {
  NFSStats *ns = (NFSStats *)meter_alloc(sizeof *ns);

  nfsmeter_init(&ns->n, parent, "NFS", 4, "RETRY/AUTH/CALL/IDLE", NFSCLTSTAT);
  ns->n.f.m.checkres = nfscheckres;
  ns->n.f.m.checkevent = nfscheckevent;

  /*  The C++ version left these uninitialised until the first sample.  */
  ns->lastcalls = ns->lastretrns = ns->lastauthrefresh = 0;

  return &ns->n.f.m;
}
