/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *  Copyright (c) 2015 Framestore
 *
 *  This file may be distributed under terms of the GPL
 */

#include "memmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMFILENAME "/proc/meminfo"

#define KB(x) ((double)((x) * 1024))

static void checkres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "memUsedColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "memBufferColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "memSlabColor"));
  fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "memCacheColor"));
  fieldmeter_setcolorname(fm, 4, xwin_getresource(m->xw, "memFreeColor"));
  m->priority = atoi(xwin_getresource(m->xw, "memPriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "memDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "memGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "memUsedFormat"));
}

static void getstats(MemMeter *mm) {
  FieldMeter *fm = &mm->f;
  FILE *f;
  /*
   * The kernel's "unsigned long" values vary in size on 64-bit and
   * 32-bit implementations.
   *
   * But there's nothing saying userland will match the kernel; we
   * must pretty much accomodate anything in ASCII that /proc gives
   * us, so 64-bit integers are always used.
   */
  unsigned long long mem_total = 0, mem_free = 0, buffers = 0, slab = 0,
                     cached = 0;

  f = fopen(MEMFILENAME, "r");
  if (!f) {
    perror(MEMFILENAME);
    exit(1);
  }

  for (;;) {
    char line[128];
    char *c, *endptr;
    unsigned long long kb;

    /*
     * Parse lines in the format: "FieldName:      12345678 kB"
     *
     * We prefer to not use scanf because it's harder with variable
     * number of fields; the 'kB' is not present if value is 0
     */

    if (!fgets(line, sizeof line, f))
      break;

    c = strchr(line, ':');
    if (!c) {
      fprintf(stderr, MEMFILENAME ": parse error, ':' expected at '%s'\n",
              line);
      exit(1);
    }

    *c = '\0';
    c++;

    kb = strtoull(c, &endptr, 10);
    if (kb == ULLONG_MAX) {
      fprintf(stderr, MEMFILENAME ": parse error, '%s' is out of range\n", c);
      exit(1);
    }

    if (strcmp(line, "MemTotal") == 0)
      mem_total = kb;
    else if (strcmp(line, "MemFree") == 0)
      mem_free = kb;
    else if (strcmp(line, "Buffers") == 0)
      buffers = kb;
    else if (strcmp(line, "Cached") == 0)
      cached = kb;
    else if (strcmp(line, "Slab") == 0)
      slab = kb;
  }

  if (fclose(f) != 0)
    abort();

  /*  Don't do arithmetic on the fields themselves; these are floating point
   *  and when memory is large are affected by inaccuracy.  */

  fm->fields[1] = KB(buffers);
  fm->fields[2] = KB(slab);
  fm->fields[3] = KB(cached);
  fm->fields[4] = KB(mem_free);

  fm->fields[0] = KB(mem_total - mem_free - buffers - cached - slab);
  fm->total = KB(mem_total);

  fieldmeter_setused(fm, KB(mem_total - mem_free), KB(mem_total));
}

static void checkevent(Meter *m) {
  getstats((MemMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *memmeter_new(XOSView *parent) {
  MemMeter *mm = (MemMeter *)meter_alloc(sizeof *mm);

  fieldmeter_init(&mm->f, parent, 5, "MemMeter", "MEM",
                  "USED/BUFF/SLAB/CACHE/FREE", 0, 0, 0);
  mm->f.m.checkres = checkres;
  mm->f.m.checkevent = checkevent;

  return &mm->f.m;
}
