# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License") 1.1!
# You may not use this file except in compliance with the License.
#
# See  https://spdx.org/licenses/CDDL-1.1.html  for the specific
# language governing permissions and limitations under the License.
#
# Copyright 2021 Jens Elkner (jel+nvmex-src@cs.ovgu.de)

VERSION = "1.1.1"
PREFIX ?= /usr
BINDIR ?= sbin
MANDIR ?= share/man/man8
# you probably want to append something like '/64', '/x86_64', '/amd64'
LIBDIR ?= lib

# via package cuda-nvml-dev-10-1
CUDA_VERS ?= 11.2
CUDA_DIR ?= /usr/local/cuda-$(CUDA_VERS)
# set to 1 if you wanna run it with legacy drivers, e.g. <= version 390 etc.
LEGACY = 0

OS := $(shell uname -s)
MACH ?= 64

INSTALL_SunOS = ginstall
INSTALL_Linux = install

INSTALL ?= $(INSTALL_$(OS))

# If CC is set to 'cc', *_cc flags (Sun studio compiler) will be used.
# If set to 'gcc', the corresponding GNU C flags (*_gcc) will be used.
# For all others one needs to add corresponding rules.
CC ?= gcc
OPTIMZE_cc ?= -xO3
OPTIMZE_gcc ?= -O3
#OPTIMZE ?= $(OPTIMZE_$(CC)) -DNDEBUG

CFLAGS_cc = -xcode=pic32
CFLAGS_cc += -errtags -erroff=%none,E_UNRECOGNIZED_PRAGMA_IGNORED,E_ATTRIBUTE_UNKNOWN,E_NONPORTABLE_BIT_FIELD_TYPE -errwarn=%all -D_XOPEN_SOURCE=600 -D__EXTENSIONS__=1
CFLAGS_cc += -pedantic -v
CFLAGS_gcc = -fPIC -fsigned-char -pipe -Wno-unknown-pragmas -Wno-unused-result
CFLAGS_gcc += -fdiagnostics-show-option -Wall -Werror
CFLAGS_gcc += -pedantic -Wpointer-arith -Wwrite-strings -Wstrict-prototypes -Wnested-externs -Winline -Wextra -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-include-dirs -Wredundant-decls -Wshadow -Wundef -Wunused -Wno-variadic-macros -Wno-parentheses -Wcast-align -Wcast-qual
CFLAGS_gcc += -Wno-unused-function -Wno-multistatement-macros

# For Ubuntu Linux the package "cuda-nvml-dev-N-M" with N >= 10 and M >= 0
# and "libnvidia-compute-X" must be installed. Otherwise adjust as needed:
# nvml.h and libnvidia-ml.so.1 are required.
CFLAGS_Linux = -I$(CUDA_DIR)/targets/x86_64-linux/include
CFLAGS_SunOS = -I/usr/include/microhttpd -D_MHD_DEPR_MACRO
CFLAGS_libprom ?= $(shell [ -d /usr/include/libprom ] && printf -- '-I/usr/include/libprom' )
#CFLAGS_libprom += $(shell [ -d ../libprom/prom/include ] && printf -- '-I../libprom/prom/include' )
CFLAGS ?= -m$(MACH) $(CFLAGS_$(CC)) $(CFLAGS_libprom) $(OPTIMZE) -g
CFLAGS += -std=c11 -DVERSION=\"$(VERSION)\"
CFLAGS += -DPROM_LOG_ENABLE -D_XOPEN_SOURCE=600 -DLEGACY=$(LEGACY)
CFLAGS += $(CFLAGS_$(OS))
# For Solaris the package 'driver/graphics/nvidia' must be installed. Also copy
# the nvml.h from Ubuntu to .
#CFLAGS += $(shell [ -e nvml.h ] && printf -- '-I.' )

LIBS_SunOS = -lsocket -lnsl
LIBS_Linux = -L $(CUDA_DIR)/targets/x86_64-linux/lib/stubs
#LIBS_libprom += $(shell [ -d ../libprom/prom/build ] && printf -- '-L ../libprom/prom/build' )
LIBS ?= $(LIBS_$(OS)) $(LIBS_libprom)
LIBS += -lmicrohttpd -lnvidia-ml -lprom

