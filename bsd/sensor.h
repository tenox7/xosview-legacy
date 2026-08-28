/*
 *  Copyright (c) 2012 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  File based on linux/lmstemp.* by
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _BSDSENSOR_H_
#define _BSDSENSOR_H_

#include "sensorfieldmeter.h"

#define NAMESIZE 32

typedef struct {
	SensorFieldMeter s;
	char name[NAMESIZE], highname[NAMESIZE], lowname[NAMESIZE];
	char val[NAMESIZE], highval[NAMESIZE], lowval[NAMESIZE];
	int nbr;
} BSDSensor;

Meter *bsdsensor_new(XOSView *parent, const char *name, const char *high,
                     const char *low, const char *label, const char *caption,
                     int nbr);

#endif
