/*
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  Read temperature entries from /proc/sys/dev/sensors/<chip>/<file>
 *  and display actual and high temperature.
 *  If actual >= high, actual temp changes color to indicate alarm.
 *
 *  File based on btrymeter.* by
 *  Copyright (c) 1997 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "lmstemp.h"
#include "xosview.h"
#include "xwin.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char PROC_SENSORS[] = "/proc/sys/dev/sensors";
static const char SYS_SENSORS[] = "/sys/class/hwmon";

static int isfile(const char *path) {
  struct stat buf;

  return stat(path, &buf) == 0 && S_ISREG(buf.st_mode);
}

static int isdir(const char *path) {
  struct stat buf;

  return stat(path, &buf) == 0 && S_ISDIR(buf.st_mode);
}

/*
 *  Logic:
 *   0) tempfile must always be found, highfile and lowfile only if given
 *   1) absolute path in any filename must match as is
 *   2) if name is given, only files in that sysfs node can match
 *   3) any filename matches as is
 *   4) tempfile + "_input" matches and tempfile + "_max" and/or
 *      tempfile + "_min" matches
 */
static int checksensors(LmsTemp *lt, const char *name, const char *tempfile,
                        const char *highfile, const char *lowfile) {
  const char *title = lt->s.f.m.title;
  DIR *dir;
  struct dirent *ent;
  char dirname[LMS_PATH_SIZE], f[LMS_PATH_SIZE], f2[LMS_PATH_SIZE];

  /*  First, check if absolute paths were given.  */
  if (tempfile[0] == '/') {
    if (isfile(tempfile)) {
      snprintf(lt->tempfile, sizeof lt->tempfile, "%s", tempfile);
      lt->temp_found = 1;
    } else {
      fprintf(stderr, "%s : Could not find file %s.\n", title, tempfile);
    }
  }
  if (highfile && highfile[0] == '/') {
    if (isfile(highfile)) {
      snprintf(lt->highfile, sizeof lt->highfile, "%s", highfile);
      lt->high_found = 1;
    } else {
      fprintf(stderr, "%s : Could not find file %s.\n", title, highfile);
    }
  }
  if (lowfile && lowfile[0] == '/') {
    if (isfile(lowfile)) {
      snprintf(lt->lowfile, sizeof lt->lowfile, "%s", lowfile);
      lt->low_found = 1;
    } else {
      fprintf(stderr, "%s : Could not find file %s.\n", title, lowfile);
    }
  }

  if (lt->temp_found && (lt->high_found || !highfile)
      && (lt->low_found || !lowfile)) {
    lt->isproc = strncmp(lt->tempfile, "/proc", 5) ? 0 : 1;
    return 1;
  }

  /*  Then, try to find the given file.  Try /proc first.  */
  if ((dir = opendir(PROC_SENSORS))) {
    while (!lt->temp_found && (ent = readdir(dir))) {
      if (!strncmp(ent->d_name, ".", 1) || !strncmp(ent->d_name, "..", 2))
        continue;

      snprintf(dirname, sizeof dirname, "%s/%s", PROC_SENSORS, ent->d_name);
      if (isdir(dirname)) {
        snprintf(f, sizeof f, "%s/%s", dirname, tempfile);
        if (isfile(f)) {
          lt->temp_found = 1;
          snprintf(lt->tempfile, sizeof lt->tempfile, "%s", f);
          lt->isproc = 1;
        }
      }
    }
    closedir(dir);
    if (lt->temp_found)
      return 1;
  }

  /*  Next, try /sys.  */
  if (!(dir = opendir(SYS_SENSORS)))
    return 0;

  while (!(lt->temp_found && (lt->high_found || !highfile)
           && (lt->low_found || !lowfile)) && (ent = readdir(dir))) {
    int i = 0;

    if (!strncmp(ent->d_name, ".", 1) || !strncmp(ent->d_name, "..", 2))
      continue;

    /*  Try every node under /sys/class/hwmon  */
    snprintf(dirname, sizeof dirname, "%s/%s", SYS_SENSORS, ent->d_name);

    do {
      if (isdir(dirname)) {
        char n[64];
        FILE *nf;

        /*  Try to get the sensor's name  */
        n[0] = '\0';
        snprintf(f, sizeof f, "%s/name", dirname);
        nf = fopen(f, "r");
        if (nf) {
          if (fscanf(nf, "%63s", n) != 1)
            n[0] = '\0';
          fclose(nf);
        }
        if (!name || strcmp(n, name) == 0) {
          /*  Either no name was given, or the name matches.  Check if the
           *  files exist here.  */
          lt->name_found = 1;
          if (highfile && !lt->high_found) {
            snprintf(f, sizeof f, "%s/%s", dirname, highfile);
            if (isfile(f)) {
              lt->high_found = 1;
              snprintf(lt->highfile, sizeof lt->highfile, "%s", f);
            }
          }
          if (lowfile && !lt->low_found) {
            snprintf(f, sizeof f, "%s/%s", dirname, lowfile);
            if (isfile(f)) {
              lt->low_found = 1;
              snprintf(lt->lowfile, sizeof lt->lowfile, "%s", f);
            }
          }
          snprintf(f, sizeof f, "%s/%s", dirname, tempfile);
          if (isfile(f)) {
            lt->temp_found = 1;
            snprintf(lt->tempfile, sizeof lt->tempfile, "%s", f);
          } else {
            snprintf(f2, sizeof f2, "%s_input", f);
            if (isfile(f2)) {
              lt->temp_found = 1;
              snprintf(lt->tempfile, sizeof lt->tempfile, "%s", f2);
              snprintf(f2, sizeof f2, "%s_max", f);
              if (!lt->high_found && !highfile && isfile(f2)) {
                lt->high_found = 1;
                snprintf(lt->highfile, sizeof lt->highfile, "%s", f2);
              }
              snprintf(f2, sizeof f2, "%s_min", f);
              if (!lt->low_found && !lowfile && isfile(f2)) {
                lt->low_found = 1;
                snprintf(lt->lowfile, sizeof lt->lowfile, "%s", f2);
              }
            }
          }
        }
      }

      /*  Some /sys sensors have the files in subdirectory /device  */
      strncat(dirname, "/device", sizeof dirname - strlen(dirname) - 1);
    } while (++i < 2 && !(lt->temp_found && (lt->high_found || !highfile)
                          && (lt->low_found || !lowfile)));
  }
  closedir(dir);

  /*  No highfile or lowfile is OK.  */
  if (!highfile)
    lt->high_found = 1;
  if (!lowfile)
    lt->low_found = 1;

  return lt->temp_found & lt->high_found & lt->low_found;
}

