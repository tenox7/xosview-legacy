/*
 *  Copyright (c) 1994, 1995, 2002, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "xosview.h"
#include "meter.h"
#include "MeterMaker.h"
#if ( defined(XOSVIEW_NETBSD) || defined(XOSVIEW_FREEBSD) || \
      defined(XOSVIEW_OPENBSD) || defined(XOSVIEW_DFBSD) )
# include "kernel.h"
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static const char versionString[] = "xosview version: Git";
static const char NAME[] = "xosview@";

double MAX_SAMPLES_PER_SECOND = 10;

static void checkVersion(int argc, char **argv);
static void checkArgs(int argc, char **argv);
static void checkOverallResources(XOSView *xv);
static void checkMeterResources(XOSView *xv);
static void figureSize(XOSView *xv);
static void dolegends(XOSView *xv);
static void resizeMeters(XOSView *xv);
void xosview_draw(XOSView *xv);
static int findx(XOSView *xv);
static int findy(XOSView *xv);
static const char *winname(XOSView *xv);

static void resizeEvent(XWin *xw, XEvent *event);
static void exposeEvent(XWin *xw, XEvent *event);
static void keyPressEvent(XWin *xw, XEvent *event);
static void visibilityEvent(XWin *xw, XEvent *event);
static void unmapEvent(XWin *xw, XEvent *event);

/*---------------------------------------------------------------------------*/

void xosdebug(const char *fmt, ...) {
#ifdef DEBUG
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
#else
  (void) fmt;
#endif
}

/*---------------------------------------------------------------------------*/

void xosview_init(XOSView *xv, const char *instName, int argc, char **argv) {
  static char defaultName[] = "xosview";

  /*  Check for version arguments first.  This allows them to work without
   *  the need for a connection to the X server.  */
  checkVersion(argc, argv);

  xv->meters = NULL;
  xv->nummeters = xv->metercap = 0;

  xrm_init(&xv->xrm, "xosview", instName);

  xwin_setdisplayname((XWin *)xv, xrm_getdisplayname(&xv->xrm, argc, argv));
  xwin_opendisplay((XWin *)xv);  /*  So that Xrm can contact the display for
                                     its default values.  */

  /*  The resources need to be initialized before calling xwin_init, because
   *  xwin_init looks at the geometry resource for its geometry.  BCG  */
  xrm_loadandmerge(&xv->xrm, &argc, argv, xv->xw.display);
  xwin_init((XWin *)xv, argc, argv, NULL, &xv->xrm);

  MAX_SAMPLES_PER_SECOND = atof(xwin_getresource((XWin *)xv, "samplesPerSec"));
  if (!MAX_SAMPLES_PER_SECOND)
    MAX_SAMPLES_PER_SECOND = 10;

  xv->usleeptime = (unsigned long)(1000000 / MAX_SAMPLES_PER_SECOND);
  if (xv->usleeptime >= 1000000) {
    /*  The syscall usleep() only takes times less than 1 sec, so split into
     *  a sleep time and a usleep time if needed.  */
    xv->sleeptime = xv->usleeptime / 1000000;
    xv->usleeptime = xv->usleeptime % 1000000;
  } else {
    xv->sleeptime = 0;
  }

#if ( defined(XOSVIEW_NETBSD) || defined(XOSVIEW_FREEBSD) || \
      defined(XOSVIEW_OPENBSD) || defined(XOSVIEW_DFBSD) )
  BSDInit();  /*  Needs to be done before processing of -N option.  */
