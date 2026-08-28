/*
 *  Copyright (c) 1994, 1995, 2002 by Mike Romberg ( romberg@fsl.noaa.gov )
 *  2007 by Samuel Thibault ( samuel.thibault@ens-lyon.org )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "MeterMaker.h"
#include "xosview.h"

#include "memmeter.h"
#include "swapmeter.h"
#include "pagemeter.h"
#include "loadmeter.h"

#include <stdlib.h>

void makeMeters(XOSView *xos) {
  if (xosview_isresourcetrue(xos, "load"))
    xosview_addmeter(xos, loadmeter_new(xos));

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
