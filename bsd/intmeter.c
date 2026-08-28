/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "intmeter.h"
#include "kernel.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/*  Index of key in the map, or -1 if it is not there.  Keys are added in
 *  ascending order, so the array stays sorted.  */
static int nums_find(const IntMeter *im, int key) {
	int i;

	for (i = 0; i < im->nnums; i++)
		if (im->nums[i].key == key)
			return i;

	return -1;
}

static void nums_set(IntMeter *im, int key, int index) {
	int at = nums_find(im, key);

	if (at >= 0) {
		im->nums[at].index = index;
		return;
	}
	if (im->nnums == im->numcap) {
		int cap = im->numcap ? im->numcap * 2 : 32;
		BsdIntNum *grown = (BsdIntNum *)realloc(im->nums,
		                                        cap * sizeof(BsdIntNum));

		if (grown == NULL) {
			fprintf(stderr, "Out of memory.\n");
			exit(1);
		}
		im->nums = grown;
		im->numcap = cap;
	}
	im->nums[im->nnums].key = key;
	im->nums[im->nnums].index = index;
	im->nnums++;
}

/*  Index of the first entry with a key above 15, or nnums if there is
 *  none.  */
static int nums_first_high(const IntMeter *im) {
	int i;

	for (i = 0; i < im->nnums; i++)
		if (im->nums[i].key > 15)
			return i;

	return im->nnums;
}

/*  Bounded append, so a machine with a great many interrupts truncates the
 *  legend rather than running off the end of the buffer.  */
static void legend_append(char *buf, size_t size, const char *s) {
	size_t used = strlen(buf);

	if (used + 1 < size)
		strncat(buf, s, size - used - 1);
}

static void getirqs(IntMeter *im) {
	BSDGetIntrStats(im->irqs, im->inbrs);
}

static void updateirqcount(IntMeter *im, int init) {
	int count = 16;
	char legend[512], piece[32];
	int first_high, i;
	unsigned int u;

	if (init) {
		getirqs(im);
		for (i = 0; i < 16; i++)
			nums_set(im, i, i);
	}
	for (u = 16; u <= im->irqcount; u++) {
		if (im->inbrs[u] != 0) {
			nums_set(im, u, count++);
			im->inbrs[u] = 0;
		}
	}
	bitmeter_setnumbits(&im->b, count);

	/*  Build the legend.  */
	strcpy(legend, "0");
	first_high = nums_first_high(im);
	if (first_high == im->nnums) {  /*  only 16 ints  */
		legend_append(legend, sizeof legend, "-15");
	} else {
		int prev = 15, prev2 = 14;

		for (i = first_high; i < im->nnums; i++) {
			int key = im->nums[i].key;

			if (i == im->nnums - 1) {  /*  last element  */
				if (key == prev + 1) {
					legend_append(legend, sizeof legend, "-");
				} else {
					if (prev == prev2 + 1) {
						snprintf(piece, sizeof piece, "-%d", prev);
						legend_append(legend, sizeof legend, piece);
					}
					legend_append(legend, sizeof legend, ",");
				}
				snprintf(piece, sizeof piece, "%d", key);
				legend_append(legend, sizeof legend, piece);
			} else if (key != prev + 1) {
				if (prev == prev2 + 1) {
					snprintf(piece, sizeof piece, "-%d", prev);
					legend_append(legend, sizeof legend, piece);
				}
				snprintf(piece, sizeof piece, ",%d", key);
				legend_append(legend, sizeof legend, piece);
			}
			prev2 = prev;
			prev = key;
		}
	}
	meter_setlegend(&im->b.m, legend);
}

static void checkres(Meter *m) {
	BitMeter *bm = (BitMeter *)m;

	meter_checkresources(m);
	bm->oncolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw,
	                                                      "intOnColor"));
	bm->offcolor = xwin_alloccolor(m->xw, xwin_getresource(m->xw,
	                                                       "intOffColor"));
	m->priority = atoi(xwin_getresource(m->xw, "intPriority"));
}

static void checkevent(Meter *m) {
	IntMeter *im = (IntMeter *)m;
	unsigned int i;

	getirqs(im);

	for (i = 0; i <= im->irqcount; i++) {
		if (im->inbrs[i] != 0) {
			int at = nums_find(im, i);

			if (at < 0) {  /*  new interrupt number  */
				updateirqcount(im, 0);
				return;
			}
			im->b.bits[im->nums[at].index] =
			    ((im->irqs[i] - im->lastirqs[i]) != 0);
			im->lastirqs[i] = im->irqs[i];
		}
	}
	bzero(im->inbrs, (im->irqcount + 1) * sizeof(im->inbrs[0]));
	bzero(im->irqs, (im->irqcount + 1) * sizeof(im->irqs[0]));

	bitmeter_checkevent(m);
}

static void destroy(Meter *m) {
	IntMeter *im = (IntMeter *)m;

	free(im->irqs);
	free(im->lastirqs);
	free(im->inbrs);
	free(im->nums);
	im->irqs = im->lastirqs = NULL;
	im->inbrs = NULL;
	im->nums = NULL;
	bitmeter_fini(m);
}

Meter *intmeter_new(XOSView *parent, int dolegends, int dousedlegends) {
	IntMeter *im = (IntMeter *)meter_alloc(sizeof *im);

	/*  The C++ base call passed dolegends as docaptions and dousedlegends
	 *  as dolegends, leaving dousedlegends at its default of 0.  */
	bitmeter_init(&im->b, parent, "IntMeter", "INTS", "IRQs", 1, dolegends,
	              dousedlegends, 0);
	im->b.m.checkres = checkres;
	im->b.m.checkevent = checkevent;
	im->b.m.destroy = destroy;

	im->nums = NULL;
	im->nnums = im->numcap = 0;

	if (!BSDIntrInit())
		bitmeter_disable(&im->b);
	im->irqcount = BSDNumInts();
	im->irqs = (uint64_t *)calloc(im->irqcount + 1, sizeof(uint64_t));
	im->lastirqs = (uint64_t *)calloc(im->irqcount + 1, sizeof(uint64_t));
	im->inbrs = (unsigned int *)calloc(im->irqcount + 1, sizeof(int));
	updateirqcount(im, 1);

	return &im->b.m;
}