SHARED_cc := -G
SHARED_gcc := -shared
LDFLAGS_cc := -zdefs -Bdirect -zdiscard-unused=dependencies $(LIBS)
LDFLAGS_gcc := -zdefs -Wl,--as-needed $(LIBS)
SONAME_OPT_cc := -h
SONAME_OPT_gcc := -Wl,-soname,
RPATH_OPT_cc := -R
RPATH_OPT_gcc := -Wl,-rpath=

LDFLAGS ?= -m$(MACH) $(LDFLAGS_$(CC))
SHARED := $(SHARED_$(CC))
SONAME_OPT := $(SONAME_OPT_$(CC))
RPATH_OPT := $(RPATH_OPT_$(CC))

LIBCFLAGS = $(CFLAGS) $(SHARED) $(LDFLAGS) -lc

PROGS= nvmex
DYNLIBEXT= .so
DYNLIB_MAJOR= 1
DYNLIB_MINOR= 0

LIBRARY= nvmex
SOBN= lib$(LIBRARY)$(DYNLIBEXT)
SONAME= $(SOBN).$(DYNLIB_MAJOR)
# uncomment to get a lib
#DYNLIB= $(SONAME).$(DYNLIB_MINOR)

FBC_0 = fbc.c
FBC_1 =
LIBSRCS= inspect.c clocks.c bar1memory.c temperature.c power.c fan.c \
	util.c pcie.c violations.c memory.c ecc.c nvlink.c enc.c $(FBC_$(LEGACY))
LIBOBJS= $(LIBSRCS:%.c=%.o)

PROGSRCS = main.c $(LIBSRCS)
PROGOBJS = $(PROGSRCS:%.c=%.o) 

all:	$(PROGS)
lib:	$(DYNLIB)

$(PROGS):	LDFLAGS += $(RPATH_OPT)\$$ORIGIN:\$$ORIGIN/../$(LIBDIR)

%.so: %.c %.h $(DYNLIB)
	$(CC) -o $@ $< $(DYNLIB) $(LIBCFLAGS)

$(DYNLIB): Makefile $(LIBOBJS)
	$(CC) -o $@ $(SHARED) $(SONAME_OPT)$(SONAME) $(LIBOBJS) $(LIBCFLAGS)
	ln -sf $(DYNLIB) $(SONAME)

$(PROGS):	Makefile $(DYNLIB) $(PROGOBJS)
	[ -z $(DYNLIB) ] && $(CC) -o $@ $(PROGOBJS) $(LDFLAGS) || \
	$(CC) -o $@ main.o $(DYNLIB) $(LDFLAGS)

.PHONY:	clean distclean install depend

# for maintainers to get _all_ deps wrt. source headers properly honored
DEPENDFILE := makefile.dep

depend: $(DEPENDFILE)

# on Ubuntu, makedepend is included in the 'xutils-dev' package
$(DEPENDFILE): *.c *.h
	makedepend -f - -Y/usr/include *.c 2>/dev/null | \
		sed -e 's@/usr/include/[^ ]*@@g' -e '/: *$$/ d' >makefile.dep

clean:
	rm -f *.o *~ *.so $(SONAME)* $(PROGS) \
		core gmon.out a.out man.1

distclean: clean
	rm -f $(DEPENDFILE) *.rej *.orig

install:	all
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/$(BINDIR)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/$(MANDIR)
	$(INSTALL) -m 755 $(PROGS) $(DESTDIR)$(PREFIX)/$(BINDIR)
	$(INSTALL) -m 644 nvmex.8 $(DESTDIR)$(PREFIX)/$(MANDIR)/nvmex.8
	[ -z $(DYNLIB) ] || $(MAKE) install-lib

install-lib: $(DYNLIB)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/$(LIBDIR)
	$(INSTALL) -m 755 $(DYNLIB) $(DESTDIR)$(PREFIX)/$(LIBDIR)
	ln -sf $(DYNLIB) $(DESTDIR)$(PREFIX)/$(LIBDIR)/$(SONAME)
	ln -sf $(DYNLIB) $(DESTDIR)$(PREFIX)/$(LIBDIR)/$(SOBN)

-include $(DEPENDFILE)
