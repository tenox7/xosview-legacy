/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#include "loadmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef NO_GETLOADAVG
#ifndef FSCALE
#define FSCALE (1<<8)
#endif
#else
#include <sys/loadavg.h>
#endif

static void checkres(Meter *m) {
	LoadMeter *lm = (LoadMeter *)m;
	FieldMeter *fm = &lm->f;
	const char *warn, *crit;

	fieldmeter_checkresources(m);

	lm->warnloadcol = xwin_alloccolor(m->xw,
	                                  xwin_getresource(m->xw,
	                                                   "loadWarnColor"));
	lm->procloadcol = xwin_alloccolor(m->xw,
	                                  xwin_getresource(m->xw,
	                                                   "loadProcColor"));
	lm->critloadcol = xwin_alloccolor(m->xw,
	                                  xwin_getresource(m->xw,
	                                                   "loadCritColor"));

	fieldmeter_setcolor(fm, 0, lm->procloadcol);
	fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "loadIdleColor"));
	m->priority = atoi(xwin_getresource(m->xw, "loadPriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "loadDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "loadGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "loadUsedFormat"));
	lm->do_cpu_speed = xwin_isresourcetrue(m->xw, "loadCpuSpeed");

	warn = xwin_getresource(m->xw, "loadWarnThreshold");
	if (strncmp(warn, "auto", 2) == 0)
		lm->warnThreshold = sysconf(_SC_NPROCESSORS_ONLN);
	else
		lm->warnThreshold = atoi(warn);

	crit = xwin_getresource(m->xw, "loadCritThreshold");
	if (strncmp(crit, "auto", 2) == 0)
		lm->critThreshold = lm->warnThreshold * 4;
	else
		lm->critThreshold = atoi(crit);

	if (fm->dodecay) {
		/*
		 * Warning: Since the loadmeter changes scale occasionally, old
		 * decay values need to be rescaled.  However, if they are
		 * rescaled, they could go off the edge of the screen.  Thus,
		 * for now, to prevent this whole problem, the load meter can
		 * not be a decay meter.  The load is a decaying average kind
		 * of thing anyway, so having a decaying load average is
		 * redundant.
		 */
		fprintf(stderr, "Warning:  The loadmeter can not be configured "
		        "as a decay\n  meter.  See the source code (%s) for "
		        "further\n  details.\n", __FILE__);
		fm->dodecay = 0;
	}
}

static void getloadinfo(LoadMeter *lm) {
	FieldMeter *fm = &lm->f;
	int alarmstate;
	unsigned int i;
#ifdef NO_GETLOADAVG
	/*  This code is mainly for Solaris 6 and earlier, but should work on
	 *  any version.  */
	kstat_named_t *k;

	if (kstat_read(lm->kc, lm->ksp, NULL) == -1) {
		xwin_setdone(fm->m.xw, 1);
		return;
	}
	k = (kstat_named_t *)kstat_data_lookup(lm->ksp, "avenrun_1min");
	if (k == NULL) {
		xwin_setdone(fm->m.xw, 1);
		return;
	}
	fm->fields[0] = kstat_to_double(k) / FSCALE;
#else
	/*  getloadavg() is found on Solaris 7 and newer.  */
	getloadavg(&fm->fields[0], 1);
#endif

	if (fm->fields[0] < lm->warnThreshold)
		alarmstate = 0;
	else if (fm->fields[0] >= lm->critThreshold)
		alarmstate = 2;
	else  /*  fields[0] >= warnThreshold  */
		alarmstate = 1;

	if (alarmstate != lm->lastalarmstate) {
		if (alarmstate == 0)
			fieldmeter_setcolor(fm, 0, lm->procloadcol);
		else if (alarmstate == 1)
			fieldmeter_setcolor(fm, 0, lm->warnloadcol);
		else  /*  alarmstate == 2  */
			fieldmeter_setcolor(fm, 0, lm->critloadcol);
		fieldmeter_drawlegend(fm);
		lm->lastalarmstate = alarmstate;
	}

	/*  Adjust total to next power-of-two of the current load.  */
	if ((fm->fields[0] * 5.0 < fm->total && fm->total > 1.0)
	    || fm->fields[0] > fm->total) {
		i = fm->fields[0];
		i |= i >> 1; i |= i >> 2; i |= i >> 4; i |= i >> 8; i |= i >> 16;
		fm->total = i + 1;  /*  i was 2^n - 1  */
	}

	fm->fields[1] = fm->total - fm->fields[0];
	fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void getspeedinfo(LoadMeter *lm) {
	unsigned int total_mhz = 0, i = 0;
	kstat_named_t *k;
	kstat_t *cpu;

	kstatlist_update(lm->cpulist, lm->kc);

	for (i = 0; i < kstatlist_count(lm->cpulist); i++) {
		cpu = kstatlist_at(lm->cpulist, i);
		if (kstat_read(lm->kc, cpu, NULL) == -1) {
			xwin_setdone(lm->f.m.xw, 1);
			return;
		}
		/*  Try current_clock_Hz first (needs frequency scaling
		 *  support), then clock_MHz.  */
		k = (kstat_named_t *)kstat_data_lookup(cpu, "current_clock_Hz");
		if (k == NULL) {
			k = (kstat_named_t *)kstat_data_lookup(cpu, "clock_MHz");
			if (k == NULL) {
				fprintf(stderr, "CPU speed is not available.\n");
				xwin_setdone(lm->f.m.xw, 1);
				return;
			}
			XOSDEBUG("Speed of cpu %u is %llu MHz\n", i,
			         kstat_to_ui64(k));
			total_mhz += kstat_to_ui64(k);
		} else {
			XOSDEBUG("Speed of cpu %u is %llu Hz\n", i,
			         kstat_to_ui64(k));
			total_mhz += (kstat_to_ui64(k) / 1000000);
		}
	}
	lm->old_cpu_speed = lm->cur_cpu_speed;
	lm->cur_cpu_speed = (i > 0 ? total_mhz / i : 0);
}

static void checkevent(Meter *m) {
	LoadMeter *lm = (LoadMeter *)m;

	getloadinfo(lm);
	if (lm->do_cpu_speed) {
		getspeedinfo(lm);
		if (lm->old_cpu_speed != lm->cur_cpu_speed) {
			/*  update the legend  */
			char l[32];
			snprintf(l, sizeof l, "PROCS/MIN %u MHz",
			         lm->cur_cpu_speed);
			meter_setlegend(m, l);
			fieldmeter_drawlegend(&lm->f);
		}
	}
	fieldmeter_drawfields(&lm->f, 0);
}

Meter *loadmeter_new(XOSView *parent, kstat_ctl_t *kc) {
	LoadMeter *lm = (LoadMeter *)meter_alloc(sizeof *lm);

	fieldmeter_init(&lm->f, parent, 2, "LoadMeter", "LOAD", "PROCS/MIN",
	                1, 1, 0);
	lm->f.m.checkres = checkres;
	lm->f.m.checkevent = checkevent;

	lm->kc = kc;
	lm->cpulist = kstatlist_get(kc, KSL_CPU_INFO);
	lm->do_cpu_speed = 0;
	lm->warnThreshold = lm->critThreshold = 0;
	lm->procloadcol = lm->warnloadcol = lm->critloadcol = 0;
#ifdef NO_GETLOADAVG
	lm->ksp = kstat_lookup(kc, "unix", 0, "system_misc");
	if (lm->ksp == NULL)
		xwin_setdone((XWin *)parent, 1);
#endif
	lm->f.total = -1;
	lm->lastalarmstate = -1;
	lm->old_cpu_speed = lm->cur_cpu_speed = 0;

	return &lm->f.m;
}
