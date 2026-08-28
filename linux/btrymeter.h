/*
 *  Copyright (c) 1997, 2005, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _BTRYMETER_H_
#define _BTRYMETER_H_

#include "fieldmeter.h"

/*  some basic fields of 'info', 'alarm', 'state'  */
typedef struct {
  int alarm;               /*  in mWh  */
  int design_capacity;     /*  in mWh  */
  int last_full_capacity;  /*  in mWh  */
  int charging_state;      /*  charged=0, discharging=-1, charging=1  */
  int present_rate;        /*  in mW, 0=unknown  */
  int remaining_capacity;  /*  in mW  */
} AcpiBatt;

typedef struct {
  FieldMeter f;
  AcpiBatt battery;

  int use_apm, use_acpi, use_syspower;

  int apm_battery_state, old_apm_battery_state;
  int acpi_charge_state, old_acpi_charge_state;

  int acpi_sum_cap, acpi_sum_remain, acpi_sum_rate, acpi_sum_alarm;
} BtryMeter;

Meter *btrymeter_new(XOSView *parent);

/*  Is there any usable source of battery information?  */
int btrymeter_has_source(void);

#endif
