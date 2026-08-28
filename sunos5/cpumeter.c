/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#include "cpumeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/sysinfo.h>

static const char *cpuStr(int num) {
	static char buffer[8] = "CPU";

	if (num >= 0)
		snprintf(buffer + 3, 4, "%d", num);
	buffer[7] = '\0';
	return buffer;
}

static void checkres(Meter *m) {
	FieldMeter *fm = (FieldMeter *)m;

	fieldmeter_checkresources(m);

	fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "cpuUserColor"));
	fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "cpuSystemColor"));
	fieldmeter_setcolorname(fm, 2,
	                        xwin_getresource(m->xw, "cpuInterruptColor"));
	fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "cpuFreeColor"));
	m->priority = atoi(xwin_getresource(m->xw, "cpuPriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "cpuDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "cpuGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "cpuUsedFormat"));
}

static void getcputime(CPUMeter *cm) {
	FieldMeter *fm = &cm->f;
	cpu_stat_t cs;
	int i, oldindex;

	fm->total = 0;

	if (cm->aggregate) {
		unsigned int n;

		kstatlist_update(cm->cpustats, cm->kc);
		bzero(cm->cputime[cm->cpuindex],
		      CPU_STATES * sizeof(cm->cputime[cm->cpuindex][0]));
		for (n = 0; n < kstatlist_count(cm->cpustats); n++) {
			if (kstat_read(cm->kc, kstatlist_at(cm->cpustats, n), &cs) == -1) {
				xwin_setdone(cm->f.m.xw, 1);
				return;
			}
			cm->cputime[cm->cpuindex][0] += cs.cpu_sysinfo.cpu[CPU_USER];
			cm->cputime[cm->cpuindex][1] += cs.cpu_sysinfo.cpu[CPU_KERNEL];
			cm->cputime[cm->cpuindex][2] += cs.cpu_sysinfo.cpu[CPU_WAIT];
			cm->cputime[cm->cpuindex][3] += cs.cpu_sysinfo.cpu[CPU_IDLE];
		}
	} else {
		if (kstat_read(cm->kc, cm->ksp, &cs) == -1) {
			xwin_setdone(cm->f.m.xw, 1);
			return;
		}
		cm->cputime[cm->cpuindex][0] = cs.cpu_sysinfo.cpu[CPU_USER];
		cm->cputime[cm->cpuindex][1] = cs.cpu_sysinfo.cpu[CPU_KERNEL];
		cm->cputime[cm->cpuindex][2] = cs.cpu_sysinfo.cpu[CPU_WAIT];
		cm->cputime[cm->cpuindex][3] = cs.cpu_sysinfo.cpu[CPU_IDLE];
	}

	oldindex = (cm->cpuindex + 1) % 2;
	for (i = 0; i < CPU_STATES; i++) {
		fm->fields[i] = cm->cputime[cm->cpuindex][i] - cm->cputime[oldindex][i];
		fm->total += fm->fields[i];
	}
	cm->cpuindex = (cm->cpuindex + 1) % 2;

	if (fm->total)
		fieldmeter_setused(fm, fm->total - fm->fields[3], fm->total);
}

static void checkevent(Meter *m) {
	getcputime((CPUMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *cpumeter_new(XOSView *parent, kstat_ctl_t *kc, int cpuid) {
	CPUMeter *cm = (CPUMeter *)meter_alloc(sizeof *cm);
	int i, j;

	fieldmeter_init(&cm->f, parent, CPU_STATES, "CPUMeter", cpuStr(cpuid),
	                "USER/SYS/WAIT/IDLE", 0, 0, 0);
	cm->f.m.checkres = checkres;
	cm->f.m.checkevent = checkevent;

	cm->kc = kc;
	cm->aggregate = (cpuid < 0);
	cm->ksp = NULL;
	for (i = 0; i < 2; i++)
		for (j = 0; j < CPU_STATES; j++)
			cm->cputime[i][j] = 0;
	cm->cpuindex = 0;
	cm->cpustats = kstatlist_get(kc, KSL_CPU_STAT);

	if (!cm->aggregate) {
		unsigned int n;
		for (n = 0; n < kstatlist_count(cm->cpustats); n++)
			if (kstatlist_at(cm->cpustats, n)->ks_instance == cpuid)
				cm->ksp = kstatlist_at(cm->cpustats, n);
	}

	return &cm->f.m;
}
