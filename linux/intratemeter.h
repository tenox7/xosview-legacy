/*
 *  Copyright (c) 2014 by Tomi Tapper (tomi.o.tapper@jyu.fi)
 *
 *  Based on bsd/intratemeter.* by
 *    Copyright (c) 1999 by Brian Grayson (bgrayson@netbsd.org)
 *  and on linux/intmeter.* by
 *    Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _IRQRATEMETER_H_
#define _IRQRATEMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  unsigned long long lastirqs;
  unsigned int cpucount;
} IrqRateMeter;

Meter *irqratemeter_new(XOSView *parent);

#endif
