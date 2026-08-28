/*
 *  Copyright (c) 2014 by Tomi Tapper <tomi.o.tapper@jyu.fi>
 *
 *  This file may be distributed under terms of the GPL
 *
 *  Put code common to *BSD and Linux sensor meters here.
 */

#include "sensorfieldmeter.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void sensorfieldmeter_init(SensorFieldMeter *sm, XOSView *parent,
                           const char *name, const char *title,
                           const char *legend, int docaptions, int dolegends,
                           int dousedlegends) {
  fieldmeter_init(&sm->f, parent, 3, name, title, legend, docaptions,
                  dolegends, dousedlegends);
  sm->f.metric = 1;
  sm->negative = 0;
  sm->unit[0] = '\0';
  sm->high = sm->low = 0.0;
  sm->has_low = sm->has_high = 0;
  sm->actcolor = sm->highcolor = sm->lowcolor = 0;
}

void sensorfieldmeter_updatelegend(SensorFieldMeter *sm) {
  FieldMeter *fm = &sm->f;
  char l[32], lscale[2], tscale[2];
  double limit = (sm->negative ? sm->low : sm->high);
  double total = meter_scalevalue(fm->total, tscale, 1);
  const char *hilo = (sm->negative ? "LOW" : "HIGH");

  if ((!sm->negative && sm->has_high) || (sm->negative && sm->has_low)) {
    if ((0.1 <= fabs(fm->total) && fabs(fm->total) < 9.95) ||
        (0.1 <= fabs(limit) && fabs(limit) < 9.95)) {
      if (strlen(sm->unit))
        snprintf(l, sizeof l, "ACT(%s)/%.1f/%.1f", sm->unit, limit, fm->total);
      else
        snprintf(l, sizeof l, "ACT/%.1f/%.1f", limit, fm->total);
    } else if ((9.95 <= fabs(fm->total) && fabs(fm->total) < 10000) ||
               (9.95 <= fabs(limit) && fabs(limit) < 10000)) {
      if (strlen(sm->unit))
        snprintf(l, sizeof l, "ACT(%s)/%.0f/%.0f", sm->unit, limit, fm->total);
      else
        snprintf(l, sizeof l, "ACT/%.0f/%0.f", limit, fm->total);
    } else {
      limit = meter_scalevalue(limit, lscale, 1);
      if (strlen(sm->unit))
        snprintf(l, sizeof l, "ACT(%s)/%.0f%s/%.0f%s", sm->unit, limit,
                 lscale, total, tscale);
      else
        snprintf(l, sizeof l, "ACT/%.0f%s/%.0f%s", limit, lscale, total,
                 tscale);
    }
  } else {
    if ((0.1 <= fabs(fm->total) && fabs(fm->total) < 9.95) ||
        (0.1 <= fabs(limit) && fabs(limit) < 9.95)) {
      if (strlen(sm->unit))
        snprintf(l, sizeof l, "ACT(%s)/%s/%.1f", sm->unit, hilo, fm->total);
      else
        snprintf(l, sizeof l, "ACT/%s/%.1f", hilo, fm->total);
    } else if ((9.95 <= fabs(fm->total) && fabs(fm->total) < 10000) ||
               (9.95 <= fabs(limit) && fabs(limit) < 10000)) {
      if (strlen(sm->unit))
        snprintf(l, sizeof l, "ACT(%s)/%s/%.0f", sm->unit, hilo, fm->total);
      else
        snprintf(l, sizeof l, "ACT/%s/%.0f", hilo, fm->total);
    } else {
      if (strlen(sm->unit))
        snprintf(l, sizeof l, "ACT(%s)/%s/%.0f%s", sm->unit, hilo, total,
                 tscale);
      else
        snprintf(l, sizeof l, "ACT/%s/%.0f%s", hilo, total, tscale);
    }
  }
  meter_setlegend(&fm->m, l);
}

