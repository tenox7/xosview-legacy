/*
 *  Copyright (c) 1994, 1995, 2006, 2008 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "Xrm.h"
#include "Xrmcommandline.h"
#include "stringutils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>  /*  for access(), etc.  BCG  */

extern const char *defaultXResourceString;

static int initialized = 0;

static void initClassName(Xrm *xrm, const char *name) {
  char className[256];

  snprintf(className, sizeof className, "%s", name);  /*  Avoid evil people
                                                          out there...  */
  className[0] = toupper(className[0]);
  if (className[0] == 'X')
    className[1] = toupper(className[1]);

  xrm->xclass = XrmStringToQuark(className);
}

void xrm_init(Xrm *xrm, const char *className, const char *instanceName) {
  XrmInitialize();

  xrm->db = NULL;
  xrm->xclass = xrm->instance = NULLQUARK;
  xrm->display_name = "";

  xrm->instance = XrmStringToQuark(instanceName);
  initClassName(xrm, className);
}

void xrm_fini(Xrm *xrm) {
  XrmDestroyDatabase(xrm->db);
  xrm->db = NULL;
}

const char *xrm_classname(const Xrm *xrm) {
  return XrmQuarkToString(xrm->xclass);
}

const char *xrm_instancename(const Xrm *xrm) {
  return XrmQuarkToString(xrm->instance);
}

const char *xrm_getdisplayname(Xrm *xrm, int argc, char **argv) {
  /*  See if '-display foo:0' is on the command line, and return it if
   *  it is.  */
  char **argp;

  (void) argc;

  for (argp = argv; (*argp != NULL) && strncasecmp(*argp, "-display", 9);
       argp++)
    ;  /*  Don't do anything.  */

  /*  If we found -display and the next word exists...  */
  if (*argp && *(++argp))
    xrm->display_name = *argp;
  else
    xrm->display_name = "";

  /*  An empty display string means use the DISPLAY environment variable.  */
  return xrm->display_name;
}

const char *xrm_getresource(const Xrm *xrm, const char *rname) {
  char frn[1024], fcn[1024];
  XrmValue val;
  char *type;

  snprintf(frn, sizeof frn, "%s.%s", xrm_instancename(xrm), rname);
  snprintf(fcn, sizeof fcn, "%s.%s", xrm_classname(xrm), rname);

  val.addr = NULL;
  XrmGetResource(xrm->db, frn, fcn, &type, &val);

  /*  This case here is a hack, because we are currently moving from
   *  always making the instance name be "xosview" to allowing
   *  user-specified ones.  And unfortunately, the class name is
   *  XOsview, and not xosview, so our old defaults (xosview.font)
   *  will not be found when searching for XOsview.font.
   *  bgrayson Dec. 1996  */
  if (!val.addr) {
    /*  Let's try with a non-uppercased class name.  */
    char fcn_lower[1024];
    char *p;

    snprintf(fcn_lower, sizeof fcn_lower, "%s", xrm_classname(xrm));
    for (p = fcn_lower; p && *p; p++)
      *p = tolower(*p);

    snprintf_or_abort(fcn, sizeof fcn, "%s.%s", fcn_lower, rname);
    XrmGetResource(xrm->db, frn, fcn, &type, &val);
  }

  return val.addr;
}

/*  Merge one app-defaults file, named by a printf format taking the class
 *  name, if the name fits in the buffer.  */
static void mergeAppDefaults(Xrm *xrm, const char *dir) {
  char rfilename[2048];
  int result = snprintf(rfilename, sizeof rfilename, "%s/%s", dir,
                        XrmQuarkToString(xrm->xclass));

  if (result >= 0 && (size_t)result < sizeof rfilename)
    XrmCombineFileDatabase(rfilename, &xrm->db, 1);
}

/*---------------------------------------------------------------------
 *  This function uses XrmParseCommand, and updates argc and argv through it.
 */
