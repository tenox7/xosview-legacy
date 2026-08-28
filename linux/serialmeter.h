/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _SERIALMETER_H_
#define _SERIALMETER_H_

/*  hack for not having linux/serial_reg.h, (Debian bug #427599)  */
#define UART_LSR        5
#define UART_MSR        6

#include "bitmeter.h"

#define SERIAL_NUM_DEVICES 10

typedef struct {
  BitMeter b;
  unsigned short port;
  int device;
} SerialMeter;

Meter *serialmeter_new(XOSView *parent, int device);

const char *serialmeter_resourcename(int dev);

#endif
