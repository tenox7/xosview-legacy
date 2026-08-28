/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "intmeter.h"
#include "cpumeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char INTFILE[] = "/proc/interrupts";
static const int max = 1024;

/*
 *  Raw interrupt number to bit index.  Interrupts 0 to 15 map to themselves
 *  and the rest are handed sequential indexes from 16 up in the order
 *  /proc/interrupts lists them, which is ascending, so this array stays
 *  sorted by key and stands in for the std::map the C++ version used.
 */
typedef struct {
  int key, index;
} IntNum;

static IntNum *intnums = NULL;
static int nintnums = 0, intnumcap = 0;

static void intnum_set(int key, int index) {
  int i;

  for (i = 0; i < nintnums; i++)
    if (intnums[i].key == key) {
      intnums[i].index = index;
      return;
    }

  if (nintnums == intnumcap) {
    int cap = intnumcap ? intnumcap * 2 : 32;
    IntNum *grown = (IntNum *)realloc(intnums, cap * sizeof(IntNum));

    if (grown == NULL) {
      fprintf(stderr, "Out of memory.\n");
      exit(1);
    }
    intnums = grown;
    intnumcap = cap;
  }
  intnums[nintnums].key = key;
  intnums[nintnums].index = index;
  nintnums++;
}

static int intnum_get(int key) {
  int i;

  for (i = 0; i < nintnums; i++)
    if (intnums[i].key == key)
      return intnums[i].index;

  return 0;
}

/*  Index of the first entry with a key above 15, or nintnums if there is
 *  none.  Interrupts 0 to 15 are always registered first.  */
static int intnum_first_high(void) {
  int i;

  for (i = 0; i < nintnums; i++)
    if (intnums[i].key > 15)
      return i;

  return nintnums;
}

static FILE *openints(void) {
  FILE *f = fopen(INTFILE, "r");

  if (!f) {
    fprintf(stderr, "Can not open file : %s\n", INTFILE);
    exit(1);
  }
  return f;
}

static void updateirqcount(IntMeter *im, int n, int init);

/*  Find the highest number of interrupts and call updateirqcount to update
 *  the number of interrupts listed.  */
static void initirqcount(IntMeter *im) {
  FILE *f = openints();
  char line[1024];
  int intno = 0, i, idx;

  for (i = 0; i < 16; i++)
    intnum_set(i, i);

  if (!fgets(line, sizeof line, f)) {  /*  header  */
    fclose(f);
    updateirqcount(im, intno, 1);
    return;
  }

  /*  just looking for the highest number interrupt that is in use, ignore
   *  the rest of the data  */
  idx = 16;
  while (fgets(line, sizeof line, f)) {
    char *end;

    i = strtol(line, &end, 10);
    /*  stop when reaching non-numeric special interrupts  */
    if (end == line)
      break;
    if (i < 16) {
      intno = i;
    } else {
      intno = idx;
      intnum_set(i, idx++);
    }
  }
  fclose(f);

  updateirqcount(im, intno, 1);
}

/*  Bounded append, so a machine with a great many interrupts truncates the
 *  legend rather than running off the end of the buffer.  */
static void legend_append(char *buf, size_t size, const char *s) {
  size_t used = strlen(buf);

  if (used + 1 < size)
    strncat(buf, s, size - used - 1);
}

/*  The highest numbered interrupt; the number of interrupts is going to be
 *  at least +1 (for int 0) and probably higher if interrupts numbered more
 *  than this one just aren't active.  Must be called with init non-zero the
 *  first time.  */
