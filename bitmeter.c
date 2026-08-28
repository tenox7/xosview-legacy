/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "bitmeter.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>

void bitmeter_init(BitMeter *bm, XOSView *parent, const char *name,
                   const char *title, const char *legend, int numbits,
                   int docaptions, int dolegends, int dousedlegends) {
  (void) dolegends;  /*  The C++ version passed dousedlegends for both the
                         dolegends and dousedlegends arguments of its base
                         constructor; kept as is so the meters that use it
                         keep drawing the same way.  */
  meter_init(&bm->m, parent, name, title, legend, docaptions, dousedlegends,
             dousedlegends);

  bm->m.draw = bitmeter_draw;
  bm->m.checkevent = bitmeter_checkevent;
  bm->m.destroy = bitmeter_fini;

  bm->bits = bm->lastbits = NULL;
  bm->disabled = 0;
  bm->oncolor = bm->offcolor = 0;
  bitmeter_setnumbits(bm, numbits);
}

void bitmeter_fini(Meter *m) {
  BitMeter *bm = (BitMeter *)m;

  free(bm->bits);
  free(bm->lastbits);
  bm->bits = bm->lastbits = NULL;
  meter_fini(m);
}

void bitmeter_setnumbits(BitMeter *bm, int n) {
  int i;

  bm->numbits = n;
  free(bm->bits);
  free(bm->lastbits);

  bm->bits = (char *)malloc(n);
  bm->lastbits = (char *)malloc(n);
  if (bm->bits == NULL || bm->lastbits == NULL) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }

  for (i = 0; i < n; i++)
    bm->bits[i] = bm->lastbits[i] = 0;
}

void bitmeter_disable(BitMeter *bm) {
  bm->disabled = 1;
  bm->oncolor = xwin_alloccolor(bm->m.xw, "gray");
  bm->offcolor = bm->oncolor;
  meter_setlegend(&bm->m, "Disabled");
}

void bitmeter_checkevent(Meter *m) {
  bitmeter_drawbits((BitMeter *)m, 0);
}

void bitmeter_drawbits(BitMeter *bm, int mandatory) {
  Meter *m = &bm->m;
  int i, x1 = m->x, x2;

  for (i = 0; i < bm->numbits; i++) {
    if (i != (bm->numbits - 1))
      x2 = m->x + ((i + 1) * (m->width + 1)) / bm->numbits - 1;
    else
      x2 = m->x + (m->width + 1) - 1;

    if ((bm->bits[i] != bm->lastbits[i]) || mandatory) {
      xwin_setforeground(m->xw, bm->bits[i] ? bm->oncolor : bm->offcolor);
      xwin_drawfilledrectangle(m->xw, x1, m->y, x2 - x1, m->height);
    }

    bm->lastbits[i] = bm->bits[i];
    x1 = x2 + 2;
  }
}

void bitmeter_draw(Meter *m) {
  BitMeter *bm = (BitMeter *)m;

  xwin_linewidth(m->xw, 1);
  xwin_setforeground(m->xw, xwin_foreground(m->xw));
  xwin_drawfilledrectangle(m->xw, m->x - 1, m->y - 1, m->width + 2,
                           m->height + 2);
  xwin_linewidth(m->xw, 0);

  if (m->dolegends) {
    int offset;

    xwin_setforeground(m->xw, m->textcolor);
    if (m->dousedlegends)
      offset = xwin_textwidth(m->xw, "XXXXXXXXXX");
    else
      offset = xwin_textwidth(m->xw, "XXXXXX");

    xwin_drawstring(m->xw, m->x - offset + 1, m->y + m->height, m->title);
    xwin_setforeground(m->xw, bm->oncolor);
    if (m->docaptions)
      xwin_drawstring(m->xw, m->x, m->y - 5, m->legend);
  }

  bitmeter_drawbits(bm, 1);
}

void bitmeter_setbits(BitMeter *bm, int startbit, unsigned char values) {
  unsigned char mask = 1;
  int i;

  for (i = startbit; i < startbit + 8; i++) {
    bm->bits[i] = values & mask;
    mask = mask << 1;
  }
}
