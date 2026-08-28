/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _SWAPMETER_H_
#define _SWAPMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  int pass;
} SwapMeter;

Meter *swapmeter_new(XOSView *parent);

#endif
