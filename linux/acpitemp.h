/*
 *  Copyright (c) 2009 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  File based on lmstemp.* by
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _ACPITEMP_H_
#define _ACPITEMP_H_

#include "fieldmeter.h"

#define PATH_SIZE 128

typedef struct {
  FieldMeter f;
  char tempfile[PATH_SIZE];
  char highfile[PATH_SIZE];
  int high;
  int usesysfs;
  unsigned long actcolor, highcolor;
} ACPITemp;

Meter *acpitemp_new(XOSView *parent, const char *tempfile,
                    const char *highfile, const char *label,
                    const char *caption);

#endif
