/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#include "diskmeter.h"
#include "sarmeter.h"
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

    sarmeter_diskinfo();

    /*  new data  */
    fm->total = dm->maxspeed;

    if (fm->fields[0] + fm->fields[1] > fm->total)
        fm->total = fm->fields[0] + fm->fields[1];

    fm->fields[2] = fm->total - (fm->fields[0] + fm->fields[1]);

    fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
    fieldmeter_timerstart(fm);
}

static void checkevent(Meter *m) {
    getdiskinfo((DiskMeter *)m);
    fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *diskmeter_new(XOSView *parent, float max) {
    DiskMeter *dm = (DiskMeter *)meter_alloc(sizeof *dm);

    fieldmeter_init(&dm->f, parent, 3, "DiskMeter", "DISK", "READ/WRITE/IDLE",
                    0, 0, 0);
    dm->f.m.checkres = checkres;
    dm->f.m.checkevent = checkevent;

    dm->maxspeed = max;
    getdiskinfo(dm);

    return &dm->f.m;
}
