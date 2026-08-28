/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _NETMETER_H_
#define _NETMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  float maxpackets;
  char netIface[64];
  int usesysfs, ignored;
  unsigned long long lastBytesIn, lastBytesOut;
} NetMeter;

Meter *netmeter_new(XOSView *parent, float max);

#endif