/*  Adapted from libsensors.  */
static void determineScale(LmsTemp *lt) {
  char type[16], subtype[32];
  const char *slash = strrchr(lt->tempfile, '/');
  const char *basename = slash ? slash + 1 : lt->tempfile;
  int n;

  if (sscanf(basename, "%15[a-z]%d_%31s", type, &n, subtype) == 3) {
    if ((!strncmp(type, "in", strlen(type)) &&
         !strncmp(subtype, "input", strlen(subtype))) ||
        (!strncmp(type, "temp", strlen(type)) &&
         !strncmp(subtype, "input", strlen(subtype))) ||
        (!strncmp(type, "temp", strlen(type)) &&
         !strncmp(subtype, "offset", strlen(subtype))) ||
        (!strncmp(type, "curr", strlen(type)) &&
         !strncmp(subtype, "input", strlen(subtype))) ||
        (!strncmp(type, "power", strlen(type)) &&
         !strncmp(subtype, "average_interval", strlen(subtype))) ||
        (!strncmp(type, "cpu", strlen(type)) &&
         !strncmp(subtype, "vid", strlen(subtype)))) {
      lt->scale = 1000.0;
      return;
    }
    if ((!strncmp(type, "power", strlen(type)) &&
         !strncmp(subtype, "average", strlen(subtype))) ||
        (!strncmp(type, "energy", strlen(type)) &&
         !strncmp(subtype, "input", strlen(subtype)))) {
      lt->scale = 1000000.0;
      return;
    }
  }
  lt->scale = 1.0;
}

