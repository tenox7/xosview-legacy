/*
 *  Copyright (c) 1999, 2006 Thomas Waldmann ( ThomasWaldmann@gmx.de )
 *  based on work of Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "bitfieldmeter.h"
#include "xwin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *xalloc(size_t n) {
  void *p = malloc(n);

  if (p == NULL) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }
  return p;
}

void bitfieldmeter_init(BitFieldMeter *bfm, XOSView *parent, int numbits,
                        int numfields, const char *name, const char *title,
                        const char *bitlegend, const char *fieldlegend,
                        int docaptions, int dolegends, int dousedlegends) {
  meter_init(&bfm->m, parent, name, title, bitlegend, docaptions, dolegends,
             dousedlegends);

  bfm->m.checkres = bitfieldmeter_checkresources;
  bfm->m.draw = bitfieldmeter_draw;
  bfm->m.checkevent = bitfieldmeter_checkevent;
  bfm->m.destroy = bitfieldmeter_fini;

  /*  We need to set print to something valid -- the meters apparently get
   *  drawn before they have a chance to call checkResources themselves.  */
  bfm->bits = NULL;
  bfm->lastbits = NULL;
  bfm->numWarnings = bfm->printedZeroTotalMesg = 0;
  bfm->print = UT_PERCENT;
  bfm->metric = 0;
  bfm->usedoffset = 0;
  bfm->used = 0;
  bfm->lastused = -1;
  bfm->fields = NULL;
  bfm->colors = NULL;
  bfm->lastvals = NULL;
  bfm->lastx = NULL;
  bfm->oncolor = bfm->offcolor = 0;

  bfm->tstart.tv_sec = bfm->tstart.tv_usec = 0;
  bfm->tstop = bfm->tstart;

  bitfieldmeter_setnumbits(bfm, numbits);
  bfm->fieldlegend = NULL;
  bitfieldmeter_setfieldlegend(bfm, fieldlegend);
  bitfieldmeter_setnumfields(bfm, numfields);
}

void bitfieldmeter_fini(Meter *m) {
  BitFieldMeter *bfm = (BitFieldMeter *)m;

  free(bfm->bits);
  free(bfm->lastbits);
  free(bfm->fields);
  free(bfm->colors);
  free(bfm->lastvals);
  free(bfm->lastx);
  free(bfm->fieldlegend);
  bfm->bits = bfm->lastbits = NULL;
  bfm->fields = NULL;
  bfm->colors = NULL;
  bfm->lastvals = bfm->lastx = NULL;
  bfm->fieldlegend = NULL;
  meter_fini(m);
}

void bitfieldmeter_checkresources(Meter *m) {
  BitFieldMeter *bfm = (BitFieldMeter *)m;

  meter_checkresources(m);
  bfm->usedcolor = xwin_alloccolor(m->xw,
                                   xwin_getresource(m->xw, "usedLabelColor"));
}

void bitfieldmeter_setnumbits(BitFieldMeter *bfm, int n) {
  int i;

  bfm->numbits = n;
  free(bfm->bits);
  free(bfm->lastbits);

  bfm->bits = (char *)xalloc(n);
  bfm->lastbits = (char *)xalloc(n);
  for (i = 0; i < n; i++)
    bfm->bits[i] = bfm->lastbits[i] = 0;
}

void bitfieldmeter_setnumfields(BitFieldMeter *bfm, int n) {
  int i;

  bfm->numfields = n;
  free(bfm->fields);
  free(bfm->colors);
  free(bfm->lastvals);
  free(bfm->lastx);

  bfm->fields = (double *)xalloc(n * sizeof(double));
  bfm->colors = (unsigned long *)xalloc(n * sizeof(unsigned long));
  bfm->lastvals = (int *)xalloc(n * sizeof(int));
  bfm->lastx = (int *)xalloc(n * sizeof(int));

  bfm->total = 0;
  for (i = 0; i < n; i++) {
    bfm->fields[i] = 0.0;
    bfm->colors[i] = 0;
    bfm->lastvals[i] = bfm->lastx[i] = 0;
  }
}

void bitfieldmeter_setfieldlegend(BitFieldMeter *bfm, const char *legend) {
  size_t len = strlen(legend);

  free(bfm->fieldlegend);
  bfm->fieldlegend = (char *)xalloc(len + 1);
  memcpy(bfm->fieldlegend, legend, len + 1);
}

void bitfieldmeter_disable(BitFieldMeter *bfm) {
  bitfieldmeter_setnumfields(bfm, 1);
  bitfieldmeter_setcolorname(bfm, 0, "grey");
  bitfieldmeter_setfieldlegend(bfm, "Disabled");
  bfm->total = bfm->fields[0] = 1.0;
  bitfieldmeter_setnumbits(bfm, 1);
  bfm->offcolor = bfm->oncolor = xwin_alloccolor(bfm->m.xw, "grey");
}

