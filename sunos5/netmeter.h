/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _NETMETER_H_
#define _NETMETER_H_

#include "fieldmeter.h"
#include "kstats.h"
#include <sys/types.h>
#include <kstat.h>
#include <net/if.h>

typedef struct {
  FieldMeter f;
  float maxpackets;
  uint64_t lastBytesIn, lastBytesOut;
  kstat_ctl_t *kc;
  KStatList *nets;
  char netIface[LIFNAMSIZ];
  int ignored;
  struct lifreq lfr;
  int socket;
} NetMeter;

Meter *netmeter_new(XOSView *parent, kstat_ctl_t *kc, float max);

#endif
