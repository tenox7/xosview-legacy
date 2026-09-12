/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _PAGEMETER_H_
#define _PAGEMETER_H_

#include "fieldmeter.h"

typedef struct {
    FieldMeter f;
    double pageinfo[2][2];
    int pageindex;
    float maxspeed;
    int sinfosz;
    int ok;
} PageMeter;

Meter *pagemeter_new(XOSView *parent, float max);

#endif
