/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _SWAPMETER_H_
#define _SWAPMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  int ok;
} SwapMeter;

Meter *swapmeter_new(XOSView *parent);

#endif
