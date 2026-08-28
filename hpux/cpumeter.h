/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _CPUMETER_H_
#define _CPUMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  float cputime[2][5];
  int cpuindex;
} CPUMeter;

Meter *cpumeter_new(XOSView *parent);

#endif
