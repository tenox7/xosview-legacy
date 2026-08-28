/*
 *  Initial port performed by Greg Onufer (exodus@cheers.bungi.com)
 */

#include "swapmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <unistd.h>

static void checkres(Meter *m) {
	FieldMeter *fm = (FieldMeter *)m;

	fieldmeter_checkresources(m);

	fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "swapUsedColor"));
	fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "swapFreeColor"));
	m->priority = atoi(xwin_getresource(m->xw, "swapPriority"));
	fm->dodecay = xwin_isresourcetrue(m->xw, "swapDecay");
	fm->usegraph = xwin_isresourcetrue(m->xw, "swapGraph");
	fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "swapUsedFormat"));
}

static void getswapinfo(SwapMeter *sm) {
	FieldMeter *fm = &sm->f;
	swaptbl_t *swaps;
	char *names;
	int numswap, i;

	fm->total = fm->fields[0] = fm->fields[1] = 0;
	numswap = swapctl(SC_GETNSWP, NULL);
	if (numswap < 0) {
		fprintf(stderr, "Can not determine number of swap spaces.\n");
		xwin_setdone(fm->m.xw, 1);
		return;
	}
	if (numswap > 0) {
		swaps = (swaptbl_t *)malloc(sizeof(swaptbl_t)
		                            + numswap * sizeof(swapent_t));
		names = (char *)calloc(numswap + 1, PATH_MAX);
		if (!swaps || !names) {
			fprintf(stderr, "malloc failed.\n");
			xwin_setdone(fm->m.xw, 1);
			free(swaps);
			free(names);
			return;
		}
		swaps->swt_n = numswap;
		for (i = 0; i <= numswap; i++)
			swaps->swt_ent[i].ste_path = names + (i * PATH_MAX);

		if (swapctl(SC_LIST, swaps) < 0) {
			fprintf(stderr, "Can not get list of swap spaces.\n");
			xwin_setdone(fm->m.xw, 1);
			free(swaps);
			free(names);
			return;
		}
		for (i = 0; i < numswap; i++) {
			fm->total += swaps->swt_ent[i].ste_pages;
			fm->fields[1] += swaps->swt_ent[i].ste_free;
			XOSDEBUG("%s: %ld kB (%ld kB free)\n",
			         swaps->swt_ent[i].ste_path,
			         swaps->swt_ent[i].ste_pages
			           * (long)(sm->pagesize / 1024),
			         swaps->swt_ent[i].ste_free
			           * (long)(sm->pagesize / 1024));
		}
		fm->fields[0] = fm->total - fm->fields[1];
		free(swaps);
		free(names);
	}

	fieldmeter_setused(fm, fm->fields[0] * sm->pagesize,
	                   fm->total * sm->pagesize);
}

static void checkevent(Meter *m) {
	getswapinfo((SwapMeter *)m);
	fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *swapmeter_new(XOSView *parent, kstat_ctl_t *kc) {
	SwapMeter *sm = (SwapMeter *)meter_alloc(sizeof *sm);

	(void) kc;
	fieldmeter_init(&sm->f, parent, 2, "SwapMeter", "SWAP", "USED/FREE",
	                0, 0, 0);
	sm->f.m.checkres = checkres;
	sm->f.m.checkevent = checkevent;

	sm->pagesize = sysconf(_SC_PAGESIZE);

	return &sm->f.m;
}
