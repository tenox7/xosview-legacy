/*
 *  Copyright (c) 2008 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  File based on linux/lmstemp.* by
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  This file may be distributed under terms of the GPL
 */
/*
 *  Read coretemp reading with sysctl and display actual temperature.
 *  If actual >= high, actual temp changes color to indicate alarm.
 */

#include "coretemp.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>

unsigned int coretemp_countcpus(void) {
	return BSDGetCPUTemperature(NULL, NULL);
}

static void checkres(Meter *m) {
	CoreTemp *ct = (CoreTemp *)m;
	FieldMeter *fm = &ct->f;
	const char *highest, *high;
	float total = -300.0;
	float *tjmax;
	char l[32];
	int i;

	fieldmeter_checkresources(m);

	ct->actcolor = xwin_alloccolor(m->xw,
	                               xwin_getresource(m->xw,
	                                                "coretempActColor"));
	ct->highcolor = xwin_alloccolor(m->xw,
	                                xwin_getresource(m->xw,
	                                                 "coretempHighColor"));
	fieldmeter_setcolor(fm, 0, ct->actcolor);
	fieldmeter_setcolorname(fm, 1,
	                        xwin_getresource(m->xw, "coretempIdleColor"));
	fieldmeter_setcolor(fm, 2, ct->highcolor);

	m->priority = atoi(xwin_getresource(m->xw, "coretempPriority"));
	highest = xwin_getresource_default(m->xw, "coretempHighest", "100");
	fm->total = atoi(highest);
	high = xwin_getresource_default(m->xw, "coretempHigh", NULL);
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw,
	                                              "coretempUsedFormat"));

	/*  Get tjMax here and use as total.  */
	tjmax = (float *)calloc(ct->cpucount, sizeof(float));
	BSDGetCPUTemperature(ct->temps, tjmax);
	for (i = 0; i < ct->cpucount; i++)
		if (tjmax[i] > total)
			total = tjmax[i];
	free(tjmax);
	if (total > 0.0)
		fm->total = total;

	if (!high) {
		ct->high = fm->total;
		snprintf(l, sizeof l, "ACT(\260C)/HIGH/%d", (int)fm->total);
	} else {
		ct->high = atoi(high);
		snprintf(l, sizeof l, "ACT(\260C)/%d/%d", (int)ct->high,
		         (int)fm->total);
	}
	meter_setlegend(m, l);
}

static void getcoretemp(CoreTemp *ct) {
	FieldMeter *fm = &ct->f;
	Meter *m = &fm->m;
	int i;

	BSDGetCPUTemperature(ct->temps, NULL);

	fm->fields[0] = 0.0;
	if (ct->cpu >= 0 && ct->cpu < ct->cpucount) {  /*  one core  */
		fm->fields[0] = ct->temps[ct->cpu];
	} else if (ct->cpu == -1) {  /*  average  */
		float tempval = 0.0;
		for (i = 0; i < ct->cpucount; i++)
			tempval += ct->temps[i];
		fm->fields[0] = tempval / (float)ct->cpucount;
	} else if (ct->cpu == -2) {  /*  maximum  */
		float tempval = -300.0;
		for (i = 0; i < ct->cpucount; i++)
			if (ct->temps[i] > tempval)
				tempval = ct->temps[i];
		fm->fields[0] = tempval;
	} else {  /*  should not happen  */
		fprintf(stderr, "Unknown CPU core number in coretemp.\n");
		xwin_setdone(m->xw, 1);
		return;
	}

	fieldmeter_setused(fm, fm->fields[0], fm->total);
	if (fm->fields[0] < 0)
		fm->fields[0] = 0;
	fm->fields[1] = ct->high - fm->fields[0];
	fm->fields[2] = fm->total - fm->fields[1] - fm->fields[0];
	if (fm->fields[0] > fm->total)
		fm->fields[0] = fm->total;
	if (fm->fields[2] < 0)
		fm->fields[2] = 0;

	if (fm->fields[1] < 0) {  /*  alarm: T > high  */
		fm->fields[1] = 0;
		if (fm->colors[0] != ct->highcolor) {
			fieldmeter_setcolor(fm, 0, ct->highcolor);
			fieldmeter_drawlegend(fm);
		}
	} else {
		if (fm->colors[0] != ct->actcolor) {
			fieldmeter_setcolor(fm, 0, ct->actcolor);
			fieldmeter_drawlegend(fm);
		}
	}
}

static void checkevent(Meter *m) {
	getcoretemp((CoreTemp *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

static void destroy(Meter *m) {
	CoreTemp *ct = (CoreTemp *)m;

	free(ct->temps);
	ct->temps = NULL;
	fieldmeter_fini(m);
}

Meter *coretemp_new(XOSView *parent, const char *label, const char *caption,
                    int cpu) {
	CoreTemp *ct = (CoreTemp *)meter_alloc(sizeof *ct);

	fieldmeter_init(&ct->f, parent, 3, "CoreTemp", label, caption, 1, 1, 1);
	ct->f.m.checkres = checkres;
	ct->f.m.checkevent = checkevent;
	ct->f.m.destroy = destroy;
	ct->f.metric = 1;

	ct->cpu = cpu;
	ct->high = 0;
	ct->actcolor = ct->highcolor = 0;
	ct->cpucount = coretemp_countcpus();
	ct->temps = (float *)calloc(ct->cpucount, sizeof(float));

	return &ct->f.m;
}
