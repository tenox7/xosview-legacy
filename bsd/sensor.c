/*
 *  Copyright (c) 2012 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  File based on linux/lmstemp.* by
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  This file may be distributed under terms of the GPL
 */

#include "sensor.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*  Split "device.value" into its two halves.  A name with no dot lands
 *  whole in both, which is what the C++ substr() calls did with npos.  */
static void splitname(const char *src, char *dev, char *val, size_t size) {
	const char *dot = strchr(src, '.');

	if (dot) {
		size_t n = (size_t)(dot - src);
		if (n > size - 1)
			n = size - 1;
		memcpy(dev, src, n);
		dev[n] = '\0';
		snprintf(val, size, "%s", dot + 1);
	} else {
		snprintf(dev, size, "%s", src);
		snprintf(val, size, "%s", src);
	}
}

static void checkres(Meter *m) {
	BSDSensor *bs = (BSDSensor *)m;
	SensorFieldMeter *sm = &bs->s;
	FieldMeter *fm = &sm->f;
	char s[32];
	const char *tmp, *f;
	float dummy;

	fieldmeter_checkresources(m);

	sm->actcolor = xwin_alloccolor(m->xw,
	                               xwin_getresource(m->xw,
	                                                "bsdsensorActColor"));
	sm->highcolor = xwin_alloccolor(m->xw,
	                                xwin_getresource(m->xw,
	                                                 "bsdsensorHighColor"));
	sm->lowcolor = xwin_alloccolor(m->xw,
	                               xwin_getresource(m->xw,
	                                                "bsdsensorLowColor"));
	fieldmeter_setcolor(fm, 0, sm->actcolor);
	fieldmeter_setcolorname(fm, 1,
	                        xwin_getresource(m->xw, "bsdsensorIdleColor"));
	fieldmeter_setcolor(fm, 2, sm->highcolor);
	m->priority = atoi(xwin_getresource(m->xw, "bsdsensorPriority"));

	tmp = xwin_getresource_default(m->xw, "bsdsensorHighest", "0");
	snprintf(s, sizeof s, "bsdsensorHighest%d", bs->nbr);
	fm->total = fabs(atof(xwin_getresource_default(m->xw, s, tmp)));
	snprintf(s, sizeof s, "bsdsensorUsedFormat%d", bs->nbr);
	f = xwin_getresource_default(m->xw, s, NULL);
	fieldmeter_setusedformat(fm, f ? f
	                         : xwin_getresource(m->xw, "bsdsensorUsedFormat"));

	if (!sm->has_high)
		sm->high = fm->total;
	if (!sm->has_low)
		sm->low = 0;

	/*  Get the unit.  */
	BSDGetSensor(bs->name, bs->val, &dummy, sm->unit);
	sensorfieldmeter_updatelegend(sm);
}

static void getsensor(BSDSensor *bs) {
	SensorFieldMeter *sm = &bs->s;
	float value = 0.0;
	float high = (float)sm->high, low = (float)sm->low;

	BSDGetSensor(bs->name, bs->val, &value, NULL);
	if (strlen(bs->highname))
		BSDGetSensor(bs->highname, bs->highval, &high, NULL);
	if (strlen(bs->lowname))
		BSDGetSensor(bs->lowname, bs->lowval, &low, NULL);

	sm->f.fields[0] = value;
	sensorfieldmeter_checkfields(sm, low, high);
}

static void checkevent(Meter *m) {
	getsensor((BSDSensor *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *bsdsensor_new(XOSView *parent, const char *name, const char *high,
                     const char *low, const char *label, const char *caption,
                     int nbr) {
	BSDSensor *bs = (BSDSensor *)meter_alloc(sizeof *bs);
	SensorFieldMeter *sm = &bs->s;

	sensorfieldmeter_init(sm, parent, "BSDSensor", label, caption, 1, 1, 0);
	sm->f.m.checkres = checkres;
	sm->f.m.checkevent = checkevent;

	bs->nbr = nbr;
	bs->highname[0] = bs->highval[0] = '\0';
	bs->lowname[0] = bs->lowval[0] = '\0';
	splitname(name, bs->name, bs->val, NAMESIZE);

	if (high) {
		sm->has_high = 1;
		/*  high given as a number?  */
		if (sscanf(high, "%lf", &sm->high) == 0)
			splitname(high, bs->highname, bs->highval, NAMESIZE);
	}
	if (low) {
		sm->has_low = 1;
		/*  low given as a number?  */
		if (sscanf(low, "%lf", &sm->low) == 0)
			splitname(low, bs->lowname, bs->lowval, NAMESIZE);
	}

	return &sm->f.m;
}
