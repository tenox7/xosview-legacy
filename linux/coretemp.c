/*
 *  Copyright (c) 2008-2014 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  Read CPU temperature readings from /sys and display actual temperature.
 *  If actual >= high, actual temp changes color to indicate alarm.
 *
 *  File based on linux/lmstemp.* by
 *  Copyright (c) 2000, 2006 by Leopold Toetsch <lt@toetsch.at>
 *
 *  This file may be distributed under terms of the GPL
 */

#include "coretemp.h"
#include "stringutils.h"
#include "xosview.h"
#include "xwin.h"
#include <dirent.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PATH_SIZE CORETEMP_PATH_SIZE

static const char SYS_HWMON[] = "/sys/class/hwmon";
static const char SYS_CORETEMP[] = "/sys/devices/platform/coretemp";
static const char SYS_VIATEMP[] = "/sys/devices/platform/via_cputemp";

static void paths_push(CoreTempPaths *p, const char *path) {
  if (p->n == p->cap) {
    unsigned int cap = p->cap ? p->cap * 2 : 8;
    char (*grown)[PATH_SIZE] = (char (*)[PATH_SIZE])realloc(p->path,
                                                            cap * PATH_SIZE);

    if (grown == NULL) {
      fprintf(stderr, "Out of memory.\n");
      exit(1);
    }
    p->path = grown;
    p->cap = cap;
  }
  snprintf(p->path[p->n], PATH_SIZE, "%s", path);
  p->n++;
}

/*  Read one whitespace separated word out of a file.  Returns 0 if the file
 *  can not be read.  */
static int readword(const char *path, char *out, size_t size) {
  FILE *f = fopen(path, "r");
  char fmt[16];
  int n;

  if (!f)
    return 0;
  snprintf(fmt, sizeof fmt, "%%%lus", (unsigned long)(size - 1));
  n = fscanf(f, fmt, out);
  fclose(f);
  return n == 1;
}

static int readdouble(const char *path, double *out) {
  FILE *f = fopen(path, "r");
  int n;

  if (!f)
    return 0;
  n = fscanf(f, "%lf", out);
  fclose(f);
  return n == 1;
}

static int isfile(const char *path) {
  struct stat buf;

  return stat(path, &buf) == 0 && S_ISREG(buf.st_mode);
}

/*  Replace the trailing "_<something>" of a sysfs temp path with suffix.  */
static void retarget(char *path, size_t size, const char *src,
                     const char *suffix) {
  char *underscore;

  snprintf(path, size, "%s", src);
  underscore = strrchr(path, '_');
  if (underscore)
    snprintf(underscore, size - (underscore - path), "%s", suffix);
}

