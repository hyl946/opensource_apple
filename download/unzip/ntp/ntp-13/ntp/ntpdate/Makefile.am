#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
sbin_PROGRAMS = ntpdate
INCLUDES = -I$(top_srcdir)/include
# LDADD might need RESLIB and ADJLIB
LDADD =	version.o ../libntp/libntp.a @LIBRSAREF@
DISTCLEANFILES = .version stamp-v
noinst_HEADERS = ntpdate.h
#EXTRA_DIST = ntpdate.mak
ETAGS_ARGS = Makefile.am

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

version.o: $(ntpdate_OBJECTS) ../libntp/libntp.a @LIBRSAREF@ Makefile
	$(top_builddir)/scripts/mkver ntpdate
	$(COMPILE) -c version.c
