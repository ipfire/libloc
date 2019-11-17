AC_PREREQ(2.60)
AC_INIT([libloc],
        [0],
        [michael.tremer@ipfire.org],
        [libloc],
        [https://www.ipfire.org/])

AC_CONFIG_SRCDIR([src/libloc.c])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([
	foreign
	1.11
	-Wall
	-Wno-portability
	silent-rules
	tar-pax
	no-dist-gzip
	dist-xz
	subdir-objects
])
AC_PROG_CC_STDC
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])
LT_INIT([
	disable-static
	pic-only
])
AC_PREFIX_DEFAULT([/usr])
gl_LD_VERSION_SCRIPT

IT_PROG_INTLTOOL([0.40.0])
GETTEXT_PACKAGE=${PACKAGE_TARNAME}
AC_SUBST(GETTEXT_PACKAGE)

AC_PROG_SED
AC_PROG_MKDIR_P

# - man ------------------------------------------------------------------------

have_manpages=no
AC_ARG_ENABLE(manpages, AS_HELP_STRING([--disable-man-pages],
	[do not install man pages]))
AS_IF([test "x$enable_manpages" != xno], [have_manpages=yes])
AM_CONDITIONAL(ENABLE_MANPAGES, [test "x$have_manpages" = "xyes"])

AC_PATH_PROG([XSLTPROC], [xsltproc])

AC_CHECK_PROGS(ASCIIDOC, [asciidoc])
if test "${have_manpages}" = "yes" && test -z "${ASCIIDOC}"; then
	AC_MSG_ERROR([Required program 'asciidoc' not found])
fi
# - debug ----------------------------------------------------------------------

AC_ARG_ENABLE([debug],
        AS_HELP_STRING([--enable-debug], [enable debug messages @<:@default=disabled@:>@]),
        [], [enable_debug=no])
AS_IF([test "x$enable_debug" = "xyes"], [
        AC_DEFINE(ENABLE_DEBUG, [1], [Debug messages.])
])

AC_CHECK_HEADERS_ONCE([
	arpa/inet.h \
	endian.h \
	netinet/in.h \
	string.h \
])

AC_CHECK_FUNCS([ \
        be16toh \
        be32toh \
        be64toh \
        htobe16 \
        htobe32 \
        htobe64 \
        mmap \
        munmap \
	__secure_getenv \
	secure_getenv \
        qsort \
])

my_CFLAGS="\
-Wall \
-Wchar-subscripts \
-Wformat-security \
-Wmissing-declarations \
-Wmissing-prototypes \
-Wnested-externs \
-Wpointer-arith \
-Wshadow \
-Wsign-compare \
-Wstrict-prototypes \
-Wtype-limits \
"
AC_SUBST([my_CFLAGS])

# ------------------------------------------------------------------------------

AC_ARG_WITH([systemd],
	AS_HELP_STRING([--with-systemd], [Enable systemd support.])
)

AS_IF([test "x$with_systemd" != "xno"],
      [PKG_CHECK_MODULES(systemd, [libsystemd],
      [have_systemd=yes], [have_systemd=no])],
      [have_systemd=no]
)

AS_IF([test "x$have_systemd" = "xyes"],
      [AC_MSG_CHECKING([for systemd system unit directory])
	AC_ARG_WITH([systemdsystemunitdir],
		AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
		[], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)]
	)

	AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])

	if test -n "$systemdsystemunitdir" -a "x$systemdsystemunitdir" != xno; then
		AC_MSG_RESULT([$systemdsystemunitdir])
	else
		AC_MSG_ERROR([not found (try --with-systemdsystemunitdir)])
	fi
      ],
      [AS_IF([test "x$with_systemd" = "xyes"],
             [AC_MSG_ERROR([Systemd support is enabled but no systemd has been found.])
      ])
])

AM_CONDITIONAL(HAVE_SYSTEMD, [test "x$have_systemd" = "xyes"])

# ------------------------------------------------------------------------------

# Python
AM_PATH_PYTHON([3.4])
PKG_CHECK_MODULES([PYTHON], [python-${PYTHON_VERSION}])

# Perl
AC_PATH_PROG(PERL, perl, no)
AC_SUBST(PERL)

AX_PROG_PERL_MODULES(ExtUtils::MakeMaker,, AC_MSG_WARN(Need some Perl modules))

AC_ARG_ENABLE(perl, AS_HELP_STRING([--disable-perl], [do not build the perl modules]), [],[enable_perl=yes])
AM_CONDITIONAL(ENABLE_PERL, test "$enable_perl" = "yes")

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([
        Makefile
        po/Makefile.in
])

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE $VERSION
        =====

        prefix:                 ${prefix}
        sysconfdir:             ${sysconfdir}
        libdir:                 ${libdir}
        includedir:             ${includedir}

        compiler:               ${CC}
        cflags:                 ${CFLAGS}
        ldflags:                ${LDFLAGS}

        debug:                  ${enable_debug}
        systemd support:        ${have_systemd}

	Bindings:
	  perl:			${enable_perl}
])
