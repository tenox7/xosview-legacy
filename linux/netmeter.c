/*
 *  Copyright (c) 1994, 1995, 2002, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  Modifications to support dynamic addresses by:
 *    Michael N. Lipp (mnl@dtro.e-technik.th-darmstadt.de)
 *
 *  This file may be distributed under terms of the GPL
 */

#include "netmeter.h"
#include "stringutils.h"
#include "xosview.h"
#include "xwin.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char PROCNETDEV[] = "/proc/net/dev";
static const char SYSCLASSNET[] = "/sys/class/net";

/*
 * Parse the integer count from the given filename
 *
 * This is quite relaxed about error conditions because the file may
 * have been removed before it was opened, or truncated, in which case
 * the count is zero.
 *
 * Return: 0 if not available, otherwise count (which may be zero)
 */
static unsigned long long getCount(const char *filename) {
  unsigned long long n, count = 0;
  FILE *f;

  f = fopen(filename, "r");
  if (!f)
    return 0;

  if (fscanf(f, "%llu", &n) == 1)
    count = n;

  if (fclose(f) != 0)
    abort();

  return count;
}

/*  Is this interface one the netIface resource asks us to count?  */
static int wanted(const NetMeter *nm, const char *name) {
  int match;

  if (strcmp(nm->netIface, "False") == 0)
    return 1;

  match = strcmp(name, nm->netIface) == 0;
  return nm->ignored ? !match : match;
}

static void checkres(Meter *m) {
  NetMeter *nm = (NetMeter *)m;
  FieldMeter *fm = &nm->f;
  const char *iface;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "netInColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "netOutColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "netBackground"));
  m->priority = atoi(xwin_getresource(m->xw, "netPriority"));
  fm->usegraph = xwin_isresourcetrue(m->xw, "netGraph");
  fm->dodecay = xwin_isresourcetrue(m->xw, "netDecay");
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

static void getSysStats(NetMeter *nm, unsigned long long *totin,
                        unsigned long long *totout) {
  DIR *dir;
  struct dirent *ent;
  char filename[128];

  if (!(dir = opendir(SYSCLASSNET))) {
    fprintf(stderr, "Can not open directory : %s\n", SYSCLASSNET);
    xwin_setdone(nm->f.m.xw, 1);
    return;
  }

  /*  walk through /sys/class/net/<iface>/statistics/{r,t}x_bytes  */
  while ((ent = readdir(dir))) {
    if (ent->d_type != DT_LNK)
      continue;
    if (!wanted(nm, ent->d_name))
      continue;

    snprintf_or_abort(filename, sizeof filename,
                      "%s/%s/statistics/rx_bytes", SYSCLASSNET, ent->d_name);
    *totin += getCount(filename);

    snprintf_or_abort(filename, sizeof filename,
                      "%s/%s/statistics/tx_bytes", SYSCLASSNET, ent->d_name);
    *totout += getCount(filename);
  }
  closedir(dir);
}

static void getProcStats(NetMeter *nm, unsigned long long *totin,
                         unsigned long long *totout) {
  FILE *f = fopen(PROCNETDEV, "r");
  char line[1024];
  int i;

  if (!f) {
    fprintf(stderr, "Can not open file : %s\n", PROCNETDEV);
    xwin_setdone(nm->f.m.xw, 1);
    return;
  }

  /*  two header lines  */
  if (!fgets(line, sizeof line, f) || !fgets(line, sizeof line, f)) {
    fclose(f);
    return;
  }

  while (fgets(line, sizeof line, f)) {
    unsigned long long vals[9];
    char *colon = strchr(line, ':');
    char *name, *cur, *end;

    if (!colon)
      continue;
    *colon = '\0';

    name = line;
    while (*name == ' ')
      name++;
    if (!wanted(nm, name))
      continue;

    cur = colon + 1;
    if (strncmp(cur, " No ", 4) == 0)
      continue;  /*  xxx: No statistics available.  */

    for (i = 0; i < 9; i++) {
      vals[i] = strtoull(cur, &end, 10);
      if (end == cur)
        break;
      cur = end;
    }
    if (i < 9)
      continue;

    *totin += vals[0];
    *totout += vals[8];
    XOSDEBUG("%s: %llu bytes received, %llu bytes sent.\n",
             name, vals[0], vals[8]);
  }
  fclose(f);
}

static void checkevent(Meter *m) {
  NetMeter *nm = (NetMeter *)m;
  FieldMeter *fm = &nm->f;
  unsigned long long totin = 0, totout = 0;
  double t;

  fm->fields[2] = nm->maxpackets;      /*  assume no  */
  fm->fields[0] = fm->fields[1] = 0;   /*  network activity  */

  fieldmeter_timerstop(fm);
  if (nm->usesysfs)
    getSysStats(nm, &totin, &totout);
  else
    getProcStats(nm, &totin, &totout);

  t = fieldmeter_secs(fm);
  fieldmeter_timerstart(fm);

  if (nm->lastBytesIn == 0 && nm->lastBytesOut == 0) {  /*  first run  */
    nm->lastBytesIn = totin;
    nm->lastBytesOut = totout;
  }

  fm->fields[0] = (totin - nm->lastBytesIn) / t;
  fm->fields[1] = (totout - nm->lastBytesOut) / t;

  nm->lastBytesIn = totin;
  nm->lastBytesOut = totout;

  fm->total = fm->fields[0] + fm->fields[1];
  if (fm->total > nm->maxpackets) {
    fm->fields[2] = 0;
  } else {
    fm->total = nm->maxpackets;
    fm->fields[2] = fm->total - fm->fields[0] - fm->fields[1];
  }

  fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
  fieldmeter_drawfields(fm, 0);
}

Meter *netmeter_new(XOSView *parent, float max) {
  NetMeter *nm = (NetMeter *)meter_alloc(sizeof *nm);
  struct stat buf;

  fieldmeter_init(&nm->f, parent, 3, "NetMeter", "NET", "IN/OUT/IDLE",
                  0, 0, 0);
  nm->f.m.checkres = checkres;
  nm->f.m.checkevent = checkevent;

  nm->maxpackets = max;
  nm->lastBytesIn = nm->lastBytesOut = 0;
  nm->usesysfs = nm->ignored = 0;
  nm->netIface[0] = '\0';

  if (stat(SYSCLASSNET, &buf) == 0 && S_ISDIR(buf.st_mode))
    nm->usesysfs = 1;

  return &nm->f.m;
}
