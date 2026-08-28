/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _CPUMETER_H_
#define _CPUMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  double cputime[2][4];
  int cpuindex;
  int ok;
} CPUMeter;

Meter *cpumeter_new(XOSView *parent);

int cpumeter_countcpus(void);

#endif
