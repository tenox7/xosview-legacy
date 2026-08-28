#include "xwin.h"
#include "Xrm.h"
#include <X11/Xatom.h>
#ifndef NO_XPM
#include <X11/xpm.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static long eventMask(int type);
static void deleteEvent(XWin *xw, XEvent *event);
static void mappingNotify(XWin *xw, XEvent *event);
static void setColors(XWin *xw);
static void getGeometry(XWin *xw);
static void setHints(XWin *xw, int argc, char **argv);
static int getPixmap(XWin *xw, Pixmap *pixmap);
static void selectEvents(XWin *xw, long mask);
static Pixmap createPixmap(XWin *xw, const char *data, unsigned int w,
                           unsigned int h);

/*---------------------------------------------------------------------------*/

void xwin_init(XWin *xw, int argc, char **argv, char *geometry, Xrm *xrm) {
  (void) argc;
  (void) argv;
  /*  Eventually, we may want to have XWin handle some arguments other
   *  than resources, so argc and argv are left as parameters.  BCG  */

  xw->geometry = geometry;  /*  Save for later use.  */
  xw->width = xw->height = xw->x = xw->y = 0;
  xw->xrm = xrm;

  xw->name = NULL;
  xw->font = NULL;
  xw->done = 0;
  xw->dostippling = 0;

  /*  Set up the default Events  */
  xw->events = NULL;
  xwin_addevent(xw, ClientMessage, deleteEvent);
  xwin_addevent(xw, MappingNotify, mappingNotify);

  /*  xwin_opendisplay() is called explicitly in xosview.c.  */
}

void xwin_fini(XWin *xw) {
  /*  delete the events  */
  Event *event = xw->events;
  while (event != NULL) {
    Event *save = event->next;
    free(event);
    event = save;
  }
  xw->events = NULL;

  XFree(xw->title.value);
  XFree(xw->iconname.value);
  XFree(xw->sizehints);
  XFree(xw->wmhints);
  XFree(xw->classhints);
  XFreeGC(xw->display, xw->gc);
  XFreeFont(xw->display, xw->font);
  XDestroyWindow(xw->display, xw->window);
  /*  close the connection to the display  */
  XCloseDisplay(xw->display);
}

/*---------------------------------------------------------------------------*/

void xwin_create(XWin *xw, int argc, char **argv) {
  XGCValues gcv;
  XSetWindowAttributes xswa;
  Pixmap background_pixmap;
  Event *tmp;

  xwin_setfont(xw);
  setColors(xw);
  getGeometry(xw);

  xw->borderwidth = atoi(xwin_getresource_default(xw, "borderwidth", "1"));

  xw->window = XCreateSimpleWindow(xw->display, DefaultRootWindow(xw->display),
                                   xw->sizehints->x, xw->sizehints->y,
                                   xw->sizehints->width, xw->sizehints->height,
                                   xw->borderwidth, xw->fgcolor, xw->bgcolor);

  setHints(xw, argc, argv);

  /*  Finally, create a graphics context for the main window  */
  gcv.font = xw->font->fid;
  gcv.foreground = xw->fgcolor;
  gcv.background = xw->bgcolor;
  xw->gc = XCreateGC(xw->display, xw->window,
                     (GCFont | GCForeground | GCBackground), &gcv);

  /*  Set main window's attributes (colormap, bit_gravity)  */
  xswa.colormap = xw->colormap;
  xswa.bit_gravity = NorthWestGravity;
  XChangeWindowAttributes(xw->display, xw->window,
                          (CWColormap | CWBitGravity), &xswa);

  /*  If there is a pixmap file, set it as the background  */
  if (getPixmap(xw, &background_pixmap))
    XSetWindowBackgroundPixmap(xw->display, xw->window, background_pixmap);

  /*  add the events  */
  for (tmp = xw->events; tmp != NULL; tmp = tmp->next)
    selectEvents(xw, tmp->mask);

  /*  Map the main window  */
  xwin_map(xw);
  xwin_flush(xw);
  if (XGetWindowAttributes(xw->display, xw->window, &xw->attr) == 0) {
    fprintf(stderr, "Error getting attributes of Main.\n");
    exit(2);
  }

  /*  Create stipple pixmaps.  */
  xw->stipples[0] = createPixmap(xw, "\000\000", 2, 2);
  xw->stipples[1] = createPixmap(xw, "\002\000\001\000", 2, 4);
  xw->stipples[2] = createPixmap(xw, "\002\001", 2, 2);
  xw->stipples[3] = createPixmap(xw, "\002\003\001\003", 2, 4);
  xw->dostippling = xwin_isresourcetrue(xw, "enableStipple");
}

