/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *  2007 by Samuel Thibault ( samuel.thibault@ens-lyon.org )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "swapmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdlib.h>
#include <error.h>

#include <mach.h>
#include <mach/mach_traps.h>
#include <mach/default_pager.h>
#include "get_def_pager.h"

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
  kern_return_t err;

  if (sm->def_pager == MACH_PORT_NULL)
    sm->def_pager = get_def_pager();

  if (!MACH_PORT_VALID(sm->def_pager)) {
    sm->def_pager = MACH_PORT_DEAD;
    xwin_setdone(fm->m.xw, 1);
    return;
  }

  err = default_pager_info(sm->def_pager, &sm->def_pager_info);
  if (err) {
    error(0, err, "default_pager_info");
    xwin_setdone(fm->m.xw, 1);
    return;
  }

  fm->total = sm->def_pager_info.dpi_total_space;
  fm->fields[1] = sm->def_pager_info.dpi_free_space;
  fm->fields[0] = fm->total - fm->fields[1];

  if (fm->total)
    fieldmeter_setused(fm, fm->fields[0], fm->total);
}

static void checkevent(Meter *m) {
  getswapinfo((SwapMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *swapmeter_new(XOSView *parent) {
  SwapMeter *sm = (SwapMeter *)meter_alloc(sizeof *sm);

  fieldmeter_init(&sm->f, parent, 2, "SwapMeter", "SWAP", "ACTIVE/USED/FREE",
                  0, 0, 0);
  sm->f.m.checkres = checkres;
  sm->f.m.checkevent = checkevent;

  sm->def_pager = MACH_PORT_NULL;

  return &sm->f.m;
}
