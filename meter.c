/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "meter.h"
#include "xosview.h"
#include "xwin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void meter_init(Meter *m, XOSView *parent, const char *name,
                const char *title, const char *legend,
                int docaptions, int dolegends, int dousedlegends) {
  m->parent = parent;
  m->xw = (XWin *)parent;
  m->name = name;
  m->title = m->legend = NULL;
  meter_settitle(m, title);
  meter_setlegend(m, legend);
  m->docaptions = docaptions;
  m->dolegends = dolegends;
  m->dousedlegends = dousedlegends;
  m->priority = 1;
  m->counter = 0;
  m->textcolor = 0;

  m->checkevent = NULL;
  m->checkres = meter_checkresources;
  m->draw = NULL;
  m->destroy = meter_fini;

  meter_resize(m, xosview_xoff(parent), xosview_newypos(parent),
               xwin_width(m->xw) - 10, 10);
}

void meter_fini(Meter *m) {
  free(m->title);
  free(m->legend);
  m->title = m->legend = NULL;
}

static char *dupstring(const char *s) {
  size_t len = strlen(s);
  char *p = (char *)malloc(len + 1);

  if (p == NULL) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }
  memcpy(p, s, len + 1);
  return p;
}

void meter_settitle(Meter *m, const char *title) {
  free(m->title);
  m->title = dupstring(title);
}

void meter_setlegend(Meter *m, const char *legend) {
  free(m->legend);
  m->legend = dupstring(legend);
}

void meter_resize(Meter *m, int x, int y, int width, int height) {
  m->x = x;
  m->y = y;
  m->width = (width >= 0) ? width : 0;     /*  fix for cosmetical bug:  */
  m->height = (height >= 0) ? height : 0;  /*  beware of values < 0 !   */
  m->width &= ~1;                          /*  only allow even widths   */
}

void meter_checkresources(Meter *m) {
  m->textcolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw,
                                                         "meterLabelColor"));
}

int meter_requestevent(Meter *m) {
  int rval;

  if (m->priority == 0) {
    fprintf(stderr, "Warning:  meter %s had an invalid priority of 0. "
            "Resetting to 1...\n", m->name);
    m->priority = 1;
  }
  rval = m->counter % m->priority;
  m->counter = (m->counter + 1) % m->priority;
  return !rval;
}

double meter_samplespersecond(const Meter *m) {
  return 1.0 * MAX_SAMPLES_PER_SECOND / m->priority;
}

double meter_secondspersample(const Meter *m) {
  return 1.0 / meter_samplespersecond(m);
}

double meter_scalevalue(double value, char *scale, int metric) {
  double scaled = (value < 0 ? -value : value);

  if (scaled >= 999.5e15) {
    scale[0] = 'E';
    scaled = value / (metric ? 1e18 : 1152921504606846976.0);
  } else if (scaled >= 999.5e12) {
    scale[0] = 'P';
    scaled = value / (metric ? 1e15 : 1125899906842624.0);
  } else if (scaled >= 999.5e9) {
    scale[0] = 'T';
    scaled = value / (metric ? 1e12 : 1099511627776.0);
  } else if (scaled >= 999.5e6) {
    scale[0] = 'G';
    scaled = value / (metric ? 1e9 : 1073741824.0);
  } else if (scaled >= 999.5e3) {
    scale[0] = 'M';
    scaled = value / (metric ? 1e6 : 1048576.0);
  } else if (scaled >= 999.5) {
    scale[0] = (metric ? 'k' : 'K');
    scaled = value / (metric ? 1e3 : 1024.0);
  } else if (scaled < 0.9995 && metric) {
    if (scaled >= 0.9995e-3) {
      scale[0] = 'm';
      scaled = value * 1e3;
    } else if (scaled >= 0.9995e-6) {
      scale[0] = '\265';
      scaled = value * 1e6;
    } else {
      scale[0] = 'n';
      scaled = value * 1e9;
    }
    /*  add more if needed  */
  } else {
    scale[0] = '\0';
    scaled = value;
  }
  scale[1] = '\0';
  return scaled;
}

int meter_parseusedformat(const char *fmt) {
  /*  Do case-insensitive compares.  */
  if (!strncasecmp(fmt, "percent", 8))
    return UT_PERCENT;
  if (!strncasecmp(fmt, "autoscale", 10))
    return UT_AUTOSCALE;
  if (!strncasecmp(fmt, "float", 6))
    return UT_FLOAT;

  fprintf(stderr, "Error:  could not parse format of '%s'.\n"
          "  I expected one of 'percent', 'autoscale', or 'float'"
          " (Case-insensitive).\n", fmt);
  exit(1);
  return UT_PERCENT;
}

void meter_formatused(char *buf, size_t bufsize, int print, double used,
                      int metric) {
  if (print == UT_PERCENT) {
    snprintf(buf, bufsize, "%d%%", (int)used);
    return;
  }

  if (print == UT_AUTOSCALE) {
    char scale[2];
    double scaled = meter_scalevalue(used, scale, metric);

    /*  For now, we can only print 3 characters, plus the optional sign and
     *  suffix, without overprinting the legends.  Thus, we can print 965,
     *  or we can print 34, but we can't print 34.7 (the decimal point takes
     *  up one character).  bgrayson  */
    if (scaled == 0.0)
      snprintf(buf, bufsize, "0");
    else if (scaled < 0 && !metric)
      snprintf(buf, bufsize, "-");
    else if (fabs(scaled) < 9.95)
      /*  9.95 or above would get rounded to 10.0, which is too wide.  */
      snprintf(buf, bufsize, "%.1f%s", scaled, scale);
    else
      snprintf(buf, bufsize, "%.0f%s", scaled, scale);
    return;
  }

  if (fabs(used) < 99.95)
    snprintf(buf, bufsize, "%.1f", used);
  else  /*  drop the decimal if the string gets too long  */
    snprintf(buf, bufsize, "%.0f", used);
}

void *meter_alloc(size_t n) {
  void *p = malloc(n);

  if (p == NULL) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }
  return p;
}
