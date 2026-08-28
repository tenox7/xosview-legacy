/*
 *  Copyright (c) 2013 by Tomi Tapper ( tomi.o.tapper@student.jyu.fi )
 *
 *  Based on linux/btrymeter.cc:
 *  Copyright (c) 1997, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "btrymeter.h"
#include "defines.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

static void checkres(Meter *m) {
	BtryMeter *bm = (BtryMeter *)m;
	FieldMeter *fm = &bm->f;

	fieldmeter_checkresources(m);

	bm->leftcolor = xwin_alloccolor(m->xw,
	                                xwin_getresource(m->xw,
	                                                 "batteryLeftColor"));
	bm->usedcolor = xwin_alloccolor(m->xw,
	                                xwin_getresource(m->xw,
	                                                 "batteryUsedColor"));
	bm->chargecolor = xwin_alloccolor(m->xw,
	                                  xwin_getresource(m->xw,
	                                                   "batteryChargeColor"));
	bm->fullcolor = xwin_alloccolor(m->xw,
	                                xwin_getresource(m->xw,
	                                                 "batteryFullColor"));
	bm->lowcolor = xwin_alloccolor(m->xw,
	                               xwin_getresource(m->xw,
	                                                "batteryLowColor"));
	bm->critcolor = xwin_alloccolor(m->xw,
	                                xwin_getresource(m->xw,
	                                                 "batteryCritColor"));
	bm->nonecolor = xwin_alloccolor(m->xw,
	                                xwin_getresource(m->xw,
	                                                 "batteryNoneColor"));

	fieldmeter_setcolor(fm, 0, bm->leftcolor);
	fieldmeter_setcolor(fm, 1, bm->usedcolor);

	m->priority = atoi(xwin_getresource(m->xw, "batteryPriority"));
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw,
	                                              "batteryUsedFormat"));
}

static void getstats(BtryMeter *bm) {
	FieldMeter *fm = &bm->f;
	Meter *m = &fm->m;
	int remaining;
	unsigned int state;

	BSDGetBatteryInfo(&remaining, &state);

	if (state != bm->old_state) {
		if (state == XOSVIEW_BATT_NONE) {  /*  no battery present  */
			fieldmeter_setcolor(fm, 0, bm->nonecolor);
			meter_setlegend(m, "NONE/NONE");
		} else if (state & XOSVIEW_BATT_FULL) {  /*  full battery  */
			fieldmeter_setcolor(fm, 0, bm->fullcolor);
			meter_setlegend(m, "CHRG/FULL");
		} else {  /*  present, not full  */
			if (state & XOSVIEW_BATT_CRITICAL)  /*  critical charge  */
				fieldmeter_setcolor(fm, 0, bm->critcolor);
			else if (state & XOSVIEW_BATT_LOW)  /*  low charge  */
				fieldmeter_setcolor(fm, 0, bm->lowcolor);
			else {  /*  above low, below full  */
				if (state & XOSVIEW_BATT_CHARGING)  /*  is charging  */
					fieldmeter_setcolor(fm, 0, bm->chargecolor);
				else
					fieldmeter_setcolor(fm, 0, bm->leftcolor);
			}
			/*  legend tells if charging or discharging  */
			if (state & XOSVIEW_BATT_CHARGING)
				meter_setlegend(m, "CHRG/AC");
			else
				meter_setlegend(m, "CHRG/USED");
		}
		fieldmeter_drawlegend(fm);
		/*  make sure the field changes colour too  */
		xosview_draw(m->parent);
		bm->old_state = state;
	}

	fm->total = 100.0;
	fm->fields[0] = remaining;
	fm->fields[1] = fm->total - remaining;
	fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
	getstats((BtryMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *btrymeter_new(XOSView *parent) {
	BtryMeter *bm = (BtryMeter *)meter_alloc(sizeof *bm);

	fieldmeter_init(&bm->f, parent, 2, "BtryMeter", "BTRY", "CHRG/USED",
	                1, 1, 0);
	bm->f.m.checkres = checkres;
	bm->f.m.checkevent = checkevent;

	bm->leftcolor = bm->usedcolor = bm->chargecolor = bm->fullcolor = 0;
	bm->lowcolor = bm->critcolor = bm->nonecolor = 0;
	bm->old_state = 255;

	return &bm->f.m;
}
