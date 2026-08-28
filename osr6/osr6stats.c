/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

/*  SCO OpenServer 6 keeps its kernel metrics in the MAS metric file, which
 *  is what sar(1) and top read.  Swap comes from swapctl(2) and the load
 *  average is accumulated from /proc, because the kernel publishes neither. */

#include "osr6stats.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/swap.h>
#include <sys/procfs.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mas.h>
#include <metreg.h>

static int masfd = -1;
static int masopen = 0;
static int ncpu = 1;

/*  Opens the metric file once.  Every entry point calls this first, so a
 *  machine without MAS simply reports nothing rather than crashing.  */
static int mas(void) {
  unsigned int *n;

  if (masopen)
    return masfd >= 0;

  masopen = 1;
  masfd = mas_open(MAS_FILE, MAS_MMAP_ACCESS);
  if (masfd < 0)
    return 0;

  n = (unsigned int *)mas_get_met(masfd, NCPU, 0);
  if (n)
    ncpu = *(short *)n;
  if (ncpu < 1)
    ncpu = 1;

  return 1;
}

/*  Per cpu metrics are registered once for each processor, so a system wide
 *  figure is the sum over all of them.  */
static unsigned int metric(int id) {
  unsigned int total = 0;
  int i;

  for (i = 0; i < ncpu; i++) {
    unsigned int *p = (unsigned int *)mas_get_met(masfd, id, i);
    if (p)
      total += *p;
  }

  return total;
}

int osr6stats_cpu(double ticks[4]) {
  if (!mas())
    return 0;

  ticks[0] = metric(MPC_CPU_USR);
  ticks[1] = metric(MPC_CPU_SYS);
  ticks[2] = metric(MPC_CPU_WIO);
  ticks[3] = metric(MPC_CPU_IDLE);

  return 1;
}

/*  freemem is an accumulator: the kernel adds the current free page count to
 *  it once a second, so the level is the difference between two samples over
 *  the seconds between them.  This is what sar -r and top do.  Returns -1
 *  until a second sample is available.  */
static long freepages(void) {
  static dl_t prev;
  static time_t prevtime = 0;
  dl_t *cur, diff, den, quot;
  time_t now;
  long secs;

  cur = (dl_t *)mas_get_met(masfd, FREEMEM, 0);
  if (!cur)
    return -1;

  now = time(NULL);
  if (prevtime == 0) {
    prev = *cur;
    prevtime = now;
    return -1;
  }

  /*  Too soon to divide; keep the baseline rather than resetting it.  */
  secs = (long)(now - prevtime);
  if (secs < 1)
    return -1;

  diff = lsub(*cur, prev);
  den.dl_lop = (ulong_t)secs;
  den.dl_hop = 0;
  quot = ldivide(diff, den);

  prev = *cur;
  prevtime = now;

  return (long)quot.dl_lop;
}

int osr6stats_memory(double *totalp, double *freep) {
  static double lastfree = 0;
  double pagesize = getpagesize();
  long pages = sysconf(_SC_TOTAL_MEMORY);
  long f;

  if (!mas() || pages <= 0)
    return 0;

  *totalp = (double)pages * pagesize;

  /*  The kernel registers freefilemem too, but keeps it identical to freemem
   *  on this release, so the file cache can not be shown separately.  */
  f = freepages();
  if (f >= 0)
    lastfree = (double)f * pagesize;
  *freep = lastfree;

  return 1;
}

int osr6stats_swap(double *totalp, double *freep) {
  swaptbl_t *swt;
  char *paths;
  double pagesize = getpagesize();
  int ok = 0;
  int n, i;

  *totalp = *freep = 0;

  n = swapctl(SC_GETNSWP, 0);
  if (n < 0)
    return 0;
  if (n == 0)
    return 1;

  swt = (swaptbl_t *)malloc(sizeof(int) + n * sizeof(swapent_t));
  /*  swapctl() writes the path of each area back through these pointers, so
   *  every entry has to be given somewhere to put one.  */
  paths = (char *)malloc(n * MAXPATHLEN);

  if (swt && paths) {
    swt->swt_n = n;
    for (i = 0; i < n; i++)
      swt->swt_ent[i].ste_path = paths + i * MAXPATHLEN;

    if (swapctl(SC_LIST, swt) >= 0) {
      for (i = 0; i < swt->swt_n; i++) {
        *totalp += (double)swt->swt_ent[i].ste_pages * pagesize;
        *freep += (double)swt->swt_ent[i].ste_free * pagesize;
      }
      ok = 1;
    }
  }

  free(paths);
  free(swt);

  return ok;
}

/*  Processes on or waiting for a processor.  Our own is always on-proc while
 *  the scan runs, so it does not count.  */
static int runnable(void) {
  DIR *dir;
  struct dirent *ent;
  int run = 0;

  dir = opendir("/proc");
  if (!dir)
    return -1;

  while ((ent = readdir(dir)) != NULL) {
    psinfo_t ps;
    char path[MAXPATHLEN];
    int fd;

    if (ent->d_name[0] < '0' || ent->d_name[0] > '9')
      continue;

    snprintf(path, sizeof(path), "/proc/%s/psinfo", ent->d_name);
    fd = open(path, O_RDONLY);
    if (fd < 0)
      continue;

    if (read(fd, &ps, sizeof(ps)) == (int)sizeof(ps) &&
        (ps.pr_lwp.pr_sname == 'R' || ps.pr_lwp.pr_sname == 'O'))
      run++;
    close(fd);
  }
  closedir(dir);

  return run > 0 ? run - 1 : 0;
}

int osr6stats_load(double avg[3]) {
  /*  avenrun exists as a symbol but the kernel never updates it, which is why
   *  uptime(1) always prints 0.00, and there is no moving run queue metric
   *  either.  Decay run queue samples into a 1 minute average here instead.
   *  Only that one is real; the 5 and 15 minute figures repeat it.  */
  static double load = -1.0;
  static time_t prevtime = 0;
  time_t now;
  int run = runnable();

  if (run < 0)
    return 0;

  now = time(NULL);
  if (load < 0.0) {
    load = run;
  } else {
    long secs = (long)(now - prevtime);
    double weight;
    if (secs < 1)
      secs = 1;
    weight = exp(-(double)secs / 60.0);
    load = load * weight + run * (1.0 - weight);
  }
  prevtime = now;

  avg[0] = avg[1] = avg[2] = load;

  return 1;
}

int osr6stats_cpus(void) {
  return mas() ? ncpu : 1;
}
