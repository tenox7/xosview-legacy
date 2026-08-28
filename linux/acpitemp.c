/*
 *  Copyright (c) 2009 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  File based on lmstemp.* by
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  This file may be distributed under terms of the GPL
 */

#include "acpitemp.h"
#include "xosview.h"
#include "xwin.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char PROC_ACPI_TZ[] = "/proc/acpi/thermal_zone";
static const char SYS_ACPI_TZ[] = "/sys/devices/virtual/thermal";

static int checkacpi(ACPITemp *at, const char *tempfile,
                     const char *highfile) {
  struct stat buf;
  char temp[PATH_SIZE], high[PATH_SIZE];
  int temp_found = 0, high_found = 0;

  if (tempfile[0] == '/') {
    if (stat(tempfile, &buf) == 0 && S_ISREG(buf.st_mode))
      temp_found = 1;
    else
      return 0;
  }
  if (highfile[0] == '/') {
    if (stat(highfile, &buf) == 0 && S_ISREG(buf.st_mode))
      high_found = 1;
    else
      return 0;
  }

  if (temp_found && high_found) {
    snprintf(at->tempfile, PATH_SIZE, "%s", tempfile);
    snprintf(at->highfile, PATH_SIZE, "%s", highfile);
    return 1;
  }

  snprintf(temp, PATH_SIZE, "%s/%s", SYS_ACPI_TZ, tempfile);
  snprintf(high, PATH_SIZE, "%s/%s", SYS_ACPI_TZ, highfile);

  if ((stat(temp, &buf) == 0 && S_ISREG(buf.st_mode)) &&
      (stat(high, &buf) == 0 && S_ISREG(buf.st_mode))) {
    snprintf(at->tempfile, PATH_SIZE, "%s", temp);
    snprintf(at->highfile, PATH_SIZE, "%s", high);
    at->usesysfs = 1;
    return 1;
  }

  at->usesysfs = 0;
  snprintf(temp, PATH_SIZE, "%s/%s", PROC_ACPI_TZ, tempfile);
  snprintf(high, PATH_SIZE, "%s/%s", PROC_ACPI_TZ, highfile);

  if ((stat(temp, &buf) == 0 && S_ISREG(buf.st_mode)) &&
      (stat(high, &buf) == 0 && S_ISREG(buf.st_mode))) {
    snprintf(at->tempfile, PATH_SIZE, "%s", temp);
    snprintf(at->highfile, PATH_SIZE, "%s", high);
    return 1;
  }
  return 0;
}

static void checkres(Meter *m) {
  ACPITemp *at = (ACPITemp *)m;
  FieldMeter *fm = &at->f;

  fieldmeter_checkresources(m);

  at->actcolor = xwin_alloccolor(m->xw,
                                 xwin_getresource(m->xw, "acpitempActColor"));
  at->highcolor = xwin_alloccolor(m->xw,
                                  xwin_getresource(m->xw,
                                                   "acpitempHighColor"));
  fieldmeter_setcolor(fm, 0, at->actcolor);
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "acpitempIdleColor"));
  fieldmeter_setcolor(fm, 2, at->highcolor);
  fm->total = atoi(xwin_getresource_default(m->xw, "acpitempHighest", "100"));
  m->priority = atoi(xwin_getresource(m->xw, "acpitempPriority"));
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "acpitempUsedFormat"));
}

static void getacpitemp(ACPITemp *at) {
  FieldMeter *fm = &at->f;
  FILE *tf, *hf;
  char dummy[64];
  float high = 0;
  int do_legend = 0;

  tf = fopen(at->tempfile, "r");
  if (!tf) {
    fprintf(stderr, "Can not open file : %s\n", at->tempfile);
    xwin_setdone(fm->m.xw, 1);
    return;
  }
  hf = fopen(at->highfile, "r");
  if (!hf) {
    fprintf(stderr, "Can not open file : %s\n", at->highfile);
    fclose(tf);
    xwin_setdone(fm->m.xw, 1);
    return;
  }

  if (at->usesysfs) {
    if (fscanf(hf, "%f", &high) == 1)
      high /= 1000.0;
    if (fscanf(tf, "%lf", &fm->fields[0]) == 1)
      fm->fields[0] /= 1000.0;
  } else {
    fscanf(hf, "%63s %63s %f", dummy, dummy, &high);
    fscanf(tf, "%63s %lf", dummy, &fm->fields[0]);
  }
  fclose(tf);
  fclose(hf);

  if (high > fm->total || high != at->high) {
    char l[16];

    if (high > fm->total)
      fm->total = 10 * (int)((high * 1.25) / 10);
    at->high = high;
    snprintf(l, sizeof l, "ACT(\260C)/%d/%d", (int)high, (int)fm->total);
    meter_setlegend(&fm->m, l);
    do_legend = 1;
  }

  fieldmeter_setused(fm, fm->fields[0], fm->total);
  if (fm->fields[0] < 0)
    fm->fields[0] = 0.0;

  fm->fields[1] = high - fm->fields[0];
  if (fm->fields[1] < 0) {  /*  alarm: T > high  */
    fm->fields[1] = 0;
    if (fm->colors[0] != at->highcolor) {
      fieldmeter_setcolor(fm, 0, at->highcolor);
      do_legend = 1;
    }
  } else {
    if (fm->colors[0] != at->actcolor) {
      fieldmeter_setcolor(fm, 0, at->actcolor);
      do_legend = 1;
    }
  }

  fm->fields[2] = fm->total - fm->fields[1] - fm->fields[0];
  if (fm->fields[2] < 0)
    fm->fields[2] = 0;

  if (do_legend)
    fieldmeter_drawlegend(fm);
}

static void checkevent(Meter *m) {
  getacpitemp((ACPITemp *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *acpitemp_new(XOSView *parent, const char *tempfile,
                    const char *highfile, const char *label,
                    const char *caption) {
  ACPITemp *at = (ACPITemp *)meter_alloc(sizeof *at);

  fieldmeter_init(&at->f, parent, 3, "ACPITemp", label, caption, 1, 1, 0);
  at->f.m.checkres = checkres;
  at->f.m.checkevent = checkevent;
  at->f.metric = 1;

  at->tempfile[0] = at->highfile[0] = '\0';
  at->actcolor = at->highcolor = 0;
  /*  The C++ version left this unset when both paths were absolute.  */
  at->usesysfs = 0;

  if (!checkacpi(at, tempfile, highfile)) {
    fprintf(stderr, "Can not find file : ");
    if (tempfile[0] == '/' && highfile[0] == '/')
      fprintf(stderr, "%s or %s", tempfile, highfile);
    else if (tempfile[0] == '/')
      fprintf(stderr, "%s, or %s under %s or %s", tempfile, highfile,
              PROC_ACPI_TZ, SYS_ACPI_TZ);
    else if (highfile[0] == '/')
      fprintf(stderr, "%s under %s or %s, or %s", tempfile, PROC_ACPI_TZ,
              SYS_ACPI_TZ, highfile);
    else
      fprintf(stderr, "%s or %s under %s or %s", tempfile, highfile,
              PROC_ACPI_TZ, SYS_ACPI_TZ);
    fprintf(stderr, ".\n");
    xwin_setdone((XWin *)parent, 1);
  }
  at->high = 0;

  return &at->f.m;
}
