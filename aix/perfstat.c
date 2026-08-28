/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

/*  The AIX 5.x back end.  libperfstat arrived in AIX 5.1 and is the supported
 *  way to ask for these statistics, so unlike the AIX 4.x back end in
 *  aix/kmem.c this one needs no access to kernel memory and no privileges,
 *  and its counters are 64 bit and do not wrap.  */

#include "aixstats.h"

#include <libperfstat.h>
#include <string.h>

/*  libperfstat reports memory and paging space in 4K units and disk traffic
 *  in 512 byte blocks, whatever the page size of the running kernel.  */
#define PERFSTAT_PAGE 4096.0
#define PERFSTAT_BLOCK 512.0

/*  Load averages are fixed point values scaled by 1<<SBITS.  */
#define FSCALE 65536.0

#define MAX_INTERFACES 64

static int cputotal(perfstat_cpu_total_t *c) {
  return perfstat_cpu_total(NULL, c, sizeof(*c), 1) == 1;
}

static int memtotal(perfstat_memory_total_t *m) {
  return perfstat_memory_total(NULL, m, sizeof(*m), 1) == 1;
}

int aixstats_load(double avg[3]) {
  perfstat_cpu_total_t c;
  int i;

  if (!cputotal(&c))
    return 0;

  for (i = 0; i < 3; i++)
    avg[i] = c.loadavg[i] / FSCALE;

  return 1;
}

int aixstats_cpu(double ticks[4]) {
  perfstat_cpu_total_t c;

  if (!cputotal(&c))
    return 0;

  ticks[0] = c.user;
  ticks[1] = c.sys;
  ticks[2] = c.wait;
  ticks[3] = c.idle;

  return 1;
}

int aixstats_memory(double *totalp, double *cachep, double *freep) {
  perfstat_memory_total_t m;

  if (!memtotal(&m))
    return 0;

  *totalp = m.real_total * PERFSTAT_PAGE;
  *cachep = m.numperm * PERFSTAT_PAGE;
  *freep = m.real_free * PERFSTAT_PAGE;

  return 1;
}

int aixstats_swap(double *totalp, double *freep) {
  perfstat_memory_total_t m;

  if (!memtotal(&m))
    return 0;

  *totalp = m.pgsp_total * PERFSTAT_PAGE;
  *freep = m.pgsp_free * PERFSTAT_PAGE;

  return 1;
}

int aixstats_paging(double *inp, double *outp) {
  perfstat_memory_total_t m;

  if (!memtotal(&m))
    return 0;

  *inp = m.pgspins;
  *outp = m.pgspouts;

  return 1;
}

int aixstats_disk(double *readp, double *writtenp) {
  perfstat_disk_total_t d;

  if (perfstat_disk_total(NULL, &d, sizeof(d), 1) != 1)
    return 0;

  *readp = d.rblks * PERFSTAT_BLOCK;
  *writtenp = d.wblks * PERFSTAT_BLOCK;

  return 1;
}

int aixstats_net(const char *iface, int ignore, double *inp, double *outp) {
  perfstat_netinterface_t ifs[MAX_INTERFACES];
  perfstat_id_t id;
  int n, i;

  if (!iface) {
    perfstat_netinterface_total_t t;

    if (perfstat_netinterface_total(NULL, &t, sizeof(t), 1) != 1)
      return 0;

    *inp = t.ibytes;
    *outp = t.obytes;
    return 1;
  }

  /*  An empty name asks for the list from its first entry onwards.  */
  strcpy(id.name, "");
  n = perfstat_netinterface(&id, ifs, sizeof(perfstat_netinterface_t),
                            MAX_INTERFACES);
  if (n < 0)
    return 0;

  *inp = *outp = 0;

  for (i = 0; i < n; i++) {
    if ((strcmp(iface, ifs[i].name) == 0) == ignore)
      continue;

    *inp += ifs[i].ibytes;
    *outp += ifs[i].obytes;
  }

  return 1;
}
