/*
 *  Copyright (c) 1997, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "btrymeter.h"
#include "stringutils.h"
#include "xosview.h"
#include "xwin.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char APMFILENAME[] = "/proc/apm";
static const char ACPIBATTERYDIR[] = "/proc/acpi/battery";
static const char SYSPOWERDIR[] = "/sys/class/power_supply";

/*---------------------------------------------------------------------------*/

/*  Read the first whitespace separated token of a file.  */
static int readtoken(const char *path, char *out, size_t size) {
  FILE *f = fopen(path, "r");
  char fmt[16];
  int n;

  if (!f)
    return 0;
  snprintf(fmt, sizeof fmt, "%%%lus", (unsigned long)(size - 1));
  n = fscanf(f, fmt, out);
  fclose(f);
  return n == 1;
}

static int isdir(const char *path) {
  struct stat buf;

  return stat(path, &buf) == 0 && S_ISDIR(buf.st_mode);
}

/*---------------------------------------------------------------------------*/

/*  determine if /proc/apm exists and is readable  */
static int has_apm(void) {
  struct stat stbuf;
  int fd;

  if (stat(APMFILENAME, &stbuf) != 0) {
    XOSDEBUG("APM: stat failed: %d - not APM ?\n", errno);
    return 0;
  }
  if (S_ISREG(stbuf.st_mode)) {
    XOSDEBUG("apm: %s exists and is a file\n", APMFILENAME);
  } else {
    XOSDEBUG("no APM file\n");
    return 0;
  }
  fd = open(APMFILENAME, O_RDONLY);
  if (fd < 0) {
    XOSDEBUG("open failed on %s: with errno=%d\n", APMFILENAME, errno);
    return 0;
  }
  close(fd);  /*  good enough ...  */

  /*  all our tests succeeded - apm seems usable  */
  return 1;
}

/*  determine if /proc/acpi/battery exists and is a DIR
 *  (XXX: too lazy - no test for actual readability is done)  */
static int has_acpi(void) {
  if (!isdir(ACPIBATTERYDIR)) {
    XOSDEBUG("no ACPI dir\n");
    return 0;
  }
  XOSDEBUG("%s exists and is a DIR.\n", ACPIBATTERYDIR);

  /*  declare ACPI as usable  */
  return 1;
}

/*  Look for something resembling a battery in /sys/class/power_supply  */
static int has_syspower(void) {
  DIR *dir;
  struct dirent *dp;
  char dirname[80], f[80], t[32];
  int found = 0;

  dir = opendir(SYSPOWERDIR);
  if (dir == NULL)
    return 0;

  while (!found && (dp = readdir(dir))) {
    if (!strncmp(dp->d_name, ".", 1))
      continue;
    if (!strncmp(dp->d_name, "..", 2))
      continue;
    snprintf_or_abort(dirname, sizeof dirname, "%s/%s", SYSPOWERDIR,
                      dp->d_name);
    if (isdir(dirname)) {
      snprintf_or_abort(f, sizeof f, "%s%s", dirname, "/type");
      if (readtoken(f, t, sizeof t) && strncmp(t, "Battery", 7) == 0)
        found = 1;
    }
  }
  if (closedir(dir) != 0)
    abort();

  return found;
}

int btrymeter_has_source(void) {
  return has_apm() || has_acpi() || has_syspower();
}

/*---------------------------------------------------------------------------*/

static void checkres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "batteryLeftColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "batteryUsedColor"));

  m->priority = atoi(xwin_getresource(m->xw, "batteryPriority"));
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "batteryUsedFormat"));
}

static void handle_apm_state(BtryMeter *bm) {
  Meter *m = &bm->f.m;

  switch (bm->apm_battery_state) {
  case 0:  /*  high (e.g. over 25% on my box)  */
    XOSDEBUG("battery_status HIGH\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryLeftColor"));
    meter_setlegend(m, "High AVAIL/USED");
    break;

  case 1:  /*  low (e.g. under 25% on my box)  */
    XOSDEBUG("battery_status LOW\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryLowColor"));
    meter_setlegend(m, "LOW avail/used");
    break;

  case 2:  /*  critical (less than 5% on my box)  */
    XOSDEBUG("battery_status CRITICAL\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryCritColor"));
    meter_setlegend(m, "Crit LOW/Used");
    break;

  case 3:  /*  Charging  */
    XOSDEBUG("battery_status CHARGING\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryChargeColor"));
    meter_setlegend(m, "AC/Charging");
    break;

  case 4:  /*  selected batt not present  */
    /*  no idea how this state ever could happen with APM  */
    XOSDEBUG("battery_status not present\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryNoneColor"));
    meter_setlegend(m, "Not Present/N.A.");
    break;

  case 255:  /*  unknown - do nothing - maybe not APM  */
    /*  on my system this state comes if you pull both batteries
     *  (while on AC of course :-)))  */
    XOSDEBUG("apm battery_state not known\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryNoneColor"));
    meter_setlegend(m, "Unknown/N.A.");
    break;
  }
}

