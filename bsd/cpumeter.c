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

#include "cpumeter.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

static void checkres(Meter *m) {
	FieldMeter *fm = (FieldMeter *)m;

	fieldmeter_checkresources(m);

	fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "cpuUserColor"));
	fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "cpuNiceColor"));
	fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "cpuSystemColor"));
	fieldmeter_setcolorname(fm, 3,
	                        xwin_getresource(m->xw, "cpuInterruptColor"));
	fieldmeter_setcolorname(fm, 4, xwin_getresource(m->xw, "cpuFreeColor"));
	m->priority = atoi(xwin_getresource(m->xw, "cpuPriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "cpuDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "cpuGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "cpuUsedFormat"));
}

static void getcputime(CPUMeter *cm) {
	FieldMeter *fm = &cm->f;
	uint64_t tempCPU[CPUSTATES];
	unsigned int oldindex;
	int i;

	fm->total = 0;

	BSDGetCPUTimes(tempCPU, cm->nbr);

	oldindex = (cm->cpuindex + 1) % 2;
	for (i = 0; i < CPUSTATES; i++) {
		cm->cputime[cm->cpuindex][i] = tempCPU[i];
		fm->fields[i] = cm->cputime[cm->cpuindex][i]
		                - cm->cputime[oldindex][i];
		fm->total += fm->fields[i];
	}
	if (fm->total) {
		fieldmeter_setused(fm, fm->total - fm->fields[4], fm->total);
		cm->cpuindex = (cm->cpuindex + 1) % 2;
	}
}

static void checkevent(Meter *m) {
	getcputime((CPUMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *cpumeter_new(XOSView *parent, unsigned int nbr) {
	CPUMeter *cm = (CPUMeter *)meter_alloc(sizeof *cm);
	char t[8] = "CPU";

	fieldmeter_init(&cm->f, parent, 5, "CPUMeter", "CPU",
	                "USR/NICE/SYS/INT/FREE", 0, 0, 0);
	cm->f.m.checkres = checkres;
	cm->f.m.checkevent = checkevent;

	cm->nbr = nbr;
	cm->cpuindex = 0;
	bzero(cm->cputime, sizeof cm->cputime);
	BSDCPUInit();

	if (cm->nbr > 0)
		snprintf(t, sizeof t, "CPU%u", cm->nbr - 1);
	meter_settitle(&cm->f.m, t);

	return &cm->f.m;
}
