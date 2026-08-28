/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  Decay drawing (Oct. 1995) by Brian Grayson ( bgrayson@netbsd.org ),
 *  graph drawing (Oct. 1998) by Scott McNab ( jedi@tartarus.uwa.edu.au ).
 *
 *  This file may be distributed under terms of the GPL or of the BSD
 *  license, whichever you choose.
 */

#include "fieldmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void drawplain(FieldMeter *fm, int mandatory);
static void drawdecay(FieldMeter *fm, int mandatory);
static void drawgraph(FieldMeter *fm, int mandatory);
static void drawbar(FieldMeter *fm, int i);

static void *xalloc(size_t n) {
  void *p = malloc(n);

  if (p == NULL) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }
  return p;
}

/*---------------------------------------------------------------------------*/

void fieldmeter_init(FieldMeter *fm, XOSView *parent, int numfields,
                     const char *name, const char *title, const char *legend,
                     int docaptions, int dolegends, int dousedlegends) {
  meter_init(&fm->m, parent, name, title, legend, docaptions, dolegends,
             dousedlegends);

  fm->m.checkres = fieldmeter_checkresources;
  fm->m.draw = fieldmeter_draw;
  fm->m.checkevent = fieldmeter_checkevent;
  fm->m.destroy = fieldmeter_fini;

  /*  We need to set print to something valid -- the meters apparently get
   *  drawn before they have a chance to call checkResources themselves.  */
  fm->numWarnings = fm->printedZeroTotalMesg = 0;
  fm->print = UT_PERCENT;
  fm->metric = 0;
  fm->usedoffset = 0;
  fm->used = 0;
  fm->lastused = -1;
  fm->fields = NULL;
  fm->colors = NULL;
  fm->lastvals = NULL;
  fm->lastx = NULL;

  fm->dodecay = 1;
  fm->firsttime = 1;
  fm->decay = NULL;
  fm->lastdecayval = NULL;

  fm->usegraph = 0;
  fm->heightfield = NULL;
  fm->lastwinstate = WV_OBSCURED;

  fm->tstart.tv_sec = fm->tstart.tv_usec = 0;
  fm->tstop = fm->tstart;

  fieldmeter_setnumfields(fm, numfields);

  /*  Set the number of columns to a reasonable default in case we can't
   *  find the resource.  */
  fieldmeter_setnumcols(fm, 100);
}

void fieldmeter_fini(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  free(fm->fields);
  free(fm->colors);
  free(fm->lastvals);
  free(fm->lastx);
  free(fm->decay);
  free(fm->lastdecayval);
  free(fm->heightfield);
  fm->fields = NULL;
  fm->colors = NULL;
  fm->lastvals = NULL;
  fm->lastx = NULL;
  fm->decay = NULL;
  fm->lastdecayval = NULL;
  fm->heightfield = NULL;
  meter_fini(m);
}

void fieldmeter_checkresources(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;
  const char *ptr;

  meter_checkresources(m);
  fm->usedcolor = xwin_alloccolor(m->xw,
                                  xwin_getresource(m->xw, "usedLabelColor"));

  ptr = xwin_getresource(m->xw, "graphNumCols");
  if (ptr) {
    int i;
    if (sscanf(ptr, "%d", &i) == 1 && i > 0)
      fieldmeter_setnumcols(fm, i);
  }
}

void fieldmeter_setnumfields(FieldMeter *fm, int n) {
  int i;

  fm->numfields = n;
  free(fm->fields);
  free(fm->colors);
  free(fm->lastvals);
  free(fm->lastx);
  free(fm->decay);
  free(fm->lastdecayval);

  fm->fields = (double *)xalloc(n * sizeof(double));
  fm->colors = (unsigned long *)xalloc(n * sizeof(unsigned long));
  fm->lastvals = (int *)xalloc(n * sizeof(int));
  fm->lastx = (int *)xalloc(n * sizeof(int));
  fm->decay = (double *)xalloc(n * sizeof(double));
  fm->lastdecayval = (double *)xalloc(n * sizeof(double));

  fm->total = 0;
  for (i = 0; i < n; i++) {
    fm->fields[i] = 0.0;
    fm->colors[i] = 0;
    fm->lastvals[i] = fm->lastx[i] = 0;
    fm->decay[i] = 0.0;
    fm->lastdecayval[i] = 0.0;
  }

  /*  The decay fields are seeded from the first drawfields() again.  */
  fm->firsttime = 1;

  /*  The graph's height field is indexed by numfields, so it has to go.  */
  free(fm->heightfield);
  fm->heightfield = NULL;
}

