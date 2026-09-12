/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "netmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <fcntl.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>

/*  The interface counters have no system call of their own and are read out
 *  of /dev/kmem at the address nlist() resolves for the head of the kernel
 *  ifnet chain.  That needs root, or xosview installed setgid sys.  */

#define MAX_INTERFACES 64

#define NOT_OPENED (-2)

static int kmemfd = NOT_OPENED;

static int kmem(void) {
    if (kmemfd == NOT_OPENED) {
        kmemfd = open("/dev/kmem", O_RDONLY);
        if (kmemfd < 0)
            fprintf(stderr, "Can not open /dev/kmem.  xosview must run as "
                    "root or be\n  installed setgid sys to read the "
                    "interface counters.\n");
    }
    return kmemfd;
}

/*  Copy size bytes from kernel address addr.  Returns 0 on failure.  */
static int kread(unsigned long addr, void *buf, int size) {
    int fd = kmem();

    if (fd < 0 || addr == 0)
        return 0;

    if (lseek(fd, (off_t)addr, SEEK_SET) == (off_t)-1)
        return 0;

    return read(fd, buf, size) == size;
}

/*  Address of the head of the kernel ifnet chain, resolved once and
 *  remembered, so that a kernel without the symbol is reported here rather
 *  than on every sample.  */
static unsigned long ifnetsym(void) {
    static unsigned long addr = 0;
    static int tried = 0;
    struct nlist nl[2];

    if (tried)
        return addr;
    tried = 1;

    memset(nl, 0, sizeof(nl));
    nl[0].n_name = "ifnet";

    if (nlist("/unix", nl) < 0 || nl[0].n_value == 0)
        fprintf(stderr, "Can not resolve the kernel symbol 'ifnet' in "
                "/unix.\n");
    else
        addr = nl[0].n_value;

    return addr;
}

static int netinfo(const char *iface, int ignore, double *inp, double *outp) {
    unsigned long next;
    int i;

    if (!kread(ifnetsym(), &next, sizeof(next)))
        return 0;

    *inp = *outp = 0;

    for (i = 0; next && i < MAX_INTERFACES; i++) {
        struct ifnet ifn;

        if (!kread(next, &ifn, sizeof(ifn)))
            break;
        next = (unsigned long)ifn.if_next;

        if (iface) {
            /*  if_name points at a string still living in kernel memory.  */
            char name[16], found[32];

            name[0] = '\0';
            if (!kread((unsigned long)ifn.if_name, name, sizeof(name)))
                continue;
            name[sizeof(name) - 1] = '\0';
            snprintf(found, sizeof(found), "%s%d", name, ifn.if_unit);

            if ((strcmp(iface, found) == 0) == ignore)
                continue;
        }

        /*  These counters are 32 bit and wrap on a busy link; the caller
         *  notices because the total it keeps moves backwards.  */
        *inp += (unsigned int)ifn.if_ibytes;
        *outp += (unsigned int)ifn.if_obytes;
    }

    return 1;
}

static void checkres(Meter *m) {
    NetMeter *nm = (NetMeter *)m;
    FieldMeter *fm = &nm->f;
    const char *iface;

    fieldmeter_checkresources(m);

    m->priority = atoi(xwin_getresource(m->xw, "netPriority"));

    /*  fieldmeter_disable() collapsed this meter to a single field, so
     *  setting the per field colours below would run off the end of the
     *  array.  */
    if (!nm->ok)
        return;

    fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "netInColor"));
    fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "netOutColor"));
    fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "netBackground"));
    fm->dodecay = xwin_isresourcetrue(m->xw, "netDecay");
    fm->usegraph = xwin_isresourcetrue(m->xw, "netGraph");
    fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "netUsedFormat"));

    iface = xwin_getresource(m->xw, "netIface");
    if (iface[0] == '-') {
        nm->ignored = 1;
        /*  A leading '-' means "every interface but this one".  */
        while (*iface == '-' || *iface == ' ')
            iface++;
    }
    snprintf(nm->netIface, sizeof nm->netIface, "%s", iface);
}

static void getnetstats(NetMeter *nm) {
    FieldMeter *fm = &nm->f;
    double nowBytesIn, nowBytesOut, t;
    int filtering = strcmp(nm->netIface, "False") != 0;

    fm->total = nm->maxpackets;

    if (!netinfo(filtering ? nm->netIface : NULL, nm->ignored,
                 &nowBytesIn, &nowBytesOut))
        return;

    fieldmeter_timerstop(fm);

    if (nm->first) {
        nm->lastBytesIn = nowBytesIn;
        nm->lastBytesOut = nowBytesOut;
        nm->first = 0;
    }

    t = fieldmeter_secs(fm);
    fm->fields[0] = nowBytesIn >= nm->lastBytesIn
                    ? (nowBytesIn - nm->lastBytesIn) / t : 0.0;
    fm->fields[1] = nowBytesOut >= nm->lastBytesOut
                    ? (nowBytesOut - nm->lastBytesOut) / t : 0.0;

    fieldmeter_timerstart(fm);
    nm->lastBytesIn = nowBytesIn;
    nm->lastBytesOut = nowBytesOut;

    if (fm->total < fm->fields[0] + fm->fields[1])
        fm->total = fm->fields[0] + fm->fields[1];
    fm->fields[2] = fm->total - fm->fields[0] - fm->fields[1];

    fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
}

static void checkevent(Meter *m) {
    getnetstats((NetMeter *)m);
    fieldmeter_drawfields((FieldMeter *)m, 0);
}

Meter *netmeter_new(XOSView *parent, float max) {
    NetMeter *nm = (NetMeter *)meter_alloc(sizeof *nm);
    double in, out;

    fieldmeter_init(&nm->f, parent, 3, "NetMeter", "NET", "IN/OUT/IDLE",
                    0, 0, 0);
    nm->f.m.checkres = checkres;
    nm->f.m.checkevent = checkevent;

    nm->maxpackets = max;
    nm->lastBytesIn = nm->lastBytesOut = 0;
    nm->first = 1;
    nm->ignored = 0;
    nm->netIface[0] = '\0';
    nm->ok = netinfo(NULL, 0, &in, &out);

    if (!nm->ok)
        fieldmeter_disable(&nm->f);

    return &nm->f.m;
}
