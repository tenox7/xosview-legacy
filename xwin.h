#ifndef _XWIN_H_
#define _XWIN_H_

#include "fwd.h"
#include "Xrm.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef void (*EventCallBack)(XWin *xw, XEvent *event);

typedef struct Event {
  XWin *parent;
  EventCallBack callback;
  int type;
  long mask;
  struct Event *next;
} Event;

struct XWin {
  int borderwidth;              /*  width of border  */
  int x, y;                     /*  position of the window  */
  int width, height;            /*  width and height of the window  */
  Display *display;             /*  Connection to X display  */
  Window window;                /*  Application's main window  */
  GC gc;                        /*  The graphics context for the window  */
  XFontStruct *font;            /*  Info on the default font  */
  char *name;                   /*  Application's name  */
  XTextProperty title;          /*  Window name for title bar  */
  XTextProperty iconname;       /*  Icon name for icon label  */
  unsigned long fgcolor;        /*  Foreground color of the window  */
  unsigned long bgcolor;        /*  Background color of the window  */
  XWindowAttributes attr;       /*  Attributes of the window  */
  XWMHints *wmhints;            /*  Hints for the window manager  */
  XSizeHints *sizehints;        /*  Size hints for window manager  */
  XClassHint *classhints;       /*  Class hint for window manager  */
  Event *events;                /*  List of Events for this window  */
  int done;                     /*  If true the application is finished.  */
  Atom wm, wmdelete;            /*  Used to handle delete Events  */
  Colormap colormap;            /*  The colormap  */
  char display_name[256];       /*  Display name string.  */
  char *geometry;               /*  geometry string.  */
  Xrm *xrm;                     /*  Pointer to the XOSView xrm.  FIXME???  */
  int dostippling;              /*  Either 0 or 1.  */
  Pixmap stipples[4];           /*  Array of Stipple masks.  */
};

void xwin_init(XWin *xw, int argc, char **argv, char *geometry, Xrm *xrm);
void xwin_create(XWin *xw, int argc, char **argv);
void xwin_fini(XWin *xw);

void xwin_opendisplay(XWin *xw);
void xwin_setdisplayname(XWin *xw, const char *name);
const char *xwin_displayname(const XWin *xw);
void xwin_setfont(XWin *xw);

void xwin_addevent(XWin *xw, int type, EventCallBack callback);
void xwin_checkevent(XWin *xw);

const char *xwin_getresource(XWin *xw, const char *name);
const char *xwin_getresource_default(XWin *xw, const char *name,
                                     const char *defaultVal);
int xwin_isresourcetrue(XWin *xw, const char *name);

unsigned long xwin_alloccolor(XWin *xw, const char *name);

int xwin_width(const XWin *xw);
int xwin_height(const XWin *xw);
void xwin_setwidth(XWin *xw, int val);
void xwin_setheight(XWin *xw, int val);
int xwin_isdone(const XWin *xw);
void xwin_setdone(XWin *xw, int val);

void xwin_settitle(XWin *xw, const char *str);
void xwin_seticonname(XWin *xw, const char *str);

void xwin_clear(XWin *xw);
void xwin_cleararea(XWin *xw, int x, int y, int width, int height);
void xwin_setforeground(XWin *xw, unsigned long pixelvalue);
void xwin_setbackground(XWin *xw, unsigned long pixelvalue);
unsigned long xwin_foreground(const XWin *xw);
unsigned long xwin_background(const XWin *xw);
void xwin_setstipplen(XWin *xw, int n);
void xwin_resizewindow(XWin *xw, int width, int height);
void xwin_linewidth(XWin *xw, int width);
void xwin_drawline(XWin *xw, int x1, int y1, int x2, int y2);
void xwin_drawrectangle(XWin *xw, int x, int y, int width, int height);
void xwin_drawfilledrectangle(XWin *xw, int x, int y, int width, int height);
void xwin_drawstring(XWin *xw, int x, int y, const char *str);
void xwin_copyarea(XWin *xw, int src_x, int src_y, int width, int height,
                   int dest_x, int dest_y);
int xwin_textwidth_n(XWin *xw, const char *str, int n);
int xwin_textwidth(XWin *xw, const char *str);
int xwin_textascent(const XWin *xw);
int xwin_textdescent(const XWin *xw);
int xwin_textheight(const XWin *xw);

void xwin_map(XWin *xw);
void xwin_unmap(XWin *xw);
void xwin_flush(XWin *xw);

#endif