static void determineUnit(LmsTemp *lt) {
  char type[16], subtype[32];
  const char *slash = strrchr(lt->tempfile, '/');
  const char *basename = slash ? slash + 1 : lt->tempfile;
  int n;

  if (sscanf(basename, "%15[a-z]%d_%31s", type, &n, subtype) == 3) {
    if (!strncmp(type, "temp", strlen(type)))
      strcpy(lt->s.unit, "\260C");
    else if (!strncmp(type, "in", strlen(type)) ||
             !strncmp(subtype, "vid", strlen(type)))
      strcpy(lt->s.unit, "V");
    else if (!strncmp(type, "fan", strlen(type)))
      strcpy(lt->s.unit, "RPM");
    else if (!strncmp(type, "power", strlen(type))) {
      if (strncmp(subtype, "average_interval", strlen(type)))
        strcpy(lt->s.unit, "s");
      else
        strcpy(lt->s.unit, "W");
    } else if (!strncmp(type, "energy", strlen(type)))
      strcpy(lt->s.unit, "J");
    else if (!strncmp(type, "curr", strlen(type)))
      strcpy(lt->s.unit, "A");
    else if (!strncmp(type, "humidity", strlen(type)))
      strcpy(lt->s.unit, "%");
  }
}

static void checkres(Meter *m) {
  LmsTemp *lt = (LmsTemp *)m;
  SensorFieldMeter *sm = &lt->s;
  FieldMeter *fm = &sm->f;
  char s[32];
  const char *tmp;

  fieldmeter_checkresources(m);

  sm->actcolor = xwin_alloccolor(m->xw,
                                 xwin_getresource(m->xw, "lmstempActColor"));
  sm->highcolor = xwin_alloccolor(m->xw,
                                  xwin_getresource(m->xw,
                                                   "lmstempHighColor"));
  sm->lowcolor = xwin_alloccolor(m->xw,
                                 xwin_getresource(m->xw, "lmstempLowColor"));
  fieldmeter_setcolor(fm, 0, sm->actcolor);
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "lmstempIdleColor"));
  fieldmeter_setcolor(fm, 2, sm->highcolor);

  tmp = xwin_getresource_default(m->xw, "lmstempHighest", "0");
  snprintf(s, sizeof s, "lmstempHighest%u", lt->nbr);
  fm->total = fabs(atof(xwin_getresource_default(m->xw, s, tmp)));
  m->priority = atoi(xwin_getresource(m->xw, "lmstempPriority"));
  tmp = xwin_getresource(m->xw, "lmstempUsedFormat");
  snprintf(s, sizeof s, "lmstempUsedFormat%u", lt->nbr);
  fieldmeter_setusedformat(fm, xwin_getresource_default(m->xw, s, tmp));

  if (lt->highfile[0])
    sm->has_high = 1;
  if (lt->lowfile[0])
    sm->has_low = 1;

  if (!sm->has_high)
    sm->high = fm->total;
  if (!sm->has_low)
    sm->low = 0;

  determineScale(lt);
  determineUnit(lt);
  sensorfieldmeter_updatelegend(sm);
}

/*  Read one whitespace separated double out of a file.  */
static int readvalue(Meter *m, const char *path, double *out) {
  FILE *f = fopen(path, "r");

  if (!f) {
    fprintf(stderr, "Can not open file : %s\n", path);
    xwin_setdone(m->xw, 1);
    return 0;
  }
  if (fscanf(f, "%lf", out) != 1) {
    fclose(f);
    return 0;
  }
  fclose(f);
  return 1;
}

