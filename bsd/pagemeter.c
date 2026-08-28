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

#include "pagemeter.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

static void checkres(Meter *m) {
	FieldMeter *fm = (FieldMeter *)m;

	fieldmeter_checkresources(m);

	fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "pageInColor"));
	fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "pageOutColor"));
	fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "pageIdleColor"));
	m->priority = atoi(xwin_getresource(m->xw, "pagePriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "pageDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "pageGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "pageUsedFormat"));
}

static void getpageinfo(PageMeter *pm) {
	FieldMeter *fm = &pm->f;
	uint64_t info[2];

	BSDGetPageStats(NULL, info);

	fm->fields[0] = info[0] - pm->previnfo[0];
	fm->fields[1] = info[1] - pm->previnfo[1];
	pm->previnfo[0] = info[0];
	pm->previnfo[1] = info[1];

	if (fm->total < fm->fields[0] + fm->fields[1])
		fm->total = fm->fields[0] + fm->fields[1];

	fm->fields[2] = fm->total - fm->fields[0] - fm->fields[1];
	fieldmeter_setused(fm, fm->total - fm->fields[2], fm->total);
}

static void checkevent(Meter *m) {
	getpageinfo((PageMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *pagemeter_new(XOSView *parent, double total) {
	PageMeter *pm = (PageMeter *)meter_alloc(sizeof *pm);

	fieldmeter_init(&pm->f, parent, 3, "PageMeter", "PAGE", "IN/OUT/IDLE",
	                0, 0, 0);
	pm->f.m.checkres = checkres;
	pm->f.m.checkevent = checkevent;

	pm->f.total = total;
	BSDPageInit();
	BSDGetPageStats(NULL, pm->previnfo);

	return &pm->f.m;
}
