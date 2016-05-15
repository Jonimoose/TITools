# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([TITools], [0.2], [floppusmaximus@users.sf.net])
AC_CONFIG_SRCDIR([src/titools.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

if test "x$GCC" = "xyes" ; then
  CFLAGS="$CFLAGS -W -Wall -Wwrite-strings"
fi

# Checks for header files.
AC_CHECK_HEADERS([stdint.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for libraries.
AC_ARG_WITH(ticalcs2,
  AC_HELP_STRING([--with-ticalcs2],
    [use libticalcs2 (default)]),
    [ use_ticalcs=$withval ], [ use_ticalcs=no ])
AC_ARG_WITH(calcprotocols,
  AC_HELP_STRING([--with-calcprotocols],
    [use libcalcprotocols]),
    [ use_calcprotocols=$withval ], [ use_calcprotocols=no ])

if test "x$TICALCS_LIBS" = "x" ; then
  PKG_PROG_PKG_CONFIG
fi

init_function=ticalcs_library_init

if test "x$TICALCS_LIBS" = "x" &&
   test "x$use_calcprotocols" = "xno" ; then
  PKG_CHECK_MODULES(TICALCS, ticalcs2, [ : ], [ : ])
  PKG_CHECK_MODULES(TICABLES, ticables2, [ : ], [ : ])
  PKG_CHECK_MODULES(TIFILES, tifiles2, [ : ], [ : ])
  PKG_CHECK_MODULES(TICONV, ticonv, [ : ], [ : ])
fi

if test "x$TICALCS_LIBS" = "x" &&
   test "x$use_ticalcs" = "xno" ; then
  PKG_CHECK_MODULES(CALCPROTOCOLS, calcprotocols,
    [ TICALCS_CFLAGS="$CALCPROTOCOLS_CFLAGS -I\$(top_srcdir)/compat"
      TICALCS_LIBS="$CALCPROTOCOLS_LIBS"
      init_function=calcprotocols_library_init ],
    [ : ])
fi

save_cflags="$CFLAGS"
save_libs="$LIBS"
CFLAGS="$CFLAGS $TICALCS_CFLAGS $TICABLES_CFLAGS $TIFILES_CFLAGS $TICONV_CFLAGS"
LIBS="$LIBS $TICALCS_LIBS $TICABLES_LIBS $TIFILES_LIBS $TICONV_LIBS"
AC_CHECK_FUNC($init_function,
  [ have_ticalcs=yes ],
  [ have_ticalcs=no ])
CFLAGS="$save_cflags"
LIBS="$save_libs"

if test "x$have_ticalcs" != "xyes" ; then
  AC_MSG_ERROR([libticalcs2/libcalcprotocols not found.
You must install one of these two libraries, including the
-dev/-devel packages if appropriate.])
fi

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile])
AC_OUTPUT
