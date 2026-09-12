/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _NETMETER_H_
#define _NETMETER_H_

#include "fieldmeter.h"

typedef struct {
    FieldMeter f;
    float maxpackets;
    int ok;
    double lastBytesIn, lastBytesOut;
    int first;
    char netIface[64];
    int ignored;
} NetMeter;

Meter *netmeter_new(XOSView *parent, float max);

#endif
