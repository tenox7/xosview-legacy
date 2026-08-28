/*
 *  Copyright (c) 2014 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  This file may be distributed under terms of the GPL
 *
 *  Put code common to *BSD and Linux sensor meters here.
 */

#ifndef _SENSORFIELDMETER_H_
#define _SENSORFIELDMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  char unit[8];
  double high, low;
  int has_high, has_low, negative;
  unsigned long actcolor, highcolor, lowcolor;
} SensorFieldMeter;

void sensorfieldmeter_init(SensorFieldMeter *sm, XOSView *parent,
                           const char *name, const char *title,
                           const char *legend, int docaptions, int dolegends,
                           int dousedlegends);

void sensorfieldmeter_updatelegend(SensorFieldMeter *sm);

/*  Check if the meter needs to be flipped, or the total or limits changed,
 *  and whether an alarm limit has been reached.  Call after reading the
 *  values.  */
void sensorfieldmeter_checkfields(SensorFieldMeter *sm, double low,
                                  double high);

#endif