void bitfieldmeter_reset(BitFieldMeter *bfm) {
  int i;

  for (i = 0; i < bfm->numfields; i++)
    bfm->lastvals[i] = bfm->lastx[i] = -1;
}

void bitfieldmeter_setcolorname(BitFieldMeter *bfm, int field,
                                const char *color) {
  bfm->colors[field] = xwin_alloccolor(bfm->m.xw, color);
}

void bitfieldmeter_setcolor(BitFieldMeter *bfm, int field,
                            unsigned long color) {
  bfm->colors[field] = color;
}

void bitfieldmeter_setusedformat(BitFieldMeter *bfm, const char *fmt) {
  bfm->print = meter_parseusedformat(fmt);
}

void bitfieldmeter_setused(BitFieldMeter *bfm, double val, double total) {
  if (bfm->print == UT_PERCENT) {
    if (total != 0.0) {
      bfm->used = val / total * 100.0;
      return;
    }
    if (!bfm->printedZeroTotalMesg) {
      bfm->printedZeroTotalMesg = 1;
      fprintf(stderr, "Warning: %s meter had a zero total field! "
              "Would have caused a div-by-zero exception.\n", bfm->m.name);
    }
    bfm->used = 0.0;
    return;
  }

  /*  UT_FLOAT and UT_AUTOSCALE both show the raw value.  */
  bfm->used = val;
}

/*---------------------------------------------------------------------------*/

void bitfieldmeter_draw(Meter *m) {
  BitFieldMeter *bfm = (BitFieldMeter *)m;

  /*  Draw the outline for the fieldmeter.  */
  xwin_setforeground(m->xw, xwin_foreground(m->xw));
  xwin_linewidth(m->xw, 1);
  xwin_drawfilledrectangle(m->xw, m->x - 1, m->y - 1, m->width / 2 + 2,
                           m->height + 2);

  xwin_drawrectangle(m->xw, m->x + m->width / 2 + 3, m->y - 1,
                     m->width / 2 - 2, m->height + 2);

  if (m->dolegends) {
    int offset;

    xwin_setforeground(m->xw, m->textcolor);
    if (m->dousedlegends)
      offset = xwin_textwidth(m->xw, "XXXXXXXXXX");
    else
      offset = xwin_textwidth(m->xw, "XXXXXX");

    xwin_drawstring(m->xw, m->x - offset + 1, m->y + m->height, m->title);

    if (m->docaptions) {
      xwin_setforeground(m->xw, bfm->oncolor);
      xwin_drawstring(m->xw, m->x, m->y - 5, m->legend);
      bitfieldmeter_drawfieldlegend(bfm);
    }
  }
  bitfieldmeter_drawbits(bfm, 1);
  bitfieldmeter_drawfields(bfm, 1);
}

void bitfieldmeter_checkevent(Meter *m) {
  BitFieldMeter *bfm = (BitFieldMeter *)m;

  bitfieldmeter_drawbits(bfm, 0);
  bitfieldmeter_drawfields(bfm, 0);
}

void bitfieldmeter_drawfieldlegend(BitFieldMeter *bfm) {
  Meter *m = &bfm->m;
  char *tmp1, *tmp2, buff[100];
  int i, n, x = m->x + m->width / 2 + 4;

  tmp1 = tmp2 = bfm->fieldlegend;
  for (i = 0; i < bfm->numfields; i++) {
    n = 0;
    while ((*tmp2 != '/') && (*tmp2 != '\0')) {
      /*  allow '/' in a field as '\/'  */
      if ((*tmp2 == '\\') && (*(tmp2 + 1) == '/'))
        memmove(tmp2, tmp2 + 1, strlen(tmp2));
      tmp2++;
      n++;
    }
    tmp2++;
    memcpy(buff, tmp1, n);
    buff[n] = '\0';
    xwin_setstipplen(m->xw, i % 4);
    xwin_setforeground(m->xw, bfm->colors[i]);
    xwin_drawstring(m->xw, x, m->y - 5, buff);
    x += xwin_textwidth_n(m->xw, buff, n);
    xwin_setforeground(m->xw, xwin_foreground(m->xw));
    if (i != bfm->numfields - 1)
      xwin_drawstring(m->xw, x, m->y - 5, "/");
    x += xwin_textwidth_n(m->xw, "/", 1);
    tmp1 = tmp2;
  }
  xwin_setstipplen(m->xw, 0);  /*  Restore default all-bits stipple.  */
}

