/*
 *  Rewritten for Solaris by Arno Augustin 1999
 *  augustin@informatik.uni-erlangen.de
 */

#include "netmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <stropts.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <unistd.h>

static void destroy(Meter *m) {
  NetMeter *nm = (NetMeter *)m;

  if (nm->socket >= 0)
    close(nm->socket);
  fieldmeter_fini(m);
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
  uint64_t nowBytesIn = 0, nowBytesOut = 0;
  uint64_t correction;
  kstat_named_t *k;
  kstat_t *ksp;
  unsigned int i;
  double t;

  fm->total = nm->maxpackets;
  kstatlist_update(nm->nets, nm->kc);

  fieldmeter_timerstop(fm);
  for (i = 0; i < kstatlist_count(nm->nets); i++) {
    int match;

    ksp = kstatlist_at(nm->nets, i);
    match = (strcmp(ksp->ks_name, nm->netIface) == 0);
    if (strcmp(nm->netIface, "False") != 0 &&
        ((!nm->ignored && !match) || (nm->ignored && match)))
      continue;
    if (kstat_read(nm->kc, ksp, NULL) == -1)
      continue;

    /*  try 64-bit byte counter first, then 32-bit one, then packet counter */
    if ((k = (kstat_named_t *)kstat_data_lookup(ksp, "rbytes64")) == NULL) {
      if ((k = (kstat_named_t *)kstat_data_lookup(ksp, "rbytes")) == NULL) {
        if ((k = (kstat_named_t *)kstat_data_lookup(ksp, "ipackets")) == NULL)
          continue;
        /*  for a packet counter, mtu is needed  */
        strncpy(nm->lfr.lifr_name, ksp->ks_name, sizeof(nm->lfr.lifr_name));
        if (ioctl(nm->socket, SIOCGLIFMTU, (caddr_t)&nm->lfr) < 0)
          continue;
        /*  not exactly, but must do  */
        nowBytesIn += kstat_to_ui64(k) * nm->lfr.lifr_mtu;
        XOSDEBUG("%s: %llu packets received\n", ksp->ks_name,
                 kstat_to_ui64(k));
      } else {
        nowBytesIn += kstat_to_ui64(k);
        XOSDEBUG("%s: %llu bytes received\n", ksp->ks_name, kstat_to_ui64(k));
      }
    } else {
      nowBytesIn += kstat_to_ui64(k);
      XOSDEBUG("%s: %llu bytes received\n", ksp->ks_name, kstat_to_ui64(k));
    }

    if ((k = (kstat_named_t *)kstat_data_lookup(ksp, "obytes64")) == NULL) {
      if ((k = (kstat_named_t *)kstat_data_lookup(ksp, "obytes")) == NULL) {
        if ((k = (kstat_named_t *)kstat_data_lookup(ksp, "opackets")) == NULL)
          continue;
        strncpy(nm->lfr.lifr_name, ksp->ks_name, sizeof(nm->lfr.lifr_name));
        if (ioctl(nm->socket, SIOCGLIFMTU, (caddr_t)&nm->lfr) < 0)
          continue;
        nowBytesOut += kstat_to_ui64(k) * nm->lfr.lifr_mtu;
        XOSDEBUG("%s: %llu packets sent\n", ksp->ks_name, kstat_to_ui64(k));
      } else {
        nowBytesOut += kstat_to_ui64(k);
        XOSDEBUG("%s: %llu bytes sent\n", ksp->ks_name, kstat_to_ui64(k));
      }
    } else {
      nowBytesOut += kstat_to_ui64(k);
      XOSDEBUG("%s: %llu bytes sent\n", ksp->ks_name, kstat_to_ui64(k));
    }
  }

  correction = 0x10000000;
  correction *= 0x10;
  /*  Deal with 32-bit wrap by making last value 2^32 less.  Yes, this is a
   *  better idea than adding to nowBytesIn -- the latter would only work for
   *  the first wrap (1+2^32 vs. 1) but not for the second (1+2*2^32 vs. 1)
   *  -- 1+2^32 - (1+2^32) is still too big.  */
  if (nowBytesIn < nm->lastBytesIn)
    nm->lastBytesIn -= correction;
  if (nowBytesOut < nm->lastBytesOut)
    nm->lastBytesOut -= correction;
  if (nm->lastBytesIn == 0)
    nm->lastBytesIn = nowBytesIn;
  if (nm->lastBytesOut == 0)
    nm->lastBytesOut = nowBytesOut;

  t = fieldmeter_secs(fm);
  fm->fields[0] = (double)(nowBytesIn - nm->lastBytesIn) / t;
  fm->fields[1] = (double)(nowBytesOut - nm->lastBytesOut) / t;

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

Meter *netmeter_new(XOSView *parent, kstat_ctl_t *kc, float max) {
  NetMeter *nm = (NetMeter *)meter_alloc(sizeof *nm);

  fieldmeter_init(&nm->f, parent, 3, "NetMeter", "NET", "IN/OUT/IDLE",
                  0, 0, 0);
  nm->f.m.checkres = checkres;
  nm->f.m.checkevent = checkevent;
  nm->f.m.destroy = destroy;

  nm->kc = kc;
  nm->ignored = 0;
  nm->maxpackets = max;
  nm->lastBytesIn = nm->lastBytesOut = 0;
  nm->netIface[0] = '\0';
  nm->nets = kstatlist_get(kc, KSL_NETS);
  if ((nm->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "Opening socket failed.\n");
    xwin_setdone((XWin *)parent, 1);
  }

  return &nm->f.m;
}
