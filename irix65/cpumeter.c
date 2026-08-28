/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#include "cpumeter.h"
#include "xosview.h"
#include "xwin.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int cpumeter_ncpus(void) {
    /*  FIXME: use NAPROCS + identify 'unused procs'  */
    return sysmp(MP_NPROCS);
}

static const char *cpuStr(int num) {
    static char buffer[32];
    const int nCPU = cpumeter_ncpus();

    if (nCPU == 1 || num == -1)
        snprintf(buffer, sizeof buffer, "CPU");
    else if (nCPU <= 10)
        snprintf(buffer, sizeof buffer, "#%d", num);
    else if (nCPU <= 100)
        snprintf(buffer, sizeof buffer, "#%02d", num);
    else if (nCPU <= 1000)
        snprintf(buffer, sizeof buffer, "#%03d", num);
    else
        snprintf(buffer, sizeof buffer, "%4.1d", num);

    return buffer;
}

static const char *toUpper(const char *str) {
    static char buffer[256];
    char *tmp;

    strncpy(buffer, str, sizeof buffer);
    buffer[sizeof buffer - 1] = '\0';
    for (tmp = buffer; *tmp != '\0'; tmp++)
        *tmp = toupper(*tmp);

    return buffer;
}

static void checkres(Meter *m) {
    FieldMeter *fm = (FieldMeter *)m;

    fieldmeter_checkresources(m);

    fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "cpuUserColor"));
    fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "cpuSystemColor"));
    fieldmeter_setcolorname(fm, 2,
                            xwin_getresource(m->xw, "cpuInterruptColor"));
    fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "cpuWaitColor"));
    fieldmeter_setcolorname(fm, 4, xwin_getresource(m->xw, "cpuFreeColor"));
    m->priority = atoi(xwin_getresource(m->xw, "cpuPriority"));
    fm->dodecay = xwin_isresourcetrue(m->xw, "cpuDecay");
    fm->usegraph = xwin_isresourcetrue(m->xw, "cpuGraph");
    fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "cpuUsedFormat"));
}

static void getcputime(CPUMeter *cm) {
    FieldMeter *fm = &cm->f;
    int i, oldindex;

    fm->total = 0;

    if (cm->cpuid == -1)
        sysmp(MP_SAGET, MPSA_SINFO, (char *)&cm->tsp, cm->sinfosz);
    else
        sysmp(MP_SAGET1, MPSA_SINFO, (char *)&cm->tsp, cm->sinfosz, cm->cpuid);

    cm->tsp.cpu[CPU_WAIT] -= (cm->tsp.wait[W_GFXF] + cm->tsp.wait[W_GFXC]);

    cm->cputime[cm->cpuindex][0] = cm->tsp.cpu[CPU_USER];
    cm->cputime[cm->cpuindex][1] = cm->tsp.cpu[CPU_KERNEL];
    cm->cputime[cm->cpuindex][2] = cm->tsp.cpu[CPU_INTR];
    cm->cputime[cm->cpuindex][3] = cm->tsp.cpu[CPU_WAIT];
    cm->cputime[cm->cpuindex][4] = cm->tsp.cpu[CPU_IDLE]
                                   + cm->tsp.cpu[CPU_SXBRK];

    oldindex = (cm->cpuindex + 1) % 2;
    for (i = 0; i < USED_CPU_STATES; i++) {
        fm->fields[i] = cm->cputime[cm->cpuindex][i] - cm->cputime[oldindex][i];
        fm->total += fm->fields[i];
    }
    cm->cpuindex = (cm->cpuindex + 1) % 2;

    /*  wait does not count as used  */
    if (fm->total)
        fieldmeter_setused(fm, fm->total - fm->fields[3] - fm->fields[4],
                           fm->total);
}

static void checkevent(Meter *m) {
    getcputime((CPUMeter *)m);
    fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *cpumeter_new(XOSView *parent, int cpuid) {
    CPUMeter *cm = (CPUMeter *)meter_alloc(sizeof *cm);
    int i, j;

    fieldmeter_init(&cm->f, parent, USED_CPU_STATES, "CPUMeter",
                    toUpper(cpuStr(cpuid)), "USER/SYS/INTR/WAIT/IDLE",
                    0, 0, 0);
    cm->f.m.checkres = checkres;
    cm->f.m.checkevent = checkevent;

    for (i = 0; i < 2; i++)
        for (j = 0; j < USED_CPU_STATES; j++)
            cm->cputime[i][j] = 0;

    cm->cpuid = cpuid;
    cm->cpuindex = 0;

    if ((cm->sinfosz = sysmp(MP_SASZ, MPSA_SINFO)) < 0) {
        fprintf(stderr, "sysinfo scall interface not supported\n");
        xwin_setdone((XWin *)parent, 1);
    }

    return &cm->f.m;
}
