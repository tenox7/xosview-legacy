/*
 *  Copyright (c) 1999 by Mike Romberg (romberg@fsl.noaa.gov)
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _DISKMETER_H_
#define _DISKMETER_H_

#include "fieldmeter.h"
#include "kstats.h"
#include <sys/types.h>
#include <kstat.h>

typedef struct {
    FieldMeter f;
    uint64_t read_prev, write_prev;
    float maxspeed;
    kstat_ctl_t *kc;
    KStatList *disks;
} DiskMeter;

Meter *diskmeter_new(XOSView *parent, kstat_ctl_t *kc, float max);

#endif
