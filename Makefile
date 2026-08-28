#  The build is configured by one file from targets/.  Copying it to .config
#  pins the choice, "make TARGET=hpux9" makes it for one build, and with
#  neither of those guess-target picks one from uname.
ifdef TARGET
CONFIG := targets/$(TARGET)
else
CONFIG := $(wildcard .config)
ifeq ($(CONFIG),)
CONFIG := targets/$(shell sh ./guess-target)
endif
endif

include $(CONFIG)

AWK ?= awk
INSTALL ?= install
#  AIX has an install(1) that does not know -d, so creating the target
#  directories is kept separate from copying the files into them.
INSTALLDIR ?= $(INSTALL) -d
PLATFORM ?= linux

# Installation paths

PREFIX ?= /usr/local

BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
XDGAPPSDIR ?= $(PREFIX)/share/applications
ICONDIR ?= $(PREFIX)/share/icons/hicolor

# Optional build arguments; user may wish to override

OPTFLAGS ?= -Wall -O3

# Platforms without libXpm clear this and define -DNO_XPM

XPMLIB ?= -lXpm

# Compilers other than gcc spell these differently; see targets/hpux11

DEPFLAGS ?= -MMD
NOWRITESTRINGS ?= -Wno-write-strings

# Required build arguments

CPPFLAGS += $(OPTFLAGS) -I. $(DEPFLAGS)
LDLIBS += -lX11 $(XPMLIB)

OBJS = Host.o \
	Xrm.o \
	bitfieldmeter.o \
	bitmeter.o \
	defaultstring.o \
	fieldmeter.o \
	fieldmeterdecay.o \
	fieldmetergraph.o \
	llist.o \
	main.o \
	meter.o \
	stringutils.o \
	xosview.o \
	xwin.o

# Optional platform type

ifeq ($(PLATFORM), linux)
ARCH = $(shell uname -m)
OBJS += sensorfieldmeter.o \
	linux/MeterMaker.o \
	linux/btrymeter.o \
	linux/cpumeter.o \
	linux/diskmeter.o \
	linux/intmeter.o \
	linux/intratemeter.o \
	linux/lmstemp.o \
	linux/loadmeter.o \
	linux/memmeter.o \
	linux/netmeter.o \
	linux/nfsmeter.o \
	linux/pagemeter.o \
	linux/raidmeter.o \
	linux/serialmeter.o \
	linux/swapmeter.o \
	linux/wirelessmeter.o \
	linux/acpitemp.o
ifeq ($(findstring 86,$(ARCH)),86)
OBJS += linux/coretemp.o
endif
CPPFLAGS += -Ilinux/
LDLIBS += -lm
endif

ifeq ($(PLATFORM), bsd)
ARCH = $(shell uname -m)
OBJS += sensorfieldmeter.o \
        bsd/MeterMaker.o \
        bsd/btrymeter.o \
        bsd/cpumeter.o \
        bsd/diskmeter.o \
        bsd/intmeter.o \
        bsd/intratemeter.o \
        bsd/kernel.o \
        bsd/loadmeter.o \
        bsd/memmeter.o \
        bsd/netmeter.o \
        bsd/pagemeter.o \
        bsd/swapmeter.o \
        bsd/sensor.o
ifeq ($(ARCH),$(filter $(ARCH),i386 amd64 x86_64))
OBJS += bsd/coretemp.o
endif
CPPFLAGS += -Ibsd/
LDLIBS += -lm
endif

ifeq ($(PLATFORM), irix65)
OBJS += irix65/MeterMaker.o \
        irix65/cpumeter.o \
        irix65/diskmeter.o \
        irix65/loadmeter.o \
        irix65/memmeter.o \
        irix65/sarmeter.o
#  The graphics pipe meter is 6.5 only; targets/irix5 clears this.
IRIXGFX ?= irix65/gfxmeter.o
OBJS += $(IRIXGFX)
CPPFLAGS += -Iirix65/
endif

ifeq ($(PLATFORM), hpux)
OBJS += hpux/MeterMaker.o \
        hpux/cpumeter.o \
        hpux/loadmeter.o \
        hpux/memmeter.o \
        hpux/pagemeter.o \
        hpux/swapmeter.o
CPPFLAGS += -Ihpux/
endif

ifeq ($(PLATFORM), osf1)
OBJS += osf1/MeterMaker.o \
        osf1/cpumeter.o \
        osf1/loadmeter.o \
        osf1/memmeter.o \
        osf1/osf1stats.o \
        osf1/pagemeter.o \
        osf1/swapmeter.o
