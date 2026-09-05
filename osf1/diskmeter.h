/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _DISKMETER_H_
#define _DISKMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  float maxspeed;
  int ok;
  double prev;
  int first;
} DiskMeter;

Meter *diskmeter_new(XOSView *parent, float max);

#endif
