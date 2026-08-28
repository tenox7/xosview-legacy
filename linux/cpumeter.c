/*
 *  Copyright (c) 1994, 1995, 2002, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "cpumeter.h"
#include "xosview.h"
#include "xwin.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

static const char STATFILENAME[] = "/proc/stat";
static int cputime_to_field[10] = { 0, 1, 2, 9, 5, 4, 3, 8, 6, 7 };

#define MAX_PROCSTAT_LENGTH 4096

static FILE *openstats(void) {
  FILE *f = fopen(STATFILENAME, "r");

  if (!f) {
    fprintf(stderr, "Can not open file : %s\n", STATFILENAME);
    exit(1);
  }
  return f;
}

int cpumeter_kernelversion(void) {
  static int major = 0, minor = 0, micro = 0;

  if (!major) {
    struct utsname myosrelease;
    uname(&myosrelease);
    sscanf(myosrelease.release, "%d.%d.%d", &major, &minor, &micro);
  }
  return major * 1000000 + minor * 1000 + micro;
}

/*  Returns the number of cpus that are on this machine.  */
int cpumeter_countcpus(void) {
  FILE *f = openstats();
  char buf[MAX_PROCSTAT_LENGTH];
  int cpuCount = 0;

  while (fgets(buf, sizeof buf, f))
    if (!strncmp(buf, "cpu", 3) && buf[3] != ' ')
      cpuCount++;

  fclose(f);
  return cpuCount;
}

const char *cpumeter_cpustr(int num) {
  static char buffer[32];

  if (num != 0)
    snprintf(buffer, sizeof buffer, "cpu%d", num - 1);
  else
    strcpy(buffer, "cpu");

  return buffer;
}

static const char *toUpper(const char *str) {
  static char buffer[MAX_PROCSTAT_LENGTH + 1];
  char *tmp;

  snprintf(buffer, sizeof buffer, "%s", str);
  buffer[MAX_PROCSTAT_LENGTH] = 0;
  for (tmp = buffer; *tmp != '\0'; tmp++)
    *tmp = toupper(*tmp);

  return buffer;
}

static int findLine(const char *cpuID) {
  FILE *f = openstats();
  char buf[MAX_PROCSTAT_LENGTH];
  size_t len = strlen(cpuID);
  int line = -1;

  while (fgets(buf, sizeof buf, f)) {
    line++;
    if (!strncmp(cpuID, buf, len) && buf[len] == ' ') {
      fclose(f);
      return line;
    }
  }
  fclose(f);
  return -1;
}

