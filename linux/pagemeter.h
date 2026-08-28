/*
 *  Copyright (c) 1996, 2007 by Massimiliano Ghilardi ( ghilardi@cibs.sns.it )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _PAGEMETER_H_
#define _PAGEMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  unsigned long pageinfo[2][2];
  int pageindex;
  float maxspeed;
  int vmstat;
  const char *statFileName;
} PageMeter;

Meter *pagemeter_new(XOSView *parent, float max);

#endif
