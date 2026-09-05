/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _IRQRATEMETER_H_
#define _IRQRATEMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  int ok;
  double lastirqcount;
  int first;
} IrqRateMeter;

Meter *irqratemeter_new(XOSView *parent);

#endif
