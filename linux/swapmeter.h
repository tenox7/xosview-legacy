/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _SWAPMETER_H_
#define _SWAPMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
} SwapMeter;

Meter *swapmeter_new(XOSView *parent);

#endif
