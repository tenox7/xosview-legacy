/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *  Copyright (c) 1995, 1996, 1997-2002 by Brian Grayson (bgrayson@netbsd.org)
 *
 *  This file was written by Brian Grayson for the NetBSD and xosview
 *    projects.
 *  This file may be distributed under terms of the GPL or of the BSD
 *    license, whichever you choose.  The full license notices are
 *    contained in the files COPYING.GPL and COPYING.BSD, which you
 *    should have received.  If not, contact one of the xosview
 *    authors for a copy.
 */

#include "MeterMaker.h"
#include "defines.h"
#include "kernel.h"
#include "loadmeter.h"
#include "cpumeter.h"
#include "memmeter.h"
#include "swapmeter.h"
#include "pagemeter.h"
#include "netmeter.h"
#include "diskmeter.h"
#include "intmeter.h"
#include "intratemeter.h"
#include "btrymeter.h"
#if defined(__i386__) || defined(__x86_64__)
#include "coretemp.h"
#endif
#include "sensor.h"
#include "xosview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void makeMeters(XOSView *xos) {
	/*  Standard meters (usually added, but users could turn them off)  */
	if (xosview_isresourcetrue(xos, "load"))
		xosview_addmeter(xos, loadmeter_new(xos));

	if (xosview_isresourcetrue(xos, "cpu")) {
		const char *format = xosview_getresource(xos, "cpuFormat");
		unsigned int cpuCount = BSDCountCpus();
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
			xosview_addmeter(xos, cpumeter_new(xos, 0));

		if (all || both)
			for (i = 1; i <= cpuCount; i++)
				xosview_addmeter(xos, cpumeter_new(xos, i));
	}

	if (xosview_isresourcetrue(xos, "mem"))
		xosview_addmeter(xos, memmeter_new(xos));

	if (xosview_isresourcetrue(xos, "swap"))
		xosview_addmeter(xos, swapmeter_new(xos));

	if (xosview_isresourcetrue(xos, "page"))
		xosview_addmeter(xos,
		                 pagemeter_new(xos,
		                               atof(xosview_getresource(xos,
		                                    "pageBandwidth"))));

	if (xosview_isresourcetrue(xos, "net"))
		xosview_addmeter(xos,
		                 netmeter_new(xos,
		                              atof(xosview_getresource(xos,
		                                   "netBandwidth"))));

	if (xosview_isresourcetrue(xos, "disk"))
		xosview_addmeter(xos,
		                 diskmeter_new(xos,
		                               atof(xosview_getresource(xos,
		                                    "diskBandwidth"))));

	if (xosview_isresourcetrue(xos, "interrupts"))
		xosview_addmeter(xos, intmeter_new(xos, 0, 0));

	if (xosview_isresourcetrue(xos, "irqrate"))
		xosview_addmeter(xos, irqratemeter_new(xos));

	if (xosview_isresourcetrue(xos, "battery") && BSDHasBattery())
		xosview_addmeter(xos, btrymeter_new(xos));

#if defined(__i386__) || defined(__x86_64__)
	if (xosview_isresourcetrue(xos, "coretemp") && coretemp_countcpus() > 0) {
		char caption[32];
		const char *displayType;

		snprintf(caption, sizeof caption, "ACT(\260C)/HIGH/%s",
		         xosview_getresource_default(xos, "coretempHighest", "100"));
		displayType = xosview_getresource_default(xos,
		                                          "coretempDisplayType",
		                                          "separate");
		if (strncmp(displayType, "separate", 1) == 0) {
			char name[5];
			unsigned int i;

			for (i = 0; i < coretemp_countcpus(); i++) {
				snprintf(name, sizeof name, "CPU%u", i);
				xosview_addmeter(xos,
				                 coretemp_new(xos, name, caption, i));
			}
		} else if (strncmp(displayType, "average", 1) == 0) {
			xosview_addmeter(xos, coretemp_new(xos, "CPU", caption, -1));
		} else if (strncmp(displayType, "maximum", 1) == 0) {
			xosview_addmeter(xos, coretemp_new(xos, "CPU", caption, -2));
		} else {
			fprintf(stderr, "Unknown value of coretempDisplayType: %s\n",
			        displayType);
			xosview_setdone(xos, 1);
		}
	}
#endif

	if (xosview_isresourcetrue(xos, "bsdsensor")) {
		char caption[16], l[8], s[16];
		int i;

		for (i = 1; ; i++) {
			float highest;
			const char *name, *high, *low, *label;

			snprintf(s, sizeof s, "bsdsensorHighest%d", i);
			highest = atof(xosview_getresource_default(xos, s, "100"));
			snprintf(caption, sizeof caption, "ACT/HIGH/%f", highest);
			snprintf(s, sizeof s, "bsdsensor%d", i);
			name = xosview_getresource_default(xos, s, NULL);
			if (!name || !*name)
				break;
			snprintf(s, sizeof s, "bsdsensorHigh%d", i);
			high = xosview_getresource_default(xos, s, NULL);
			snprintf(s, sizeof s, "bsdsensorLow%d", i);
			low = xosview_getresource_default(xos, s, NULL);
			snprintf(s, sizeof s, "bsdsensorLabel%d", i);
			snprintf(l, sizeof l, "SEN%d", i);
			label = xosview_getresource_default(xos, s, l);
			xosview_addmeter(xos,
			                 bsdsensor_new(xos, name, high, low, label,
			                               caption, i));
		}
	}
}
