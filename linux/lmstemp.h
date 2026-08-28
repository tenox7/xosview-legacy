/*
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  File based on btrymeter.* by
 *  Copyright (c) 1997 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _LMSTEMP_H_
#define _LMSTEMP_H_

#include "sensorfieldmeter.h"

#define LMS_PATH_SIZE 256

typedef struct {
  SensorFieldMeter s;
  char tempfile[LMS_PATH_SIZE];
  char highfile[LMS_PATH_SIZE];
  char lowfile[LMS_PATH_SIZE];
  unsigned int nbr;
  double scale;
  int isproc, name_found, temp_found, high_found, low_found;
} LmsTemp;

Meter *lmstemp_new(XOSView *parent, const char *name, const char *tempfile,
                   const char *highfile, const char *lowfile,
                   const char *label, const char *caption, unsigned int nbr);

#endif
