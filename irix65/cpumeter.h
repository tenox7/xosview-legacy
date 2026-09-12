/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#ifndef _CPUMETER_H_
#define _CPUMETER_H_

#include "fieldmeter.h"

#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>

/*  5.3 names the cpu states but never counts them.  */
#ifndef CPU_STATES
#define CPU_STATES 6
#endif

#define USED_CPU_STATES (CPU_STATES-1)  /*  SXBRK + IDLE merged  */

typedef struct {
    FieldMeter f;
    time_t cputime[2][USED_CPU_STATES];
    int cpuindex;
    struct sysinfo tsp;
    int sinfosz;
    int cpuid;
} CPUMeter;

/*  cpuid of -1 aggregates every processor into one meter.  */
Meter *cpumeter_new(XOSView *parent, int cpuid);

int cpumeter_ncpus(void);

#endif