static void handle_acpi_state(BtryMeter *bm) {
  Meter *m = &bm->f.m;

  switch (bm->acpi_charge_state) {
  case 0:  /*  charged  */
    XOSDEBUG("battery_status CHARGED\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryFullColor"));
    meter_setlegend(m, "CHRG/FULL");
    break;

  case -1:  /*  discharging  */
    XOSDEBUG("battery_status DISCHARGING\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryLeftColor"));
    meter_setlegend(m, "CHRG/USED");
    break;

  case -2:  /*  discharging - below alarm  */
    XOSDEBUG("battery_status ALARM DISCHARGING\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryCritColor"));
    meter_setlegend(m, "CHRG/USED");
    break;

  case -3:  /*  not present  */
    XOSDEBUG("battery_status NOT PRESENT\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryNoneColor"));
    meter_setlegend(m, "NONE/NONE");
    break;

  case 1:  /*  charging  */
    XOSDEBUG("battery_status CHARGING\n");
    fieldmeter_setcolorname(&bm->f, 0,
                            xwin_getresource(m->xw, "batteryChargeColor"));
    meter_setlegend(m, "CHRG/AC");
    break;
  }
}

/*---------------------------------------------------------------------------*/

static int getapminfo(BtryMeter *bm) {
  FieldMeter *fm = &bm->f;
  FILE *f = fopen(APMFILENAME, "r");
  char buff[256];
  int battery_status = 0xff;  /*  assume unknown as default  */

  /*  just a tiny note here about APM states:
   *  See: arch/i386/kernel/apm.c apm_get_info()
   *
   *    Arguments, with symbols from linux/apm_bios.h.  Information is
   *    from the Get Power Status (0x0a) call unless otherwise noted.
   *
   *    0) Linux driver version (this will change if format changes)
   *    1) APM BIOS Version.  Usually 1.0, 1.1 or 1.2.
   *    2) APM flags from APM Installation Check (0x00):
   *       bit 0: APM_16_BIT_SUPPORT
   *       bit 1: APM_32_BIT_SUPPORT
   *       bit 2: APM_IDLE_SLOWS_CLOCK
   *       bit 3: APM_BIOS_DISABLED
   *       bit 4: APM_BIOS_DISENGAGED
   *    3) AC line status
   *       0x00: Off-line
   *       0x01: On-line
   *       0x02: On backup power (BIOS >= 1.1 only)
   *       0xff: Unknown
   *    4) Battery status
   *       0x00: High
   *       0x01: Low
   *       0x02: Critical
   *       0x03: Charging
   *       0x04: Selected battery not present (BIOS >= 1.2 only)
   *       0xff: Unknown
   *    5) Battery flag
   *       bit 0: High
   *       bit 1: Low
   *       bit 2: Critical
   *       bit 3: Charging
   *       bit 7: No system battery
   *       0xff: Unknown
   *    6) Remaining battery life (percentage of charge):
   *       0-100: valid
   *       -1: Unknown
   *    7) Remaining battery life (time units):
   *       Number of remaining minutes or seconds
   *       -1: Unknown
   *    8) min = minutes; sec = seconds
   */

  if (!f) {
    XOSDEBUG("Can not open file : %s\n", APMFILENAME);
    return 0;
  }

  fscanf(f, "%255s %255s %255s %255s %i %255s %lf", buff, buff, buff, buff,
         &battery_status, buff, &fm->fields[0]);
  fclose(f);

  /*  save previous state; if there was no state change the gui won't do a
   *  full redraw  */
  bm->old_apm_battery_state = bm->apm_battery_state;
  bm->apm_battery_state = battery_status;

  /*  If the battery status is reported as a negative number, it means we are
   *  running on AC power and no battery status is available - report it as
   *  completely empty (0).  (Refer to Debian bug report #281565)  */
  if (fm->fields[0] < 0)
    fm->fields[0] = 0;

  fm->total = 100;

  if (bm->apm_battery_state != 0xFF) {
    fm->fields[1] = fm->total - fm->fields[0];
  } else {  /*  prevent setting it to '-1' if no batt  */
    fm->fields[0] = 0;
    fm->fields[1] = fm->total;
  }

  fieldmeter_setused(fm, fm->fields[0], fm->total);

  return 1;
}