CPPFLAGS += -Iosf1/ -DNO_XPM
XPMLIB =
LDLIBS += -lm
endif

ifeq ($(PLATFORM), osr6)
OBJS += osr6/MeterMaker.o \
        osr6/cpumeter.o \
        osr6/loadmeter.o \
        osr6/memmeter.o \
        osr6/osr6stats.o \
        osr6/swapmeter.o
CPPFLAGS += -Iosr6/ -DNO_XPM
XPMLIB =
LDLIBS += -lmas -lsocket -lnsl -lm
endif

ifeq ($(PLATFORM), unixware)
OBJS += unixware/MeterMaker.o \
        unixware/cpumeter.o \
        unixware/loadmeter.o \
        unixware/memmeter.o \
        unixware/swapmeter.o \
        unixware/unixwarestats.o
CPPFLAGS += -Iunixware/ -DNO_XPM
XPMLIB =
LDLIBS += -lmas -lelf -lsocket -lnsl -lm
endif

ifeq ($(PLATFORM), sunos5)
OBJS += sunos5/MeterMaker.o \
        sunos5/cpumeter.o \
        sunos5/diskmeter.o \
        sunos5/loadmeter.o \
        sunos5/memmeter.o \
        sunos5/netmeter.o \
        sunos5/pagemeter.o \
        sunos5/swapmeter.o \
        sunos5/intratemeter.o
CPPFLAGS += -Isunos5/ -Wno-write-strings
LDLIBS += -lkstat -lnsl -lsocket
INSTALL = ginstall
endif

ifeq ($(PLATFORM), aix)
#  AIXSTATS picks the back end the meters read their statistics through:
#  perfstat uses libperfstat, which arrived in AIX 5.1, and kmem reads kernel
#  memory directly for the AIX 4.x releases that predate it.
AIXSTATS ?= perfstat
OBJS += aix/MeterMaker.o \
        aix/cpumeter.o \
        aix/diskmeter.o \
        aix/loadmeter.o \
        aix/memmeter.o \
        aix/netmeter.o \
        aix/pagemeter.o \
        aix/swapmeter.o \
        aix/$(AIXSTATS).o
CPPFLAGS += -Iaix/ -DNO_XPM
XPMLIB =
LDLIBS += -lm
ifeq ($(AIXSTATS), perfstat)
LDLIBS += -lperfstat
else
#  Only the AIX 4.x toolchain needs the declarations aixcompat.h supplies.
CPPFLAGS += -include aix/aixcompat.h
endif
endif

ifeq ($(PLATFORM), gnu)
OBJS += gnu/get_def_pager.o \
	gnu/loadmeter.o \
	gnu/memmeter.o \
	gnu/MeterMaker.o \
	gnu/pagemeter.o \
	gnu/swapmeter.o
CPPFLAGS += -Ignu/
endif

#  gcc 2.x writes the -MMD dependency file into the current directory rather
#  than next to the object, and is too old to have -MF, so look for both names.
DEPS := $(OBJS:.o=.d) $(notdir $(OBJS:.o=.d))

#  HP-UX keeps template implementations in a .cc next to the extension-less
#  standard headers, so make's builtin "program out of a source file" rule
#  tries to remake <limits> from limits.cc when a .d file names it.
%: %.cc
%: %.o

#  Toolchains that can not drive the linker themselves override this; see
#  targets/irix5.
LINK ?= $(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

xosview:	$(OBJS)
		$(LINK)

defaultstring.cc:	Xdefaults defresources.awk
		$(AWK) -f defresources.awk Xdefaults > defaultstring.cc

Xrm.o:		CXXFLAGS += $(NOWRITESTRINGS)

.PHONY:		dist install clean

dist:
		./mkdist $(VERSION)

install:	xosview
		$(INSTALLDIR) $(DESTDIR)$(BINDIR)
		$(INSTALLDIR) $(DESTDIR)$(MANDIR)/man1
		$(INSTALLDIR) $(DESTDIR)$(XDGAPPSDIR)
		$(INSTALLDIR) $(DESTDIR)$(ICONDIR)/32x32/apps
		$(INSTALL) -m 755 xosview $(DESTDIR)$(BINDIR)/xosview
		$(INSTALL) -m 644 xosview.1 $(DESTDIR)$(MANDIR)/man1/xosview.1
		$(INSTALL) -m 644 xosview.desktop $(DESTDIR)$(XDGAPPSDIR)
		$(INSTALL) -m 644 xosview.png $(DESTDIR)$(ICONDIR)/32x32/apps

clean:
		rm -f xosview $(OBJS) $(DEPS) defaultstring.cc

-include $(DEPS)
