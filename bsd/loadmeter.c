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

#include "loadmeter.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void checkres(Meter *m) {
	LoadMeter *lm = (LoadMeter *)m;
	FieldMeter *fm = &lm->f;
	const char *warn, *crit;

	fieldmeter_checkresources(m);

	lm->procloadcol = xwin_alloccolor(m->xw,
	                                  xwin_getresource(m->xw,
	                                                   "loadProcColor"));
	lm->warnloadcol = xwin_alloccolor(m->xw,
	                                  xwin_getresource(m->xw,
	                                                   "loadWarnColor"));
	lm->critloadcol = xwin_alloccolor(m->xw,
	                                  xwin_getresource(m->xw,
	                                                   "loadCritColor"));

	fieldmeter_setcolor(fm, 0, lm->procloadcol);
	fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "loadIdleColor"));
	m->priority = atoi(xwin_getresource(m->xw, "loadPriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "loadDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "loadGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "loadUsedFormat"));
	lm->do_cpu_speed = xwin_isresourcetrue(m->xw, "loadCpuSpeed");

	warn = xwin_getresource(m->xw, "loadWarnThreshold");
	if (strncmp(warn, "auto", 2) == 0)
		lm->warnThreshold = BSDCountCpus();
	else
		lm->warnThreshold = atoi(warn);

	crit = xwin_getresource(m->xw, "loadCritThreshold");
	if (strncmp(crit, "auto", 2) == 0)
		lm->critThreshold = lm->warnThreshold * 4;
	else
		lm->critThreshold = atoi(crit);

	lm->alarmstate = lm->lastalarmstate = 0;

	if (fm->dodecay) {
		/*  Warning:  Since the loadmeter changes scale occasionally, old
		 *  decay values need to be rescaled.  However, if they are
		 *  rescaled, they could go off the edge of the screen.  Thus, for
		 *  now, to prevent this whole problem, the load meter can not be a
		 *  decay meter.  The load is a decaying average kind of thing
		 *  anyway, so having a decaying load average is redundant.  */
		fprintf(stderr, "Warning:  The loadmeter can not be configured as "
		        "a decay\n  meter.  See the source code (%s) for further\n"
		        "  details.\n", __FILE__);
		fm->dodecay = 0;
	}
}

static void getloadinfo(LoadMeter *lm) {
	FieldMeter *fm = &lm->f;
	double oneMinLoad;
	unsigned int i;

	/*  Only get the 1-minute-average sample.  */
	getloadavg(&oneMinLoad, 1);
	fm->fields[0] = oneMinLoad;

	if (fm->fields[0] < lm->warnThreshold)
		lm->alarmstate = 0;
	else if (fm->fields[0] >= lm->critThreshold)
		lm->alarmstate = 2;
	else
		lm->alarmstate = 1;

	if (lm->alarmstate != lm->lastalarmstate) {
		if (lm->alarmstate == 0)
			fieldmeter_setcolor(fm, 0, lm->procloadcol);
		else if (lm->alarmstate == 1)
			fieldmeter_setcolor(fm, 0, lm->warnloadcol);
		else
			fieldmeter_setcolor(fm, 0, lm->critloadcol);
		fieldmeter_drawlegend(fm);
		lm->lastalarmstate = lm->alarmstate;
	}

	/*  Adjust total to next power-of-two of the current load.  */
	if ((fm->fields[0] * 5.0 < fm->total && fm->total > 1.0)
	    || fm->fields[0] > fm->total) {
		i = fm->fields[0];
		i |= i >> 1; i |= i >> 2; i |= i >> 4; i |= i >> 8; i |= i >> 16;
		fm->total = i + 1;  /*  i was 2^n - 1  */
	}

	fm->fields[1] = fm->total - fm->fields[0];
	fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
	LoadMeter *lm = (LoadMeter *)m;

	getloadinfo(lm);

	if (lm->do_cpu_speed) {
		lm->old_cpu_speed = lm->cur_cpu_speed;
		lm->cur_cpu_speed = BSDGetCPUSpeed();

		if (lm->old_cpu_speed != lm->cur_cpu_speed) {
			char l[25];
			snprintf(l, sizeof l, "PROCS/MIN %d MHz", lm->cur_cpu_speed);
			meter_setlegend(m, l);
			fieldmeter_drawlegend(&lm->f);
		}
	}
	fieldmeter_drawfields(&lm->f, 0);
}

Meter *loadmeter_new(XOSView *parent) {
	LoadMeter *lm = (LoadMeter *)meter_alloc(sizeof *lm);

	fieldmeter_init(&lm->f, parent, 2, "LoadMeter", "LOAD", "PROCS/MIN",
	                1, 1, 0);
	lm->f.m.checkres = checkres;
	lm->f.m.checkevent = checkevent;

	lm->procloadcol = lm->warnloadcol = lm->critloadcol = 0;
	lm->warnThreshold = lm->critThreshold = 0;
	lm->alarmstate = lm->lastalarmstate = 0;
	lm->old_cpu_speed = lm->cur_cpu_speed = 0;
	lm->do_cpu_speed = 0;
	lm->f.total = -1.0;

	return &lm->f.m;
}
