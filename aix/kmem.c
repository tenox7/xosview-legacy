/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

/*  The AIX 4.x back end.  These releases predate libperfstat, so every
 *  statistic is read straight out of /dev/kmem at an address resolved with
 *  knlist().  See aix/perfstat.c for the AIX 5.x back end.  */

#include "aixstats.h"
#include "vmker.h"

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/vminfo.h>
#include <sys/iostat.h>
#include <sys/socket.h>
#include <net/if.h>
#include <nlist.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

/*  AIX 4.1 ships no prototype for knlist(); readx() is declared in unistd.h. */
int knlist(struct nlist *, int, int);

#define MAX_DISKS 256UL
#define MAX_INTERFACES 64

/*  avenrun holds the load averages as fixed point values scaled by 1<<16,
 *  which is what uptime(1) and friends divide by.  */
#define FSCALE 65536.0

#define NOT_OPENED (-2)

static int kmemfd = NOT_OPENED;

static int kmem(void) {
  if (kmemfd == NOT_OPENED) {
    kmemfd = open("/dev/kmem", O_RDONLY);
    if (kmemfd < 0)
      fprintf(stderr, "Can not open /dev/kmem.  xosview must run as root or "
              "be\n  installed setgid system to read kernel statistics.\n");
  }
  return kmemfd;
}

/*  Address of a kernel symbol, or 0 if it can not be resolved.  */
static unsigned long symbol(const char *name) {
  struct nlist nl[2];

  memset(nl, 0, sizeof(nl));
  nl[0].n_name = (char *)name;
  nl[1].n_name = NULL;

  if (knlist(nl, 1, sizeof(struct nlist)) != 0 || nl[0].n_value == 0) {
    fprintf(stderr, "Can not resolve kernel symbol '%s'.\n", name);
    return 0;
  }

  return nl[0].n_value;
}

/*  Resolve a symbol on the first call and remember the answer, so that an
 *  unresolvable one is reported once rather than on every sample.  */
static unsigned long cached(const char *name, unsigned long *addr, int *tried) {
  if (!*tried) {
    *tried = 1;
    *addr = symbol(name);
  }
  return *addr;
}

/*  Copy size bytes from kernel address addr.  Returns 0 on failure.  */
static int kread(unsigned long addr, void *buf, int size) {
  int fd = kmem();
  int upper_2gb = 0;

  if (fd < 0 || addr == 0)
    return 0;

  /*  Addresses above 2GB are reached by seeking to addr % 2GB and passing 1
   *  as the extension argument of readx().  See the AIX kmem(4) man page.  */
  if (addr > 0x7fffffff) {
    upper_2gb = 1;
    addr &= 0x7fffffff;
  }

  if (lseek(fd, addr, SEEK_SET) == -1)
    return 0;

  return readx(fd, (char *)buf, size, upper_2gb) == size;
}

static int vmkerstats(struct vmker *vmk) {
  static unsigned long addr = 0;
  static int tried = 0;

  return kread(cached("vmker", &addr, &tried), vmk, sizeof(*vmk));
}

int aixstats_load(double avg[3]) {
  static unsigned long addr = 0;
  static int tried = 0;
  int avenrun[3];
  int i;

  if (!kread(cached("avenrun", &addr, &tried), avenrun, sizeof(avenrun)))
    return 0;

  for (i = 0; i < 3; i++)
    avg[i] = avenrun[i] / FSCALE;

  return 1;
}

int aixstats_cpu(double ticks[4]) {
  static unsigned long addr = 0;
  static int tried = 0;
  struct sysinfo si;

  if (!kread(cached("sysinfo", &addr, &tried), &si, sizeof(si)))
    return 0;

  ticks[0] = si.cpu[CPU_USER];
  ticks[1] = si.cpu[CPU_KERNEL];
  ticks[2] = si.cpu[CPU_WAIT];
  ticks[3] = si.cpu[CPU_IDLE];

  return 1;
}

int aixstats_memory(double *totalp, double *cachep, double *freep) {
  struct vmker vmk;
  double pagesize = getpagesize();

  if (!vmkerstats(&vmk))
    return 0;

  *totalp = (double)vmk.totalmem * pagesize;
  *cachep = (double)vmk.numperm * pagesize;
  *freep = (double)vmk.freemem * pagesize;

  return 1;
}

int aixstats_swap(double *totalp, double *freep) {
  struct vmker vmk;
  double pagesize = getpagesize();

  if (!vmkerstats(&vmk))
    return 0;

  *totalp = (double)vmk.totalvmem * pagesize;
  *freep = (double)vmk.freevmem * pagesize;

  return 1;
}

int aixstats_paging(double *inp, double *outp) {
  static unsigned long addr = 0;
  static int tried = 0;
  struct vminfo vmi;

  if (!kread(cached("vmminfo", &addr, &tried), &vmi, sizeof(vmi)))
    return 0;

  *inp = vmi.pgspgins;
  *outp = vmi.pgspgouts;

  return 1;
}

int aixstats_disk(double *readp, double *writtenp) {
  /*  The kernel iostat struct heads a chain of per disk dkstat entries, the
   *  same ones iostat(1) reports.  Block counts are scaled by the per disk
   *  block size to get bytes.  */
  static unsigned long addr = 0;
  static int tried = 0;
  struct iostat ios;
  unsigned long next, i;

  if (!kread(cached("iostat", &addr, &tried), &ios, sizeof(ios)))
    return 0;

  *readp = *writtenp = 0;
  next = (unsigned long)ios.dkstatp;

  for (i = 0; next && i < ios.dk_cnt && i < MAX_DISKS; i++) {
    struct dkstat dk;

    if (!kread(next, &dk, sizeof(dk)))
      break;

    *readp += (double)dk.dk_rblks * dk.dk_bsize;
    *writtenp += (double)dk.dk_wblks * dk.dk_bsize;
    next = (unsigned long)dk.dknextp;
  }

  return 1;
}

int aixstats_net(const char *iface, int ignore, double *inp, double *outp) {
  /*  Interfaces are counted by walking the kernel ifnet chain.  */
  static unsigned long addr = 0;
  static int tried = 0;
  unsigned long next;
  int i;

  if (!kread(cached("ifnet", &addr, &tried), &next, sizeof(next)))
    return 0;

  *inp = *outp = 0;

  for (i = 0; next && i < MAX_INTERFACES; i++) {
    struct ifnet ifn;

    if (!kread(next, &ifn, sizeof(ifn)))
      break;
    next = (unsigned long)ifn.if_next;

    if (iface) {
      /*  if_name points at a string still living in kernel memory.  */
      char name[16], found[32];

      name[0] = '\0';
      if (!kread((unsigned long)ifn.if_name, name, sizeof(name)))
        continue;
      name[sizeof(name) - 1] = '\0';
      snprintf(found, sizeof(found), "%s%d", name, ifn.if_unit);

      if ((strcmp(iface, found) == 0) == ignore)
        continue;
    }

    /*  These counters are 32 bit and wrap on a busy link; the caller notices
     *  because the total it keeps moves backwards.  */
    *inp += (unsigned int)ifn.if_ibytes;
    *outp += (unsigned int)ifn.if_obytes;
  }

  return 1;
}
