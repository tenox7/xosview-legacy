/*
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _NFSMETER_H_
#define _NFSMETER_H_

#include "fieldmeter.h"

/*  Common to both NFS meters.  The interval timer FieldMeter already
 *  carries stands in for the one the C++ version added here.  */
typedef struct {
  FieldMeter f;
  const char *statfile;
} NFSMeter;

typedef struct {
  NFSMeter n;
  unsigned long lastcalls, lastretrns, lastauthrefresh;
} NFSStats;

typedef struct {
  NFSMeter n;
  float maxpackets;
  unsigned long lastTcp, lastUdp, lastNetCnt, lastBad;
} NFSDStats;

Meter *nfsstats_new(XOSView *parent);
Meter *nfsdstats_new(XOSView *parent);

#endif
