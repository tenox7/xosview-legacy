/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _MEMMETER_H_
#define _MEMMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
} MemMeter;

Meter *memmeter_new(XOSView *parent);

#endif
