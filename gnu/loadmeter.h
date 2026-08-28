/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *  2007 by Samuel Thibault ( samuel.thibault@ens-lyon.org )
 *
 *  This file may be distributed under terms of the GPL
 */
/*
 *  Most of this code was written by Werner Fink <werner\@suse.de>
 *  Only small changes were made on my part (M.R.)
 */

#ifndef _LOADMETER_H_
#define _LOADMETER_H_

#include "fieldmeter.h"

typedef struct {
  FieldMeter f;
  unsigned long procloadcol, warnloadcol, critloadcol;
  int warnThreshold, critThreshold, alarmstate, lastalarmstate;
} LoadMeter;

Meter *loadmeter_new(XOSView *parent);

#endif
