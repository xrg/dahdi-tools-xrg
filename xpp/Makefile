PEDANTIC	= -ansi -pedantic -std=c99

RANLIB		= ranlib
INSTALL		= install
INSTALL_DATA	= install -m 644

#
# Ugly hack to find kernel directories before/after the split
# to kernel/user-space.
#
# These variables should be passed to us. But until then...
#
DAHDI_TOOLSDIR	?= ..
DAHDI_KERNELDIR	=

-include $(DAHDI_TOOLSDIR)/makeopts

INSTALL_DATA	= $(INSTALL) -m 644

# In 1.4 those are provided by autoconf through makeopts
prefix		?= /usr
datadir		?= $(prefix)/share
mandir		?= $(datadir)/man
INSTALL		?= install

INSTALL_DATA	= $(INSTALL) -m 644

SBINDIR		= $(prefix)/sbin
DATADIR		= $(datadir)/dahdi
MANDIR		= $(mandir)/man8
HOTPLUG_USB_DIR	= /etc/hotplug/usb
PERLLIBDIR	:= $(shell eval `perl -V:sitelib`; echo "$$sitelib")
PERL_DIRS	:= $(shell cd perl_modules; find * -name '[A-Z]*' -type d| xargs)
PERL_MODS_PAT	:= *.pm $(PERL_DIRS:%=%/*.pm)
PERL_MODS	:= $(shell cd perl_modules; echo $(PERL_MODS_PAT))

# Variables that should be defined above, but need sane defaults:
# FIXME: Are those values really sane?
HOSTCC		?= $(CC)


CFLAGS		= -g -Wall $(EXTRA_CFLAGS)

%.8: %
	pod2man --section 8 $^ > $@ || $(RM) $@
PERL_SCRIPTS	=	\
		dahdi_registration	\
		xpp_sync	\
		lsdahdi		\
		xpp_blink	\
		dahdi_genconf	\
		dahdi_hardware	\
		#

PERL_MANS	= $(PERL_SCRIPTS:%=%.8)

TARGETS	= perlcheck
PROG_INSTALL	= #genzaptelconf
MAN_INSTALL	= $(PROG_INSTALL:%=%.8)
ifeq	(1,$(PBX_USB))
TARGETS	+= libhexfile.a fpga_load test_parse
PROG_INSTALL	+= fpga_load
endif
ifneq	(,$(PERLLIBDIR))
PROG_INSTALL	+= $(PERL_SCRIPTS)
TARGETS		+= $(PERL_MANS)
endif

all: $(TARGETS)

docs: $(PERL_MANS)

install: all
	$(INSTALL) -d $(DESTDIR)$(SBINDIR)
	$(INSTALL) $(PROG_INSTALL) $(DESTDIR)$(SBINDIR)/
	$(INSTALL) -d $(DESTDIR)$(DATADIR)
	$(INSTALL) xpp_fxloader $(DESTDIR)$(DATADIR)/
	$(INSTALL) -d $(DESTDIR)$(MANDIR)
	$(INSTALL_DATA) $(MAN_INSTALL) $(DESTDIR)$(MANDIR)/
	$(INSTALL) -d $(DESTDIR)$(HOTPLUG_USB_DIR)
	$(INSTALL_DATA) xpp_fxloader.usermap $(DESTDIR)$(HOTPLUG_USB_DIR)/
	# for backward compatibility and for hotplug users:
	ln -sf $(DATADIR)/xpp_fxloader $(DESTDIR)$(HOTPLUG_USB_DIR)/
ifneq	(,$(PERLLIBDIR))
	$(INSTALL) -d $(DESTDIR)$(PERLLIBDIR)
	for i in $(PERL_DIRS); \
	do \
		$(INSTALL) -d "$(DESTDIR)$(PERLLIBDIR)/$$i"; \
	done
	for i in $(PERL_MODS); \
	do \
		$(INSTALL_DATA) "perl_modules/$$i" "$(DESTDIR)$(PERLLIBDIR)/$$i"; \
	done
endif

libhexfile.a: hexfile.o
	$(AR) cru $@ $^
	$(RANLIB) $@

fpga_load: fpga_load.o libhexfile.a
	$(CC) -L. -o $@ $@.o $(EXTRA_LIBS) -lhexfile -lusb

fpga_load.o: CFLAGS+=-D_GNU_SOURCE	# We use memrchr()

hexfile.o: hexfile.c hexfile.h
	$(CC) $(CFLAGS) $(PEDANTIC) -c $<

test_parse.o: test_parse.c hexfile.h
	$(CC) $(CFLAGS) $(PEDANTIC) -c $<

test_parse: test_parse.o libhexfile.a
	$(CC) -L. -o $@ $@.o $(EXTRA_LIBS) -lhexfile -lusb

perlcheck: $(PERL_SCRIPTS)
	for i in $^; do perl -I./perl_modules -c $$i || exit 1; done
	touch $@

clean:
	$(RM) *.o $(TARGETS)
