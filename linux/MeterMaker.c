/*
 *  Copyright (c) 1994, 1995, 2002, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "MeterMaker.h"
#include "xosview.h"

#include "loadmeter.h"
#include "cpumeter.h"
#include "memmeter.h"
#include "diskmeter.h"
#include "raidmeter.h"
#include "swapmeter.h"
#include "pagemeter.h"
#include "wirelessmeter.h"
#include "netmeter.h"
#include "nfsmeter.h"
#include "serialmeter.h"
#include "intmeter.h"
#include "intratemeter.h"
#include "btrymeter.h"
#if defined(__i386__) || defined(__x86_64__)
#include "coretemp.h"
#endif
#include "lmstemp.h"
#include "acpitemp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void makeMeters(XOSView *xos) {
  /*  check for the load meter  */
  if (xosview_isresourcetrue(xos, "load"))
    xosview_addmeter(xos, loadmeter_new(xos));

  /*  Standard meters (usually added, but users could turn them off)  */
  if (xosview_isresourcetrue(xos, "cpu")) {
    const char *format = xosview_getresource(xos, "cpuFormat");
    unsigned int cpuCount = cpumeter_countcpus();
    unsigned int i;
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
      xosview_addmeter(xos, cpumeter_new(xos, cpumeter_cpustr(0)));

    if (all || both)
      for (i = 1; i <= cpuCount; i++)
        xosview_addmeter(xos, cpumeter_new(xos, cpumeter_cpustr(i)));
  }
  if (xosview_isresourcetrue(xos, "mem"))
    xosview_addmeter(xos, memmeter_new(xos));
  if (xosview_isresourcetrue(xos, "disk"))
    xosview_addmeter(xos,
                     diskmeter_new(xos,
                                   atof(xosview_getresource(xos,
                                        "diskBandwidth"))));

  /*  check for the RAID meter  */
  if (xosview_isresourcetrue(xos, "RAID")) {
    int RAIDCount = atoi(xosview_getresource(xos, "RAIDdevicecount"));
    int i;

    for (i = 0; i < RAIDCount; i++)
      xosview_addmeter(xos, raidmeter_new(xos, i));
  }

  if (xosview_isresourcetrue(xos, "swap"))
    xosview_addmeter(xos, swapmeter_new(xos));

  if (xosview_isresourcetrue(xos, "page"))
    xosview_addmeter(xos,
                     pagemeter_new(xos,
                                   atof(xosview_getresource(xos,
                                        "pageBandwidth"))));

  /*  check for the wireless meter  */
  if (xosview_isresourcetrue(xos, "wireless")) {
    struct stat buf;

    if (stat(WLFILENAME, &buf) != 0) {
      fprintf(stderr, "Wireless Meter needs Linux Wireless Extensions or "
              "cfg80211-WEXT compatibility to work.\n");
    } else {
      int count = wirelessmeter_countdevices();
      int i;

      for (i = 0; i < count; i++)
        xosview_addmeter(xos,
                         wirelessmeter_new(xos, i,
                                           count == 1 ? "WLAN"
                                             : wirelessmeter_str(i)));
    }
  }

  /*  check for the net meter  */
  if (xosview_isresourcetrue(xos, "net"))
    xosview_addmeter(xos,
                     netmeter_new(xos,
                                  atof(xosview_getresource(xos,
                                       "netBandwidth"))));

  /*  check for the NFS meters  */
  if (xosview_isresourcetrue(xos, "NFSDStats"))
    xosview_addmeter(xos, nfsdstats_new(xos));
  if (xosview_isresourcetrue(xos, "NFSStats"))
    xosview_addmeter(xos, nfsstats_new(xos));

  /*  check for the serial meters.  */
#if defined(__aarch64__) || defined (__arm__) || defined(__mc68000__) || \
    defined(__powerpc__) || defined(__powerpc64__) || defined(__sparc__) || \
    defined(__s390__) || defined(__s390x__)
  /*  these architectures have no ioperm()  */
#else
  {
    int i;

    for (i = 0; i < SERIAL_NUM_DEVICES; i++) {
      const char *res = serialmeter_resourcename(i);
      int ok = xosview_isresourcetrue(xos, res);

      if (!ok) {
        long val;
        /*  A port base may be given instead, in any base.  */
        ok = sscanf(xosview_getresource(xos, res), "%li", &val) == 1
             && (val & 0xFFFF);
      }

      if (ok)
        xosview_addmeter(xos, serialmeter_new(xos, i));
    }
  }
#endif

  /*  check for the interrupt meter  */
  if (xosview_isresourcetrue(xos, "interrupts")) {
    int cpuCount = cpumeter_countcpus();

    cpuCount = cpuCount == 0 ? 1 : cpuCount;
    if (xosview_isresourcetrue(xos, "intSeparate")) {
      int i;
      for (i = 0; i < cpuCount; i++)
        xosview_addmeter(xos, intmeter_new(xos, i));
    } else {
      xosview_addmeter(xos, intmeter_new(xos, cpuCount - 1));
    }
  }

  /*  check for irqrate meter  */
  if (xosview_isresourcetrue(xos, "irqrate"))
    xosview_addmeter(xos, irqratemeter_new(xos));

  /*  check for the battery meter  */
  if (xosview_isresourcetrue(xos, "battery") && btrymeter_has_source())
    xosview_addmeter(xos, btrymeter_new(xos));

