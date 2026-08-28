/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

/*  UnixWare 7 keeps its kernel metrics in the MAS metric file, which is what
 *  sar(1) and top read, and swap in swapctl(2).  The load average is the one
 *  figure MAS does not carry, so it comes out of kernel memory.  */

#include "unixwarestats.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/swap.h>
#include <fcntl.h>
#include <nlist.h>
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

int unixwarestats_cpu(double ticks[4]) {
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

int unixwarestats_memory(double *totalp, double *freep) {
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

int unixwarestats_swap(double *totalp, double *freep) {
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

/*  MAS carries no load average, so read avenrun out of kernel memory at the
 *  address nlist() resolves from the boot image.  This is the one statistic
 *  here that needs privileges: root, or setgid sys.  */
static const char KERNEL[] = "/stand/unix";

/*  avenrun is fixed point scaled by 1 << FSHIFT, and FSHIFT is 8 here.  */
#define FSCALE 256.0

int unixwarestats_load(double avg[3]) {
  static int kmemfd = -1;
  static unsigned long addr = 0;
  static int tried = 0;
  long avenrun[3];
  int i;

  if (!tried) {
    struct nlist nl[2];

    tried = 1;
    memset(nl, 0, sizeof(nl));
    nl[0].n_name = (char *)"avenrun";

    if (nlist(KERNEL, nl) != 0 || nl[0].n_value == 0) {
      fprintf(stderr, "Can not resolve 'avenrun' in %s.\n", KERNEL);
    } else {
      addr = nl[0].n_value;
      kmemfd = open("/dev/kmem", O_RDONLY);
      if (kmemfd < 0)
        fprintf(stderr, "Can not open /dev/kmem.  xosview must run as root "
                "or be\n  installed setgid sys to show the load average.\n");
    }
  }

  if (kmemfd < 0 || addr == 0)
    return 0;

  if (lseek(kmemfd, addr, SEEK_SET) == -1 ||
      read(kmemfd, avenrun, sizeof(avenrun)) != (int)sizeof(avenrun))
    return 0;

  for (i = 0; i < 3; i++)
    avg[i] = avenrun[i] / FSCALE;

  return 1;
}

int unixwarestats_cpus(void) {
  return mas() ? ncpu : 1;
}