/*---------------------------------------------------------------------------*/

void xwin_setfont(XWin *xw) {
  const char *fontName;

  if (xw->font != NULL)
    return;

  fontName = xwin_getresource(xw, "font");
  if ((xw->font = XLoadQueryFont(xw->display, fontName)) == NULL) {
    fprintf(stderr, "%s: font \"%s\" does not exist; see xlsfonts(1)\n",
            xw->name ? xw->name : "xosview", fontName);
    exit(1);
  }
}

/*---------------------------------------------------------------------------*/

static void setHints(XWin *xw, int argc, char **argv) {
  /*  Set up class hint  */
  if ((xw->classhints = XAllocClassHint()) == NULL) {
    fprintf(stderr, "Error allocating class hint!\n");
    exit(1);
  }
  /*  We have to cast away the const's.  */
  xw->classhints->res_name = (char *)xrm_instancename(xw->xrm);
  xw->classhints->res_class = (char *)xrm_classname(xw->xrm);

  /*  Set up the window manager hints  */
  if ((xw->wmhints = XAllocWMHints()) == NULL) {
    fprintf(stderr, "Error allocating Window Manager hints!\n");
    exit(1);
  }
  xw->wmhints->flags = (InputHint | StateHint);
  xw->wmhints->input = True;
  xw->wmhints->initial_state = NormalState;

  /*  Set up XTextProperty for window name and icon name  */
  if (XStringListToTextProperty(&xw->name, 1, &xw->title) == 0) {
    fprintf(stderr, "Error creating XTextProperty!\n");
    exit(1);
  }
  if (XStringListToTextProperty(&xw->name, 1, &xw->iconname) == 0) {
    fprintf(stderr, "Error creating XTextProperty!\n");
    exit(1);
  }

  XSetWMProperties(xw->display, xw->window, &xw->title, &xw->iconname,
                   argv, argc, xw->sizehints, xw->wmhints, xw->classhints);

  /*  Set up the Atoms for delete messages  */
  xw->wm = XInternAtom(xw->display, "WM_PROTOCOLS", False);
  xw->wmdelete = XInternAtom(xw->display, "WM_DELETE_WINDOW", False);
  XChangeProperty(xw->display, xw->window, xw->wm, XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)(&xw->wmdelete), 1);
}

/*---------------------------------------------------------------------------*/

void xwin_opendisplay(XWin *xw) {
  /*  Open connection to display selected by user  */
  if ((xw->display = XOpenDisplay(xw->display_name)) == NULL) {
    fprintf(stderr, "Can't open display named %s\n", xw->display_name);
    exit(1);
  }

  xw->colormap = DefaultColormap(xw->display, DefaultScreen(xw->display));
}

void xwin_setdisplayname(XWin *xw, const char *name) {
  snprintf(xw->display_name, sizeof xw->display_name, "%s", name);
}

const char *xwin_displayname(const XWin *xw) {
  return xw->display_name;
}

/*---------------------------------------------------------------------------*/

static void setColors(XWin *xw) {
  XColor color;
  int screen = DefaultScreen(xw->display);

  /*  Main window's background color  */
  if (XParseColor(xw->display, xw->colormap,
                  xwin_getresource(xw, "background"), &color) == 0 ||
      XAllocColor(xw->display, xw->colormap, &color) == 0)
    xw->bgcolor = WhitePixel(xw->display, screen);
  else
    xw->bgcolor = color.pixel;

  /*  Main window's foreground color  */
  if (XParseColor(xw->display, xw->colormap,
                  xwin_getresource(xw, "foreground"), &color) == 0 ||
      XAllocColor(xw->display, xw->colormap, &color) == 0)
    xw->fgcolor = BlackPixel(xw->display, screen);
  else
    xw->fgcolor = color.pixel;
}

/*---------------------------------------------------------------------------*/

