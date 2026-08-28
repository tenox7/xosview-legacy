/*
 *  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
 *  2007 by Samuel Thibault ( samuel.thibault@ens-lyon.org )
 *
 *  This file may be distributed under terms of the GPL
 */
/*
 *  Most of this code was written by Werner Fink <werner\@suse.de>.
 *  Only small changes were made on my part (M.R.)
 */

#include "loadmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <error.h>

#include <mach/mach_traps.h>
#include <mach/mach_host.h>

static void checkres(Meter *m) {
  LoadMeter *lm = (LoadMeter *)m;
  FieldMeter *fm = &lm->f;

  fieldmeter_checkresources(m);

  lm->procloadcol = xwin_alloccolor(m->xw,
                                    xwin_getresource(m->xw, "loadProcColor"));
  lm->warnloadcol = xwin_alloccolor(m->xw,
                                    xwin_getresource(m->xw, "loadWarnColor"));
  lm->critloadcol = xwin_alloccolor(m->xw,
                                    xwin_getresource(m->xw, "loadCritColor"));

  fieldmeter_setcolor(fm, 0, lm->procloadcol);
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "loadIdleColor"));
  m->priority = atoi(xwin_getresource(m->xw, "loadPriority"));
  fm->usegraph = xwin_isresourcetrue(m->xw, "loadGraph");
  fm->dodecay = xwin_isresourcetrue(m->xw, "loadDecay");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "loadUsedFormat"));

  lm->warnThreshold = atoi(xwin_getresource(m->xw, "loadWarnThreshold"));
  lm->critThreshold = atoi(xwin_getresource(m->xw, "loadCritThreshold"));

  if (fm->dodecay) {
    /*  Warning:  Since the loadmeter changes scale occasionally, old decay
     *  values need to be rescaled.  However, if they are rescaled, they
     *  could go off the edge of the screen.  Thus, for now, to prevent this
     *  whole problem, the load meter can not be a decay meter.  The load is
     *  a decaying average kind of thing anyway, so having a decaying load
     *  average is redundant.  */
    fprintf(stderr, "Warning:  The loadmeter can not be configured as a "
            "decay\n  meter.  See the source code (%s) for further\n"
            "  details.\n", __FILE__);
    fm->dodecay = 0;
  }
}

static void getloadinfo(LoadMeter *lm) {
  FieldMeter *fm = &lm->f;
  host_load_info_data_t info;
  mach_msg_type_number_t count = HOST_LOAD_INFO_COUNT;
  kern_return_t err;

  err = host_info(mach_host_self(), HOST_LOAD_INFO, (host_info_t)&info,
                  &count);
  if (err) {
    fprintf(stderr, "Can not get host info");
    xwin_setdone(fm->m.xw, 1);
    return;
  }
  fm->fields[0] = (float)info.avenrun[0] / LOAD_SCALE;

  if (fm->fields[0] < lm->warnThreshold)
    lm->alarmstate = 0;
  else if (fm->fields[0] >= lm->critThreshold)
    lm->alarmstate = 2;
  else  /*  fields[0] >= warnThreshold  */
    lm->alarmstate = 1;

  if (lm->alarmstate != lm->lastalarmstate) {
    if (lm->alarmstate == 0)
      fieldmeter_setcolor(fm, 0, lm->procloadcol);
    else if (lm->alarmstate == 1)
      fieldmeter_setcolor(fm, 0, lm->warnloadcol);
    else  /*  alarmstate == 2  */
      fieldmeter_setcolor(fm, 0, lm->critloadcol);
    fieldmeter_drawlegend(fm);
    lm->lastalarmstate = lm->alarmstate;
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
  getloadinfo((LoadMeter *)m);
  fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *loadmeter_new(XOSView *parent) {
  LoadMeter *lm = (LoadMeter *)meter_alloc(sizeof *lm);

  fieldmeter_init(&lm->f, parent, 2, "LoadMeter", "LOAD", "PROCS/MIN",
                  1, 1, 0);
  lm->f.m.checkres = checkres;
  lm->f.m.checkevent = checkevent;

  lm->lastalarmstate = -1;
  lm->alarmstate = 0;
  lm->warnThreshold = lm->critThreshold = 0;
  lm->procloadcol = lm->warnloadcol = lm->critloadcol = 0;
  lm->f.total = 2.0;

  return &lm->f.m;
}
