/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "swapmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <sys/pstat.h>

#define MAX_SWAP_AREAS 16

/*  Size of one swap pool, in 1 KB blocks.  */
static unsigned long swapblocks(const struct pst_swapinfo *si) {
#ifdef NO_PSS_NBLKSENABLED
  /*  HP-UX 9 has no pss_nblksenabled and keeps the size of a block device
   *  pool and of a file system pool in a union instead.  */
  if (!(si->pss_flags & SW_ENABLED))
    return 0;
  return (si->pss_flags & SW_BLOCK) ? si->pss_nblks : si->pss_allocated;
#else
  return si->pss_nblksenabled;
#endif
}

static void checkres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "swapUsedColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "swapFreeColor"));
  m->priority = atoi(xwin_getresource(m->xw, "swapPriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "swapDecay");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "swapUsedFormat"));
}

static void getswapinfo(SwapMeter *sm) {
  FieldMeter *fm = &sm->f;
  struct pst_swapinfo swapinfo;
  int i;

  fm->total = 0;
  fm->fields[1] = 0;

  for (i = 0; i < MAX_SWAP_AREAS; i++) {
    pstat_getswap(&swapinfo, sizeof(swapinfo), 1, i);
    if (swapinfo.pss_idx == (unsigned)i) {
      /*  In doubles: 4 GB of swap overflows a 32 bit block count.  */
      fm->total += (double)swapblocks(&swapinfo) * 1024.0;
      fm->fields[1] += (double)swapinfo.pss_nfpgs * 4.0 * 1024.0;
    }
  }

  fm->fields[0] = fm->total - fm->fields[1];
  fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  SwapMeter *sm = (SwapMeter *)m;

  sm->pass = (sm->pass + 1) % 5;
  if (sm->pass != 0)
    return;

  getswapinfo(sm);
  fieldmeter_drawfields(&sm->f, 0);
}

Meter *swapmeter_new(XOSView *parent) {
  SwapMeter *sm = (SwapMeter *)meter_alloc(sizeof *sm);

  fieldmeter_init(&sm->f, parent, 2, "SwapMeter", "SWAP", "USED/FREE",
                  0, 0, 0);
  sm->f.m.checkres = checkres;
  sm->f.m.checkevent = checkevent;
  sm->pass = 0;

  return &sm->f.m;
}
