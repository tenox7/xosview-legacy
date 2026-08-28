/*
 *  Copyright (c) 1999, 2006 Thomas Waldmann ( ThomasWaldmann@gmx.de )
 *  based on work of Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _RAIDMETER_H_
#define _RAIDMETER_H_

#include "bitfieldmeter.h"

#define MAX_MD 8

typedef struct {
  BitFieldMeter b;

  int raiddev;

  char state[20];
  char type[20];
  char working_map[20];
  char resync_state[20];
  int disknum;

  unsigned long doneColor, todoColor, completeColor;
} RAIDMeter;

Meter *raidmeter_new(XOSView *parent, int raiddev);

#endif
