/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _MEMMETER_H_
#define _MEMMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  int pageSize;
  int pass;
} MemMeter;

Meter *memmeter_new(XOSView *parent);

#endif