void xrm_loadandmerge(Xrm *xrm, int *argc, char **argv, Display *display) {
  char *xappdir, *displayString, *screenString, *home, *xenvfile;
  XrmDatabase cmdlineRdb = NULL;

  /*  init the database if it needs it  */
  if (!initialized) {
    XrmInitialize();
    initialized = 1;
  } else {
    fprintf(stderr, "Error:  xrm_loadandmerge() called twice!\n");
    exit(-1);
  }

  /*  This is ugly code.  According to X and Xt rules, many files need
   *  to be checked for resource settings.  Since we aren't (yet) using
   *  Xt or any other package, we need to do all of these checks
   *  individually.  BCG
   *
   *  This all needs to be done in the proper order, listed from weakest
   *  to strongest:
   *    (from code-builtin-resources) (stored in defaultstring.c)
   *    app-defaults
   *    XOSView (from XAPPLRESDIR directory)
   *    from RESOURCE_MANAGER property on server (reads .Xdefaults if needed)
   *    from file specified in XENVIRONMENT
   *    from command line (i.e., handled with XrmParseCommand)
   */

  /*  Put the default, compile-time options as the lowest priority.  */
  xrm->db = XrmGetStringDatabase(defaultXResourceString);

  /*  Merge in the system resource database.  */
  mergeAppDefaults(xrm, "/etc/X11/app-defaults");
  mergeAppDefaults(xrm, "/usr/lib/X11/app-defaults");
  mergeAppDefaults(xrm, "/usr/X11R6/lib/X11/app-defaults");
  mergeAppDefaults(xrm, "/usr/share/X11/app-defaults");
  /*  Try a few more, for SunOS/Solaris folks.  */
  mergeAppDefaults(xrm, "/usr/openwin/lib/X11/app-defaults");
  mergeAppDefaults(xrm, "/usr/local/X11R6/lib/X11/app-defaults");

  /*  Now, check for an XOSView file in the XAPPLRESDIR directory...  */
  xappdir = getenv("XAPPLRESDIR");
  if (xappdir != NULL) {
    char xappfile[1024];
    snprintf(xappfile, sizeof xappfile, "%s/%s", xappdir,
             xrm_classname(xrm));
    /*  X_OK did not work for XAPPLRESDIR.  */
    if (!access(xappfile, R_OK))
      XrmCombineFileDatabase(xappfile, &xrm->db, 1);
  }

  /*  Now, check the display's RESOURCE_MANAGER property...  */
  displayString = XResourceManagerString(display);
  if (displayString != NULL) {
    XrmDatabase displayrdb = XrmGetStringDatabase(displayString);
    XrmMergeDatabases(displayrdb, &xrm->db);  /*  Destroys displayrdb.  */
  }

  /*  And check this screen of the display...  */
  screenString = XScreenResourceString(DefaultScreenOfDisplay(display));
  if (screenString != NULL) {
    XrmDatabase screenrdb = XrmGetStringDatabase(screenString);
    XrmMergeDatabases(screenrdb, &xrm->db);   /*  Destroys screenrdb.  */
  }

  /*  Now, check for a user resource file, and merge it in if there is
   *  one...  */
  home = getenv("HOME");
  if (home != NULL) {
    char userrfilename[1024];
    snprintf(userrfilename, sizeof userrfilename, "%s/.Xdefaults", home);
    /*  User file overrides system (db).  */
    XrmCombineFileDatabase(userrfilename, &xrm->db, 1);
  }

  /*  Second-to-last, parse any resource file specified in the
   *  environment variable XENVIRONMENT.  */
  xenvfile = getenv("XENVIRONMENT");
  if (xenvfile != NULL) {
    /*  The XENVIRONMENT file overrides all of the above.  */
    XrmCombineFileDatabase(xenvfile, &xrm->db, 1);
  }

  /*  Command-line resources override system and user defaults.  */
  XrmParseCommand(&cmdlineRdb, options, NUM_OPTIONS, xrm_instancename(xrm),
                  argc, argv);
  XrmCombineDatabase(cmdlineRdb, &xrm->db, 1);  /*  Keeps cmdlineRdb around. */
}
