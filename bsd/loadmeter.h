/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  NetBSD port:
 *  Copyright (c) 1995, 1996, 1997-2002 by Brian Grayson (bgrayson@netbsd.org)
 *
 *  This file was written by Brian Grayson for the NetBSD and xosview
 *    projects.
 *  This file may be distributed under terms of the GPL or of the BSD
 *    license, whichever you choose.  The full license notices are
 *    contained in the files COPYING.GPL and COPYING.BSD, which you
 *    should have received.  If not, contact one of the xosview
 *    authors for a copy.
 */

#ifndef _LOADMETER_H_
#define _LOADMETER_H_

#include "fieldmeter.h"

typedef struct {
	FieldMeter f;
	unsigned long procloadcol, warnloadcol, critloadcol;
	int warnThreshold, critThreshold, alarmstate, lastalarmstate;
	int old_cpu_speed, cur_cpu_speed;
	int do_cpu_speed;
} LoadMeter;

Meter *loadmeter_new(XOSView *parent);

#endif
