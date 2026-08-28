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

#include "diskmeter.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <err.h>
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

static void getstats(DiskMeter *dm) {
	FieldMeter *fm = &dm->f;
	uint64_t reads = 0, writes = 0;

	/*  Reset to desired full-scale settings.  */
	fm->total = dm->maxBandwidth;

	fieldmeter_timerstop(fm);
	BSDGetDiskXFerBytes(&reads, &writes);

	/*  Adjust this to bytes/second.  */
#if defined(HAVE_DEVSTAT)
	fm->fields[0] = reads / fieldmeter_secs(fm);
	fm->fields[1] = writes / fieldmeter_secs(fm);
#else
	fm->fields[0] = (reads - dm->prevreads) / fieldmeter_secs(fm);
	fm->fields[1] = (writes - dm->prevwrites) / fieldmeter_secs(fm);
	dm->prevreads = reads;
	dm->prevwrites = writes;
#endif
	fieldmeter_timerstart(fm);

	/*  Adjust in case of first call.  */
	if (fm->fields[0] < 0.0)
		fm->fields[0] = 0.0;
	if (fm->fields[1] < 0.0)
		fm->fields[1] = 0.0;

	/*  Adjust total if needed.  */
	if (fm->fields[0] + fm->fields[1] > fm->total)
		fm->total = fm->fields[0] + fm->fields[1];

	fm->fields[2] = fm->total - (fm->fields[0] + fm->fields[1]);
	if (fm->fields[0] < 0.0)
		warnx("diskmeter: fields[0] of %f is < 0!", fm->fields[0]);
	if (fm->fields[1] < 0.0)
		warnx("diskmeter: fields[1] of %f is < 0!", fm->fields[1]);
	if (fm->fields[2] < 0.0)
		warnx("diskmeter: fields[2] of %f is < 0!", fm->fields[2]);

	fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
}

static void checkevent(Meter *m) {
	getstats((DiskMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *diskmeter_new(XOSView *parent, double max) {
	DiskMeter *dm = (DiskMeter *)meter_alloc(sizeof *dm);

	fieldmeter_init(&dm->f, parent, 3, "DiskMeter", "DISK",
	                "READ/WRITE/IDLE", 0, 0, 0);
	dm->f.m.checkres = checkres;
	dm->f.m.checkevent = checkevent;

	dm->f.dodecay = 0;
	dm->maxBandwidth = max;
	dm->f.total = max;
#ifndef HAVE_DEVSTAT
	dm->prevreads = dm->prevwrites = 0;
#endif
	if (!BSDDiskInit())
		fieldmeter_disable(&dm->f);

	/*  Since at the first call it will look like we transferred a gazillion
	 *  bytes, reset total again and do another call.  This forces total to
	 *  be something reasonable.  */
	getstats(dm);
	dm->f.total = max;
	getstats(dm);
	fieldmeter_timerstart(&dm->f);
	/*  By doing this check, we eliminate the startup situation where all
	 *  fields are 0, and total is 0, leading to nothing being drawn on the
	 *  meter.  So, make it look like nothing was transferred, out of a
	 *  total of 1 byte.  */
	dm->f.fields[0] = dm->f.fields[1] = 0.0;
	dm->f.total = 1.0;
	dm->f.fields[2] = dm->f.total;

	return &dm->f.m;
}
