/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#include "gfxmeter.h"
#include "xosview.h"
#include "xwin.h"

#include <invent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*  GfxMeter displays swapbuffers per second.  max is the base rate for one
 *  gfx pipe.  */

int gfxmeter_npipes(void) {
    inventory_t *inv;
    int n = 0;

    setinvent();
    for (inv = getinvent(); inv != NULL; inv = getinvent())
        if (inv->inv_class == INV_GRAPHICS)
            n++;

    return n;
}

static void checkres(Meter *m) {
    GfxMeter *gm = (GfxMeter *)m;
    FieldMeter *fm = &gm->f;

    fieldmeter_checkresources(m);

    gm->swapgfxcol = xwin_alloccolor(m->xw,
                                     xwin_getresource(m->xw, "gfxSwapColor"));
    gm->warngfxcol = xwin_alloccolor(m->xw,
                                     xwin_getresource(m->xw, "gfxWarnColor"));
    gm->critgfxcol = xwin_alloccolor(m->xw,
                                     xwin_getresource(m->xw, "gfxCritColor"));

    fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "gfxSwapColor"));
    fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "gfxIdleColor"));
    m->priority = atoi(xwin_getresource(m->xw, "gfxPriority"));
    fm->usegraph = xwin_isresourcetrue(m->xw, "gfxGraph");
    fm->dodecay = xwin_isresourcetrue(m->xw, "gfxDecay");
    fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "gfxUsedFormat"));

    gm->warnThreshold = atoi(xwin_getresource(m->xw, "gfxWarnThreshold"))
                        * gm->nPipes;
    gm->critThreshold = atoi(xwin_getresource(m->xw, "gfxCritThreshold"))
                        * gm->nPipes;

    if (fm->dodecay) {
        /*  Warning:  Since the gfxmeter changes scale occasionally, old
         *  decay values need to be rescaled.  However, if they are rescaled,
         *  they could go off the edge of the screen.  Thus, for now, to
         *  prevent this whole problem, the gfx meter can not be a decay
         *  meter.  The gfx is a decaying average kind of thing anyway, so
         *  having a decaying gfx average is redundant.  */
        fprintf(stderr, "Warning:  The gfxmeter can not be configured as a "
                "decay\n  meter. See the source code (%s) for further\n"
                "  details.\n", __FILE__);
        fm->dodecay = 0;
    }
}

static void getgfxinfo(GfxMeter *gm) {
    FieldMeter *fm = &gm->f;
    SarGfxInfo *gi = sarmeter_gfxinfo();

    fm->fields[0] = (float)gi->swapBuf;

    if (fm->fields[0] < gm->warnThreshold)
        gm->alarmstate = 0;
    else if (fm->fields[0] >= gm->critThreshold)
        gm->alarmstate = 2;
    else
        gm->alarmstate = 1;

    if (gm->alarmstate != gm->lastalarmstate) {
        if (gm->alarmstate == 0)
            fieldmeter_setcolor(fm, 0, gm->swapgfxcol);
        else if (gm->alarmstate == 1)
            fieldmeter_setcolor(fm, 0, gm->warngfxcol);
        else
            fieldmeter_setcolor(fm, 0, gm->critgfxcol);
        fieldmeter_drawlegend(fm);
        gm->lastalarmstate = gm->alarmstate;
    }

    if (fm->fields[0] * 5.0 < fm->total)
        fm->total = fm->fields[0];
    else if (fm->fields[0] > fm->total)
        fm->total = fm->fields[0] * 5.0;

    if (fm->total < 1.0)
        fm->total = 1.0;

    fm->fields[1] = fm->total - fm->fields[0];

    fieldmeter_setused(fm, fm->fields[0], 1.0);
}

static void checkevent(Meter *m) {
    getgfxinfo((GfxMeter *)m);
    fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *gfxmeter_new(XOSView *parent, int max) {
    GfxMeter *gm = (GfxMeter *)meter_alloc(sizeof *gm);

    fieldmeter_init(&gm->f, parent, 2, "GfxMeter", "GFX", "SWAPBUF/S",
                    1, 1, 0);
    gm->f.m.checkres = checkres;
    gm->f.m.checkevent = checkevent;

    gm->lastalarmstate = -1;
    gm->alarmstate = 0;
    gm->warnThreshold = gm->critThreshold = 0;
    gm->swapgfxcol = gm->warngfxcol = gm->critgfxcol = 0;
    gm->nPipes = gfxmeter_npipes();
    gm->f.total = gm->nPipes * max;

    if (gm->f.total == 0)
        xwin_setdone((XWin *)parent, 1);

    return &gm->f.m;
}
