/*
 *  Copyright (c) 1999, 2006 by Mike Romberg (mike.romberg@noaa.gov)
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _DISKMETER_H_
#define _DISKMETER_H_

#include "fieldmeter.h"

/*  Sector counts from the last sysfs sample, one entry per disk.  This
 *  stands in for the std::map the C++ version kept; a machine has few
 *  enough disks that a linear scan is not worth improving on.  */
typedef struct {
  char name[32];
  unsigned long value;
} DiskEntry;

typedef struct {
  DiskEntry *e;
  int n, cap;
} DiskMap;

typedef struct {
  FieldMeter f;

  /*  sysfs  */
  DiskMap sysfs_read_prev, sysfs_write_prev;
  int sysfs;

  unsigned long read_prev;
  unsigned long write_prev;
  float maxspeed;
  int vmstat;
  const char *statFileName;
} DiskMeter;

Meter *diskmeter_new(XOSView *parent, float max);

#endif
