/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _INTMETER_H_
#define _INTMETER_H_

#include "bitmeter.h"

typedef struct {
  BitMeter b;
  unsigned long *irqs, *lastirqs;
  int cpu;
  int separate;
} IntMeter;

Meter *intmeter_new(XOSView *parent, int cpu);

#endif
