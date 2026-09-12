# IRIX and other vendor makes run recipes with $SHELL, which is csh for the
# stock root account; every recipe here is Bourne shell.
SHELL = /bin/sh

CC = gcc
CFLAGS = -O2 -I.
LDFLAGS =
LIBS = -lX11 -lXpm
TARGET = xosview

AWK = awk

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man
XDGAPPSDIR = $(PREFIX)/share/applications
ICONDIR = $(PREFIX)/share/icons/hicolor

CORE_OBJS = Xrm.o bitfieldmeter.o bitmeter.o defaultstring.o fieldmeter.o \
	main.o meter.o stringutils.o xosview.o xwin.o

# Each OS target below passes its own object list in.
PLAT_OBJS =

OBJECTS = $(CORE_OBJS) $(PLAT_OBJS)

LINUX_OBJS = sensorfieldmeter.o linux/MeterMaker.o linux/acpitemp.o \
	linux/btrymeter.o linux/cpumeter.o linux/diskmeter.o linux/intmeter.o \
	linux/intratemeter.o linux/lmstemp.o linux/loadmeter.o \
	linux/memmeter.o linux/netmeter.o linux/nfsmeter.o linux/pagemeter.o \
	linux/raidmeter.o linux/serialmeter.o linux/swapmeter.o \
	linux/wirelessmeter.o

BSD_OBJS = sensorfieldmeter.o bsd/MeterMaker.o bsd/btrymeter.o \
	bsd/cpumeter.o bsd/diskmeter.o bsd/intmeter.o bsd/intratemeter.o \
	bsd/kernel.o bsd/loadmeter.o bsd/memmeter.o bsd/netmeter.o \
	bsd/pagemeter.o bsd/sensor.o bsd/swapmeter.o

SUNOS5_OBJS = sunos5/MeterMaker.o sunos5/cpumeter.o sunos5/diskmeter.o \
	sunos5/intratemeter.o sunos5/kstats.o sunos5/loadmeter.o \
	sunos5/memmeter.o sunos5/netmeter.o sunos5/pagemeter.o \
	sunos5/swapmeter.o

# aix/perfstat.c is the AIX 5.1 and later back end, aix/kmem.c the 4.x one.
AIX_OBJS = aix/MeterMaker.o aix/cpumeter.o aix/diskmeter.o aix/loadmeter.o \
	aix/memmeter.o aix/netmeter.o aix/pagemeter.o aix/swapmeter.o

HPUX_OBJS = hpux/MeterMaker.o hpux/cpumeter.o hpux/loadmeter.o \
	hpux/memmeter.o hpux/pagemeter.o hpux/swapmeter.o

IRIX_OBJS = irix65/MeterMaker.o irix65/cpumeter.o irix65/loadmeter.o \
	irix65/memmeter.o

# The gfx meter is 6.5 only, and the disk meter reads its figures out of the
# 6.5 sadc record stream, which 5.3 does not write; the irix5 target leaves
# all three out.
IRIX65_OBJS = $(IRIX_OBJS) irix65/diskmeter.o irix65/gfxmeter.o \
	irix65/sarmeter.o

OSF1_OBJS = osf1/MeterMaker.o osf1/cpumeter.o osf1/diskmeter.o \
	osf1/intratemeter.o osf1/loadmeter.o osf1/memmeter.o osf1/netmeter.o \
	osf1/osf1stats.o osf1/pagemeter.o osf1/swapmeter.o

OSR6_OBJS = osr6/MeterMaker.o osr6/cpumeter.o osr6/loadmeter.o \
	osr6/memmeter.o osr6/osr6stats.o osr6/swapmeter.o

UNIXWARE_OBJS = unixware/MeterMaker.o unixware/cpumeter.o \
	unixware/loadmeter.o unixware/memmeter.o unixware/swapmeter.o \
	unixware/unixwarestats.o

GNU_OBJS = gnu/MeterMaker.o gnu/get_def_pager.o gnu/loadmeter.o \
	gnu/memmeter.o gnu/pagemeter.o gnu/swapmeter.o

