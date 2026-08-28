/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

/*  Tru64 UNIX, Digital UNIX and OSF/1 all answer these through table(2),
 *  except for real memory, which comes from the Mach vm_statistics() call
 *  the kernel is built around.  */

#include "osf1stats.h"

#include <sys/types.h>
#include <sys/table.h>
#include <mach.h>
#include <mach/mach_types.h>
#include <mach/vm_statistics.h>
#include <unistd.h>

/*  TBL_SWAPINFO reports partition sizes in 512 byte blocks.  If the swap
 *  meter comes out scaled wrong on some release, this is the constant to
 *  correct.  */
#define SWAPBLOCK 512.0

static int vmstats(vm_statistics_data_t *vms) {
  return vm_statistics(task_self(), vms) == KERN_SUCCESS;
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
  struct swapinfo swi;
  int i;

  *totalp = *freep = 0;

  /*  One entry per swap partition, walked until the call runs out of them. */
  for (i = 0; table(TBL_SWAPINFO, i, &swi, 1, sizeof(swi)) > 0; i++) {
    *totalp += (double)swi.si_swapsize * SWAPBLOCK;
    *freep += (double)swi.si_free * SWAPBLOCK;
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

int osf1stats_cpus(void) {
  long n = sysconf(_SC_NPROCESSORS_ONLN);

  return n > 0 ? (int)n : 1;
}