static int getPixmap(XWin *xw, Pixmap *pixmap) {
#ifdef NO_XPM
  /*  Built without libXpm, so background pixmaps are simply unavailable.  */
  (void) xw;
  (void) pixmap;
  return 0;
#else
  char *pixmap_file;
  XWindowAttributes root_att;
  XpmAttributes pixmap_att;

  pixmap_file = (char *)xwin_getresource_default(xw, "pixmapName", NULL);
  if (!pixmap_file)
    return 0;

  XGetWindowAttributes(xw->display, DefaultRootWindow(xw->display), &root_att);
  pixmap_att.closeness = 30000;
  pixmap_att.colormap = root_att.colormap;
  pixmap_att.valuemask = XpmSize | XpmReturnPixels | XpmColormap | XpmCloseness;

  if (XpmReadFileToPixmap(xw->display, DefaultRootWindow(xw->display),
                          pixmap_file, pixmap, NULL, &pixmap_att)) {
    fprintf(stderr, "Pixmap %s not found\n", pixmap_file);
    fprintf(stderr, "Defaulting to blank\n");
    return 0;
  }

  return 1;
#endif
}

/*---------------------------------------------------------------------------*/

static void getGeometry(XWin *xw) {
  char default_geometry[80];
  int bitmask;

  /*  Fill out a XSizeHints structure to inform the window manager
   *  of desired size and location of main window.  */
  if ((xw->sizehints = XAllocSizeHints()) == NULL) {
    fprintf(stderr, "Error allocating size hints!\n");
    exit(1);
  }
  xw->sizehints->flags = PSize;
  xw->sizehints->height = xw->height;
  xw->sizehints->min_height = xw->sizehints->height;
  xw->sizehints->width = xw->width;
  xw->sizehints->min_width = xw->sizehints->width;
  xw->sizehints->x = xw->x;
  xw->sizehints->y = xw->y;

  /*  Construct a default geometry string  */
  snprintf(default_geometry, sizeof default_geometry, "%dx%d+%d+%d",
           xw->sizehints->width, xw->sizehints->height,
           xw->sizehints->x, xw->sizehints->y);

  /*  Process the geometry specification  */
  bitmask = XGeometry(xw->display, DefaultScreen(xw->display),
                      xwin_getresource_default(xw, "geometry", xw->geometry),
                      default_geometry, 0, 1, 1, 0, 0,
                      &(xw->sizehints->x), &(xw->sizehints->y),
                      &(xw->sizehints->width), &(xw->sizehints->height));

  /*  Check bitmask and set flags in XSizeHints structure  */
  if (bitmask & (WidthValue | HeightValue)) {
    xw->sizehints->flags |= PPosition;
    xw->width = xw->sizehints->width;
    xw->height = xw->sizehints->height;
  }

  if (bitmask & (XValue | YValue)) {
    xw->sizehints->flags |= USPosition;
    xw->x = xw->sizehints->x;
    xw->y = xw->sizehints->y;
  }
}

/*---------------------------------------------------------------------------*/

static void selectEvents(XWin *xw, long mask) {
  XWindowAttributes xAttr;
  XSetWindowAttributes xSwAttr;

  if (XGetWindowAttributes(xw->display, xw->window, &xAttr) != 0) {
    xSwAttr.event_mask = xAttr.your_event_mask | mask;
    XChangeWindowAttributes(xw->display, xw->window, CWEventMask, &xSwAttr);
  }
}

/*---------------------------------------------------------------------------*/

void xwin_checkevent(XWin *xw) {
  XEvent event;

  while (XEventsQueued(xw->display, QueuedAfterReading)) {
    Event *tmp;

    XNextEvent(xw->display, &event);

    /*  call all of the Event's call back functions to process this event  */
    for (tmp = xw->events; tmp != NULL; tmp = tmp->next)
      if (event.type == tmp->type)
        tmp->callback(tmp->parent, &event);
  }
}

/*---------------------------------------------------------------------------*/

void xwin_addevent(XWin *xw, int type, EventCallBack callback) {
  Event *event = (Event *)malloc(sizeof(Event));

  if (event == NULL) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }
  event->parent = xw;
  event->type = type;
  event->callback = callback;
  event->mask = eventMask(type);
  event->next = NULL;

  if (xw->events == NULL) {
    xw->events = event;
  } else {
    Event *tmp = xw->events;
    while (tmp->next != NULL)
      tmp = tmp->next;
    tmp->next = event;
  }
}

/*---------------------------------------------------------------------------*/