void sensorfieldmeter_checkfields(SensorFieldMeter *sm, double low,
                                  double high) {
  /*  Most sensors stay at either positive or negative values.  Consider the
   *  actual value and alarm limits when deciding whether the meter should be
   *  showing a positive or a negative scale.  */
  FieldMeter *fm = &sm->f;
  int do_legend = 0;
  double highest;

  if (sm->negative) {  /*  negative at previous run  */
    if (fm->fields[0] >= 0 || low > 0) {  /*  flip to positive  */
      sm->negative = 0;
      fm->total = fabs(fm->total);
      sm->high = (sm->has_high ? high : fm->total);
      sm->low = (sm->has_low ? low : 0);
      fieldmeter_setcolor(fm, 2, sm->highcolor);
      do_legend = 1;
    } else {
      if (sm->has_low && low != sm->low) {
        sm->low = low;
        do_legend = 1;
      }
      if (sm->has_high && high != sm->high)
        sm->high = high;
    }
  } else {  /*  positive at previous run  */
    /*  flip to negative if the value and either limit is below 0  */
    if (fm->fields[0] < 0 &&
        ((!sm->has_low && !sm->has_high) || low < 0 || high < 0)) {
      sm->negative = 1;
      fm->total = -fabs(fm->total);
      sm->high = (sm->has_high ? high : 0);
      sm->low = (sm->has_low ? low : fm->total);
      fieldmeter_setcolor(fm, 2, sm->lowcolor);
      do_legend = 1;
    } else {
      if (sm->has_high && high != sm->high) {
        sm->high = high;
        do_legend = 1;
      }
      if (sm->has_low && low != sm->low)
        sm->low = low;
    }
  }

  /*  change total if the value or the alarms will not fit  */
  highest = fabs(sm->high);
  if (fabs(fm->fields[0]) > highest)
    highest = fabs(fm->fields[0]);
  if (fabs(sm->low) > highest)
    highest = fabs(sm->low);
  if (highest > fabs(fm->total)) {
    int scale = floor(log10(highest));

    do_legend = 1;
    fm->total = ceil((highest / pow(10, scale)) * 1.25) * pow(10, scale);
    if (sm->negative) {
      fm->total = -fabs(fm->total);
      if (!sm->has_low)
        sm->low = fm->total;
      if (!sm->has_high)
        sm->high = 0;
    } else {
      if (!sm->has_low)
        sm->low = 0;
      if (!sm->has_high)
        sm->high = fm->total;
    }
  }
  if (do_legend)
    sensorfieldmeter_updatelegend(sm);

  /*  check for alarms  */
  if (fm->fields[0] > sm->high) {          /*  alarm: T > max  */
    if (fm->colors[0] != sm->highcolor) {
      fieldmeter_setcolor(fm, 0, sm->highcolor);
      do_legend = 1;
    }
  } else if (fm->fields[0] < sm->low) {    /*  alarm: T < min  */
    if (fm->colors[0] != sm->lowcolor) {
      fieldmeter_setcolor(fm, 0, sm->lowcolor);
      do_legend = 1;
    }
  } else {
    if (fm->colors[0] != sm->actcolor) {
      fieldmeter_setcolor(fm, 0, sm->actcolor);
      do_legend = 1;
    }
  }

  fieldmeter_setused(fm, fm->fields[0], fm->total);
  if (sm->negative) {
    fm->fields[1] = (fm->fields[0] < sm->low ? 0 : sm->low - fm->fields[0]);
  } else {
    if (fm->fields[0] < 0)
      fm->fields[0] = 0;
    fm->fields[1] = (fm->fields[0] > sm->high ? 0 : sm->high - fm->fields[0]);
  }
  fm->fields[2] = fm->total - fm->fields[1] - fm->fields[0];

  if (do_legend) {
    fieldmeter_drawlegend(fm);
    fieldmeter_drawfields(fm, 1);  /*  force a draw in case only a limit
                                       changed  */
  }
}
