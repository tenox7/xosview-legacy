/*
 *  Copyright (c) 2014 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  File based on bsd/intratemeter.* by
 *  Copyright (c) 1999 by Brian Grayson (bgrayson@netbsd.org)
 *
 *  This file may be distributed under terms of the GPL
 */

#include "intratemeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

static void checkres(Meter *m) {
	FieldMeter *fm = (FieldMeter *)m;

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
}

static void getinfo(IrqRateMeter *im) {
	FieldMeter *fm = &im->f;
	kstat_named_t *k;
	uint64_t irqcount = 0;
	unsigned int i;

	kstatlist_update(im->cpus, im->kc);
	fieldmeter_timerstop(fm);
	for (i = 0; i < kstatlist_count(im->cpus); i++) {
		kstat_t *ksp = kstatlist_at(im->cpus, i);
		if (kstat_read(im->kc, ksp, NULL) == -1) {
			xwin_setdone(fm->m.xw, 1);
			return;
		}
		k = (kstat_named_t *)kstat_data_lookup(ksp, "intr");
		if (k == NULL) {
			xwin_setdone(fm->m.xw, 1);
			return;
		}
		irqcount += kstat_to_ui64(k);
	}
	if (im->lastirqcount == 0)
		im->lastirqcount = irqcount;

	fm->fields[0] = (irqcount - im->lastirqcount) / fieldmeter_secs(fm);
	im->lastirqcount = irqcount;
	fieldmeter_timerstart(fm);

	if (fm->fields[0] > fm->total)
		fm->total = fm->fields[0];

	fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
	getinfo((IrqRateMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *irqratemeter_new(XOSView *parent, kstat_ctl_t *kc) {
	IrqRateMeter *im = (IrqRateMeter *)meter_alloc(sizeof *im);

	fieldmeter_init(&im->f, parent, 2, "IrqRateMeter", "IRQs",
	                "IRQs per sec/IDLE", 1, 1, 0);
	im->f.m.checkres = checkres;
	im->f.m.checkevent = checkevent;

	im->lastirqcount = 0;
	im->kc = kc;
	im->cpus = kstatlist_get(kc, KSL_CPU_SYS);

	return &im->f.m;
}
