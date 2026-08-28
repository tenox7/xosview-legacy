/*
 *  Copyright (c) 1997 by Mike Romberg (romberg@fsl.noaa.gov)
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _PAGEMETER_H_
#define _PAGEMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  float pageinfo[2][2];
  int pageindex;
  float maxspeed;
} PageMeter;

Meter *pagemeter_new(XOSView *parent, float max);

#endif
