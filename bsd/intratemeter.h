/*
 *  Copyright (c) 1999 by Brian Grayson (bgrayson@netbsd.org)
 *
 *  This file may be distributed under terms of the GPL or of the BSD
 *    license, whichever you choose.  The full license notices are
 *    contained in the files COPYING.GPL and COPYING.BSD, which you
 *    should have received.  If not, contact one of the xosview
 *    authors for a copy.
 */

#ifndef _IRQRATEMETER_H_
#define _IRQRATEMETER_H_

#include "fieldmeter.h"

typedef struct {
	FieldMeter f;
	uint64_t *irqs, *lastirqs;
	unsigned int irqcount;
} IrqRateMeter;

Meter *irqratemeter_new(XOSView *parent);

#endif
