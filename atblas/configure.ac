dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.68)
AC_INIT([atblas],[0.1.0],[marco.maggi-ipsu@poste.it])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE
LT_PREREQ([2.2])
LT_INIT
AC_CONFIG_SRCDIR([blas/])
AM_MAINTAINER_MODE
AC_SUBST(SHARED_VERSION_INFO)
AC_CANONICAL_HOST

AC_LANG([Fortran 77])

dnl Checks for programs.
AC_PROG_F77
AC_PROG_F77_C_O
AC_F77_LIBRARY_LDFLAGS
AC_F77_WRAPPERS

AC_LANG([C])
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CC_STDC

AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
dnl AC_PROG_LIBTOOL

dnl Checks for header files.
AC_HEADER_STDC

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

dnl This is needed by CBLAS.
CFLAGS="$CFLAGS -DADD_"

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile blas/Makefile cblas/Makefile tests/Makefile])
AC_OUTPUT

dnl end of file
