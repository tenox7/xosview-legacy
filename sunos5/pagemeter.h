/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#ifndef _PAGEMETER_H_
#define _PAGEMETER_H_

#include "fieldmeter.h"
#include "kstats.h"
#include <kstat.h>

typedef struct {
	FieldMeter f;
	float pageinfo[2][2];
	int pageindex;
	float maxspeed;
	KStatList *cpustats;
	kstat_ctl_t *kc;
} PageMeter;

Meter *pagemeter_new(XOSView *parent, kstat_ctl_t *kc, float max);

#endif