#endif

  xv->hmargin = atoi(xwin_getresource((XWin *)xv, "horizontalMargin"));
  xv->vmargin = atoi(xwin_getresource((XWin *)xv, "verticalMargin"));
  xv->vspacing = atoi(xwin_getresource((XWin *)xv, "verticalSpacing"));
  if (xv->hmargin < 0)
    xv->hmargin = 0;
  if (xv->vmargin < 0)
    xv->vmargin = 0;
  if (xv->vspacing < 0)
    xv->vspacing = 0;

  checkArgs(argc, argv);  /*  Check for any other unhandled args.  */
  xv->xoff = xv->hmargin;
  xv->yoff = 0;
  xv->xw.name = defaultName;
  xv->deferred_resize = 1;
  xv->deferred_redraw = 1;
  xv->visibility = WV_OBSCURED;

  /*  set up the X events  */
  xwin_addevent((XWin *)xv, ConfigureNotify, resizeEvent);
  xwin_addevent((XWin *)xv, Expose, exposeEvent);
  xwin_addevent((XWin *)xv, KeyPress, keyPressEvent);
  xwin_addevent((XWin *)xv, VisibilityNotify, visibilityEvent);
  xwin_addevent((XWin *)xv, UnmapNotify, unmapEvent);

  /*  see if legends are to be used  */
  checkOverallResources(xv);

  /*  add in the meters  */
  makeMeters(xv);

  if (xv->nummeters == 0) {
    fprintf(stderr, "No meters were enabled!  Exiting...\n");
    exit(0);
  }

  /*  Have the meters re-check the resources.  */
  checkMeterResources(xv);

  /*  determine the width and height of the window then create it  */
  figureSize(xv);
  xwin_create((XWin *)xv, argc, argv);
  xwin_settitle((XWin *)xv, winname(xv));
  xwin_seticonname((XWin *)xv, winname(xv));
  dolegends(xv);
}

void xosview_fini(XOSView *xv) {
  int i;

  for (i = 0; i < xv->nummeters; i++) {
    xv->meters[i]->destroy(xv->meters[i]);
    free(xv->meters[i]);
  }
  free(xv->meters);
  xv->meters = NULL;
  xv->nummeters = xv->metercap = 0;

  /*  The C++ version destroyed the Xrm member before the XWin base; keep
   *  that order.  */
  xrm_fini(&xv->xrm);
  xwin_fini((XWin *)xv);
}

/*---------------------------------------------------------------------------*/

void xosview_addmeter(XOSView *xv, Meter *m) {
  if (xv->nummeters == xv->metercap) {
    int cap = xv->metercap ? xv->metercap * 2 : 16;
    Meter **grown = (Meter **)realloc(xv->meters, cap * sizeof(Meter *));

    if (grown == NULL) {
      fprintf(stderr, "Out of memory.\n");
      exit(1);
    }
    xv->meters = grown;
    xv->metercap = cap;
  }
  xv->meters[xv->nummeters++] = m;
}

int xosview_xoff(const XOSView *xv) { return xv->xoff; }

int xosview_newypos(const XOSView *xv) { return 15 + 25 * xv->nummeters; }

int xosview_visibility(const XOSView *xv) { return xv->visibility; }

const char *xosview_getresource(XOSView *xv, const char *name) {
  return xwin_getresource((XWin *)xv, name);
}

int xosview_isresourcetrue(XOSView *xv, const char *name) {
  return xwin_isresourcetrue((XWin *)xv, name);
}

const char *xosview_getresource_default(XOSView *xv, const char *name,
                                        const char *defaultVal) {
  return xwin_getresource_default((XWin *)xv, name, defaultVal);
}

void xosview_setdone(XOSView *xv, int val) {
  xwin_setdone((XWin *)xv, val);
}

/*---------------------------------------------------------------------------*/

static void checkVersion(int argc, char **argv) {
  int i;

  for (i = 0; i < argc; i++)
    if (!strncasecmp(argv[i], "-v", 2)
        || !strncasecmp(argv[i], "--version", 10)) {
      fprintf(stderr, "%s\n", versionString);
      exit(0);
    }
}

