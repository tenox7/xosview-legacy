/*
 *  Rewritten for Solaris by Arno Augustin 1999
 *  augustin@informatik.uni-erlangen.de
 */

#include "diskmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

static void checkres(Meter *m) {
    FieldMeter *fm = (FieldMeter *)m;

    fieldmeter_checkresources(m);

    fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "diskReadColor"));
    fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "diskWriteColor"));
    fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "diskIdleColor"));
    m->priority = atoi(xwin_getresource(m->xw, "diskPriority"));
    fm->dodecay = xwin_isresourcetrue(m->xw, "diskDecay");
    fm->usegraph = xwin_isresourcetrue(m->xw, "diskGraph");
    fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "diskUsedFormat"));
}

static void getdiskinfo(DiskMeter *dm) {
    FieldMeter *fm = &dm->f;
    kstat_io_t kio;
    uint64_t read_curr = 0, write_curr = 0;
    unsigned int i;
    double t;

    fm->total = dm->maxspeed;
    kstatlist_update(dm->disks, dm->kc);

    fieldmeter_timerstop(fm);
    for (i = 0; i < kstatlist_count(dm->disks); i++) {
        kstat_t *ksp = kstatlist_at(dm->disks, i);
        if (kstat_read(dm->kc, ksp, &kio) == -1)
            continue;
        XOSDEBUG("%s: %llu bytes read %llu bytes written.\n",
                 ksp->ks_name, kio.nread, kio.nwritten);
        read_curr += kio.nread;
        write_curr += kio.nwritten;
    }
    if (dm->read_prev == 0)
        dm->read_prev = read_curr;
    if (dm->write_prev == 0)
        dm->write_prev = write_curr;

    t = fieldmeter_secs(fm);
    fm->fields[0] = (double)(read_curr - dm->read_prev) / t;
    fm->fields[1] = (double)(write_curr - dm->write_prev) / t;

    fieldmeter_timerstart(fm);
    dm->read_prev = read_curr;
    dm->write_prev = write_curr;

    if (fm->fields[0] < 0)
        fm->fields[0] = 0;
    if (fm->fields[1] < 0)
        fm->fields[1] = 0;
    if (fm->fields[0] + fm->fields[1] > fm->total)
        fm->total = fm->fields[0] + fm->fields[1];
    fm->fields[2] = fm->total - (fm->fields[0] + fm->fields[1]);
    fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
}

static void checkevent(Meter *m) {
    getdiskinfo((DiskMeter *)m);
    fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *diskmeter_new(XOSView *parent, kstat_ctl_t *kc, float max) {
    DiskMeter *dm = (DiskMeter *)meter_alloc(sizeof *dm);

    fieldmeter_init(&dm->f, parent, 3, "DiskMeter", "DISK", "READ/WRITE/IDLE",
                    0, 0, 0);
    dm->f.m.checkres = checkres;
    dm->f.m.checkevent = checkevent;

    dm->kc = kc;
    dm->read_prev = dm->write_prev = 0;
    dm->maxspeed = max;
    dm->disks = kstatlist_get(kc, KSL_DISKS);

    return &dm->f.m;
}
