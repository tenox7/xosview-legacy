/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _OSF1STATS_H_
#define _OSF1STATS_H_

/*
 *  The meters read every statistic through this interface, implemented in
 *  osf1/osf1stats.c on top of table(2) and the Mach vm_statistics() call.
 *  Neither needs any privileges.
 *
 *  Every call returns 0 when its statistic can not be obtained.  Meters
 *  disable themselves in that case rather than fail to start, so a kernel
 *  that does not export one of them loses a single meter.
 *
 *  Counters that accumulate are returned as doubles holding a byte, page or
 *  tick count since boot; callers difference successive samples themselves.
 */

/*  Load averages over the last 1, 5 and 15 minutes, in processes.  */
int osf1stats_load(double avg[3]);

/*  Cumulative cpu ticks: user, nice, system, io wait, idle.  */
int osf1stats_cpu(double ticks[5]);

/*  Real memory in bytes.  cache is the inactive queue, where the unified
 *  buffer cache parks clean file pages that the kernel reclaims on demand.  */
int osf1stats_memory(double *totalp, double *cachep, double *freep);

/*  Swap in bytes, totalled over every swap partition.  */
int osf1stats_swap(double *totalp, double *freep);

/*  Cumulative pages paged in and out.  */
int osf1stats_paging(double *inp, double *outp);

/*  Processors online, at least 1.  */
int osf1stats_cpus(void);

#endif
