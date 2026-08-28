/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _Xrm_h
#define _Xrm_h

#include <X11/Xlib.h>
#include <X11/Xresource.h>

typedef struct {
  XrmDatabase db;
  XrmClass xclass, instance;
  const char *display_name;  /*  Used solely for getting the display's
                                 resources.  */
} Xrm;

void xrm_init(Xrm *xrm, const char *className, const char *instanceName);
void xrm_fini(Xrm *xrm);

const char *xrm_classname(const Xrm *xrm);
const char *xrm_instancename(const Xrm *xrm);
const char *xrm_getresource(const Xrm *xrm, const char *rname);
const char *xrm_getdisplayname(Xrm *xrm, int argc, char **argv);
void xrm_loadandmerge(Xrm *xrm, int *argc, char **argv, Display *display);

#endif
