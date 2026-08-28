/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#include "MeterMaker.h"
#include "xosview.h"

#include "loadmeter.h"
#include "cpumeter.h"
#include "memmeter.h"
#ifndef IRIX5
#include "gfxmeter.h"
#endif
#include "diskmeter.h"

#include <stdlib.h>
#include <string.h>

void makeMeters(XOSView *xos) {
    if (xosview_isresourcetrue(xos, "load"))
        xosview_addmeter(xos, loadmeter_new(xos));

    /*  Standard meters (usually added, but users could turn them off)  */
    if (xosview_isresourcetrue(xos, "cpu")) {
        const char *format = xosview_getresource(xos, "cpuFormat");
        const int cpuCount = cpumeter_ncpus();
        int any = 0;
        int i;

        if (strncmp(format, "single", 2) == 0
            || strncmp(format, "both", 2) == 0) {
            xosview_addmeter(xos, cpumeter_new(xos, -1));
            any = 1;
        }

        if (strncmp(format, "all", 2) == 0
            || strncmp(format, "both", 2) == 0) {
            for (i = 0; i < cpuCount; i++)
                xosview_addmeter(xos, cpumeter_new(xos, i));
            any = 1;
        }

        if (strncmp(format, "auto", 2) == 0) {
            xosview_addmeter(xos, cpumeter_new(xos, -1));

            if (cpuCount > 1)
                for (i = 0; i < cpuCount; i++)
                    xosview_addmeter(xos, cpumeter_new(xos, i));
            any = 1;
        }

        if (!any)
            for (i = 0; i < cpuCount; i++)
                xosview_addmeter(xos, cpumeter_new(xos, i));
    }

#ifndef IRIX5
    if (xosview_isresourcetrue(xos, "gfx") && gfxmeter_npipes() > 0)
        xosview_addmeter(xos,
                         gfxmeter_new(xos,
                                      atoi(xosview_getresource(xos,
                                           "gfxWarnThreshold"))));
#endif

    if (xosview_isresourcetrue(xos, "mem"))
        xosview_addmeter(xos, memmeter_new(xos));
}