static void updateirqcount(IntMeter *im, int n, int init) {
  BitMeter *bm = &im->b;
  int old_bits = bm->numbits;
  unsigned long *old_irqs = im->irqs, *old_lastirqs = im->lastirqs;
  char legend[512], piece[32];
  int first_high, i;

  bitmeter_setnumbits(bm, n + 1);

  strcpy(legend, "0");

  first_high = intnum_first_high();
  if (first_high == nintnums) {  /*  only 16 ints  */
    legend_append(legend, sizeof legend, "-15");
  } else {
    int prev = 15, prev2 = 14;

    for (i = first_high; i < nintnums; i++) {
      int key = intnums[i].key;

      if (i == nintnums - 1) {  /*  last element  */
        if (key == prev + 1) {
          legend_append(legend, sizeof legend, "-");
        } else {
          if (prev == prev2 + 1) {
            snprintf(piece, sizeof piece, "-%d", prev);
            legend_append(legend, sizeof legend, piece);
          }
          legend_append(legend, sizeof legend, ",");
        }
        snprintf(piece, sizeof piece, "%d", key);
        legend_append(legend, sizeof legend, piece);
      } else if (key != prev + 1) {
        if (prev == prev2 + 1) {
          snprintf(piece, sizeof piece, "-%d", prev);
          legend_append(legend, sizeof legend, piece);
        }
        snprintf(piece, sizeof piece, ",%d", key);
        legend_append(legend, sizeof legend, piece);
      }
      prev2 = prev;
      prev = key;
    }
  }

  meter_setlegend(&bm->m, legend);

  im->irqs = (unsigned long *)meter_alloc((n + 1) * sizeof(unsigned long));
  im->lastirqs = (unsigned long *)meter_alloc((n + 1)
                                              * sizeof(unsigned long));
  /*  If we are in init, set it to zero, otherwise copy over the old set.  */
  if (init) {
    for (i = 0; i < bm->numbits; i++)
      im->irqs[i] = im->lastirqs[i] = 0;
  } else {
    for (i = 0; i < old_bits; i++) {
      im->irqs[i] = old_irqs[i];
      im->lastirqs[i] = old_lastirqs[i];
    }
    /*  zero to the end the irqs that haven't been seen before  */
    for (i = old_bits; i < bm->numbits; i++)
      im->irqs[i] = im->lastirqs[i] = 0;
  }
  free(old_irqs);
  free(old_lastirqs);
}

static void getirqs(IntMeter *im) {
  BitMeter *bm = &im->b;
  FILE *f = openints();
  char line[1024];

  if (!fgets(line, sizeof line, f)) {  /*  header  */
    fclose(f);
    return;
  }

  while (fgets(line, sizeof line, f)) {
    char *end, *cur;
    unsigned long count, tmp;
    int intno, idx, i;
    size_t digit = strcspn(line, "0123456789");
    size_t colon = strcspn(line, ":");

    if (digit > colon)
      break;  /*  reached non-numeric interrupts  */

    idx = strtoul(line, &end, 10);
    if (idx >= max)
      break;
    intno = intnum_get(idx);
    if (intno >= bm->numbits)
      updateirqcount(im, intno, 0);

    cur = end + 1;
    count = tmp = 0;
    i = 0;
    while (*cur && i++ <= im->cpu) {
      tmp = strtoul(cur, &end, 10);
      if (end == cur)
        break;
      count += tmp;
      cur = end;
    }
    im->irqs[intno] = (im->separate ? tmp : count);
  }
  fclose(f);
}

static void checkres(Meter *m) {
  IntMeter *im = (IntMeter *)m;
  BitMeter *bm = &im->b;

  meter_checkresources(m);
  bm->oncolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw, "intOnColor"));
  bm->offcolor = xwin_alloccolor(m->xw,
                                 xwin_getresource(m->xw, "intOffColor"));
  m->priority = atoi(xwin_getresource(m->xw, "intPriority"));
  im->separate = xwin_isresourcetrue(m->xw, "intSeparate");
}

static void checkevent(Meter *m) {
  IntMeter *im = (IntMeter *)m;
  BitMeter *bm = &im->b;
  int i;

  getirqs(im);

  for (i = 0; i < bm->numbits; i++) {
    bm->bits[i] = ((im->irqs[i] - im->lastirqs[i]) != 0);
    im->lastirqs[i] = im->irqs[i];
  }

  bitmeter_checkevent(m);
}

static void destroy(Meter *m) {
  IntMeter *im = (IntMeter *)m;

  free(im->irqs);
  free(im->lastirqs);
  im->irqs = im->lastirqs = NULL;
  bitmeter_fini(m);
}

Meter *intmeter_new(XOSView *parent, int cpu) {
  IntMeter *im = (IntMeter *)meter_alloc(sizeof *im);

  bitmeter_init(&im->b, parent, "IntMeter", "INTS", "", 1, 0, 0, 0);
  im->b.m.checkres = checkres;
  im->b.m.checkevent = checkevent;
  im->b.m.destroy = destroy;

  im->cpu = cpu;
  im->separate = 0;
  im->irqs = im->lastirqs = NULL;
  initirqcount(im);

  return &im->b.m;
}
