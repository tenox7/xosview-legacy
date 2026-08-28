/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#ifndef _GFXMETER_H_
#define _GFXMETER_H_

#include "sarmeter.h"
#include "fieldmeter.h"

#include <rpcsvc/rstat.h>

typedef struct {
    FieldMeter f;
    unsigned long swapgfxcol, warngfxcol, critgfxcol;
    int warnThreshold, critThreshold, alarmstate, lastalarmstate;
    int nPipes;
} GfxMeter;

Meter *gfxmeter_new(XOSView *parent, int max);

int gfxmeter_npipes(void);

#endif
