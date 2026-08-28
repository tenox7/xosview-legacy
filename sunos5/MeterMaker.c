/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#include "MeterMaker.h"
#include "xosview.h"
#include "kstats.h"
#include "cpumeter.h"
#include "memmeter.h"
#include "swapmeter.h"
#include "loadmeter.h"
#include "pagemeter.h"
#include "diskmeter.h"
#include "netmeter.h"
#include "intratemeter.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void makeMeters(XOSView *xos) {
  kstat_ctl_t *kc = kstat_open();

  if (kc == NULL)
    return;

  if (xosview_isresourcetrue(xos, "load"))
    xosview_addmeter(xos, loadmeter_new(xos, kc));

  /*  Standard meters (usually added, but users could turn them off)  */
  if (xosview_isresourcetrue(xos, "cpu")) {
    const char *format = xosview_getresource(xos, "cpuFormat");
    int cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
    int single, both, all;

    single = (strncmp(format, "single", 2) == 0);
    both = (strncmp(format, "both", 2) == 0);
    all = (strncmp(format, "all", 2) == 0);

    if (strncmp(format, "auto", 2) == 0) {
      if (cpuCount == 1 || cpuCount > 4)
        single = 1;
      else
        all = 1;
    }

    if (single || both)
      xosview_addmeter(xos, cpumeter_new(xos, kc, -1));

    if (all || both) {
      KStatList *cpulist = kstatlist_get(kc, KSL_CPU_STAT);
      unsigned int i;
      for (i = 0; i < kstatlist_count(cpulist); i++)
        xosview_addmeter(xos,
                         cpumeter_new(xos, kc,
                                      kstatlist_at(cpulist, i)->ks_instance));
    }
  }

  if (xosview_isresourcetrue(xos, "mem"))
    xosview_addmeter(xos, memmeter_new(xos, kc));

  if (xosview_isresourcetrue(xos, "disk"))
    xosview_addmeter(xos,
                     diskmeter_new(xos, kc,
                                   atof(xosview_getresource(xos,
                                        "diskBandwidth"))));

  if (xosview_isresourcetrue(xos, "swap"))
    xosview_addmeter(xos, swapmeter_new(xos, kc));

  if (xosview_isresourcetrue(xos, "page"))
    xosview_addmeter(xos,
                     pagemeter_new(xos, kc,
                                   atof(xosview_getresource(xos,
                                        "pageBandwidth"))));

  if (xosview_isresourcetrue(xos, "net"))
    xosview_addmeter(xos,
                     netmeter_new(xos, kc,
                                  atof(xosview_getresource(xos,
                                       "netBandwidth"))));

  if (xosview_isresourcetrue(xos, "irqrate"))
    xosview_addmeter(xos, irqratemeter_new(xos, kc));
}
