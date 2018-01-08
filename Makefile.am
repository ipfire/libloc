EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-I${top_srcdir}/src

AM_CFLAGS = ${my_CFLAGS} \
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

pythondir    = $(pyexecdir)
pkgconfigdir = $(libdir)/pkgconfig

%.pc: %.pc.in Makefile
	$(SED_PROCESS)

pkginclude_HEADERS = \
	src/loc/libloc.h \
	src/loc/as.h \
	src/loc/database.h \
	src/loc/format.h \
	src/loc/network.h \
	src/loc/private.h \
	src/loc/stringpool.h \
	src/loc/writer.h

lib_LTLIBRARIES = \
	src/libloc.la

src_libloc_la_SOURCES = \
	src/libloc.c \
	src/as.c \
	src/database.c \
	src/network.c \
	src/stringpool.c \
	src/writer.c

EXTRA_DIST += src/libloc.sym

src_libloc_la_CFLAGS = \
	$(AM_CFLAGS) \
	-DLIBLOC_PRIVATE \
	-fvisibility=hidden

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

pkgpyexec_LTLIBRARIES = \
	src/python/location.la

src_python_location_la_SOURCES = \
	src/python/locationmodule.c \
	src/python/locationmodule.h \
	src/python/as.c \
	src/python/as.h \
	src/python/database.c \
	src/python/database.h \
	src/python/network.c \
	src/python/network.h \
	src/python/writer.c \
	src/python/writer.h

src_python_location_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(PYTHON_CFLAGS)

src_python_location_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared \
	-module \
	-avoid-version

src_python_location_la_LIBADD = \
	src/libloc.la \
	$(PYTHON_LIBS)

TESTS_CFLAGS = \
	$(AM_CFLAGS) \
	-DLIBLOC_PRIVATE

TESTS = \
	src/test-libloc \
	src/test-stringpool \
	src/test-database \
	src/test-as \
	src/test-network

check_PROGRAMS = \
	src/test-libloc \
	src/test-stringpool \
	src/test-database \
	src/test-as \
	src/test-network

src_test_libloc_SOURCES = \
	src/test-libloc.c

src_test_libloc_CFLAGS = \
	$(TESTS_CFLAGS)

src_test_libloc_LDADD = \
	src/libloc.la

src_test_as_SOURCES = \
	src/test-as.c

src_test_as_CFLAGS = \
	$(TESTS_CFLAGS)

src_test_as_LDADD = \
	src/libloc.la

src_test_network_SOURCES = \
	src/test-network.c

src_test_network_CFLAGS = \
	$(TESTS_CFLAGS)

src_test_network_LDADD = \
	src/libloc.la

src_test_stringpool_SOURCES = \
	src/test-stringpool.c

src_test_stringpool_CFLAGS = \
	$(TESTS_CFLAGS)

src_test_stringpool_LDADD = \
	src/libloc.la

src_test_database_SOURCES = \
	src/test-database.c

src_test_database_CFLAGS = \
	$(TESTS_CFLAGS)

src_test_database_LDADD = \
	src/libloc.la
