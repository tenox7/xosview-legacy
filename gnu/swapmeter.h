/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *  2007 by Samuel Thibault ( samuel.thibault@ens-lyon.org )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _SWAPMETER_H_
#define _SWAPMETER_H_

#include "fieldmeter.h"
#include <mach/mach_types.h>
#include <mach/default_pager_types.h>

typedef struct {
  FieldMeter f;
  struct default_pager_info def_pager_info;
  mach_port_t def_pager;
} SwapMeter;

Meter *swapmeter_new(XOSView *parent);

#endif