static void checkArgs(int argc, char **argv) {
  /*  xwin_init() and xrm_loadandmerge() modify argc and argv, so by this
   *  point, all XResource arguments should be removed.  Since we currently
   *  don't have any other command-line arguments, perform a check here to
   *  make sure we don't get any more.  */
  if (argc == 1)
    return;  /*  No arguments besides X resources.  */

  /*  Skip to the first real argument.  */
  argc--;
  argv++;
  while (argc > 0 && argv && *argv) {
    switch (argv[0][1]) {
    case 'n':
      /*  Check for -name option that was already parsed and acted upon by
       *  main().  */
      if (!strncasecmp(*argv, "-name", 6)) {
        argv++;  /*  Skip arg to -name.  */
        argc--;
      }
      break;
#if ( defined(XOSVIEW_NETBSD) || defined(XOSVIEW_FREEBSD) || \
      defined(XOSVIEW_OPENBSD) || defined(XOSVIEW_DFBSD) )
    case 'N':
      if (strlen(argv[0]) > 2) {
        SetKernelName(argv[0] + 2);
      } else {
        SetKernelName(argv[1]);
        argc--;
        argv++;
      }
      break;
#endif
      /*  Fall through to default/error case.  */
    default:
      fprintf(stderr, "Ignoring unknown option '%s'.\n", argv[0]);
      break;
    }
    argc--;
    argv++;
  }
}

static void checkOverallResources(XOSView *xv) {
  XWin *xw = (XWin *)xv;

  /*  Set 'off' value.  This is not necessarily a default value -- the value
   *  in the defaultXResourceString is the default value.  */
  xv->usedlabels = xv->legend = xv->caption = 0;

  xwin_setfont(xw);

  if (xwin_isresourcetrue(xw, "captions"))     /*  use captions  */
    xv->caption = 1;
  if (xwin_isresourcetrue(xw, "labels"))       /*  use labels  */
    xv->legend = 1;
  if (xwin_isresourcetrue(xw, "usedlabels"))   /*  use "free" labels  */
    xv->usedlabels = 1;
}

static void checkMeterResources(XOSView *xv) {
  int i;

  for (i = 0; i < xv->nummeters; i++)
    xv->meters[i]->checkres(xv->meters[i]);
}

static void dolegends(XOSView *xv) {
  int i;

  for (i = 0; i < xv->nummeters; i++) {
    xv->meters[i]->docaptions = xv->caption;
    xv->meters[i]->dolegends = xv->legend;
    xv->meters[i]->dousedlegends = xv->usedlabels;
  }
}

static void figureSize(XOSView *xv) {
  XWin *xw = (XWin *)xv;

  if (xv->legend) {
    if (!xv->usedlabels)
      xv->xoff = xwin_textwidth(xw, "XXXXXX");
    else
      xv->xoff = xwin_textwidth(xw, "XXXXXXXXXX");

    xv->yoff = xv->caption ? xwin_textheight(xw) + xwin_textheight(xw) / 4 : 0;
  }
  xwin_setwidth(xw, findx(xv));
  xwin_setheight(xw, findy(xv));
}