/*---------------------------------------------------------------------------*/

/*  present yes/no can change anytime, by adding/removing a battery  */
static int acpi_battery_present(const char *filename) {
  FILE *f = fopen(filename, "r");
  char argname[64], argval[64];

  if (!f)
    return 0;

  while (fscanf(f, "%63s %63s", argname, argval) == 2) {
    XOSDEBUG("batt ?: a=\"%s\" v=\"%s\"\n", argname, argval);
    if (strcmp(argname, "present:") == 0 && strcmp(argval, "yes") == 0) {
      fclose(f);
      return 1;
    }
  }
  fclose(f);
  XOSDEBUG("batt %s not present\n", filename);
  return 0;
}

/*  Split "design capacity:  4400 mWh" into the part before the colon and the
 *  first whitespace separated token after it.  Returns 0 if there is no
 *  colon on the line.  */
static int splitline(char *line, char **name, char **value) {
  char *colon = strchr(line, ':');
  char *v, *end;

  if (!colon)
    return 0;
  *colon = '\0';
  *name = line;

  v = colon + 1;
  while (*v == ' ' || *v == '\t')
    v++;
  end = v;
  while (*end && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r')
    end++;
  *end = '\0';
  *value = v;

  return 1;
}

static int acpi_parse_battery(BtryMeter *bm, const char *dirname) {
  /*  actually there are THREE files to check: 'alarm', 'info' and 'state'  */
  char filename[256], line[256];
  char *argname, *argval;
  FILE *f;

  snprintf(filename, sizeof filename, "%s/alarm", dirname);
  f = fopen(filename, "r");
  if (f) {
    while (fgets(line, sizeof line, f)) {
      if (!splitline(line, &argname, &argval))
        continue;
      XOSDEBUG("alarm: a=\"%s\" v=\"%s\"\n", argname, argval);
      if (strcmp(argname, "alarm") == 0) {
        bm->battery.alarm = atoi(argval);
        break;
      }
    }
    fclose(f);
  }

  snprintf(filename, sizeof filename, "%s/info", dirname);
  f = fopen(filename, "r");
  if (f) {
    while (fgets(line, sizeof line, f)) {
      if (!splitline(line, &argname, &argval))
        continue;
      XOSDEBUG("info: a=\"%s\" v=\"%s\"\n", argname, argval);
      if (strcmp(argname, "design capacity") == 0)
        bm->battery.design_capacity = atoi(argval);
      if (strcmp(argname, "last full capacity") == 0)
        bm->battery.last_full_capacity = atoi(argval);
    }
    fclose(f);
  }

  snprintf(filename, sizeof filename, "%s/state", dirname);
  f = fopen(filename, "r");
  if (f) {
    while (fgets(line, sizeof line, f)) {
      if (!splitline(line, &argname, &argval))
        continue;
      XOSDEBUG("state: a=\"%s\" v=\"%s\"\n", argname, argval);

      if (strcmp(argname, "charging state") == 0) {
        if (strcmp(argval, "charged") == 0)
          bm->battery.charging_state = 0;
        if (strcmp(argval, "discharging") == 0)
          bm->battery.charging_state = -1;
        if (strcmp(argval, "charging") == 0)
          bm->battery.charging_state = 1;
      }
      if (strcmp(argname, "last full capacity") == 0)
        bm->battery.last_full_capacity = atoi(argval);
      if (strcmp(argname, "remaining capacity") == 0)
        bm->battery.remaining_capacity = atoi(argval);
    }
    fclose(f);
  }

  return 1;
}

/*  present yes/no can change anytime, by adding/removing a battery  */
static int sys_battery_present(const char *filename) {
  FILE *f = fopen(filename, "r");
  char value[32];

  if (!f)
    return 0;

  while (fscanf(f, "%31s", value) == 1)
    if (strcmp(value, "1") == 0) {
      fclose(f);
      return 1;
    }
  fclose(f);
  XOSDEBUG("batt %s not present\n", filename);
  return 0;
}

static int sys_parse_battery(BtryMeter *bm, const char *dirname) {
  char filename[256], alt[256], value[64];
  struct stat stbuf;

  snprintf(filename, sizeof filename, "%s/alarm", dirname);
  if (readtoken(filename, value, sizeof value)) {
    XOSDEBUG("alarm (%s): v=\"%s\"\n", filename, value);
    bm->battery.alarm = atoi(value);
  }

  snprintf(filename, sizeof filename, "%s/energy_full_design", dirname);
  if (stat(filename, &stbuf) != 0)
    snprintf(filename, sizeof filename, "%s/charge_full_design", dirname);
  if (readtoken(filename, value, sizeof value)) {
    XOSDEBUG("design_capacity (%s): v=\"%s\"\n", filename, value);
    bm->battery.design_capacity = atoi(value);
  }

  snprintf(filename, sizeof filename, "%s/energy_full", dirname);
  if (stat(filename, &stbuf) != 0)
    snprintf(filename, sizeof filename, "%s/charge_full", dirname);
  if (!readtoken(filename, value, sizeof value)) {
    snprintf(alt, sizeof alt, "%s/charge_full_design", dirname);
    snprintf(filename, sizeof filename, "%s", alt);
    if (!readtoken(filename, value, sizeof value))
      value[0] = '\0';
  }
  if (value[0]) {
    XOSDEBUG("last_full_capacity (%s): v=\"%s\"\n", filename, value);
    bm->battery.last_full_capacity = atoi(value);
  }

  snprintf(filename, sizeof filename, "%s/energy_now", dirname);
  if (stat(filename, &stbuf) != 0)
    snprintf(filename, sizeof filename, "%s/charge_now", dirname);
  if (readtoken(filename, value, sizeof value)) {
    XOSDEBUG("remaining_capacity (%s): v=\"%s\"\n", filename, value);
    bm->battery.remaining_capacity = atoi(value);
  }

  snprintf(filename, sizeof filename, "%s/status", dirname);
  if (readtoken(filename, value, sizeof value)) {
    XOSDEBUG("status (%s): v=\"%s\"\n", filename, value);
    bm->battery.charging_state = 0;
    if (strcmp(value, "Discharging") == 0)
      bm->battery.charging_state = -1;
    if (strcmp(value, "Charging") == 0)
      bm->battery.charging_state = 1;
  }

  return 1;
}

/*  ACPI provides a lot of info, but munging it into something useful is
 *  ugly, especially as you can have more than one battery ...  */
static int getacpi_or_sys_info(BtryMeter *bm) {
  FieldMeter *fm = &bm->f;
  const char *abs_battery_dir = bm->use_acpi ? ACPIBATTERYDIR : SYSPOWERDIR;
  DIR *dir;
  struct dirent *dirent;
  int found = 0;  /*  whether we found at least ONE battery  */

  dir = opendir(abs_battery_dir);
  if (dir == NULL) {
    XOSDEBUG("ACPI/SYS: Cannot open directory : %s\n", abs_battery_dir);
    return 0;
  }

  /*  reset all sums  */
  bm->acpi_sum_cap = 0;
  bm->acpi_sum_remain = 0;
  bm->acpi_sum_rate = 0;
  bm->acpi_sum_alarm = 0;

  /*  save old state  */
  bm->old_acpi_charge_state = bm->acpi_charge_state;

  bm->acpi_charge_state = 0;  /*  assume charged  */

  while ((dirent = readdir(dir)) != NULL) {
    char abs_battery_name[256], probe[256];

    if (strncmp(dirent->d_name, ".", 1) == 0
        || strncmp(dirent->d_name, "..", 2) == 0)
      continue;

    snprintf(abs_battery_name, sizeof abs_battery_name, "%s/%s",
             abs_battery_dir, dirent->d_name);

    XOSDEBUG("ACPI/SYS Batt: %s\n", dirent->d_name);

    /*  still can happen that it's not present  */
    snprintf(probe, sizeof probe, "%s/%s", abs_battery_name,
             bm->use_acpi ? "info" : "present");

    if ((bm->use_acpi && acpi_battery_present(probe)
         && acpi_parse_battery(bm, abs_battery_name)) ||
        (bm->use_syspower && sys_battery_present(probe)
         && sys_parse_battery(bm, abs_battery_name))) {
      /*  sum up, clipping values to get realistic on full-charged
       *  batteries  */
      if (bm->battery.remaining_capacity >= bm->battery.last_full_capacity)
        bm->battery.remaining_capacity = bm->battery.last_full_capacity;

      bm->acpi_sum_cap += bm->battery.last_full_capacity;
      bm->acpi_sum_remain += bm->battery.remaining_capacity;
      bm->acpi_sum_rate += bm->battery.present_rate;
      bm->acpi_sum_alarm += bm->battery.alarm;

      /*  sum up charge state ... works only w. signed formats  */
      bm->acpi_charge_state |= bm->battery.charging_state;

      found = 1;  /*  found at least one  */
    }
  }

  closedir(dir);

  fm->total = 100;

  /*  below alarm ?  */
  if (bm->acpi_sum_alarm >= bm->acpi_sum_remain && bm->acpi_charge_state != 1)
    bm->acpi_charge_state = -2;

  if (found) {
    fm->fields[0] = (float)bm->acpi_sum_remain / (float)bm->acpi_sum_cap
                    * 100.0;
  } else {
    /*  none of the batts is present (just pull out both while on AC)  */
    fm->fields[0] = 0;
    bm->acpi_charge_state = -3;
  }

  fm->fields[1] = fm->total - fm->fields[0];

  fieldmeter_setused(fm, fm->fields[0], fm->total);

  return 1;
}

static void getpwrinfo(BtryMeter *bm) {
  FieldMeter *fm = &bm->f;

  if (bm->use_acpi || bm->use_syspower) {
    getacpi_or_sys_info(bm);
    return;
  }
  if (bm->use_apm) {
    getapminfo(bm);
    return;
  }

  /*  We can hit this spot in any of two cases:
   *  - We have an ACPI system and the battery is removed
   *  - We have neither ACPI nor APM in the system
   *  We report an empty battery (i.e., we are running off AC power) instead
   *  of the original behavior of just exiting the program.
   *  (Refer to Debian bug report #281565)  */
  fm->total = 100;
  fm->fields[0] = 0;
  fm->fields[1] = 100;
  fieldmeter_setused(fm, fm->fields[0], fm->total);
}

/*  All redraws come from this function (not children)  */
static void checkevent(Meter *m) {
  BtryMeter *bm = (BtryMeter *)m;

  getpwrinfo(bm);

  if (bm->old_apm_battery_state != bm->apm_battery_state) {
    /*  APM only changes if we have APM  */
    handle_apm_state(bm);
    fieldmeter_drawlegend(&bm->f);
    fieldmeter_drawfields(&bm->f, 1);
    return;
  }

  if (bm->old_acpi_charge_state != bm->acpi_charge_state) {
    handle_acpi_state(bm);
    fieldmeter_drawlegend(&bm->f);
    fieldmeter_drawfields(&bm->f, 1);
    return;
  }

  fieldmeter_drawfields(&bm->f, 0);
}

Meter *btrymeter_new(XOSView *parent) {
  BtryMeter *bm = (BtryMeter *)meter_alloc(sizeof *bm);

  fieldmeter_init(&bm->f, parent, 2, "BtryMeter", "BTRY", "CHRG/USED",
                  1, 1, 0);
  bm->f.m.checkres = checkres;
  bm->f.m.checkevent = checkevent;

  memset(&bm->battery, 0, sizeof bm->battery);

  /*  find out ONCE whether to use ACPI, APM or sysfs  */
  bm->use_acpi = bm->use_apm = bm->use_syspower = 0;
  if (has_apm()) {
    bm->use_apm = 1;
    bm->use_acpi = 0;
    bm->use_syspower = 0;
  }
  if (has_acpi()) {
    bm->use_acpi = 1;
    bm->use_apm = 0;
    bm->use_syspower = 0;
  }
  if (has_syspower()) {
    bm->use_acpi = 0;
    bm->use_apm = 0;
    bm->use_syspower = 1;
  }

  bm->old_apm_battery_state = bm->apm_battery_state = 0xFF;
  bm->old_acpi_charge_state = bm->acpi_charge_state = -2;
  bm->acpi_sum_cap = bm->acpi_sum_remain = 0;
  bm->acpi_sum_rate = bm->acpi_sum_alarm = 0;

  return &bm->f.m;
}