void fieldmeter_setnumcols(FieldMeter *fm, int n) {
  fm->graphnumcols = n;
  fm->graphpos = n - 1;

  free(fm->heightfield);
  fm->heightfield = NULL;
}

void fieldmeter_disable(FieldMeter *fm) {
  fieldmeter_setnumfields(fm, 1);
  fieldmeter_setcolorname(fm, 0, "gray");
  meter_setlegend(&fm->m, "Disabled");
  /*  And specify the total of 1.0, so the meter is grayed out.  */
  fm->total = 1.0;
  fm->fields[0] = 1.0;
}

void fieldmeter_reset(FieldMeter *fm) {
  int i;

  for (i = 0; i < fm->numfields; i++)
    fm->lastvals[i] = fm->lastx[i] = -1;
}

void fieldmeter_setcolorname(FieldMeter *fm, int field, const char *color) {
  fm->colors[field] = xwin_alloccolor(fm->m.xw, color);
}

void fieldmeter_setcolor(FieldMeter *fm, int field, unsigned long color) {
  fm->colors[field] = color;
}

void fieldmeter_setusedformat(FieldMeter *fm, const char *fmt) {
  fm->print = meter_parseusedformat(fmt);
}

void fieldmeter_setused(FieldMeter *fm, double val, double total) {
  if (fm->print == UT_PERCENT) {
    if (total != 0.0) {
      fm->used = val / total * 100.0;
      return;
    }
    if (!fm->printedZeroTotalMesg) {
      fm->printedZeroTotalMesg = 1;
      fprintf(stderr, "Warning: %s meter had a zero total field! "
              "Would have caused a div-by-zero exception.\n", fm->m.name);
    }
    fm->used = 0.0;
    return;
  }

  /*  UT_FLOAT and UT_AUTOSCALE both show the raw value.  */
  fm->used = val;
}

/*---------------------------------------------------------------------------*/

void fieldmeter_draw(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  /*  Draw the outline for the fieldmeter.  */
  xwin_setforeground(m->xw, xwin_foreground(m->xw));
  xwin_drawrectangle(m->xw, m->x - 1, m->y - 1, m->width + 2, m->height + 2);

  if (m->dolegends) {
    int offset;

    xwin_setforeground(m->xw, m->textcolor);
    if (m->dousedlegends)
      offset = xwin_textwidth(m->xw, "XXXXXXXXXX");
    else
      offset = xwin_textwidth(m->xw, "XXXXXX");

    xwin_drawstring(m->xw, m->x - offset + 1, m->y + m->height, m->title);
  }

  fieldmeter_drawlegend(fm);
  fieldmeter_drawfields(fm, 1);
}

