/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _MEMMETER_H_
#define _MEMMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  int ok;
} MemMeter;

Meter *memmeter_new(XOSView *parent);

#endif
