/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#ifndef _LOADMETER_H_
#define _LOADMETER_H_

#include "fieldmeter.h"
#include "kstats.h"
#include <kstat.h>

typedef struct {
	FieldMeter f;
	unsigned long procloadcol, warnloadcol, critloadcol;
	unsigned int warnThreshold, critThreshold;
	unsigned int old_cpu_speed, cur_cpu_speed;
	int lastalarmstate;
	int do_cpu_speed;
	KStatList *cpulist;
	kstat_ctl_t *kc;
#ifdef NO_GETLOADAVG
	kstat_t *ksp;
#endif
} LoadMeter;

Meter *loadmeter_new(XOSView *parent, kstat_ctl_t *kc);

#endif