void bitfieldmeter_drawused(BitFieldMeter *bfm, int mandatory) {
  static int onechar = 0;
  Meter *m = &bfm->m;
  char buf[10];

  if (!mandatory && bfm->lastused == bfm->used)
    return;

  xwin_setstipplen(m->xw, 0);  /*  Use all-bits stipple.  */
  if (!onechar)
    onechar = xwin_textwidth(m->xw, "X");
  if (!bfm->usedoffset)  /*  metric meters need extra space for '-' sign  */
    bfm->usedoffset = bfm->metric ? xwin_textwidth(m->xw, "XXXXXX")
                                  : xwin_textwidth(m->xw, "XXXXX");

  meter_formatused(buf, sizeof buf, bfm->print, bfm->used, bfm->metric);

  xwin_cleararea(m->xw, m->x - bfm->usedoffset,
                 m->y + m->height - xwin_textheight(m->xw),
                 bfm->usedoffset - onechar / 2, xwin_textheight(m->xw) + 1);
  xwin_setforeground(m->xw, bfm->usedcolor);
  xwin_drawstring(m->xw, m->x - (strlen(buf) + 1) * onechar + 2,
                  m->y + m->height, buf);
  bfm->lastused = bfm->used;
}

void bitfieldmeter_drawbits(BitFieldMeter *bfm, int mandatory) {
  Meter *m = &bfm->m;
  int i, x1 = m->x;
  int w = (m->width / 2 - (bfm->numbits + 1)) / bfm->numbits;

  for (i = 0; i < bfm->numbits; i++) {
    if ((bfm->bits[i] != bfm->lastbits[i]) || mandatory) {
      xwin_setforeground(m->xw, bfm->bits[i] ? bfm->oncolor : bfm->offcolor);
      xwin_drawfilledrectangle(m->xw, x1, m->y, w, m->height);
    }

    bfm->lastbits[i] = bfm->bits[i];
    x1 += (w + 2);
  }
}

void bitfieldmeter_drawfields(BitFieldMeter *bfm, int mandatory) {
  Meter *m = &bfm->m;
  int i, twidth, x = m->x + m->width / 2 + 4;

  if (bfm->total == 0)
    return;

  for (i = 0; i < bfm->numfields; i++) {
    /*  Look for bogus values.  */
    if (bfm->fields[i] < 0.0 && !bfm->metric) {
      /*  Only print a warning 5 times per meter, followed by a message
       *  about no more warnings.  */
      bfm->numWarnings++;
      if (bfm->numWarnings < 5)
        fprintf(stderr, "Warning: meter %s had a negative value of %f for "
                "field %d\n", m->name, bfm->fields[i], i);
      if (bfm->numWarnings == 5)
        fprintf(stderr, "Future warnings from the %s meter will not be "
                "displayed.\n", m->name);
    }

    twidth = (int)fabs(((m->width / 2 - 3) * bfm->fields[i]) / bfm->total);
    if ((i == bfm->numfields - 1) && ((x + twidth) != (m->x + m->width)))
      twidth = m->width + m->x - x;

    if (mandatory || (twidth != bfm->lastvals[i]) || (x != bfm->lastx[i])) {
      xwin_setforeground(m->xw, bfm->colors[i]);
      xwin_setstipplen(m->xw, i % 4);
      xwin_drawfilledrectangle(m->xw, x, m->y, twidth, m->height);
      xwin_setstipplen(m->xw, 0);  /*  Restore all-bits stipple.  */
      bfm->lastvals[i] = twidth;
      bfm->lastx[i] = x;

      if (m->dousedlegends)
        bitfieldmeter_drawused(bfm, mandatory);
    }
    x += twidth;
  }
}

void bitfieldmeter_setbits(BitFieldMeter *bfm, int startbit,
                           unsigned char values) {
  unsigned char mask = 1;
  int i;

  for (i = startbit; i < startbit + 8; i++) {
    bfm->bits[i] = values & mask;
    mask = mask << 1;
  }
}

int bitfieldmeter_checkx(const BitFieldMeter *bfm, int x, int width) {
  const Meter *m = &bfm->m;
  int i;

  if ((x >= m->x) && (x + width >= m->x)
      && (x <= m->x + m->width) && (x + width <= m->x + m->width))
    return 1;

  fprintf(stderr, "bitfieldmeter_checkx() : bad horiz values for meter : %s\n",
          m->name);
  fprintf(stderr, "value %d, width %d, total = %f\n", x, width, bfm->total);
  for (i = 0; i < bfm->numfields; i++)
    fprintf(stderr, "fields[%d] = %f,", i, bfm->fields[i]);
  fprintf(stderr, "\n");

  return 0;
}

/*---------------------------------------------------------------------------*/

void bitfieldmeter_timerstart(BitFieldMeter *bfm) {
  gettimeofday(&bfm->tstart, NULL);
}

void bitfieldmeter_timerstop(BitFieldMeter *bfm) {
  gettimeofday(&bfm->tstop, NULL);
}

double bitfieldmeter_usecs(const BitFieldMeter *bfm) {
  return (bfm->tstop.tv_sec - bfm->tstart.tv_sec) * 1000000.0
         + bfm->tstop.tv_usec - bfm->tstart.tv_usec;
}

double bitfieldmeter_secs(const BitFieldMeter *bfm) {
  return bitfieldmeter_usecs(bfm) / 1e6;
}