static void getlmstemp(LmsTemp *lt) {
  SensorFieldMeter *sm = &lt->s;
  FieldMeter *fm = &sm->f;
  Meter *m = &fm->m;
  double high = sm->high, low = sm->low;

  if (lt->isproc) {
    FILE *f = fopen(lt->tempfile, "r");

    if (!f) {
      fprintf(stderr, "Can not open file : %s\n", lt->tempfile);
      xwin_setdone(m->xw, 1);
      return;
    }
    if (fscanf(f, "%lf %lf %lf", &high, &low, &fm->fields[0]) != 3) {
      fclose(f);
      return;
    }
    fclose(f);
  } else {
    if (!readvalue(m, lt->tempfile, &fm->fields[0]))
      return;
    fm->fields[0] /= lt->scale;
    if (lt->highfile[0]) {
      if (!readvalue(m, lt->highfile, &high))
        return;
      high /= lt->scale;
    }
    if (lt->lowfile[0]) {
      if (!readvalue(m, lt->lowfile, &low))
        return;
      low /= lt->scale;
    }
  }

  sensorfieldmeter_checkfields(sm, low, high);
}

static void checkevent(Meter *m) {
  getlmstemp((LmsTemp *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *lmstemp_new(XOSView *parent, const char *name, const char *tempfile,
                   const char *highfile, const char *lowfile,
                   const char *label, const char *caption, unsigned int nbr) {
  LmsTemp *lt = (LmsTemp *)meter_alloc(sizeof *lt);
  SensorFieldMeter *sm = &lt->s;

  sensorfieldmeter_init(sm, parent, "LmsTemp", label, caption, 1, 1, 0);
  sm->f.m.checkres = checkres;
  sm->f.m.checkevent = checkevent;

  lt->nbr = nbr;
  lt->scale = 1.0;
  lt->isproc = lt->name_found = lt->temp_found = 0;
  lt->high_found = lt->low_found = 0;
  lt->tempfile[0] = lt->highfile[0] = lt->lowfile[0] = '\0';

  /*  Check if high is given as a value  */
  if (highfile && sscanf(highfile, "%lf", &sm->high) > 0) {
    sm->has_high = lt->high_found = 1;
    highfile = NULL;
  }
  /*  Check if low is given as a value  */
  if (lowfile && sscanf(lowfile, "%lf", &sm->low) > 0) {
    sm->has_low = lt->low_found = 1;
    lowfile = NULL;
  }

  if (!checksensors(lt, name, tempfile, highfile, lowfile)) {
    if (!lt->name_found &&
        ((!lt->temp_found && tempfile[0] != '/') ||
         (!lt->high_found && (highfile && highfile[0] != '/')) ||
         (!lt->low_found && (lowfile && lowfile[0] != '/')))) {
      fprintf(stderr, "%s : No sensor named %s was found in %s.\n", label,
              name ? name : "(none)", SYS_SENSORS);
    } else {
      if (!lt->temp_found && tempfile[0] != '/') {
        fprintf(stderr, "%s : Could not find file %s{,_input}", label,
                tempfile);
        if (name)
          fprintf(stderr, " under %s in %s", name, SYS_SENSORS);
        else
          fprintf(stderr, " under %s or %s", PROC_SENSORS, SYS_SENSORS);
        fprintf(stderr, ".\n");
      }
      if (!lt->high_found && highfile && highfile[0] != '/') {
        fprintf(stderr, "%s : Could not find file %s", label, highfile);
        if (name)
          fprintf(stderr, " under %s in %s", name, SYS_SENSORS);
        else
          fprintf(stderr, " under %s", SYS_SENSORS);
        fprintf(stderr, ".\n");
      }
      if (!lt->low_found && lowfile && lowfile[0] != '/') {
        fprintf(stderr, "%s : Could not find file %s", label, lowfile);
        if (name)
          fprintf(stderr, " under %s in %s", name, SYS_SENSORS);
        else
          fprintf(stderr, " under %s", SYS_SENSORS);
        fprintf(stderr, ".\n");
      }
    }
    xwin_setdone((XWin *)parent, 1);
  }

  return &sm->f.m;
}
