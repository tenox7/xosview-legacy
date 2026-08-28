/*
 *  Copyright (c) 2008 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  File based on linux/lmstemp.* by
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _CORETEMP_H_
#define _CORETEMP_H_

#include "fieldmeter.h"

typedef struct {
	FieldMeter f;
	int cpu, cpucount;
	float high, *temps;
	unsigned long actcolor, highcolor;
} CoreTemp;

/*  cpu >= 0 is a single core, -1 averages them and -2 takes the maximum.  */
Meter *coretemp_new(XOSView *parent, const char *label, const char *caption,
                    int cpu);

unsigned int coretemp_countcpus(void);

#endif
