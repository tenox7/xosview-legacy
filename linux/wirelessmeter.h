/*
 *  Copyright (c) 1997 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _WIRELESSMETER_H_
#define _WIRELESSMETER_H_

#include "fieldmeter.h"

extern const char WLFILENAME[];

typedef struct {
  FieldMeter f;
  unsigned long poorqualcol, fairqualcol, goodqualcol;
  int lastquality, number;
  char devname[32];
  int lastlink;
} WirelessMeter;

Meter *wirelessmeter_new(XOSView *parent, int ID, const char *wlID);

int wirelessmeter_countdevices(void);
const char *wirelessmeter_str(int num);

#endif
