/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#ifndef _CPUMETER_H_
#define _CPUMETER_H_

#include "fieldmeter.h"
#include "kstats.h"
#include <kstat.h>
#include <sys/sysinfo.h>

typedef struct {
	FieldMeter f;
	float cputime[2][CPU_STATES];
	int cpuindex;
	KStatList *cpustats;
	int aggregate;
	kstat_ctl_t *kc;
	kstat_t *ksp;
} CPUMeter;

/*  A cpuid below zero aggregates every processor into one meter.  */
Meter *cpumeter_new(XOSView *parent, kstat_ctl_t *kc, int cpuid);

#endif
