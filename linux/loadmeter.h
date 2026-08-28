/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 *
 *  Most of this code was written by Werner Fink <werner@suse.de>
 *  Only small changes were made on my part (M.R.)
 */

#ifndef _LOADMETER_H_
#define _LOADMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  unsigned long procloadcol, warnloadcol, critloadcol;
  int warnThreshold, critThreshold, alarmstate, lastalarmstate;
  int old_cpu_speed, cur_cpu_speed;
  int do_cpu_speed;
} LoadMeter;

Meter *loadmeter_new(XOSView *parent);

#endif
