/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  NetBSD port:
 *  Copyright (c) 1995, 1996, 1997-2002 by Brian Grayson (bgrayson@netbsd.org)
 *
 *  This file was written by Brian Grayson for the NetBSD and xosview
 *    projects.
 *  This file may be distributed under terms of the GPL or of the BSD
 *    license, whichever you choose.  The full license notices are
 *    contained in the files COPYING.GPL and COPYING.BSD, which you
 *    should have received.  If not, contact one of the xosview
 *    authors for a copy.
 */
/*
 *  The NetBSD memmeter was improved by Tom Pavel (pavel\@slac.stanford.edu)
 *    to provide active and inactive values, rather than just "used."
 */

#include "memmeter.h"
#include "defines.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>

static void checkres(Meter *m) {
	FieldMeter *fm = (FieldMeter *)m;

	fieldmeter_checkresources(m);

	fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "memActiveColor"));
	fieldmeter_setcolorname(fm, 1,
	                        xwin_getresource(m->xw, "memInactiveColor"));
	fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "memWiredColor"));
#if defined(HAVE_UVM)
	fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "memFreeColor"));
#else
	fieldmeter_setcolorname(fm, 3, xwin_getresource(m->xw, "memCacheColor"));
	fieldmeter_setcolorname(fm, 4, xwin_getresource(m->xw, "memFreeColor"));
#endif
	m->priority = atoi(xwin_getresource(m->xw, "memPriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "memDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "memGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "memUsedFormat"));
}

static void getmeminfo(MemMeter *mm) {
	FieldMeter *fm = &mm->f;

	BSDGetPageStats(mm->meminfo, NULL);
	fm->fields[0] = (double)mm->meminfo[0];
	fm->fields[1] = (double)mm->meminfo[1];
	fm->fields[2] = (double)mm->meminfo[2];
#if defined(HAVE_UVM)
	fm->fields[3] = (double)mm->meminfo[4];
#else
	fm->fields[3] = (double)mm->meminfo[3];
	fm->fields[4] = (double)mm->meminfo[4];
#endif
	fm->total = (double)(mm->meminfo[0] + mm->meminfo[1] + mm->meminfo[2]
	                     + mm->meminfo[3] + mm->meminfo[4]);
	fieldmeter_setused(fm, fm->total - (double)mm->meminfo[4], fm->total);
}

static void checkevent(Meter *m) {
	getmeminfo((MemMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *memmeter_new(XOSView *parent) {
	MemMeter *mm = (MemMeter *)meter_alloc(sizeof *mm);

#if defined(HAVE_UVM)
	fieldmeter_init(&mm->f, parent, 4, "MemMeter", "MEM",
	                "ACT/INACT/WRD/FREE", 0, 0, 0);
#else
	fieldmeter_init(&mm->f, parent, 5, "MemMeter", "MEM",
	                "ACT/INACT/WRD/CA/FREE", 0, 0, 0);
#endif
	mm->f.m.checkres = checkres;
	mm->f.m.checkevent = checkevent;

	BSDPageInit();

	return &mm->f.m;
}