/*  Find absolute paths of files to be used.  */
static void findSysFiles(CoreTemp *ct) {
  Meter *m = &ct->f.m;
  int cpu = 0;
  unsigned int i, cpucount = coretemp_countcores(ct->pkg);
  char name[PATH_SIZE], dummy[64];
  glob_t gbuf;
  DIR *dir;
  struct dirent *dent;

  /*  Intel and VIA CPUs.
   *  Platform device sysfs node changed in kernel 3.15 -> try both paths.  */
  snprintf(name, PATH_SIZE, "%s.%d/temp*_label", SYS_CORETEMP, ct->pkg);
  glob(name, 0, NULL, &gbuf);
  snprintf(name, PATH_SIZE, "%s.%d/hwmon/hwmon*/temp*_label", SYS_CORETEMP,
           ct->pkg);
  glob(name, GLOB_APPEND, NULL, &gbuf);
  snprintf(name, PATH_SIZE, "%s.%d/temp*_label", SYS_VIATEMP, ct->pkg);
  glob(name, GLOB_APPEND, NULL, &gbuf);
  snprintf(name, PATH_SIZE, "%s.%d/hwmon/hwmon*/temp*_label", SYS_VIATEMP,
           ct->pkg);
  glob(name, GLOB_APPEND, NULL, &gbuf);
  for (i = 0; i < gbuf.gl_pathc; i++) {
    /*  "Core n" or "Physical id n"  */
    if (!readword(gbuf.gl_pathv[i], dummy, sizeof dummy))
      continue;
    if (strncmp(dummy, "Core", 4) == 0) {
      char *underscore = strrchr(gbuf.gl_pathv[i], '_');

      if (underscore)
        strcpy(underscore, "_input");
      if (ct->cpu < 0)
        paths_push(&ct->cpus, gbuf.gl_pathv[i]);
      else if (cpu == ct->cpu)
        paths_push(&ct->cpus, gbuf.gl_pathv[i]);
      /*  The numbers seen in temp*_label might not be continuously
       *  growing!  */
      cpu++;
    }
  }
  globfree(&gbuf);
  if (ct->cpus.n)
    return;

  /*  AMD CPUs.  */
  if (!(dir = opendir(SYS_HWMON))) {
    fprintf(stderr, "Can not open %s directory.\n", SYS_HWMON);
    xwin_setdone(m->xw, 1);
    return;
  }
  while ((dent = readdir(dir))) {
    if (!strncmp(dent->d_name, ".", 1) || !strncmp(dent->d_name, "..", 2))
      continue;

    snprintf_or_abort(name, PATH_SIZE, "%s/%s/name", SYS_HWMON, dent->d_name);
    if (!readword(name, dummy, sizeof dummy)) {
      /*  Older kernels place the name in device directory.  */
      snprintf_or_abort(name, PATH_SIZE, "%s/%s/device/name", SYS_HWMON,
                        dent->d_name);
      if (!readword(name, dummy, sizeof dummy))
        continue;
    }

    if (strncmp(dummy, "k8temp", 6) == 0 ||
        strncmp(dummy, "k10temp", 7) == 0) {
      if (cpu++ != ct->pkg)
        continue;
      /*  K8 core has two sensors with index starting from 1.
       *  K10 has only one sensor per physical CPU.  */
      if (ct->cpu < 0) {  /*  avg or max  */
        for (i = 1; i <= cpucount; i++) {
          snprintf_or_abort(name, PATH_SIZE, "%s/%s/temp%u_input", SYS_HWMON,
                            dent->d_name, i);
          if (!isfile(name))
            snprintf_or_abort(name, PATH_SIZE, "%s/%s/device/temp%u_input",
                              SYS_HWMON, dent->d_name, i);
          paths_push(&ct->cpus, name);
        }
      } else {  /*  single sensor  */
        snprintf_or_abort(name, PATH_SIZE, "%s/%s/temp%d_input", SYS_HWMON,
                          dent->d_name, ct->cpu + 1);
        if (!isfile(name))
          snprintf_or_abort(name, PATH_SIZE, "%s/%s/device/temp%u_input",
                            SYS_HWMON, dent->d_name, i);
        paths_push(&ct->cpus, name);
      }
    }
  }
  closedir(dir);
}

static void checkres(Meter *m) {
  CoreTemp *ct = (CoreTemp *)m;
  FieldMeter *fm = &ct->f;
  char dummy[PATH_SIZE], l[32];
  double v;

  fieldmeter_checkresources(m);

  ct->actcolor = xwin_alloccolor(m->xw,
                                 xwin_getresource(m->xw, "coretempActColor"));
  ct->highcolor = xwin_alloccolor(m->xw,
                                  xwin_getresource(m->xw,
                                                   "coretempHighColor"));
  fieldmeter_setcolor(fm, 0, ct->actcolor);
  fieldmeter_setcolorname(fm, 1,
                          xwin_getresource(m->xw, "coretempIdleColor"));
  fieldmeter_setcolor(fm, 2, ct->highcolor);
  m->priority = atoi(xwin_getresource(m->xw, "coretempPriority"));
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "coretempUsedFormat"));

  findSysFiles(ct);
  if (ct->cpus.n == 0) {  /*  should not happen at this point  */
    fprintf(stderr, "BUG: Could not determine sysfs file(s) for coretemp.\n");
    xwin_setdone(m->xw, 1);
    return;
  }

  /*  Get TjMax and use it for total, if available.
   *  Not found on k8temp and via-cputemp.  */
  retarget(dummy, sizeof dummy, ct->cpus.path[0], "_crit");
  if (readdouble(dummy, &v))
    fm->total = v / 1000.0;
  else
    fm->total = atoi(xwin_getresource_default(m->xw, "coretempHighest",
                                              "100"));

  /*  Use tTarget/tCtl (when maximum cooling needs to be turned on) as high,
   *  if found.  On older Cores this MSR is empty and the kernel sets this
   *  equal to tjMax.  Not found on k8temp and via-cputemp.  On k10temp this
   *  is a fixed value.  */
  l[0] = '\0';
  retarget(dummy, sizeof dummy, ct->cpus.path[0], "_max");
  if (readdouble(dummy, &v)) {
    ct->high = v / 1000.0;
    snprintf(l, sizeof l, "ACT(\260C)/%d/%d", (int)ct->high, (int)fm->total);
  } else {
    ct->high = fm->total;
  }

  if (ct->high == (int)fm->total) {  /*  No tTarget/tCtl  */
    /*  Use user-defined high, or "no high".  */
    const char *high = xwin_getresource_default(m->xw, "coretempHigh", NULL);

    if (high) {
      ct->high = atoi(high);
      snprintf(l, sizeof l, "ACT(\260C)/%d/%d", (int)ct->high,
               (int)fm->total);
    } else {
      snprintf(l, sizeof l, "ACT(\260C)/HIGH/%d", (int)fm->total);
    }
  }
  meter_setlegend(m, l);
}

