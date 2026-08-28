/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 *
 *  Small changes for Irix 6.5 port Stefan Eilemann <eilemann@gmail.com>
 */

#ifndef _LOADMETER_H_
#define _LOADMETER_H_

#include "fieldmeter.h"
#include <rpcsvc/rstat.h>

typedef struct {
    FieldMeter f;
    unsigned long procloadcol, warnloadcol, critloadcol;
    int warnThreshold, critThreshold, alarmstate, lastalarmstate;
    char hostname[256];
    struct statstime res;
} LoadMeter;

Meter *loadmeter_new(XOSView *parent);

#endif
