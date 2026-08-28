/*
 *  Copyright (c) 1999 by Brian Grayson (bgrayson@netbsd.org)
 *
 *  This file may be distributed under terms of the GPL or of the BSD
 *    license, whichever you choose.  The full license notices are
 *    contained in the files COPYING.GPL and COPYING.BSD, which you
 *    should have received.  If not, contact one of the xosview
 *    authors for a copy.
 */

#include "intratemeter.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <err.h>
#include <stdlib.h>
#include <strings.h>

static void checkres(Meter *m) {
	IrqRateMeter *im = (IrqRateMeter *)m;
	FieldMeter *fm = &im->f;

	fieldmeter_checkresources(m);
	fieldmeter_setcolorname(fm, 0,
	                        xwin_getresource(m->xw, "irqrateUsedColor"));
	fieldmeter_setcolorname(fm, 1,
	                        xwin_getresource(m->xw, "irqrateIdleColor"));
	m->priority = atoi(xwin_getresource(m->xw, "irqratePriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "irqrateDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "irqrateGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "irqrateUsedFormat"));
	fm->total = 2000;

	BSDGetIntrStats(im->lastirqs, NULL);
}

static void getinfo(IrqRateMeter *im) {
	FieldMeter *fm = &im->f;
	int delta = 0;
	unsigned int i;

	fieldmeter_timerstop(fm);
	BSDGetIntrStats(im->irqs, NULL);

	for (i = 0; i <= im->irqcount; i++) {
		delta += im->irqs[i] - im->lastirqs[i];
		im->lastirqs[i] = im->irqs[i];
	}
	bzero(im->irqs, (im->irqcount + 1) * sizeof(im->irqs[0]));

	/*  Scale delta by the priority.  */
	fm->fields[0] = delta / fieldmeter_secs(fm);

	/*  Bump total, if needed.  */
	if (fm->fields[0] > fm->total)
		fm->total = fm->fields[0];

	fieldmeter_setused(fm, fm->fields[0], fm->total);
	fieldmeter_timerstart(fm);
}

static void checkevent(Meter *m) {
	getinfo((IrqRateMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

static void destroy(Meter *m) {
	IrqRateMeter *im = (IrqRateMeter *)m;

	free(im->irqs);
	free(im->lastirqs);
	im->irqs = im->lastirqs = NULL;
	fieldmeter_fini(m);
}

Meter *irqratemeter_new(XOSView *parent) {
	IrqRateMeter *im = (IrqRateMeter *)meter_alloc(sizeof *im);

	fieldmeter_init(&im->f, parent, 2, "IrqRateMeter", "IRQs",
	                "IRQs per sec/IDLE", 1, 1, 0);
	im->f.m.checkres = checkres;
	im->f.m.checkevent = checkevent;
	im->f.m.destroy = destroy;

	if (!BSDIntrInit()) {
		warnx("The kernel does not seem to have the symbols needed for "
		      "the IrqRateMeter.");
		warnx("The IrqRateMeter has been disabled.");
		fieldmeter_disable(&im->f);
	}
	im->irqcount = BSDNumInts();
	im->irqs = (uint64_t *)calloc(im->irqcount + 1, sizeof(uint64_t));
	im->lastirqs = (uint64_t *)calloc(im->irqcount + 1, sizeof(uint64_t));

	return &im->f.m;
}
