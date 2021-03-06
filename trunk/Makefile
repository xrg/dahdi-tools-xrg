#
# Makefile for DAHDI tools
#
# Copyright (C) 2001-2008 Digium, Inc.
#
#

CFLAGS+=-DBUILDING_TONEZONE

# If the file .dahdi.makeopts is present in your home directory, you can
# include all of your favorite menuselect options so that every time you download
# a new version of Asterisk, you don't have to run menuselect to set them.
# The file /etc/dahdi.makeopts will also be included but can be overridden
# by the file in your home directory.

GLOBAL_MAKEOPTS=$(wildcard /etc/dahdi.makeopts)
USER_MAKEOPTS=$(wildcard ~/.dahdi.makeopts)

ifeq ($(strip $(foreach var,clean distclean dist-clean update,$(findstring $(var),$(MAKECMDGOALS)))),)
 ifneq ($(wildcard menuselect.makeopts),)
  include menuselect.makeopts
 endif
endif

ifeq ($(strip $(foreach var,clean distclean dist-clean update,$(findstring $(var),$(MAKECMDGOALS)))),)
 ifneq ($(wildcard makeopts),)
  include makeopts
 endif
endif

SUBDIRS_UTILS_ALL:= ppp
SUBDIRS_UTILS := xpp

OPTFLAGS=-O2
CFLAGS+=-I. $(OPTFLAGS) -g -fPIC -Wall -DBUILDING_TONEZONE #-DTONEZONE_DRIVER
ifneq (,$(findstring ppc,$(UNAME_M)))
CFLAGS_PPC:=-fsigned-char
endif
ifneq (,$(findstring x86_64,$(UNAME_M)))
CFLAGS_x86_64:=-m64
endif
CFLAGS+=$(CFLAGS_PPC) $(CFLAGS_x86_64)

ROOT_PREFIX=

CFLAGS+=$(DAHDI_INCLUDE)

CHKCONFIG	:= $(wildcard /sbin/chkconfig)
UPDATE_RCD	:= $(wildcard /usr/sbin/update-rc.d)
ifeq (,$(DESTDIR))
  ifneq (,$(CHKCONFIG))
    ADD_INITD	:= $(CHKCONFIG) --add dahdi
  else
    ifndef (,$(UPDATE_RCD))
      ADD_INITD	:= $(UPDATE_RCD) dahdi defaults 15 30
    endif
  endif
endif

INITRD_DIR	:= $(firstword $(wildcard /etc/rc.d/init.d /etc/init.d))
ifneq (,$(INITRD_DIR))
  INIT_TARGET	:= $(DESTDIR)$(INITRD_DIR)/dahdi
  COPY_INITD	:= install -D dahdi.init $(INIT_TARGET)
endif

RCCONF_FILE	= /etc/dahdi/init.conf
MODULES_FILE	= /etc/dahdi/modules
MODPROBE_FILE	= /etc/modprobe.d/dahdi
BLACKLIST_FILE	= /etc/modprobe.d/dahdi.blacklist

NETSCR_DIR	:= $(firstword $(wildcard /etc/sysconfig/network-scripts ))
ifneq (,$(NETSCR_DIR))
  NETSCR_TARGET	:= $(DESTDIR)$(NETSCR_DIR)/ifup-hdlc
  COPY_NETSCR	:= install -D ifup-hdlc $(NETSCR_TARGET)
endif

ifneq ($(wildcard .version),)
  TOOLSVERSION:=$(shell cat .version)
else
ifneq ($(wildcard .svn),)
  TOOLSVERSION=$(shell build_tools/make_version . dahdi/tools)
endif
endif

LTZ_A:=libtonezone.a
LTZ_A_OBJS:=zonedata.o tonezone.o
LTZ_SO:=libtonezone.so
LTZ_SO_OBJS:=zonedata.lo tonezone.lo
LTZ_SO_MAJOR_VER:=1
LTZ_SO_MINOR_VER:=0

# sbindir, libdir, includedir and mandir are defined in makeopts
# (from configure).
BIN_DIR:=$(sbindir)
LIB_DIR:=$(libdir)
INC_DIR:=$(includedir)/dahdi
MAN_DIR:=$(mandir)/man8
CONFIG_DIR:=$(sysconfdir)/dahdi
CONFIG_FILE:=$(CONFIG_DIR)/system.conf

# Utilities we build with a standard build procedure:
UTILS		= dahdi_tool dahdi_test dahdi_monitor dahdi_speed sethdlc dahdi_cfg \
		  fxstest fxotune dahdi_diag dahdi_scan

# some tests:
UTILS		+= patgen pattest patlooptest hdlcstress hdlctest hdlcgen \
		   hdlcverify timertest

UTILSO		= $(UTILS:%=%.o)

BINS:=fxotune fxstest sethdlc dahdi_cfg dahdi_diag dahdi_monitor dahdi_speed dahdi_test dahdi_scan dahdi_tool
BINS:=$(filter-out $(MENUSELECT_UTILS),$(BINS))
MAN_PAGES:=$(wildcard $(BINS:%=doc/%.8))