const char *xwin_getresource_default(XWin *xw, const char *name,
                                     const char *defaultVal) {
  const char *retval = xrm_getresource(xw->xrm, name);
  return retval ? retval : defaultVal;
}

const char *xwin_getresource(XWin *xw, const char *name) {
  const char *retval = xrm_getresource(xw->xrm, name);

  if (retval)
    return retval;

  fprintf(stderr, "Error:  Couldn't find '%s' resource in the resource "
          "database!\n", name);
  exit(-1);
  /*  Some compilers aren't smart enough to know that exit() exits.  */
  return NULL;
}

int xwin_isresourcetrue(XWin *xw, const char *name) {
  return !strncasecmp(xwin_getresource(xw, name), "True", 5);
}

/*---------------------------------------------------------------------------*/

unsigned long xwin_alloccolor(XWin *xw, const char *name) {
  XColor exact, closest;
  XColor cells[256];
  int screen = DefaultScreen(xw->display);
  int ncells, i;
  unsigned long best;
  double bestdist;

  if (XAllocNamedColor(xw->display, xw->colormap, name, &closest, &exact))
    return closest.pixel;

  /*  Allocation fails on a colormap that is already full, which is the normal
   *  state of an 8 bit display running a desktop, so settle for the nearest
   *  colour that is in it rather than for an uninitialised pixel.  */
  if (!XParseColor(xw->display, xw->colormap, name, &exact)) {
    fprintf(stderr, "xwin_alloccolor() : no such color : %s\n", name);
    return BlackPixel(xw->display, screen);
  }

  ncells = DisplayCells(xw->display, screen);
  if (ncells > 256)
    ncells = 256;

  for (i = 0; i < ncells; i++)
    cells[i].pixel = i;
  XQueryColors(xw->display, xw->colormap, cells, ncells);

  best = BlackPixel(xw->display, screen);
  bestdist = -1.0;
  for (i = 0; i < ncells; i++) {
    double dr = (double)cells[i].red - (double)exact.red;
    double dg = (double)cells[i].green - (double)exact.green;
    double db = (double)cells[i].blue - (double)exact.blue;
    double dist = dr * dr + dg * dg + db * db;
    if (bestdist < 0.0 || dist < bestdist) {
      bestdist = dist;
      best = cells[i].pixel;
    }
  }

  return best;
}

/*---------------------------------------------------------------------------*/

static void deleteEvent(XWin *xw, XEvent *event) {
  if ((event->xclient.message_type == xw->wm) &&
      ((unsigned)event->xclient.data.l[0] == xw->wmdelete))
    xw->done = 1;
}

static void mappingNotify(XWin *xw, XEvent *event) {
  (void) xw;
  XRefreshKeyboardMapping(&event->xmapping);
}

/*---------------------------------------------------------------------------*/

static long eventMask(int type) {
  switch (type) {
  case ButtonPress:
    return ButtonPressMask;
  case ButtonRelease:
    return ButtonReleaseMask;
  case EnterNotify:
    return EnterWindowMask;
  case LeaveNotify:
    return LeaveWindowMask;
  case MotionNotify:
    return PointerMotionMask;
  case FocusIn:
  case FocusOut:
    return FocusChangeMask;
  case KeymapNotify:
    return KeymapStateMask;
  case KeyPress:
    return KeyPressMask;
  case KeyRelease:
    return KeyReleaseMask;
  case MapNotify:
  case SelectionClear:
  case SelectionNotify:
  case SelectionRequest:
  case ClientMessage:
  case MappingNotify:
    return NoEventMask;
  case Expose:
  case GraphicsExpose:
  case NoExpose:
    return ExposureMask;
  case ColormapNotify:
    return ColormapChangeMask;
  case PropertyNotify:
    return PropertyChangeMask;
  case UnmapNotify:
  case ReparentNotify:
  case GravityNotify:
  case DestroyNotify:
  case CirculateNotify:
  case ConfigureNotify:
    return StructureNotifyMask | SubstructureNotifyMask;
  case CreateNotify:
    return SubstructureNotifyMask;
  case VisibilityNotify:
    return VisibilityChangeMask;
  /*  The following are used by window managers  */
  case CirculateRequest:
  case ConfigureRequest:
  case MapRequest:
    return SubstructureRedirectMask;
  case ResizeRequest:
    return ResizeRedirectMask;
  default:
    fprintf(stderr, "xwin_addevent() : unknown event type : %d\n", type);
    return NoEventMask;
  }
}

