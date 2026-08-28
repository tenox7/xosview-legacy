/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _MeterMaker_h
#define _MeterMaker_h

#include "fwd.h"

/*  Each platform provides this.  It creates the meters the resources ask
 *  for and hands each one to xosview_addmeter().  */
void makeMeters(XOSView *xos);

#endif
