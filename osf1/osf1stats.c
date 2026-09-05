/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

/*  Tru64 UNIX, Digital UNIX and OSF/1 all answer these through table(2),
 *  except for real memory, which comes from the Mach vm_statistics() call
 *  the kernel is built around, and the interface counters, which have no
 *  interface at all and are read out of /dev/kmem.  */

#include "osf1stats.h"

#include <sys/types.h>
#include <sys/table.h>
#include <sys/socket.h>
#include <net/if.h>
#include <mach.h>
#include <mach/mach_types.h>
#include <mach/vm_statistics.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*  di_wds counts 64 byte units, the b_bcount >> 6 the disk drivers add up.  */
#define DK_WDSIZE 64

#define MAX_DISKS 256
#define MAX_INTERFACES 64

#define NOT_OPENED (-2)

static int kmemfd = NOT_OPENED;

static int vmstats(vm_statistics_data_t *vms) {
  return vm_statistics(task_self(), vms) == KERN_SUCCESS;
}

static int kmem(void) {
  if (kmemfd == NOT_OPENED) {
    kmemfd = open("/dev/kmem", O_RDONLY);
    if (kmemfd < 0)
      fprintf(stderr, "Can not open /dev/kmem.  xosview must run as root or "
              "be\n  installed setgid mem to read the interface counters.\n");
  }
  return kmemfd;
}

/*  Copy size bytes from kernel address addr.  Returns 0 on failure.  */
static int kread(unsigned long addr, void *buf, int size) {
  int fd = kmem();

  if (fd < 0 || addr == 0)
    return 0;

  if (lseek(fd, (off_t)addr, SEEK_SET) == (off_t)-1)
    return 0;

  return read(fd, buf, size) == size;
}

/*  Address of the head of the kernel ifnet chain, resolved once and
 *  remembered, so that a kernel without the symbol is reported here rather
 *  than on every sample.  */
static unsigned long ifnetsym(void) {
  static unsigned long addr = 0;
  static int tried = 0;
  struct nlist nl[2];

  if (tried)
    return addr;
  tried = 1;

  memset(nl, 0, sizeof(nl));
  nl[0].n_name = "ifnet";

  if (nlist("/vmunix", nl) < 0 || nl[0].n_value == 0)
    fprintf(stderr, "Can not resolve the kernel symbol 'ifnet' in "
            "/vmunix.\n");
  else
    addr = nl[0].n_value;

  return addr;
}

int osf1stats_load(double avg[3]) {
  struct tbl_loadavg lb;
  int i;

  if (table(TBL_LOADAVG, 0, &lb, 1, sizeof(lb)) < 0)
    return 0;

  /*  tl_lscale is the fixed point scale the averages are kept in, or zero
   *  on the releases that keep them as doubles instead.  */
  for (i = 0; i < 3; i++)
    avg[i] = lb.tl_lscale ? (double)lb.tl_avenrun.l[i] / lb.tl_lscale
                          : lb.tl_avenrun.d[i];

  return 1;
}

int osf1stats_cpu(double ticks[5]) {
  struct tbl_sysinfo si;

  if (table(TBL_SYSINFO, 0, &si, 1, sizeof(si)) < 0)
    return 0;

  /*  The io wait counter is the one field of this struct without the si_
   *  prefix.  */
  ticks[0] = si.si_user;
  ticks[1] = si.si_nice;
  ticks[2] = si.si_sys;
  ticks[3] = si.wait;
  ticks[4] = si.si_idle;

  return 1;
}

int osf1stats_memory(double *totalp, double *cachep, double *freep) {
  vm_statistics_data_t vms;
  double pagesize = getpagesize();

  if (!vmstats(&vms))
    return 0;

  /*  There is no total in the struct; the four queues account for all of
   *  real memory between them.  */
  *totalp = ((double)vms.free_count + vms.active_count + vms.inactive_count
             + vms.wire_count) * pagesize;
  *cachep = (double)vms.inactive_count * pagesize;
  *freep = (double)vms.free_count * pagesize;

  return 1;
}

int osf1stats_swap(double *totalp, double *freep) {
  struct tbl_swapinfo swi;
  double pagesize = getpagesize();
  int i;

  *totalp = *freep = 0;

  /*  One entry per swap partition, walked until the call runs out of them.
   *  Both counts are in pages, which swapon -s agrees with.  */
  for (i = 0; table(TBL_SWAPINFO, i, &swi, 1, sizeof(swi)) > 0; i++) {
    *totalp += (double)swi.size * pagesize;
    *freep += (double)swi.free * pagesize;
  }

  return i > 0;
}

int osf1stats_paging(double *inp, double *outp) {
  vm_statistics_data_t vms;

  if (!vmstats(&vms))
    return 0;

  *inp = vms.pageins;
  *outp = vms.pageouts;

  return 1;
}

int osf1stats_disk(double *bytesp) {
  /*  These are the counters iostat(1) reports.  di_ndrive is the size of the
   *  table rather than the number of drives on the machine, and the slots
   *  that carry no drive read back as zeroes.  */
  struct tbl_dkinfo dki;
  int i, ndrive;

  if (table(TBL_DKINFO, 0, &dki, 1, sizeof(dki)) <= 0)
    return 0;

  ndrive = dki.di_ndrive < MAX_DISKS ? dki.di_ndrive : MAX_DISKS;
  *bytesp = (double)dki.di_wds * DK_WDSIZE;

  for (i = 1; i < ndrive; i++) {
    if (table(TBL_DKINFO, i, &dki, 1, sizeof(dki)) <= 0)
      break;
    *bytesp += (double)dki.di_wds * DK_WDSIZE;
  }

  return 1;
}

int osf1stats_net(const char *iface, int ignore, double *inp, double *outp) {
  /*  Interfaces are counted by walking the kernel ifnet chain.  */
  unsigned long next;
  int i;

  if (!kread(ifnetsym(), &next, sizeof(next)))
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

int osf1stats_intr(double *countp) {
  struct tbl_intr in;

  if (table(TBL_INTR, 0, &in, 1, sizeof(in)) <= 0)
    return 0;

  *countp = in.in_devintr;

  return 1;
}

int osf1stats_cpus(void) {
  long n = sysconf(_SC_NPROCESSORS_ONLN);

  return n > 0 ? (int)n : 1;
}
