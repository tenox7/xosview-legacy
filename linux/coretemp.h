/*
 *  Copyright (c) 2008-2014 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  File based on linux/lmstemp.* by
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _CORETEMP_H_
#define _CORETEMP_H_

#include "fieldmeter.h"

#define CORETEMP_PATH_SIZE 128

/*  The sysfs paths this meter samples, one per sensor.  */
typedef struct {
  char (*path)[CORETEMP_PATH_SIZE];
  unsigned int n, cap;
} CoreTempPaths;

typedef struct {
  FieldMeter f;
  int pkg, cpu, high;
  CoreTempPaths cpus;
  unsigned long actcolor, highcolor;
} CoreTemp;

/*  cpu >= 0 is a single core, -1 averages the package and -2 takes its
 *  maximum.  */
Meter *coretemp_new(XOSView *parent, const char *label, const char *caption,
                    int pkg, int cpu);

unsigned int coretemp_countcores(unsigned int pkg);
unsigned int coretemp_countcpus(void);

#endif