# Auto detect OS
all:
	@s=`uname -s`; r=`uname -r`; v=`uname -v`; \
	case "$$s" in \
	  Linux)     t=linux;; \
	  GNU)       t=gnu;; \
	  OSF1)      t=osf1;; \
	  FreeBSD)   t=freebsd;; \
	  NetBSD)    t=netbsd;; \
	  OpenBSD)   t=openbsd;; \
	  DragonFly) t=dragonflybsd;; \
	  SunOS)     case "$$r" in 5.*) t=sunos5;; *) echo "SunOS $$r is not supported"; exit 1;; esac;; \
	  IRIX|IRIX64) case "$$r" in 5.*) t=irix5;; *) t=irix65;; esac;; \
	  UnixWare|UNIX_SV) t=unixware;; \
	  SCO_SV)    case "$$v" in 6.*) t=osr6;; *) echo "OpenServer $$v is not supported"; exit 1;; esac;; \
	  AIX)       if [ -f /usr/lib/libperfstat.a ]; then t=aix5; else t=aix4; fi;; \
	  HP-UX)     case "$$r" in \
	               *.09.*) t=hpux9;; \
	               *.10.*) t=hpux10;; \
	               *) t=hpux11;; \
	             esac;; \
	  *) echo "$$s not known here, building for linux"; t=linux;; \
	esac; \
	echo "=> make $$t"; \
	$(MAKE) $$t

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

# POSIX suffix rule, not a GNU "%.o: %.c" pattern rule: the vendor makes
# ignore pattern rules, and their built-in .c.o has no -o $@, which drops
# the object in the current directory instead of beside its source.
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

defaultstring.c: Xdefaults defresources.awk
	$(AWK) -f defresources.awk Xdefaults > defaultstring.c

# linux/coretemp.c and bsd/coretemp.c read x86 MSRs and build there only.
linux::
	@case `uname -m` in \
	  *86*) x=linux/coretemp.o;; \
	  *) x=;; \
	esac; \
	$(MAKE) CFLAGS="$(CFLAGS) -Ilinux" LIBS="-lX11 -lXpm -lm" \
	  PLAT_OBJS="$(LINUX_OBJS) $$x" $(TARGET)

gnu::
	$(MAKE) CFLAGS="$(CFLAGS) -Ignu" LIBS="-lX11 -lXpm" \
	  PLAT_OBJS="$(GNU_OBJS)" $(TARGET)

freebsd::
	@case `uname -m` in \
	  i386|amd64|x86_64) x=bsd/coretemp.o;; \
	  *) x=;; \
	esac; \
	$(MAKE) CC=cc CFLAGS="$(CFLAGS) -Ibsd -I/usr/local/include" \
	  LDFLAGS="-L/usr/local/lib" LIBS="-lX11 -lXpm -ldevstat -lkvm -lm" \
	  PLAT_OBJS="$(BSD_OBJS) $$x" $(TARGET)

netbsd::
	@case `uname -m` in \
	  i386|amd64|x86_64) x=bsd/coretemp.o;; \
	  *) x=;; \
	esac; \
	$(MAKE) CFLAGS="$(CFLAGS) -Ibsd -I/usr/X11R7/include" \
	  LDFLAGS="-L/usr/X11R7/lib -Wl,--rpath=/usr/X11R7/lib" \
	  LIBS="-lX11 -lXpm -lkvm -lprop -lm" \
	  PLAT_OBJS="$(BSD_OBJS) $$x" $(TARGET)

openbsd::
	@case `uname -m` in \
	  i386|amd64|x86_64) x=bsd/coretemp.o;; \
	  *) x=;; \
	esac; \
	$(MAKE) CFLAGS="$(CFLAGS) -Ibsd -I/usr/X11R6/include" \
	  LDFLAGS="-L/usr/X11R6/lib" LIBS="-lX11 -lXpm -lkvm -lm" \
	  PLAT_OBJS="$(BSD_OBJS) $$x" $(TARGET)

