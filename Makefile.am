EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-I${top_srcdir}/src

AM_CFLAGS = ${my_CFLAGS} \
	-fvisibility=hidden \
	-ffunction-sections \
	-fdata-sections

AM_LDFLAGS = \
	-Wl,--gc-sections \
	-Wl,--as-needed

LIBLOC_CURRENT=0
LIBLOC_REVISION=0
LIBLOC_AGE=0

SED_PROCESS = \
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(SED) \
	-e 's,@VERSION\@,$(VERSION),g' \
	-e 's,@prefix\@,$(prefix),g' \
	-e 's,@exec_prefix\@,$(exec_prefix),g' \
	-e 's,@libdir\@,$(libdir),g' \
	-e 's,@includedir\@,$(includedir),g' \
	< $< > $@ || rm $@

pkgconfigdir = $(libdir)/pkgconfig

%.pc: %.pc.in Makefile
	$(SED_PROCESS)

pkginclude_HEADERS = \
	src/loc/libloc.h

lib_LTLIBRARIES = \
	src/libloc.la

src_libloc_la_SOURCES =\
	src/libloc-private.h \
	src/libloc.c \
	src/stringpool.c \
	src/stringpool.h

EXTRA_DIST += src/libloc.sym

src_libloc_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(LIBLOC_CURRENT):$(LIBLOC_REVISION):$(LIBLOC_AGE) \
	-Wl,--version-script=$(top_srcdir)/src/libloc.sym

src_libloc_la_DEPENDENCIES = \
	${top_srcdir}/src/libloc.sym

pkgconfig_DATA = \
	src/libloc.pc

EXTRA_DIST += \
	src/libloc.pc.in

CLEANFILES += \
	src/libloc.pc

TESTS = \
	src/test-libloc \
	src/test-stringpool

check_PROGRAMS = \
	src/test-libloc \
	src/test-stringpool

src_test_libloc_SOURCES = \
	src/test-libloc.c

src_test_libloc_LDADD = \
	src/libloc.la

src_test_stringpool_SOURCES = \
	src/test-stringpool.c

src_test_stringpool_LDADD = \
	src/libloc.la