static void getcoretemp(CoreTemp *ct) {
  FieldMeter *fm = &ct->f;
  Meter *m = &fm->m;
  double dummy;
  unsigned int i;

  fm->fields[0] = 0.0;

  if (ct->cpu >= 0) {  /*  only one core  */
    const char *path = ct->cpus.path[ct->cpus.n - 1];

    if (!readdouble(path, &fm->fields[0])) {
      fprintf(stderr, "Can not open file : %s\n", path);
      xwin_setdone(m->xw, 1);
      return;
    }
  } else if (ct->cpu == -1) {  /*  average  */
    for (i = 0; i < ct->cpus.n; i++) {
      if (!readdouble(ct->cpus.path[i], &dummy)) {
        fprintf(stderr, "Can not open file : %s\n", ct->cpus.path[i]);
        xwin_setdone(m->xw, 1);
        return;
      }
      fm->fields[0] += dummy;
    }
    fm->fields[0] /= (double)ct->cpus.n;
  } else if (ct->cpu == -2) {  /*  maximum  */
    for (i = 0; i < ct->cpus.n; i++) {
      if (!readdouble(ct->cpus.path[i], &dummy)) {
        fprintf(stderr, "Can not open file : %s\n", ct->cpus.path[i]);
        xwin_setdone(m->xw, 1);
        return;
      }
      if (dummy > fm->fields[0])
        fm->fields[0] = dummy;
    }
  } else {  /*  should not happen  */
    fprintf(stderr, "Unknown CPU core number %d in coretemp.\n", ct->cpu);
    xwin_setdone(m->xw, 1);
    return;
  }

  fm->fields[0] /= 1000.0;
  fieldmeter_setused(fm, fm->fields[0], fm->total);

  if (fm->fields[0] < 0)
    fm->fields[0] = 0.0;
  fm->fields[1] = ct->high - fm->fields[0];
  if (fm->fields[1] < 0) {  /*  alarm: T > high  */
    fm->fields[1] = 0;
    if (fm->colors[0] != ct->highcolor) {
      fieldmeter_setcolor(fm, 0, ct->highcolor);
      fieldmeter_drawlegend(fm);
    }
  } else {
    if (fm->colors[0] != ct->actcolor) {
      fieldmeter_setcolor(fm, 0, ct->actcolor);
      fieldmeter_drawlegend(fm);
    }
  }

  fm->fields[2] = fm->total - fm->fields[1] - fm->fields[0];
  if (fm->fields[2] < 0)
    fm->fields[2] = 0;
}

