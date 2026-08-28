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

#include "swapmeter.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

static void checkres(Meter *m) {
	FieldMeter *fm = (FieldMeter *)m;

	fieldmeter_checkresources(m);

	fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "swapUsedColor"));
	fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "swapFreeColor"));
	m->priority = atoi(xwin_getresource(m->xw, "swapPriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "swapDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "swapGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "swapUsedFormat"));
}

static void getswapinfo(SwapMeter *sm) {
	FieldMeter *fm = &sm->f;
	uint64_t total = 0, used = 0;

	BSDGetSwapInfo(&total, &used);

	fm->total = (double)total;
	if (fm->total == 0.0)
		fm->total = 1.0;  /*  no division by zero, now, do we?  :)  */
	fm->fields[0] = (double)used;
	fm->fields[1] = fm->total;

	fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
	getswapinfo((SwapMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *swapmeter_new(XOSView *parent) {
	SwapMeter *sm = (SwapMeter *)meter_alloc(sizeof *sm);

	fieldmeter_init(&sm->f, parent, 2, "SwapMeter", "SWAP", "USED/FREE",
	                0, 0, 0);
	sm->f.m.checkres = checkres;
	sm->f.m.checkevent = checkevent;

	BSDSwapInit();

	return &sm->f.m;
}
