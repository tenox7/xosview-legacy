/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _OSR6STATS_H_
#define _OSR6STATS_H_

/*
 *  The meters read every statistic through this interface, implemented in
 *  osr6/osr6stats.c on top of the MAS kernel metric file, swapctl(2) and
 *  /proc.  None of them need any privileges.
 *
 *  Every call returns 0 when its statistic can not be obtained.  Meters
 *  disable themselves in that case rather than fail to start, so a machine
 *  whose kernel does not register one of these metrics loses a single meter.
 */

/*  Load averages over the last 1, 5 and 15 minutes, in processes.  The
 *  kernel publishes none of these, so they are accumulated here; see the
 *  implementation.  */
int osr6stats_load(double avg[3]);

/*  Cumulative cpu ticks: user, system, io wait, idle.  */
int osr6stats_cpu(double ticks[4]);

/*  Real memory in bytes.  free is zero until a second sample is available,
 *  because the kernel publishes it as an accumulator rather than a level.  */
int osr6stats_memory(double *totalp, double *freep);

/*  Swap in bytes, totalled over every swap area.  */
int osr6stats_swap(double *totalp, double *freep);

/*  Processors online, at least 1.  */
int osr6stats_cpus(void);

#endif