void fieldmeter_checkevent(Meter *m) {
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

void fieldmeter_drawlegend(FieldMeter *fm) {
  Meter *m = &fm->m;
  char *tmp1, *tmp2, buff[100];
  int i, n, x = m->x;

  if (!m->docaptions || !m->dolegends)
    return;

  xwin_cleararea(m->xw, m->x, m->y - 5 - xwin_textheight(m->xw),
                 m->width + 5, xwin_textheight(m->xw) + 4);

  tmp1 = tmp2 = m->legend;
  for (i = 0; i < fm->numfields; i++) {
    n = 0;
    while ((*tmp2 != '/') && (*tmp2 != '\0')) {
      /*  allow '/' in a field as '\/'  */
      if ((*tmp2 == '\\') && (*(tmp2 + 1) == '/'))
        memmove(tmp2, tmp2 + 1, strlen(tmp2));
      tmp2++;
      n++;
    }
    tmp2++;
    strncpy(buff, tmp1, n);
    buff[n] = '\0';
    xwin_setstipplen(m->xw, i % 4);
    xwin_setforeground(m->xw, fm->colors[i]);
    xwin_drawstring(m->xw, x, m->y - 5, buff);
    x += xwin_textwidth_n(m->xw, buff, n);
    xwin_setforeground(m->xw, xwin_foreground(m->xw));
    if (i != fm->numfields - 1)
      xwin_drawstring(m->xw, x, m->y - 5, "/");
    x += xwin_textwidth_n(m->xw, "/", 1);
    tmp1 = tmp2;
  }
  xwin_setstipplen(m->xw, 0);  /*  Restore default all-bits stipple.  */
}

void fieldmeter_drawused(FieldMeter *fm, int mandatory) {
  static int onechar = 0;
  Meter *m = &fm->m;
  char buf[10];

  if (!mandatory && fm->lastused == fm->used)
    return;

  xwin_setstipplen(m->xw, 0);  /*  Use all-bits stipple.  */
  if (!onechar)
    onechar = xwin_textwidth(m->xw, "X");
  if (!fm->usedoffset)  /*  metric meters need extra space for '-' sign  */
    fm->usedoffset = fm->metric ? xwin_textwidth(m->xw, "XXXXXX")
                                : xwin_textwidth(m->xw, "XXXXX");

  meter_formatused(buf, sizeof buf, fm->print, fm->used, fm->metric);

  xwin_cleararea(m->xw, m->x - fm->usedoffset,
                 m->y + m->height - xwin_textheight(m->xw),
                 fm->usedoffset - onechar / 2, xwin_textheight(m->xw) + 1);
  xwin_setforeground(m->xw, fm->usedcolor);
  xwin_drawstring(m->xw, m->x - (strlen(buf) + 1) * onechar + 2,
                  m->y + m->height, buf);
  fm->lastused = fm->used;
}

int fieldmeter_checkx(const FieldMeter *fm, int x, int width) {
  const Meter *m = &fm->m;
  int i;

  if ((x >= m->x) && (x + width >= m->x)
      && (x <= m->x + m->width) && (x + width <= m->x + m->width))
    return 1;

  fprintf(stderr, "fieldmeter_checkx() : bad horiz values for meter : %s\n",
          m->name);
  fprintf(stderr, "value %d, width %d, total = %f\n", x, width, fm->total);
  for (i = 0; i < fm->numfields; i++)
    fprintf(stderr, "fields[%d] = %f,", i, fm->fields[i]);
  fprintf(stderr, "\n");

  return 0;
}

/*---------------------------------------------------------------------------*/

void fieldmeter_drawfields(FieldMeter *fm, int mandatory) {
  if (fm->usegraph) {
    drawgraph(fm, mandatory);
    return;
  }
  if (fm->dodecay) {
    drawdecay(fm, mandatory);
    return;
  }
  drawplain(fm, mandatory);
}

static void drawplain(FieldMeter *fm, int mandatory) {
  Meter *m = &fm->m;
  int i, twidth, x = m->x;

  if (fm->total == 0)
    return;

  for (i = 0; i < fm->numfields; i++) {
    /*  Look for bogus values.  */
    if (fm->fields[i] < 0.0 && !fm->metric) {
      /*  Only print a warning 5 times per meter, followed by a message
       *  about no more warnings.  */
      fm->numWarnings++;
      if (fm->numWarnings < 5)
        fprintf(stderr, "Warning: meter %s had a negative value of %f for "
                "field %d\n", m->name, fm->fields[i], i);
      if (fm->numWarnings == 5)
        fprintf(stderr, "Future warnings from the %s meter will not be "
                "displayed.\n", m->name);
    }

    twidth = (int)fabs(((m->width * fm->fields[i]) / fm->total));
    if ((i == fm->numfields - 1) && ((x + twidth) != (m->x + m->width)))
      twidth = m->width + m->x - x;

    if (mandatory || (twidth != fm->lastvals[i]) || (x != fm->lastx[i])) {
      xwin_setforeground(m->xw, fm->colors[i]);
      xwin_setstipplen(m->xw, i % 4);
      xwin_drawfilledrectangle(m->xw, x, m->y, twidth, m->height);
      xwin_setstipplen(m->xw, 0);  /*  Restore all-bits stipple.  */
      fm->lastvals[i] = twidth;
      fm->lastx[i] = x;
    }
    x += twidth;
  }
  if (m->dousedlegends)
    fieldmeter_drawused(fm, mandatory);
}

/*
 *  The constant below can be modified for quicker or slower exponential
 *  rates for the average.  No fancy math is done to set it to correspond
 *  to a five-second decay or anything -- I just played with it until I
 *  thought it looked good!  :)  BCG
 */
#define ALPHA 0.97

static void drawdecay(FieldMeter *fm, int mandatory) {
  Meter *m = &fm->m;
  int i, twidth, x = m->x;
  int decay_changed = 0;
  int halfheight, decaytwidth, decayx = m->x;

  if (fm->total == 0.0)
    return;

  halfheight = m->height / 2;

  /*  This code is supposed to make the average display look just like the
   *  ordinary display for the first drawfields, but it doesn't seem to work
   *  too well.  But it's better than setting all decay fields to 0.0
   *  initially!  */
  if (fm->firsttime) {
    fm->firsttime = 0;
    mandatory = 1;
    for (i = 0; i < fm->numfields; i++)
      fm->decay[i] = 1.0 * fm->fields[i] / fm->total;
  }

  /*  Update the decay fields.  This is not quite accurate, since if the
   *  screen is refreshed, we will update the decay fields more often than
   *  we need to.  However, this makes the decay stuff TOTALLY independent
   *  of the ????Meter methods.
   *
   *  This is majorly ugly code.  It needs a rewrite.  BCG
   *  I think one good way to do it may be to normalize all of the fields
   *  in a temporary array into the range 0.0 .. 1.0, calculate the shifted
   *  starting positions and ending positions for coloring, multiply by the
   *  pixel width of the meter, and then turn to ints.  I think this will
   *  solve a whole bunch of our problems with rounding that before we
   *  tackled at a whole lot of places.  BCG  */
  for (i = 0; i < fm->numfields; i++) {
    fm->decay[i] = ALPHA * fm->decay[i]
                   + (1 - ALPHA) * (fm->fields[i] * 1.0 / fm->total);

    /*  We want to round the widths, rather than truncate.  */
    twidth = (int)(0.5 + (m->width * fm->fields[i]) / fm->total);
    decaytwidth = (int)(0.5 + m->width * fm->decay[i]);
    if (decaytwidth < 0)
      fprintf(stderr, "Error:  decay meter %s:  decaytwidth of %d, width of "
              "%d, decay[%d] of %f\n", m->name, decaytwidth, m->width, i,
              fm->decay[i]);

    /*  However, due to rounding, we may have gone one pixel too far by the
     *  time we get to the later fields...  */
    if (x + twidth > m->x + m->width)
      twidth = m->width + m->x - x;
    if (decayx + decaytwidth > m->x + m->width)
      decaytwidth = m->width + m->x - decayx;

    /*  Also, due to rounding error, the last field may not go far
     *  enough...  */
    if ((i == fm->numfields - 1) && ((x + twidth) != (m->x + m->width)))
      twidth = m->width + m->x - x;
    if ((i == fm->numfields - 1)
        && ((decayx + decaytwidth) != (m->x + m->width)))
      decaytwidth = m->width + m->x - decayx;

    xwin_setforeground(m->xw, fm->colors[i]);
    xwin_setstipplen(m->xw, i % 4);

    /*  xwin_drawfilledrectangle() adds one to its width and height.
     *  Let's correct for that here.  */
    if (mandatory || (twidth != fm->lastvals[i]) || (x != fm->lastx[i])) {
      if (!fieldmeter_checkx(fm, x, twidth))
        fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
      xwin_drawfilledrectangle(m->xw, x, m->y, twidth, halfheight);
    }

    if (mandatory || decay_changed || (fm->decay[i] != fm->lastdecayval[i])) {
      if (!fieldmeter_checkx(fm, decayx, decaytwidth))
        fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
      decay_changed = 1;
      xwin_drawfilledrectangle(m->xw, decayx, m->y + halfheight + 1,
                               decaytwidth, m->height - halfheight - 1);
    }

    fm->lastvals[i] = twidth;
    fm->lastx[i] = x;
    fm->lastdecayval[i] = fm->decay[i];

    xwin_setstipplen(m->xw, 0);  /*  Restore all-bits stipple.  */
    if (m->dousedlegends)
      fieldmeter_drawused(fm, mandatory);
    x += twidth;
    decayx += decaytwidth;
  }
}

static void drawgraph(FieldMeter *fm, int mandatory) {
  Meter *m = &fm->m;
  int i, j, currwinstate;

  if (fm->total <= 0.0)
    return;

  /*  Allocate memory for height field graph storage.  This is done here as
   *  it is not certain that both numfields and graphnumcols are defined in
   *  the constructor.  */
  if (fm->heightfield == NULL) {
    if (fm->numfields <= 0 || fm->graphnumcols <= 0)
      return;

    fm->heightfield = (double *)xalloc(fm->numfields * fm->graphnumcols
                                       * sizeof(double));
    for (i = 0; i < fm->graphnumcols; i++)
      for (j = 0; j < fm->numfields; j++)
        fm->heightfield[i * fm->numfields + j] =
            (j < fm->numfields - 1) ? 0.0 : 1.0;
  }

  /*  check current position here and slide graph if necessary  */
  if (fm->graphpos >= fm->graphnumcols) {
    for (i = 0; i < fm->graphnumcols - 1; i++)
      for (j = 0; j < fm->numfields; j++)
        fm->heightfield[i * fm->numfields + j] =
            fm->heightfield[(i + 1) * fm->numfields + j];
    fm->graphpos = fm->graphnumcols - 1;
  }

  /*  get current values to be plotted  */
  for (i = 0; i < fm->numfields; i++) {
    double a = fm->fields[i] / fm->total;
    if (a <= 0.0)
      a = 0.0;
    if (a >= 1.0)
      a = 1.0;
    fm->heightfield[fm->graphpos * fm->numfields + i] = a;
  }

  currwinstate = xosview_visibility(m->parent);

  /*  Try to avoid having to redraw everything.  */
  if (!mandatory && currwinstate == WV_FULLY_VISIBLE
      && currwinstate == fm->lastwinstate) {
    /*  scroll area  */
    int col_width = m->width / fm->graphnumcols;
    int sx, swidth, sheight;

    if (col_width < 1)
      col_width = 1;

    sx = m->x + col_width;
    swidth = m->width - col_width;
    sheight = m->height + 1;
    if (sx > m->x && swidth > 0 && sheight > 0)
      xwin_copyarea(m->xw, sx, m->y, swidth, sheight, m->x, m->y);
    drawbar(fm, fm->graphnumcols - 1);
  } else {
    /*  need to draw entire graph for some reason  */
    for (i = 0; i < fm->graphnumcols; i++)
      drawbar(fm, i);
  }

  fm->lastwinstate = currwinstate;
  fm->graphpos++;
  xwin_setstipplen(m->xw, 0);  /*  Restore all-bits stipple.  */
  if (m->dousedlegends)
    fieldmeter_drawused(fm, mandatory);
}

static void drawbar(FieldMeter *fm, int i) {
  Meter *m = &fm->m;
  int j;
  int y = m->y + m->height;
  int x = m->x + i * m->width / fm->graphnumcols;
  int barwidth = (m->x + (i + 1) * m->width / fm->graphnumcols) - x;

  if (barwidth <= 0)
    return;

  for (j = 0; j < fm->numfields; j++) {
    /*  Round up, by adding 0.5 before converting to an int.  */
    int barheight = (int)((fm->heightfield[i * fm->numfields + j] * m->height)
                          + 0.5);

    xwin_setforeground(m->xw, fm->colors[j]);
    xwin_setstipplen(m->xw, j % 4);

    if (barheight > (y - m->y))
      barheight = y - m->y;

    /*  hack to ensure last field always reaches top of graph area  */
    if (j == fm->numfields - 1)
      barheight = y - m->y;

    y -= barheight;
    if (barheight > 0)
      xwin_drawfilledrectangle(m->xw, x, y, barwidth, barheight);
  }
}

/*---------------------------------------------------------------------------*/

void fieldmeter_timerstart(FieldMeter *fm) {
  gettimeofday(&fm->tstart, NULL);
}

void fieldmeter_timerstop(FieldMeter *fm) {
  gettimeofday(&fm->tstop, NULL);
}

/*  Doubles throughout, to avoid the wrap/overflow/sign-bit problems that
 *  an integer usec count suffers from.  */
double fieldmeter_usecs(const FieldMeter *fm) {
  return (fm->tstop.tv_sec - fm->tstart.tv_sec) * 1000000.0
         + fm->tstop.tv_usec - fm->tstart.tv_usec;
}

double fieldmeter_secs(const FieldMeter *fm) {
  return fieldmeter_usecs(fm) / 1e6;
}
