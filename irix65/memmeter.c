/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#include "memmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <invent.h>
#include <stdlib.h>
#include <unistd.h>

static void checkres(Meter *m) {
    FieldMeter *fm = (FieldMeter *)m;

    fieldmeter_checkresources(m);

    fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "memKernelColor"));
    fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "memCacheColor"));
    fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "memUsedColor"));
    fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "memFreeColor"));
    m->priority = atoi(xwin_getresource(m->xw, "memPriority"));
    fm->dodecay = xwin_isresourcetrue(m->xw, "memDecay");
    fm->usegraph = xwin_isresourcetrue(m->xw, "memGraph");
    fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "memUsedFormat"));
}

static void getmeminfo(MemMeter *mm) {
    FieldMeter *fm = &mm->f;

    sysmp(MP_SAGET, MPSA_RMINFO, (char *)&mm->mp, mm->minfosz);

    fm->fields[0] = fm->total - (mm->mp.availrmem + mm->mp.bufmem);

#ifdef IRIX5
    /*  5.3 predates the chunk allocator, so the file system share of memory
     *  is just the buffer cache.  */
    fm->fields[1] = mm->mp.bufmem;

    fm->fields[2] = mm->mp.availrmem - mm->mp.freemem;
#else
    fm->fields[1] = mm->mp.bufmem +
        mm->mp.dchunkpages + mm->mp.dpages +
        mm->mp.chunkpages - mm->mp.dchunkpages;

    fm->fields[2] = mm->mp.availrmem -
        (mm->mp.freemem + mm->mp.chunkpages + mm->mp.dpages);
#endif

    fm->fields[3] = mm->mp.freemem;

    fieldmeter_setused(fm,
                       (fm->fields[0] + fm->fields[1] + fm->fields[2])
                         * mm->pageSize,
                       fm->total * mm->pageSize);
}

static void checkevent(Meter *m) {
    getmeminfo((MemMeter *)m);
    fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *memmeter_new(XOSView *parent) {
    MemMeter *mm = (MemMeter *)meter_alloc(sizeof *mm);
    inventory_t *inv;

    fieldmeter_init(&mm->f, parent, 4, "MemMeter", "MEM",
                    "KERNEL/FS/USER/FREE", 0, 0, 0);
    mm->f.m.checkres = checkres;
    mm->f.m.checkevent = checkevent;

    mm->pageSize = getpagesize();
    mm->minfosz = sysmp(MP_SASZ, MPSA_RMINFO);

    mm->f.total = 0;
    setinvent();
    for (inv = getinvent(); inv != NULL; inv = getinvent()) {
        if (inv->inv_class == INV_MEMORY && inv->inv_type == INV_MAIN_MB) {
            mm->f.total = inv->inv_state * 1024 / mm->pageSize * 1024;
            break;
        }
    }
    if (mm->f.total == 0)
        xwin_setdone((XWin *)parent, 1);

    return &mm->f.m;
}
