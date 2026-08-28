/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#ifndef _DISKMETER_H_
#define _DISKMETER_H_

#include "fieldmeter.h"

typedef struct {
    FieldMeter f;
    float maxspeed;
} DiskMeter;

Meter *diskmeter_new(XOSView *parent, float max);

#endif