dragonflybsd::
	@case `uname -m` in \
	  i386|amd64|x86_64) x=bsd/coretemp.o;; \
	  *) x=;; \
	esac; \
	$(MAKE) CFLAGS="$(CFLAGS) -Ibsd -I/usr/pkg/include -I/usr/local/include" \
	  LDFLAGS="-L/usr/pkg/lib -L/usr/local/lib" \
	  LIBS="-lX11 -lXpm -lkvm -lkinfo -ldevstat -lm" \
	  PLAT_OBJS="$(BSD_OBJS) $$x" $(TARGET)

sunos5::
	$(MAKE) CC=cc CFLAGS="$(CFLAGS) -Isunos5" \
	  LIBS="-lX11 -lXpm -lkstat -lnsl -lsocket" \
	  PLAT_OBJS="$(SUNOS5_OBJS)" $(TARGET)

# AIX ships no libXpm; only the pixmapName resource is lost with -DNO_XPM.
# 4.x predates libperfstat, so the statistics come out of kernel memory, and
# its headers declare neither snprintf nor the strcasecmp family.
aix4::
	$(MAKE) CFLAGS="$(CFLAGS) -Iaix -DNO_XPM -include aix/aixcompat.h" \
	  LIBS="-lX11 -lm" PLAT_OBJS="$(AIX_OBJS) aix/kmem.o" $(TARGET)

aix5::
	$(MAKE) CFLAGS="$(CFLAGS) -Iaix -DNO_XPM" \
	  LIBS="-lX11 -lm -lperfstat" \
	  PLAT_OBJS="$(AIX_OBJS) aix/perfstat.o" $(TARGET)

# HP-UX 10.20 and up have the ANSI C compiler at this path; 9.x has it as cc.
HPUX_CC = /opt/ansic/bin/cc

# 9.x keeps X11R5 off the default paths, its linker makes one pass over the
# archives, so libX11 has to come round again after libXpm, and its libc has
# routines its headers never declare; hpux/hpux9 makes up the difference.
# That shim is force included, which HP cc cannot do, so this one target
# wants gcc rather than the ANSI C compiler the other two use.
hpux9::
	$(MAKE) CC=gcc \
	  CFLAGS="$(CFLAGS) -Ihpux -Ihpux/hpux9 -I/usr/include/X11R5 \
	    -DNO_PSS_NBLKSENABLED -include hpux/hpux9/compat.h" \
	  LDFLAGS="-L/usr/lib/X11R5" LIBS="-lXpm -lX11 -lX11" \
	  PLAT_OBJS="$(HPUX_OBJS) hpux/hpux9/compat.o" $(TARGET)

# 10.20 keeps X11R6 off the default paths and ships no libXpm.
hpux10::
	$(MAKE) CC=$(HPUX_CC) \
	  CFLAGS="-Ae -O -I. -Ihpux -DNO_XPM -I/usr/include/X11R6 \
	    -I/usr/contrib/X11R6/include" \
	  LDFLAGS="-L/usr/lib/X11R6 -L/usr/contrib/X11R6/lib" LIBS="-lX11 -lm" \
	  PLAT_OBJS="$(HPUX_OBJS)" $(TARGET)

hpux11::
	$(MAKE) CC=$(HPUX_CC) CFLAGS="-Ae +O3 -I. -Ihpux -DNO_XPM" \
	  LDFLAGS="-L/usr/lib/X11R6" LIBS="-lX11 -lm" \
	  PLAT_OBJS="$(HPUX_OBJS)" $(TARGET)

irix65::
	$(MAKE) CC=cc CFLAGS="$(CFLAGS) -Iirix65" LIBS="-lX11 -lXpm" \
	  PLAT_OBJS="$(IRIX65_OBJS)" $(TARGET)

IRIX5_CC = /usr/tgcware/gcc45/bin/gcc
IRIX5_GCCLIB = /usr/tgcware/gcc45/lib/gcc/mips-sgi-irix5.3/4.5.3
IRIX5_LD = /usr/tgcware/mips-sgi-irix5.3/bin/ld

