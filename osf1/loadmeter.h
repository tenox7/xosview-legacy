/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _LOADMETER_H_
#define _LOADMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  unsigned long procloadcol, warnloadcol, critloadcol;
  int ok;
  int warnThreshold, critThreshold, alarmstate, lastalarmstate;
} LoadMeter;

Meter *loadmeter_new(XOSView *parent);

#endif
