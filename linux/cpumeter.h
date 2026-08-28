/*
 *  Copyright (c) 1994, 1995, 2004, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _CPUMETER_H_
#define _CPUMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  int lineNum;
  unsigned long long cputime[2][10];
  int cpuindex;
  int kernel;
  int statfields;
} CPUMeter;

/*  cpuID is a /proc/stat line prefix: "cpu" for the aggregate, "cpu0" and
 *  so on for one processor.  */
Meter *cpumeter_new(XOSView *parent, const char *cpuID);

int cpumeter_countcpus(void);
const char *cpumeter_cpustr(int num);
int cpumeter_kernelversion(void);

#endif