/*---------------------------------------------------------------------------*/
/*  Thin wrappers over Xlib.  These were inline members in the C++ version.  */

static Pixmap createPixmap(XWin *xw, const char *data, unsigned int w,
                           unsigned int h) {
  return XCreatePixmapFromBitmapData(xw->display, xw->window, (char *)data,
                                     w, h, 0, 1, 1);
}

int xwin_width(const XWin *xw) { return xw->width; }
int xwin_height(const XWin *xw) { return xw->height; }
void xwin_setwidth(XWin *xw, int val) { xw->width = val; }
void xwin_setheight(XWin *xw, int val) { xw->height = val; }
int xwin_isdone(const XWin *xw) { return xw->done; }
void xwin_setdone(XWin *xw, int val) { xw->done = val; }

void xwin_settitle(XWin *xw, const char *str) {
  XStoreName(xw->display, xw->window, str);
}

void xwin_seticonname(XWin *xw, const char *str) {
  XSetIconName(xw->display, xw->window, str);
}

void xwin_clear(XWin *xw) {
  XClearWindow(xw->display, xw->window);
}

void xwin_cleararea(XWin *xw, int x, int y, int width, int height) {
  XClearArea(xw->display, xw->window, x, y, width, height, False);
}

void xwin_setforeground(XWin *xw, unsigned long pixelvalue) {
  XSetForeground(xw->display, xw->gc, pixelvalue);
}

void xwin_setbackground(XWin *xw, unsigned long pixelvalue) {
  XSetBackground(xw->display, xw->gc, pixelvalue);
}

unsigned long xwin_foreground(const XWin *xw) { return xw->fgcolor; }
unsigned long xwin_background(const XWin *xw) { return xw->bgcolor; }

void xwin_setstipplen(XWin *xw, int n) {
  XGCValues xgcv;

  if (!xw->dostippling)
    return;
  XSetStipple(xw->display, xw->gc, xw->stipples[n]);
  xgcv.fill_style = FillOpaqueStippled;
  XChangeGC(xw->display, xw->gc, GCFillStyle, &xgcv);
}

void xwin_resizewindow(XWin *xw, int width, int height) {
  XResizeWindow(xw->display, xw->window, width, height);
}

void xwin_linewidth(XWin *xw, int width) {
  XGCValues xgcv;

  xgcv.line_width = width;
  XChangeGC(xw->display, xw->gc, GCLineWidth, &xgcv);
}

void xwin_drawline(XWin *xw, int x1, int y1, int x2, int y2) {
  XDrawLine(xw->display, xw->window, xw->gc, x1, y1, x2, y2);
}

void xwin_drawrectangle(XWin *xw, int x, int y, int width, int height) {
  XDrawRectangle(xw->display, xw->window, xw->gc, x, y, width, height);
}

void xwin_drawfilledrectangle(XWin *xw, int x, int y, int width, int height) {
  XFillRectangle(xw->display, xw->window, xw->gc, x, y, width + 1, height + 1);
}

void xwin_drawstring(XWin *xw, int x, int y, const char *str) {
  XDrawString(xw->display, xw->window, xw->gc, x, y, str, strlen(str));
}

void xwin_copyarea(XWin *xw, int src_x, int src_y, int width, int height,
                   int dest_x, int dest_y) {
  XCopyArea(xw->display, xw->window, xw->window, xw->gc, src_x, src_y,
            width, height, dest_x, dest_y);
}

int xwin_textwidth_n(XWin *xw, const char *str, int n) {
  return XTextWidth(xw->font, str, n);
}

int xwin_textwidth(XWin *xw, const char *str) {
  return xwin_textwidth_n(xw, str, strlen(str));
}

int xwin_textascent(const XWin *xw) { return xw->font->ascent; }
int xwin_textdescent(const XWin *xw) { return xw->font->descent; }
int xwin_textheight(const XWin *xw) {
  return xwin_textascent(xw) + xwin_textdescent(xw);
}

void xwin_map(XWin *xw) { XMapWindow(xw->display, xw->window); }
void xwin_unmap(XWin *xw) { XUnmapWindow(xw->display, xw->window); }
void xwin_flush(XWin *xw) { XFlush(xw->display); }
