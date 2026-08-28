/*
 *  Copyright (c) 2014 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  File based on bsd/intratemeter.* by
 *  Copyright (c) 1999 by Brian Grayson (bgrayson@netbsd.org)
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _IRQRATEMETER_H_
#define _IRQRATEMETER_H_

#include "fieldmeter.h"
#include "kstats.h"
#include <sys/types.h>
#include <kstat.h>

typedef struct {
	FieldMeter f;
	uint64_t lastirqcount;
	kstat_ctl_t *kc;
	KStatList *cpus;
} IrqRateMeter;

Meter *irqratemeter_new(XOSView *parent, kstat_ctl_t *kc);

#endif
