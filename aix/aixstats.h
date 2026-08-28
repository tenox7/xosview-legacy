/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _AIXSTATS_H_
#define _AIXSTATS_H_

/*
 *  The meters read every statistic through this interface, which has two
 *  interchangeable implementations selected by the build target:
 *
 *    aix/perfstat.c  libperfstat, the supported interface, on AIX 5.1 and up
 *    aix/kmem.c      /dev/kmem at addresses resolved with knlist(), for the
 *                    AIX 4.x releases that predate libperfstat
 *
 *  Every call returns 0 when its statistic can not be obtained.  Meters
 *  disable themselves in that case rather than fail to start, so a kernel
 *  that does not export one of them loses a single meter.
 *
 *  Counters that accumulate are returned as doubles holding a byte, page or
 *  tick count since boot.  Callers difference successive samples themselves,
 *  and must cope with a sample that moves backwards: the AIX 4.x kernel keeps
 *  several of these counters in 32 bits and they wrap on a busy machine.
 */

/*  Load averages over the last 1, 5 and 15 minutes, in processes.  */
int aixstats_load(double avg[3]);

/*  Cumulative cpu ticks: user, system, io wait, idle.  */
int aixstats_cpu(double ticks[4]);

/*  Real memory in bytes.  cache counts file pages, which are resident but
 *  which the kernel hands back under memory pressure.  */
int aixstats_memory(double *totalp, double *cachep, double *freep);

/*  Paging space in bytes.  */
int aixstats_swap(double *totalp, double *freep);

/*  Cumulative pages moved in from and out to paging space.  */
int aixstats_paging(double *inp, double *outp);

/*  Cumulative bytes read from and written to all disks.  */
int aixstats_disk(double *readp, double *writtenp);

/*  Cumulative bytes received and sent.  iface names a single interface to
 *  report on, or is null to total every interface; when ignore is set the
 *  named interface is the only one left out.  */
int aixstats_net(const char *iface, int ignore, double *inp, double *outp);

#endif
