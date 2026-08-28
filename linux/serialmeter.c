/*
 *  Copyright (c) 1994, 1995, 2006 by Mike Romberg ( mike.romberg@noaa.gov )
 *
 *  This file may be distributed under terms of the GPL
 */

/*
 *  In order to use this new serial meter, xosview needs to be suid root.
 */
#include "serialmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <fcntl.h>
#include <linux/serial.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * To fetch status information requires ioperm() and inb()
 * otherwise this meter is largely a no-op.
 */
#if defined(__i386__) || defined(__amd64__)
#include <sys/io.h>
#define HAVE_IOPERM
#endif

static const char *getTitle(int dev) {
  static const char *names[] = { "ttyS0", "ttyS1", "ttyS2", "ttyS3",
                                 "ttyS4", "ttyS5", "ttyS6", "ttyS7",
                                 "ttyS8", "ttyS9" };
  return names[dev];
}

const char *serialmeter_resourcename(int dev) {
  static const char *names[] = { "serial0", "serial1",
                                 "serial2", "serial3",
                                 "serial4", "serial5",
                                 "serial6", "serial7",
                                 "serial8", "serial9" };
  return names[dev];
}

static int getport(unsigned short port) {
#ifdef HAVE_IOPERM
  return ioperm(port, 1, 1) != -1;
#else
  (void) port;
  return 0;
#endif
}

static unsigned short getPortBase(SerialMeter *sm, int dev) {
  static const char *deviceFile[] = { "/dev/ttyS0", "/dev/ttyS1",
                                      "/dev/ttyS2", "/dev/ttyS3",
                                      "/dev/ttyS4", "/dev/ttyS5",
                                      "/dev/ttyS6", "/dev/ttyS7",
                                      "/dev/ttyS8", "/dev/ttyS9" };
  const char *res = xwin_getresource(sm->b.m.xw, serialmeter_resourcename(dev));

  if (!strncasecmp(res, "True", 5)) {  /*  Autodetect portbase.  */
    struct serial_struct serinfo;
    int fd;

    /*  get the real serial port (code stolen from setserial 2.11)  */
    if ((fd = open(deviceFile[dev], O_RDONLY | O_NONBLOCK)) < 0) {
      fprintf(stderr, "serialmeter: failed to open %s.\n", deviceFile[dev]);
      exit(1);
    }
    if (ioctl(fd, TIOCGSERIAL, &serinfo) < 0) {
      fprintf(stderr, "Failed to detect port base for %s\n", deviceFile[dev]);
      close(fd);
      exit(1);
    }

    close(fd);
    return serinfo.port;
  } else {  /*  Use user specified port base.  */
    unsigned int tmp = 0;
    sscanf(res, "%x", &tmp);
    return (unsigned short)tmp;
  }
}

static void checkres(Meter *m) {
  SerialMeter *sm = (SerialMeter *)m;
  BitMeter *bm = &sm->b;

  meter_checkresources(m);
  bm->oncolor = xwin_alloccolor(m->xw,
                                xwin_getresource(m->xw, "serialOnColor"));
  bm->offcolor = xwin_alloccolor(m->xw,
                                 xwin_getresource(m->xw, "serialOffColor"));
  m->priority = atoi(xwin_getresource(m->xw, "serialPriority"));

  sm->port = getPortBase(sm, sm->device);
  if (!getport(sm->port + UART_LSR) || !getport(sm->port + UART_MSR)) {
    fprintf(stderr, "serialmeter: xosview must be suid root to use the "
            "serial meter.\n");
    xwin_setdone(m->xw, 1);
  }
}

static void getserial(SerialMeter *sm) {
#ifdef HAVE_IOPERM
  /*  get the LSR and MSR  */
  unsigned char lsr = inb(sm->port + UART_LSR);
  unsigned char msr = inb(sm->port + UART_MSR);

  bitmeter_setbits(&sm->b, 0, lsr);
  bitmeter_setbits(&sm->b, 8, msr);
#else
  (void) sm;
#endif
}

static void checkevent(Meter *m) {
  getserial((SerialMeter *)m);
  bitmeter_checkevent(m);
}

Meter *serialmeter_new(XOSView *parent, int device) {
  SerialMeter *sm = (SerialMeter *)meter_alloc(sizeof *sm);

  bitmeter_init(&sm->b, parent, "SerialMeter", getTitle(device),
                "LSR bits(0-7), MSR bits(0-7)", 16, 0, 0, 0);
  sm->b.m.checkres = checkres;
  sm->b.m.checkevent = checkevent;

  sm->device = device;
  sm->port = 0;

  return &sm->b.m;
}