# All the man pages. Not just installed ones:
GROFF_PAGES	:= $(wildcard doc/*.8 xpp/*.8)
GROFF_HTML	:= $(GROFF_PAGES:%=%.html)

all: menuselect.makeopts 
	@$(MAKE) _all

_all: prereq programs

libs: $(LTZ_SO) $(LTZ_A)

utils-subdirs:
	@for dir in $(SUBDIRS_UTILS); do \
		$(MAKE) -C $$dir; \
	done

programs: libs utils

utils: $(BINS) utils-subdirs

version.h:
	@TOOLSVERSION="${TOOLSVERSION}" build_tools/make_version_h > $@.tmp
	@if cmp -s $@.tmp $@ ; then :; else \
		mv $@.tmp $@ ; \
	fi
	@rm -f $@.tmp

tests: patgen pattest patlooptest hdlcstress hdlctest hdlcgen hdlcverify timertest

zonedata.o: tonezone.h

zonedata.lo: zonedata.c tonezone.h
	$(CC) -c $(CFLAGS) -o $@ $<

tonezone.o: tonezone.h

tonezone.lo: tonezone.c tonezone.h
	$(CC) -c $(CFLAGS) -o $@ $<

prereq: config.status version.h

dahdi_tool.o: CFLAGS+=$(NEWT_INCLUDE)
dahdi_tool: LDLIBS+=$(NEWT_LIB)

dahdi_speed: CFLAGS+=-O0

$(LTZ_A): $(LTZ_A_OBJS)
	ar rcs $@ $^
	ranlib $@

$(LTZ_SO): $(LTZ_SO_OBJS)
	$(CC) $(CFLAGS) -shared -Wl,-soname,$(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER) -o $@ $^ $(LDFLAGS) $(LDLIBS) -lm

dahdi_cfg: dahdi_cfg.o $(LTZ_A)
dahdi_cfg: LDLIBS+=-lm

dahdi_cfg-shared: dahdi_cfg.o $(LTZ_SO)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) -lm

# FIXME: we assume CC can build the C++ modules:
complex.o mknotch.o: %.o: %.cc
	$(CC) $(CFLAGS) -o $@ -c $<

fxstest: $(LTZ_SO)
fxstest: LDLIBS+=-lm
fxotune: LDLIBS+=-lm

tonezones.txt: zonedata.c
	perl -ne 'next unless (/\.(country|description) = *"([^"]*)/); \
		print (($$1 eq "country")? "* $$2\t":"$$2\n");' $<  \
	>$@

%.asciidoc: %.sample
	perl -n -e \
		'if (/^#($$|\s)(.*)/){ if (!$$in_doc){print "\n"}; $$in_doc=1; print "$$2\n" } else { if ($$in_doc){print "\n"}; $$in_doc=0; print "  $$_" }' \
		$< \
	| perl -p -e 'if (/^  #?(\w+)=/ && ! exists $$cfgs{$$1}){my $$cfg = $$1; $$cfgs{$$cfg} = 1; s/^/\n[[cfg_$$cfg]]\n/}'  >$@

README.html: README system.conf.asciidoc init.conf.asciidoc tonezones.txt
	$(ASCIIDOC) -n -a toc -a toclevels=3 $<

kernel/xpp/README.Astribank.html: kernel/xpp/README.Astribank
	cd $(@D); $(ASCIIDOC) -o $(@F) -n -a toc -a toclevels=4 $(<F)

# on Debian: this requires the full groof, not just groff-base.
%.8.html: %.8
	man -Thtml $^ >$@

htmlman: $(GROFF_HTML)

$(UTILS): %: %.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(UTILSO): %.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

install: all install-programs
	@echo "###################################################"
	@echo "###"
	@echo "### DAHDI tools installed successfully."
	@echo "### If you have not done so before, install init scripts with:"
	@echo "###"
	@echo "###   make config"
	@echo "###"
	@echo "###################################################"

install-programs: install-utils install-libs

install-utils: utils install-utils-subdirs
ifneq (,$(BINS))
	install -d $(DESTDIR)$(BIN_DIR)
	install  $(BINS) $(DESTDIR)$(BIN_DIR)/
	install -d $(DESTDIR)$(MAN_DIR)
	install -m 644 $(MAN_PAGES) $(DESTDIR)$(MAN_DIR)/
endif
ifeq (,$(wildcard $(DESTDIR)$(CONFIG_FILE)))
	$(INSTALL) -D -m 644 system.conf.sample $(DESTDIR)$(CONFIG_FILE)
endif

install-libs: libs
	$(INSTALL) -D -m 755 $(LTZ_A) $(DESTDIR)$(LIB_DIR)/$(LTZ_A)
	$(INSTALL) -D -m 755 $(LTZ_SO) $(DESTDIR)$(LIB_DIR)/$(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER)
ifeq (,$(DESTDIR))
	if [ `id -u` = 0 ]; then \
		/sbin/ldconfig || : ;\
	fi
endif
	rm -f $(DESTDIR)$(LIB_DIR)$(LTZ_SO)
	$(LN) -sf $(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER) \
		$(DESTDIR)$(LIB_DIR)/$(LTZ_SO).$(LTZ_SO_MAJOR_VER)
	$(LN) -sf $(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER) \
		$(DESTDIR)$(LIB_DIR)/$(LTZ_SO)
ifneq (no,$(USE_SELINUX))
  ifeq (,$(DESTDIR))
	/sbin/restorecon -v $(DESTDIR)$(LIB_DIR)/$(LTZ_SO)
  endif
endif
	$(INSTALL) -D -m 644 tonezone.h $(DESTDIR)$(INC_DIR)/tonezone.h

install-utils-subdirs:
	@for dir in $(SUBDIRS_UTILS); do \
		$(MAKE) -C $$dir install; \
	done

config:
ifneq (,$(COPY_INITD))
	$(COPY_INITD)
endif
ifeq (,$(wildcard $(DESTDIR)$(RCCONF_FILE)))
	$(INSTALL) -D -m 644 init.conf.sample $(DESTDIR)$(RCCONF_FILE)
endif
ifeq (,$(wildcard $(DESTDIR)$(MODULES_FILE)))
	$(INSTALL) -D -m 644 modules.sample $(DESTDIR)$(MODULES_FILE)
endif
ifeq (,$(wildcard $(DESTDIR)$(MODPROBE_FILE)))
	$(INSTALL) -D -m 644 modprobe.conf.sample $(DESTDIR)$(MODPROBE_FILE)
endif
ifeq (,$(wildcard $(DESTDIR)$(BLACKLIST_FILE)))
	$(INSTALL) -D -m 644 blacklist.sample $(DESTDIR)$(BLACKLIST_FILE)
endif
ifneq (,$(COPY_NETSCR))
	$(COPY_NETSCR)
endif
ifneq (,$(ADD_INITD))
	$(ADD_INITD)
endif
	@echo "DAHDI has been configured."
	@echo ""
	@echo "If you have any DAHDI hardware it is now recommended to "
	@echo "edit /etc/dahdi/init.conf and set there an optimal value "
	@echo "for the variable MODULES ."
	@echo ""
	@echo "I think that the DAHDI hardware you have on your system is:"
	@kernel/xpp/utils/dahdi_hardware || true

update:
	@if [ -d .svn ]; then \
		echo "Updating from Subversion..." ; \
		svn update | tee update.out; \
		rm -f .version; \
		if [ `grep -c ^C update.out` -gt 0 ]; then \
			echo ; echo "The following files have conflicts:" ; \
			grep ^C update.out | cut -b4- ; \
		fi ; \
		rm -f update.out; \
	else \
		echo "Not under version control";  \
	fi

clean:
	-@$(MAKE) -C menuselect clean
	rm -f $(BINS)
	rm -f *.o dahdi_cfg tzdriver sethdlc
	rm -f $(LTZ_SO) $(LTZ_A) *.lo
	@for dir in $(SUBDIRS_UTILS_ALL); do \
		$(MAKE) -C $$dir clean; \
	done
	@for dir in $(SUBDIRS_UTILS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -f libtonezone*
	rm -f fxotune
	rm -f core
	rm -f dahdi_cfg-shared fxstest
	rm -rf $(GROFF_HTML)
	rm -rf README.html xpp/README.Astribank.html *.asciidoc tonezones.txt

distclean: dist-clean

dist-clean: clean
	@$(MAKE) -C menuselect dist-clean
	rm -f makeopts menuselect.makeopts menuselect-tree build_tools/menuselect-deps
	rm -f config.log config.status

config.status: configure
	@CFLAGS="" ./configure
	@echo "****"
	@echo "**** The configure script was just executed, so 'make' needs to be"
	@echo "**** restarted."
	@echo "****"
	@exit 1

menuselect.makeopts: menuselect/menuselect menuselect-tree
	@menuselect/menuselect --check-deps ${GLOBAL_MAKEOPTS} ${USER_MAKEOPTS} $@

menuconfig: menuselect

menuselect: menuselect/menuselect menuselect-tree
	-@menuselect/menuselect $(GLOBAL_MAKEOPTS) $(USER_MAKEOPTS) menuselect.makeopts && echo "menuselect changes saved!" || echo "menuselect changes NOT saved!"

menuselect/menuselect: menuselect/menuselect.c menuselect/menuselect_curses.c menuselect/menuselect_stub.c menuselect/menuselect.h menuselect/linkedlists.h config.status
	@CFLAGS="" $(MAKE) -C menuselect CC=$(HOSTCC)

menuselect-tree: dahdi.xml
	@echo "Generating input for menuselect ..."
	@build_tools/make_tree > $@

.PHONY: menuselect distclean dist-clean clean version.h all _all install programs tests devel data config update install-programs install-libs install-utils-subdirs utils-subdirs
