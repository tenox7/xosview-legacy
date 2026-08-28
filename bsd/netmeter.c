/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  NetBSD port:
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

#include "netmeter.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void getstats(NetMeter *nm) {
	FieldMeter *fm = &nm->f;
	uint64_t nowBytesIn, nowBytesOut;
	double t;

	/*  Reset total to the expected maximum.  If it is too low, it is
	 *  adjusted below.  bgrayson  */
	fm->total = nm->netBandwidth;
	fm->fields[0] = fm->fields[1] = 0;

	fieldmeter_timerstop(fm);
	BSDGetNetInOut(&nowBytesIn, &nowBytesOut, nm->netIface, nm->ignored);
	t = 1.0 / fieldmeter_secs(fm);
	fieldmeter_timerstart(fm);

	fm->fields[0] = (double)(nowBytesIn - nm->lastBytesIn) * t;
	nm->lastBytesIn = nowBytesIn;
	fm->fields[1] = (double)(nowBytesOut - nm->lastBytesOut) * t;
	nm->lastBytesOut = nowBytesOut;

	if (fm->total < (fm->fields[0] + fm->fields[1]))
		fm->total = fm->fields[0] + fm->fields[1];
	fm->fields[2] = fm->total - fm->fields[0] - fm->fields[1];
	/*  The field values have already been scaled into bytes/sec by the
	 *  manipulations (* t) above.  */
	fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
}

static void checkevent(Meter *m) {
	getstats((NetMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *netmeter_new(XOSView *parent, double max) {
	NetMeter *nm = (NetMeter *)meter_alloc(sizeof *nm);

	fieldmeter_init(&nm->f, parent, 3, "NetMeter", "NET", "IN/OUT/IDLE",
	                0, 0, 0);
	nm->f.m.checkres = checkres;
	nm->f.m.checkevent = checkevent;

	nm->netBandwidth = max;
	nm->lastBytesIn = nm->lastBytesOut = 0;
	nm->ignored = 0;
	snprintf(nm->netIface, sizeof nm->netIface, "False");

	if (!BSDNetInit()) {
		warnx("The kernel does not seem to have the symbols needed for "
		      "the NetMeter.");
		warnx("The NetMeter has been disabled.");
		fieldmeter_disable(&nm->f);
	} else {
		nm->f.total = max;
		BSDGetNetInOut(&nm->lastBytesIn, &nm->lastBytesOut, nm->netIface,
		               nm->ignored);
		fieldmeter_timerstart(&nm->f);
	}

	return &nm->f.m;
}