#if defined(__i386__) || defined(__x86_64__)
  /*  Check for the CPU temperature meter  */
  if (xosview_isresourcetrue(xos, "coretemp")) {
    char caption[32], name[8] = "CPU";
    unsigned int coreCount, pkgCount, cpu, pkg = 0;
    const char *displayType;

    snprintf(caption, sizeof caption, "ACT(\260C)/HIGH/%s",
             xosview_getresource_default(xos, "coretempHighest", "100"));
    displayType = xosview_getresource_default(xos, "coretempDisplayType",
                                              "separate");

    pkgCount = coretemp_countcpus();
    if (strncmp(displayType, "separate", 1) == 0) {
      for (pkg = 0; pkg < pkgCount; pkg++) {
        coreCount = coretemp_countcores(pkg);
        for (cpu = 0; cpu < coreCount; cpu++) {
          if (pkgCount > 1) {
            /*  give a title only to the first core of each physical cpu  */
            if (cpu == 0)
              snprintf(name, sizeof name, "CPU%u", pkg);
            else
              name[0] = '\0';
          } else {
            if (coreCount > 1)
              snprintf(name, sizeof name, "CPU%u", cpu);
          }
          xosview_addmeter(xos, coretemp_new(xos, name, caption, pkg, cpu));
        }
      }
    } else if (strncmp(displayType, "average", 1) == 0) {
      do {
        if (pkgCount > 1)
          snprintf(name, sizeof name, "CPU%u", pkg);
        coreCount = coretemp_countcores(pkg);
        if (coreCount > 0)
          xosview_addmeter(xos, coretemp_new(xos, name, caption, pkg, -1));
      } while (++pkg < pkgCount);
    } else if (strncmp(displayType, "maximum", 1) == 0) {
      do {
        if (pkgCount > 1)
          snprintf(name, sizeof name, "CPU%u", pkg);
        coreCount = coretemp_countcores(pkg);
        if (coreCount > 0)
          xosview_addmeter(xos, coretemp_new(xos, name, caption, pkg, -2));
      } while (++pkg < pkgCount);
    } else {
      fprintf(stderr, "Unknown value of coretempDisplayType: %s\n",
              displayType);
      fprintf(stderr, "Supported types are: separate, average and "
              "maximum.\n");
      xosview_setdone(xos, 1);
    }
  }
#endif

  /*  check for the LmsTemp meter  */
  if (xosview_isresourcetrue(xos, "lmstemp")) {
    char caption[16], s[16];
    const char *tempfile, *highfile, *lowfile, *name, *label;
    int i;

    snprintf(caption, sizeof caption, "ACT/HIGH/%s",
             xosview_getresource_default(xos, "lmstempHighest", "100"));
    for (i = 1; i < 1000; i++) {
      snprintf(s, sizeof s, "lmstemp%d", i);
      tempfile = xosview_getresource_default(xos, s, NULL);
      if (!tempfile || !*tempfile)
        break;
      snprintf(s, sizeof s, "lmshigh%d", i);
      highfile = xosview_getresource_default(xos, s, NULL);
      snprintf(s, sizeof s, "lmslow%d", i);
      lowfile = xosview_getresource_default(xos, s, NULL);
      snprintf(s, sizeof s, "lmsname%d", i);
      name = xosview_getresource_default(xos, s, NULL);
      snprintf(s, sizeof s, "lmstempLabel%d", i);
      label = xosview_getresource_default(xos, s, "TMP");
      xosview_addmeter(xos, lmstemp_new(xos, name, tempfile, highfile,
                                        lowfile, label, caption, i));
    }
  }

  /*  check for the ACPITemp meter  */
  if (xosview_isresourcetrue(xos, "acpitemp")) {
    char caption[32], s[16];
    int i;

    snprintf(caption, sizeof caption, "ACT(\260C)/HIGH/%s",
             xosview_getresource_default(xos, "acpitempHighest", "100"));
    for (i = 1; i < 100; i++) {
      const char *tempfile, *highfile, *lab;

      snprintf(s, sizeof s, "acpitemp%d", i);
      tempfile = xosview_getresource_default(xos, s, NULL);
      if (!tempfile || !*tempfile)
        break;
      snprintf(s, sizeof s, "acpihigh%d", i);
      highfile = xosview_getresource_default(xos, s, NULL);
      if (!highfile || !*highfile)
        break;
      snprintf(s, sizeof s, "acpitempLabel%d", i);
      lab = xosview_getresource_default(xos, s, "TMP");
      xosview_addmeter(xos, acpitemp_new(xos, tempfile, highfile, lab,
                                         caption));
    }
  }
}
