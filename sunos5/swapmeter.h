/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#ifndef _SWAPMETER_H_
#define _SWAPMETER_H_

#include "fieldmeter.h"
#include <kstat.h>
#include <stddef.h>

typedef struct {
	FieldMeter f;
	size_t pagesize;
} SwapMeter;

Meter *swapmeter_new(XOSView *parent, kstat_ctl_t *kc);

#endif