# 5.3 has no libXpm, no graphics pipe meter and no snprintf or usleep, and
# its sadc writes none of the records the disk meter reads.  gcc from
# tgcware cannot drive the linker itself: the crt objects and the gcc
# runtime have to be named by hand.  Correct the three paths above to match
# what is installed.  gcc fakes _COMPILER_VERSION, which makes SGI's
# offsetof() reach for the MIPSpro builtin __INTADDR__ that gcc does not
# have.
irix5::
	$(MAKE) CC=$(IRIX5_CC) $(CORE_OBJS) $(IRIX_OBJS) irix65/irix5/compat.o \
	  CFLAGS="$(CFLAGS) -Iirix65 -Iirix65/irix5 -DIRIX5 -DNO_XPM -U_COMPILER_VERSION -include irix65/irix5/compat.h"
	$(IRIX5_LD) -o $(TARGET) -init __gcc_init -fini __gcc_fini \
	  /usr/lib/crt1.o $(IRIX5_GCCLIB)/irix-crti.o $(IRIX5_GCCLIB)/crtbegin.o \
	  -L$(IRIX5_GCCLIB) -L$(IRIX5_GCCLIB)/../../.. -L/usr/lib \
	  $(CORE_OBJS) $(IRIX_OBJS) irix65/irix5/compat.o \
	  -lX11 -lrpcsvc -lm -lgcc -lgcc_eh -lc \
	  $(IRIX5_GCCLIB)/crtend.o $(IRIX5_GCCLIB)/irix-crtn.o /usr/lib/crtn.o

# Tru64 ships no libXpm.  Set CC=gcc where that is what is installed.
osf1::
	$(MAKE) CC=cc CFLAGS="$(CFLAGS) -Iosf1 -DNO_XPM" \
	  LDFLAGS="-L/usr/shlib" LIBS="-lX11 -lm -lmach" \
	  PLAT_OBJS="$(OSF1_OBJS)" $(TARGET)

# OpenServer keeps X11R6 off the default paths and ships no libXpm.  cc is
# the UDK C driver, also installed as /udk/usr/ccs/bin/cc.  Set CC=gcc where
# that is what is installed.
osr6::
	$(MAKE) CC=cc CFLAGS="$(CFLAGS) -Iosr6 -DNO_XPM -I/usr/X11R6/include" \
	  LDFLAGS="-L/usr/X11R6/lib" LIBS="-lX11 -lmas -lsocket -lnsl -lm" \
	  PLAT_OBJS="$(OSR6_OBJS)" $(TARGET)

# UnixWare ships no libXpm.  cc is the UDK C driver at /usr/ccs/bin/cc.
# Set CC=gcc where that is what is installed.
unixware::
	$(MAKE) CC=cc CFLAGS="$(CFLAGS) -Iunixware -DNO_XPM" \
	  LIBS="-lX11 -lmas -lelf -lsocket -lnsl -lm" \
	  PLAT_OBJS="$(UNIXWARE_OBJS)" $(TARGET)

clean:
	rm -f $(TARGET) *.o */*.o */*/*.o defaultstring.c

dist::
	./mkdist $(VERSION)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(XDGAPPSDIR)
	mkdir -p $(DESTDIR)$(ICONDIR)/32x32/apps
	cp $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	chmod 755 $(DESTDIR)$(BINDIR)/$(TARGET)
	cp xosview.1 $(DESTDIR)$(MANDIR)/man1/xosview.1
	cp xosview.desktop $(DESTDIR)$(XDGAPPSDIR)/xosview.desktop
	cp xosview.png $(DESTDIR)$(ICONDIR)/32x32/apps/xosview.png

# The vendor makes ignore .PHONY, and linux, gnu, sunos5, irix65, osf1, osr6,
# unixware and dist each name a directory as well as a target, which then
# always looks up to date.  A double colon rule with no prerequisites is the
# one form POSIX says always runs.
.PHONY: all clean dist install linux gnu freebsd netbsd openbsd dragonflybsd \
	sunos5 aix4 aix5 hpux9 hpux10 hpux11 irix5 irix65 osf1 osr6 unixware

