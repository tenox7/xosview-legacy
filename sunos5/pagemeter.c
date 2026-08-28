/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#include "pagemeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <sys/sysinfo.h>

static void checkres(Meter *m) {
	PageMeter *pm = (PageMeter *)m;
	FieldMeter *fm = &pm->f;

	fieldmeter_checkresources(m);

	fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "pageInColor"));
	fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "pageOutColor"));
	fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "pageIdleColor"));
	m->priority = atoi(xwin_getresource(m->xw, "pagePriority"));
	pm->maxspeed *= m->priority / 10.0;
	fm->dodecay = xwin_isresourcetrue(m->xw, "pageDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "pageGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "pageUsedFormat"));
}

static void getpageinfo(PageMeter *pm) {
	FieldMeter *fm = &pm->f;
	cpu_stat_t cs;
	unsigned int n;
	int i, oldindex;

	fm->total = 0;
	pm->pageinfo[pm->pageindex][0] = 0;  /*  pgin  */
	pm->pageinfo[pm->pageindex][1] = 0;  /*  pgout  */
	kstatlist_update(pm->cpustats, pm->kc);

	for (n = 0; n < kstatlist_count(pm->cpustats); n++) {
		if (kstat_read(pm->kc, kstatlist_at(pm->cpustats, n), &cs) == -1)
			continue;
		pm->pageinfo[pm->pageindex][0] += cs.cpu_vminfo.pgpgin;
		pm->pageinfo[pm->pageindex][1] += cs.cpu_vminfo.pgpgout;
	}

	oldindex = (pm->pageindex + 1) % 2;
	for (i = 0; i < 2; i++) {
		if (pm->pageinfo[oldindex][i] == 0)
			pm->pageinfo[oldindex][i] = pm->pageinfo[pm->pageindex][i];

		fm->fields[i] = pm->pageinfo[pm->pageindex][i]
		                - pm->pageinfo[oldindex][i];
		fm->total += fm->fields[i];
	}

	if (fm->total > pm->maxspeed) {
		fm->fields[2] = 0.0;
	} else {
		fm->fields[2] = pm->maxspeed - fm->total;
		fm->total = pm->maxspeed;
	}

	fieldmeter_setused(fm, fm->total - fm->fields[2], pm->maxspeed);
	pm->pageindex = (pm->pageindex + 1) % 2;
}

static void checkevent(Meter *m) {
	getpageinfo((PageMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *pagemeter_new(XOSView *parent, kstat_ctl_t *kc, float max) {
	PageMeter *pm = (PageMeter *)meter_alloc(sizeof *pm);
	int i, j;

	fieldmeter_init(&pm->f, parent, 3, "PageMeter", "PAGE", "IN/OUT/IDLE",
	                0, 0, 0);
	pm->f.m.checkres = checkres;
	pm->f.m.checkevent = checkevent;

	pm->kc = kc;
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
			pm->pageinfo[j][i] = 0;

	pm->maxspeed = max;
	pm->pageindex = 0;
	pm->cpustats = kstatlist_get(kc, KSL_CPU_STAT);

	return &pm->f.m;
}
