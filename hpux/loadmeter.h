/*
 *  Copyright (c) 1994, 1995, 1997 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _LOADMETER_H_
#define _LOADMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  unsigned long procloadcol, warnloadcol, critloadcol;
  int warnThreshold, critThreshold, alarmstate, lastalarmstate;
} LoadMeter;

Meter *loadmeter_new(XOSView *parent);

#endif