static void checkevent(Meter *m) {
  getcoretemp((CoreTemp *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

static void destroy(Meter *m) {
  CoreTemp *ct = (CoreTemp *)m;

  free(ct->cpus.path);
  ct->cpus.path = NULL;
  ct->cpus.n = ct->cpus.cap = 0;
  fieldmeter_fini(m);
}

/*  Count sensors available to coretemp in the given package.  */
unsigned int coretemp_countcores(unsigned int pkg) {
  glob_t gbuf;
  char s[PATH_SIZE], dummy[64];
  unsigned int i, count = 0, cpu = 0;
  DIR *dir;
  struct dirent *dent;

  /*  Intel or VIA CPU.
   *  Platform device sysfs node changed in kernel 3.15 -> try both paths.  */
  snprintf(s, PATH_SIZE, "%s.%u/temp*_label", SYS_CORETEMP, pkg);
  glob(s, 0, NULL, &gbuf);
  snprintf(s, PATH_SIZE, "%s.%u/hwmon/hwmon*/temp*_label", SYS_CORETEMP, pkg);
  glob(s, GLOB_APPEND, NULL, &gbuf);
  snprintf(s, PATH_SIZE, "%s.%u/temp*_label", SYS_VIATEMP, pkg);
  glob(s, GLOB_APPEND, NULL, &gbuf);
  snprintf(s, PATH_SIZE, "%s.%u/hwmon/hwmon*/temp*_label", SYS_VIATEMP, pkg);
  glob(s, GLOB_APPEND, NULL, &gbuf);
  /*  loop through paths in gbuf and check if it is a core or package  */
  for (i = 0; i < gbuf.gl_pathc; i++)
    if (readword(gbuf.gl_pathv[i], dummy, sizeof dummy)
        && strncmp(dummy, "Core", 4) == 0)
      count++;
  globfree(&gbuf);
  if (count > 0)
    return count;

  /*  AMD CPU.  */
  if (!(dir = opendir(SYS_HWMON)))
    return 0;
  /*  loop through hwmon devices and when an AMD sensor is found, count its
   *  inputs  */
  while ((dent = readdir(dir))) {
    if (!strncmp(dent->d_name, ".", 1) || !strncmp(dent->d_name, "..", 2))
      continue;
    snprintf_or_abort(s, PATH_SIZE, "%s/%s/name", SYS_HWMON, dent->d_name);
    if (!isfile(s)) {
      /*  Older kernels place the name in device directory.  */
      snprintf_or_abort(s, PATH_SIZE, "%s/%s/device/name", SYS_HWMON,
                        dent->d_name);
    }

    if (readword(s, dummy, sizeof dummy)) {
      if (strncmp(dummy, "k8temp", 6) == 0 ||
          strncmp(dummy, "k10temp", 7) == 0) {
        if (cpu++ < pkg)
          continue;
        snprintf_or_abort(s, PATH_SIZE, "%s/%s/temp*_input", SYS_HWMON,
                          dent->d_name);
        glob(s, 0, NULL, &gbuf);
        snprintf_or_abort(s, PATH_SIZE, "%s/%s/device/temp*_input",
                          SYS_HWMON, dent->d_name);
        glob(s, GLOB_APPEND, NULL, &gbuf);
        count += gbuf.gl_pathc;
        globfree(&gbuf);
      }
    }
  }
  closedir(dir);
  return count;
}

/*  Count physical CPUs with sensors.  */
unsigned int coretemp_countcpus(void) {
  glob_t gbuf;
  char s[PATH_SIZE], dummy[64];
  unsigned int count = 0;
  DIR *dir;
  struct dirent *dent;

  /*  Count Intel and VIA packages.  */
  snprintf(s, PATH_SIZE, "%s.*", SYS_CORETEMP);
  glob(s, 0, NULL, &gbuf);
  snprintf(s, PATH_SIZE, "%s.*", SYS_VIATEMP);
  glob(s, GLOB_APPEND, NULL, &gbuf);
  count += gbuf.gl_pathc;
  globfree(&gbuf);
  if (count > 0)
    return count;

  /*  Count AMD packages.  */
  if (!(dir = opendir(SYS_HWMON)))
    return 0;
  while ((dent = readdir(dir))) {
    if (!strncmp(dent->d_name, ".", 1) || !strncmp(dent->d_name, "..", 2))
      continue;
    snprintf_or_abort(s, PATH_SIZE, "%s/%s/name", SYS_HWMON, dent->d_name);
    if (!isfile(s))
      /*  Older kernels place the name in device directory.  */
      snprintf_or_abort(s, PATH_SIZE, "%s/%s/device/name", SYS_HWMON,
                        dent->d_name);
    if (readword(s, dummy, sizeof dummy))
      if (strncmp(dummy, "k8temp", 6) == 0 ||
          strncmp(dummy, "k10temp", 7) == 0)
        count++;
  }
  closedir(dir);
  return count;
}

Meter *coretemp_new(XOSView *parent, const char *label, const char *caption,
                    int pkg, int cpu) {
  CoreTemp *ct = (CoreTemp *)meter_alloc(sizeof *ct);

  fieldmeter_init(&ct->f, parent, 3, "CoreTemp", label, caption, 1, 1, 1);
  ct->f.m.checkres = checkres;
  ct->f.m.checkevent = checkevent;
  ct->f.m.destroy = destroy;
  ct->f.metric = 1;

  ct->pkg = pkg;
  ct->cpu = cpu;
  ct->high = 0;
  ct->actcolor = ct->highcolor = 0;
  ct->cpus.path = NULL;
  ct->cpus.n = ct->cpus.cap = 0;

  return &ct->f.m;
}
