/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#ifndef _MEMMETER_H_
#define _MEMMETER_H_

#include "fieldmeter.h"
#include <kstat.h>

typedef struct {
	FieldMeter f;
	int pageSize;
	kstat_ctl_t *kc;
	kstat_t *ksp_sp, *ksp_zfs;
} MemMeter;

Meter *memmeter_new(XOSView *parent, kstat_ctl_t *kc);

#endif
