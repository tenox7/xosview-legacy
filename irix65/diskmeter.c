/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "diskmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>

/*  bread and bwrite are the blocks the buffer cache moves to and from the
 *  disks, what sar(1) reports as bread/s and bwrite/s, and phread and
 *  phwrite are the same for raw device traffic, which bypasses the cache.
 *  All four are 512 byte blocks and are machine wide, so this meter has no
 *  per drive breakdown.  */

#define DISK_BLOCK 512

static int diskinfo(int sinfosz, double *readp, double *writep) {
    struct sysinfo si;

    if (sinfosz <= 0
        || sysmp(MP_SAGET, MPSA_SINFO, (char *)&si, sinfosz) < 0)
        return 0;

    *readp = ((double)si.bread + si.phread) * DISK_BLOCK;
    *writep = ((double)si.bwrite + si.phwrite) * DISK_BLOCK;

    return 1;
}

static void checkres(Meter *m) {
    DiskMeter *dm = (DiskMeter *)m;
    FieldMeter *fm = &dm->f;

    fieldmeter_checkresources(m);

    m->priority = atoi(xwin_getresource(m->xw, "diskPriority"));

    /*  fieldmeter_disable() collapsed this meter to a single field, so
     *  setting the per field colours below would run off the end of the
     *  array.  */
    if (!dm->ok)
        return;

    fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "diskReadColor"));
    fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "diskWriteColor"));
    fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "diskIdleColor"));
    fm->dodecay = xwin_isresourcetrue(m->xw, "diskDecay");
    fm->usegraph = xwin_isresourcetrue(m->xw, "diskGraph");
    fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "diskUsedFormat"));
}

static void getdiskinfo(DiskMeter *dm) {
    FieldMeter *fm = &dm->f;
    double nowread, nowwrite, t;

    fm->total = dm->maxspeed;

    if (!diskinfo(dm->sinfosz, &nowread, &nowwrite))
        return;

    fieldmeter_timerstop(fm);

    if (dm->first) {
        dm->readprev = nowread;
        dm->writeprev = nowwrite;
        dm->first = 0;
    }

    /*  These counters are 32 bit and wrap on a busy machine, which shows up
     *  as one going backwards.  Skip that sample rather than plotting an
     *  enormous spike.  */
    t = fieldmeter_secs(fm);
    fm->fields[0] = nowread >= dm->readprev ? (nowread - dm->readprev) / t : 0.0;
    fm->fields[1] = nowwrite >= dm->writeprev
                    ? (nowwrite - dm->writeprev) / t : 0.0;

    fieldmeter_timerstart(fm);
    dm->readprev = nowread;
    dm->writeprev = nowwrite;

    if (fm->fields[0] + fm->fields[1] > fm->total)
        fm->total = fm->fields[0] + fm->fields[1];
    fm->fields[2] = fm->total - (fm->fields[0] + fm->fields[1]);

    fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
}

static void checkevent(Meter *m) {
    getdiskinfo((DiskMeter *)m);
    fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *diskmeter_new(XOSView *parent, float max) {
    DiskMeter *dm = (DiskMeter *)meter_alloc(sizeof *dm);
    double rd, wr;

    fieldmeter_init(&dm->f, parent, 3, "DiskMeter", "DISK", "READ/WRITE/IDLE",
                    0, 0, 0);
    dm->f.m.checkres = checkres;
    dm->f.m.checkevent = checkevent;

    dm->maxspeed = max;
    dm->readprev = dm->writeprev = 0;
    dm->first = 1;
    dm->sinfosz = sysmp(MP_SASZ, MPSA_SINFO);
    dm->ok = diskinfo(dm->sinfosz, &rd, &wr);

    if (!dm->ok)
        fieldmeter_disable(&dm->f);

    return &dm->f.m;
}
