/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *  2007 by Samuel Thibault ( samuel.thibault@ens-lyon.org )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _MEMMETER_H_
#define _MEMMETER_H_

#include "fieldmeter.h"
#include <mach/vm_statistics.h>

typedef struct {
  FieldMeter f;
  struct vm_statistics vmstats;
} MemMeter;

Meter *memmeter_new(XOSView *parent);

#endif
