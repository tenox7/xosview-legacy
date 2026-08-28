/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _INTMETER_H_
#define _INTMETER_H_

#include "bitmeter.h"

/*  Raw interrupt number to bit index, kept sorted by key.  This stands in
 *  for the std::map the C++ version used.  */
typedef struct {
	int key, index;
} BsdIntNum;

typedef struct {
	BitMeter b;
	uint64_t *irqs, *lastirqs;
	unsigned int *inbrs;
	unsigned int irqcount;
	BsdIntNum *nums;
	int nnums, numcap;
} IntMeter;

Meter *intmeter_new(XOSView *parent, int dolegends, int dousedlegends);

#endif