static int findx(XOSView *xv) {
  XWin *xw = (XWin *)xv;

  if (xv->legend) {
    if (!xv->usedlabels)
      return xwin_textwidth(xw, "XXXXXXXXXXXXXXXXXXXXXXXX");
    return xwin_textwidth(xw, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
  }
  return 80;
}

static int findy(XOSView *xv) {
  XWin *xw = (XWin *)xv;

  if (xv->legend)
    return 10 + xwin_textheight(xw) * xv->nummeters * (xv->caption ? 2 : 1);

  return 15 * xv->nummeters;
}

static const char *winname(XOSView *xv) {
  char host[100];
  static char name[120];  /*  We return a pointer to this, so it can't be
                              local.  */

  gethostname(host, 99);
  host[99] = 0;
  snprintf(name, sizeof name, "%s%s", NAME, host);
  /*  Allow overriding of this name through the -title option.  */
  return xwin_getresource_default((XWin *)xv, "title", name);
}

static void resizeMeters(XOSView *xv) {
  XWin *xw = (XWin *)xv;
  int spacing = xv->vspacing + 1;
  int topmargin = xv->vmargin;
  int rightmargin = xv->hmargin;
  int newwidth = xwin_width(xw) - xv->xoff - rightmargin;
  int newheight, i;

  newheight = (xwin_height(xw)
               - (topmargin + topmargin + (xv->nummeters - 1) * spacing
                  + xv->nummeters * xv->yoff)) / xv->nummeters;
  if (newheight < 2)
    newheight = 2;

  for (i = 0; i < xv->nummeters; i++) {
    int counter = i + 1;
    meter_resize(xv->meters[i], xv->xoff,
                 topmargin + counter * xv->yoff
                   + (counter - 1) * (newheight + spacing),
                 newwidth, newheight);
  }
}

void xosview_draw(XOSView *xv) {
  int i;

  if (xv->visibility == WV_OBSCURED) {
    XOSDEBUG("Skipping draw:  not visible.\n");
    return;
  }

  XOSDEBUG("Doing draw.\n");
  xwin_clear((XWin *)xv);

  for (i = 0; i < xv->nummeters; i++)
    xv->meters[i]->draw(xv->meters[i]);
}

/*---------------------------------------------------------------------------*/

void xosview_run(XOSView *xv) {
  XWin *xw = (XWin *)xv;

  while (!xwin_isdone(xw)) {
    int i;

    /*  Check for X11 events  */
    xwin_checkevent(xw);

    /*  Check if the window has been resized (at least once)  */
    if (xv->deferred_resize) {
      resizeMeters(xv);
      xv->deferred_resize = 0;
      xv->deferred_redraw = 1;
    }

    /*  redraw everything if needed  */
    if (xv->deferred_redraw) {
      xosview_draw(xv);
      xv->deferred_redraw = 0;
    }

    /*  Update the metrics & meters  */
    for (i = 0; i < xv->nummeters; i++)
      if (meter_requestevent(xv->meters[i]))
        xv->meters[i]->checkevent(xv->meters[i]);

    xwin_flush(xw);

    /*  First, sleep for the proper integral number of seconds -- usleep
     *  only deals with times less than 1 sec.  */
    if (xv->sleeptime)
      sleep((unsigned int)xv->sleeptime);
    if (xv->usleeptime)
      usleep((unsigned int)xv->usleeptime);
  }
}

/*---------------------------------------------------------------------------*/

static void keyPressEvent(XWin *xw, XEvent *event) {
  char c = 0;
  KeySym key;

  XLookupString(&event->xkey, &c, 1, &key, NULL);

  if ((c == 'q') || (c == 'Q'))
    xw->done = 1;
}

static void exposeEvent(XWin *xw, XEvent *event) {
  XOSView *xv = (XOSView *)xw;

  (void) event;
  xv->deferred_redraw = 1;
  XOSDEBUG("Got expose event.\n");
}

/*
 *  All window changes come in via XConfigureEvent (not XResizeRequestEvent)
 */
static void resizeEvent(XWin *xw, XEvent *event) {
  XOSView *xv = (XOSView *)xw;

  XOSDEBUG("Got configure event.\n");

  if (event->xconfigure.width == xw->width
      && event->xconfigure.height == xw->height)
    return;

  XOSDEBUG("Window has resized\n");

  xwin_setwidth(xw, event->xconfigure.width);
  xwin_setheight(xw, event->xconfigure.height);

  xv->deferred_resize = 1;
}

static void visibilityEvent(XWin *xw, XEvent *event) {
  XOSView *xv = (XOSView *)xw;
  int state = event->xvisibility.state;

  if (state == VisibilityPartiallyObscured) {
    if (xv->visibility != WV_FULLY_VISIBLE)
      xv->deferred_redraw = 1;
    xv->visibility = WV_PARTIALLY_VISIBLE;
  } else if (state == VisibilityFullyObscured) {
    xv->visibility = WV_OBSCURED;
    xv->deferred_redraw = 0;
  } else {
    if (xv->visibility != WV_FULLY_VISIBLE)
      xv->deferred_redraw = 1;
    xv->visibility = WV_FULLY_VISIBLE;
  }

  XOSDEBUG("Got visibility event: %s\n",
           (xv->visibility == WV_FULLY_VISIBLE) ? "Full"
             : (xv->visibility == WV_PARTIALLY_VISIBLE) ? "Partial"
             : "Obscured");
}

static void unmapEvent(XWin *xw, XEvent *event) {
  XOSView *xv = (XOSView *)xw;

  /*  unclutter creates a subwindow of our window if it hides the cursor,
   *  we get the unmap event if the cursor is moved again.  Don't treat it
   *  as main window unmap.  */
  if (event->xunmap.window == xw->window)
    xv->visibility = WV_OBSCURED;
}
