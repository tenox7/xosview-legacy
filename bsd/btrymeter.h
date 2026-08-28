/*
 *  Copyright (c) 2013 by Tomi Tapper ( tomi.o.tapper@student.jyu.fi )
 *
 *  Based on linux/btrymeter.h:
 *  Copyright (c) 1997, 2005, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _BTRYMETER_H_
#define _BTRYMETER_H_

#include "fieldmeter.h"

typedef struct {
	FieldMeter f;
	unsigned long leftcolor, usedcolor, chargecolor, fullcolor,
	              lowcolor, critcolor, nonecolor;
	unsigned int old_state;
} BtryMeter;

Meter *btrymeter_new(XOSView *parent);

#endif