static void checkres(Meter *m) {
  CPUMeter *cm = (CPUMeter *)m;
  FieldMeter *fm = &cm->f;
  unsigned long usercolor, nicecolor, syscolor, sintcolor, intcolor;
  unsigned long waitcolor, gstcolor, ngstcolor, stealcolor, idlecolor;
  const char *fields;
  char lgnd[64];
  int field = 0;

  fieldmeter_checkresources(m);

  usercolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw, "cpuUserColor"));
  nicecolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw, "cpuNiceColor"));
  syscolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw, "cpuSystemColor"));
  sintcolor = xwin_alloccolor(m->xw,
                              xwin_getresource(m->xw, "cpuSInterruptColor"));
  intcolor = xwin_alloccolor(m->xw,
                             xwin_getresource(m->xw, "cpuInterruptColor"));
  waitcolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw, "cpuWaitColor"));
  gstcolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw, "cpuGuestColor"));
  ngstcolor = xwin_alloccolor(m->xw,
                              xwin_getresource(m->xw, "cpuNiceGuestColor"));
  stealcolor = xwin_alloccolor(m->xw,
                               xwin_getresource(m->xw, "cpuStolenColor"));
  idlecolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw, "cpuFreeColor"));

  m->priority = atoi(xwin_getresource(m->xw, "cpuPriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "cpuDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "cpuGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "cpuUsedFormat"));

  /* Use user-defined fields.
   * Fields         Including if not its own field
   * --------------|------------------------------
   *   USED         all used time, including user and system times
   *     USR        user time, including nice and guest times
   *       NIC      niced time, including niced guest unless guest is present
   *       GST      guest time, including niced guest time
   *         NGS    niced guest time
   *     SYS        system time, including interrupt and stolen times
   *       INT      interrupt time, including soft and hard interrupt times
   *         HI     hard interrupt time
   *         SI     soft interrupt time
   *       STL      stolen time
   *   IDLE         idle time, including io wait time
   *     WIO        io wait time
   *
   * Stolen time is a class of its own in kernel scheduler, in cpufreq it is
   * considered used time. Here it is part of used and system time, but can be
   * separate field as well.
   * Idle field is always present.
   * Either USED or at least USR+SYS must be included.
   */

  fields = xwin_getresource(m->xw, "cpuFields");
  lgnd[0] = '\0';

  /*  Check for possible fields and define field mapping.  Assign colors and
   *  build the legend on the way.  */
  if (strstr(fields, "USED")) {
    /*  USED = USR+NIC+SYS+SI+HI+GST+NGS(+STL)  */
    if (strstr(fields, "USR") || strstr(fields, "NIC") ||
        strstr(fields, "SYS") || strstr(fields, "INT") ||
        strstr(fields, "HI") || strstr(fields, "SI") ||
        strstr(fields, "GST") || strstr(fields, "NGS")) {
      fprintf(stderr, "'USED' cannot be in cpuFields together with either "
              "'USR', 'NIC', 'SYS', 'INT', 'HI', 'SI', 'GST' or 'NGS'.\n");
      exit(1);
    }
    fieldmeter_setcolor(fm, field, usercolor);
    if (cm->kernel >= 2006000)  /*  SI and HI  */
      cputime_to_field[5] = cputime_to_field[6] = field;
    if (cm->kernel >= 2006024)  /*  GST  */
      cputime_to_field[8] = field;
    if (cm->kernel >= 2006032)  /*  NGS  */
      cputime_to_field[9] = field;
    if (cm->kernel >= 2006011 && !strstr(fields, "STL"))
      cputime_to_field[7] = field;  /*  STL can be separate as well  */
    /*  USR, NIC and SYS  */
    cputime_to_field[0] = cputime_to_field[1] = cputime_to_field[2] = field++;
    strcpy(lgnd, "USED");
  }
  if (strstr(fields, "USR")) {
    fieldmeter_setcolor(fm, field, usercolor);
    /*  add NIC if not on its own  */
    if (!strstr(fields, "NIC"))
      cputime_to_field[1] = field;
    /*  add GST if not on its own  */
    if (cm->kernel >= 2006024 && !strstr(fields, "GST"))
      cputime_to_field[8] = field;
    /*  add NGS if not on its own and neither NIC nor GST is present  */
    if (cm->kernel >= 2006032 && !strstr(fields, "NGS") &&
        !strstr(fields, "NIC") && !strstr(fields, "GST"))
      cputime_to_field[9] = field;
    cputime_to_field[0] = field++;
    strcpy(lgnd, "USR");
  } else if (!strstr(fields, "USED")) {
    fprintf(stderr, "Either 'USED' or 'USR' is mandatory in cpuFields.\n");
    exit(1);
  }
  if (strstr(fields, "NIC")) {
    fieldmeter_setcolor(fm, field, nicecolor);
    /*  add NGS if not on its own and GST is not present  */
    if (cm->kernel >= 2006032 && !strstr(fields, "NGS") &&
        !strstr(fields, "GST"))
      cputime_to_field[9] = field;
    cputime_to_field[1] = field++;
    strcat(lgnd, "/NIC");
  }
  if (strstr(fields, "SYS")) {
    fieldmeter_setcolor(fm, field, syscolor);
    /*  add SI if not on its own and INT is not present  */
    if (cm->kernel >= 2006000 && !strstr(fields, "SI") &&
        !strstr(fields, "INT"))
      cputime_to_field[6] = field;
    /*  add HI if not on its own and INT is not present  */
    if (cm->kernel >= 2006000 && !strstr(fields, "HI") &&
        !strstr(fields, "INT"))
      cputime_to_field[5] = field;
    /*  add STL if not on its own  */
    if (cm->kernel >= 2006011 && !strstr(fields, "STL"))
      cputime_to_field[7] = field;
    cputime_to_field[2] = field++;
    strcat(lgnd, "/SYS");
  } else if (!strstr(fields, "USED")) {
    fprintf(stderr, "Either 'USED' or 'SYS' is mandatory in cpuFields.\n");
    exit(1);
  }
  if (cm->kernel >= 2006000) {
    if (strstr(fields, "INT")) {
      /*  combine soft and hard interrupt times  */
      fieldmeter_setcolor(fm, field, intcolor);
      cputime_to_field[5] = cputime_to_field[6] = field++;
      strcat(lgnd, "/INT");
      /*  Maybe should warn if both INT and HI/SI are requested ???  */
    } else {  /*  separate soft and hard interrupt times  */
      if (strstr(fields, "SI")) {
        fieldmeter_setcolor(fm, field, sintcolor);
        cputime_to_field[5] = field++;
        strcat(lgnd, "/SI");
      }
      if (strstr(fields, "HI")) {
        fieldmeter_setcolor(fm, field, intcolor);
        cputime_to_field[6] = field++;
        strcat(lgnd, "/HI");
      }
    }
    if (strstr(fields, "WIO")) {
      fieldmeter_setcolor(fm, field, waitcolor);
      cputime_to_field[4] = field++;
      strcat(lgnd, "/WIO");
    }
    if (cm->kernel >= 2006024 && strstr(fields, "GST")) {
      fieldmeter_setcolor(fm, field, gstcolor);
      /*  add NGS if not on its own  */
      if (cm->kernel >= 2006032 && !strstr(fields, "NGS"))
        cputime_to_field[9] = field;
      cputime_to_field[8] = field++;
      strcat(lgnd, "/GST");
    }
    if (cm->kernel >= 2006032 && strstr(fields, "NGS")) {
      fieldmeter_setcolor(fm, field, ngstcolor);
      cputime_to_field[9] = field++;
      strcat(lgnd, "/NGS");
    }
    if (cm->kernel >= 2006011 && strstr(fields, "STL")) {
      fieldmeter_setcolor(fm, field, stealcolor);
      cputime_to_field[7] = field++;
      strcat(lgnd, "/STL");
    }
  }
  /*  always add IDLE field  */
  fieldmeter_setcolor(fm, field, idlecolor);
  /*  add WIO if not on its own  */
  if (cm->kernel >= 2006000 && !strstr(fields, "WIO"))
    cputime_to_field[4] = field;
  cputime_to_field[3] = field++;
  strcat(lgnd, "/IDLE");

  meter_setlegend(m, lgnd);
  /*  can't use fieldmeter_setnumfields as it destroys the color mapping  */
  fm->numfields = field;
}

static void getcputime(CPUMeter *cm) {
  FieldMeter *fm = &cm->f;
  FILE *f = openstats();
  char buf[MAX_PROCSTAT_LENGTH];
  char *line, *end;
  int i, col = 0, oldindex;

  fm->total = 0;

  /*  read until we are at the right line.  */
  for (i = 0; i <= cm->lineNum; i++)
    if (!fgets(buf, sizeof buf, f)) {
      fclose(f);
      return;
    }
  fclose(f);

  line = strchr(buf, ' ');
  if (!line)
    return;
  line++;

  while (*line && col < 10) {
    cm->cputime[cm->cpuindex][col++] = strtoull(line, &end, 10);
    if (end == line)
      break;
    line = end;
  }

  /*  Guest time is already included in user time.  */
  cm->cputime[cm->cpuindex][0] -= cm->cputime[cm->cpuindex][8];
  /*  Same applies to niced guest time.  */
  cm->cputime[cm->cpuindex][1] -= cm->cputime[cm->cpuindex][9];

  oldindex = (cm->cpuindex + 1) % 2;
  /*  zero all the fields  */
  memset(fm->fields, 0, fm->numfields * sizeof(fm->fields[0]));
  for (i = 0; i < cm->statfields; i++) {
    long long delta = cm->cputime[cm->cpuindex][i] - cm->cputime[oldindex][i];

    if (delta < 0)  /*  counters in /proc/stat do sometimes go backwards  */
      delta = 0;
    fm->fields[cputime_to_field[i]] += delta;
    fm->total += delta;
  }

  if (fm->total) {
    /*  any non-idle time  */
    fieldmeter_setused(fm, fm->total - fm->fields[fm->numfields - 1],
                       fm->total);
    cm->cpuindex = (cm->cpuindex + 1) % 2;
  }
}

static void checkevent(Meter *m) {
  getcputime((CPUMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *cpumeter_new(XOSView *parent, const char *cpuID) {
  CPUMeter *cm = (CPUMeter *)meter_alloc(sizeof *cm);
  int i, j;

  fieldmeter_init(&cm->f, parent, 10, "CPUMeter", toUpper(cpuID),
                  "USR/NIC/SYS/SI/HI/WIO/GST/NGS/STL/IDLE", 0, 0, 0);
  cm->f.m.checkres = checkres;
  cm->f.m.checkevent = checkevent;

  cm->lineNum = findLine(cpuID);
  for (i = 0; i < 2; i++)
    for (j = 0; j < 10; j++)
      cm->cputime[i][j] = 0;
  cm->cpuindex = 0;
  cm->kernel = cpumeter_kernelversion();
  if (cm->kernel < 2006000)
    cm->statfields = 4;
  else if (cm->kernel < 2006011)
    cm->statfields = 7;
  else if (cm->kernel < 2006024)
    cm->statfields = 8;
  else if (cm->kernel < 2006032)
    cm->statfields = 9;
  else
    cm->statfields = 10;

  return &cm->f.m;
}
