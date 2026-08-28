/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "MeterMaker.h"
#include "xosview.h"

#include "cpumeter.h"
#include "loadmeter.h"
#include "memmeter.h"
#include "pagemeter.h"
#include "swapmeter.h"

#include <stdlib.h>

void makeMeters(XOSView *xos) {
  if (xosview_isresourcetrue(xos, "load"))
    xosview_addmeter(xos, loadmeter_new(xos));
  if (xosview_isresourcetrue(xos, "cpu"))
    xosview_addmeter(xos, cpumeter_new(xos));
  if (xosview_isresourcetrue(xos, "mem"))
    xosview_addmeter(xos, memmeter_new(xos));
  if (xosview_isresourcetrue(xos, "swap"))
    xosview_addmeter(xos, swapmeter_new(xos));

  if (xosview_isresourcetrue(xos, "page"))
    xosview_addmeter(xos,
                     pagemeter_new(xos,
                                   atof(xosview_getresource(xos,
                                        "pageBandwidth"))));
}
