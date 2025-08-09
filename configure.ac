AC_INIT([genius], [1.0.28])
#AX_IS_RELEASE([git-directory])
AX_IS_RELEASE([always])
AC_CONFIG_SRCDIR([src/calc.c])
AM_INIT_AUTOMAKE([dist-xz])

AC_CONFIG_MACRO_DIRS([m4])

AM_CONFIG_HEADER(config.h)

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "$ACLOCAL_FLAGS")

AM_MAINTAINER_MODE

AC_PROG_INTLTOOL([0.21])

PKG_PROG_PKG_CONFIG

dnl ================= Requirements ================================================

VTE_REQUIRED=0.50.0
GTK_REQUIRED=3.21.4
GLIB_REQUIRED=2.41.1
GTKSOURCEVIEW4_REQUIRED=3.99.7
GIO_REQUIRED=2.16.0

dnl ===============================================================================

AC_ARG_ENABLE(extra-gcc-optimization,
[  --disable-extra-gcc-optimization  Compile without extra GCC optimization (if using GCC)],
use_extraopt="$enableval",use_extraopt=yes)
AC_PROG_CC
if test "x$GCC" = "xyes" -a "x$use_extraopt" = "xyes" ; then
	echo "Turning on extra gcc flags -finline-functions -frename-registers"
	CFLAGS="-finline-functions -frename-registers $CFLAGS"
fi
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_LEX
AM_PROG_LIBTOOL
AC_PROG_YACC

AM_BINRELOC

AX_COMPILER_FLAGS([WARN_CFLAGS],[WARN_LDFLAGS])

dnl remove stupid warning
WARN_CFLAGS=`echo $WARN_CFLAGS | sed 's/ -Wswitch-enum / /'`

dnl ================= Translation Stuff ==============================================

GETTEXT_PACKAGE=genius
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",[gettext package name])
AC_SUBST(GETTEXT_PACKAGE)

ALL_LINGUAS="ar az bs ca cs da de dz el en_CA en_GB eo es eu fi fr ga gl hi hr hu id it ja ko lv mk ml nb ne nl oc pa pt_BR pt ro ru rw sl sq sr@latin sr sv uk zh_CN zh_TW"
AM_GLIB_GNU_GETTEXT

dnl ===============================================================================

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

# glib-genmarshal
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

dnl Check for termcap
AC_CHECK_LIB(termcap, tgetent,[TERMCAP_LIB="-ltermcap"],
	     [PKG_CHECK_MODULES(NCURSES, ncurses,
                           [TERMCAP_LIB="$NCURSES_LIBS"
                            AC_DEFINE(USE_NCURSES,[1], [Use ncurses library])
                            AC_CHECK_HEADER(ncurses/curses.h, [
					    AC_DEFINE(INC_NCURSES,[1],[ncurses include])])], [
			   AC_MSG_ERROR(Termcap library not found)])])
AC_SUBST(TERMCAP_LIB)

dnl Check for readline
AC_CHECK_LIB(readline,readline, [READLINE_LIB="-lreadline"],
	     AC_MSG_ERROR(Readline library not found),
	     $TERMCAP_LIB)
AC_SUBST(READLINE_LIB)

GMP_LIB=""
AC_CHECK_LIB(gmp, __gmpz_init_set,[GMP_LIB=-lgmp], [
	AC_CHECK_LIB(gmp2, __gmpz_init_set_d,[GMP_LIB=-lgmp2
		AC_DEFINE(HAVE_GMP2_INCLUDE_DIR,[1],[Have gmp2 include directory])],
		AC_MSG_ERROR(GMP Library not found))])
AC_SUBST(GMP_LIB)

AC_CHECK_LIB(mpfr, mpfr_j0, [],
	[AC_MSG_ERROR([New enough MPFR (2.3.0+) not found, see http://www.mpfr.org])],
	"$GMP_LIB")

# No schemas yet so ignore this section for now
#AC_PATH_PROG(GCONFTOOL, gconftool-2)
#dnl Specify the gconf configuration source,
#dnl default to xml::$(sysconfdir)/gconf/gconf.xml.defaults
#
#GCONF_CONFIG_SOURCE=
#
#AC_ARG_ENABLE(gconf-source, [  --enable-gconf-source=sourceaddress Where to install schema files.], GCONF_CONFIG_SOURCE=$enable_gconf_source,)
#if test "x$GCONF_CONFIG_SOURCE" = "x"; then
#        GCONF_CONFIG_SOURCE="xml::\${sysconfdir}/gconf/gconf.xml.defaults"
#fi
#
#AC_SUBST(GCONF_CONFIG_SOURCE)
#AC_SUBST(INSTALL_GCONF_CONFIG_SOURCE)

AC_ARG_ENABLE(gtksourceview,
[  --disable-gtksourceview Do not use GtkSourceView library even if detected],
use_gtksourceview="$enableval",use_gtksourceview=yes)

AC_ARG_ENABLE(gnome,
[  --disable-gnome         Compile without the GNOME frontend (no GUI, no plotting)],
use_gnome="$enableval",use_gnome=yes)

# Used for the 'genius' binary
PKG_CHECK_MODULES(GENIUS_NOGUI, glib-2.0 >= $GLIB_REQUIRED gmodule-2.0 >= $GLIB_REQUIRED)
AC_SUBST(GENIUS_NOGUI_CFLAGS)
AC_SUBST(GENIUS_NOGUI_LIBS)

# glib
PKG_CHECK_MODULES(GLIB, glib-2.0 >= $GLIB_REQUIRED)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

GENIUS_MODULES="gtk+-3.0 >= $GTK_REQUIRED gio-2.0 >= $GIO_REQUIRED vte-2.91 >= $VTE_REQUIRED"

if test "x$use_gnome" = "xyes" ; then
  echo " 
Testing for presence of GNOME (GTK and GIO is enough) and GtkSourceView:
Note:
      You can compile without GNOME by using the --disable-gnome option
"
  PKG_CHECK_MODULES(GENIUS, $GENIUS_MODULES)

  if test "x$use_gtksourceview" = "xyes" ; then
    PKG_CHECK_MODULES(GSV, gtksourceview-4 >= $GTKSOURCEVIEW4_REQUIRED, have_gtksourceview4=yes, have_gtksourceview4=no)

    if test "x$have_gtksourceview4" = "xyes"; then
      AC_DEFINE(HAVE_GTKSOURCEVIEW,[1],[have gtksourceview4])
    else
      use_gtksourceview=no
    fi
  else
    PKG_CHECK_MODULES(GENIUS, $GENIUS_MODULES)
  fi

  VICIOUS_MODULES="gtk+-3.0 >= $GTK_REQUIRED"
  VICIOUS_UI_LIBS="libviciousui.a"
  
  GNOME_GENIUS="gnome-genius\$(EXEEXT)"
  GENIUS_READLINE_HELPER_FIFO="genius-readline-helper-fifo\$(EXEEXT)"
  GNOME_GENIUS_DESKTOP_IN="gnome-genius.desktop.in"

else
  use_gtksourceview=no
  VICIOUS_MODULES="glib-2.0 >= $GLIB_REQUIRED gmodule-2.0 >= $GLIB_REQUIRED"
  VICIOUS_UI_LIBS=
  GENIUS_CFLAGS="$GENIUS_NOGUI_CFLAGS"
  GENIUS_LIBS="$GENIUS_NOGUI_LIBS"
  GNOME_GENIUS=
  GENIUS_READLINE_HELPER_FIFO=
  GNOME_GENIUS_DESKTOP_IN=
fi

AC_SUBST(VICIOUS_UI_LIBS)
AC_SUBST(GENIUS_CFLAGS)
AC_SUBST(GENIUS_LIBS)
AC_SUBST(GSV_CFLAGS)
AC_SUBST(GSV_LIBS)
AC_SUBST(GNOME_GENIUS)
AC_SUBST(GENIUS_READLINE_HELPER_FIFO)
AC_SUBST(GNOME_GENIUS_DESKTOP_IN)

# This is used for vicious-extensions
PKG_CHECK_MODULES(VICIOUS, $VICIOUS_MODULES)
AC_SUBST(VICIOUS_CFLAGS)
AC_SUBST(VICIOUS_LIBS)

dnl ******************************
dnl More compiler warnings
dnl ******************************

dnl Warnings done above

#AC_ARG_ENABLE(more-warnings,
#[  --enable-more-warnings  Maximum compiler warnings],
#set_more_warnings="$enableval",[
#if test -f $srcdir/.git/config; then
#	is_cvs_version=true
#	set_more_warnings=yes
#else
#	set_more_warnings=no
#fi
#])
#warning_flags=
#realsave_CFLAGS="$CFLAGS"
#AC_MSG_CHECKING(for more warnings)
#if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
#	AC_MSG_RESULT(yes)
#	warning_flags="-Wall -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith"
#	CFLAGS="$warning_flags $CFLAGS"
#
#	for option in -Wno-sign-compare; do
#		SAVE_CFLAGS="$CFLAGS"
#		CFLAGS="$CFLAGS $option"
#		AC_MSG_CHECKING([whether gcc understands $option])
#		AC_TRY_COMPILE([], [],
#			has_option=yes,
#			has_option=no,)
#		CFLAGS="$SAVE_CFLAGS"
#		AC_MSG_RESULT($has_option)
#		if test $has_option = yes; then
#		  warning_flags="$warning_flags $option"
#		fi
#		unset has_option
#		unset SAVE_CFLAGS
#	done
#	unset option
#else
#	AC_MSG_RESULT(no)
#fi
#CFLAGS="$realsave_CFLAGS"
#EXTRA_WARNING_CFLAGS="$warning_flags"
#	-DG_DISABLE_DEPRECATED \
#	-DGDK_PIXBUF_DISABLE_DEPRECATED \
#	-DGDK_DISABLE_DEPRECATED \
#	-DGTK_DISABLE_DEPRECATED \
#	-DGNOME_DISABLE_DEPRECATED \
#
#AC_SUBST(EXTRA_WARNING_CFLAGS)

dnl wordexp check, we use glob otherwise but that's not perfect
dnl warn the user, we do this on the end so that the user notices
AC_CHECK_FUNC(wordexp,AC_DEFINE(HAVE_WORDEXP,[1],[Have the wordexp function]),
  	      [AC_MSG_WARN([*** You don't have wordexp, glob will be used instead ***])])

AC_PATH_PROG(GTK_UPDATE_ICON_CACHE, gtk-update-icon-cache)
if test -z "$GTK_UPDATE_ICON_CACHE"; then
    AC_MSG_ERROR([Could not find gtk-update-icon-cache])
fi

#dnl scrollkeeper checks 
#AC_ARG_ENABLE([scrollkeeper],
#	[AC_HELP_STRING([--disable-scrollkeeper],
#		[do not make updates to the scrollkeeper database])],,
#		enable_scrollkeeper=yes)
#AM_CONDITIONAL([ENABLE_SK],[test "$enable_scrollkeeper" = "yes"])
#
#AC_PATH_PROG(SCROLLKEEPER_CONFIG, scrollkeeper-config,no)
#if test x$SCROLLKEEPER_CONFIG = xno; then
#  AC_MSG_ERROR(Couldn't find scrollkeeper-config. Please install the scrollkeeper package)
#fi

dnl mime checks
AC_ARG_ENABLE(update-mimedb,
	AC_HELP_STRING([--disable-update-mimedb],
		[do not update mime database after installation]),,
		enable_update_mimedb=yes)
AM_CONDITIONAL(ENABLE_UPDATE_MIMEDB, test x$enable_update_mimedb = xyes)


AC_OUTPUT([
genius.spec
Makefile
src/Makefile
lib/Makefile
lib/calculus/Makefile
lib/combinatorics/Makefile
lib/equation_solving/Makefile
lib/functions/Makefile
lib/linear_algebra/Makefile
lib/misc/Makefile
lib/sets/Makefile
lib/number_theory/Makefile
lib/statistics/Makefile
lib/symbolic/Makefile
lib/commutative_algebra/Makefile
ve/Makefile
gtkextra/Makefile
examples/Makefile
pixmaps/Makefile
pixmaps/8x8/Makefile
pixmaps/16x16/Makefile
pixmaps/22x22/Makefile
pixmaps/24x24/Makefile
pixmaps/32x32/Makefile
pixmaps/48x48/Makefile
pixmaps/256x256/Makefile
pixmaps/scalable/Makefile
po/Makefile.in
help/Makefile
])

echo "
Configuration:

	Source code location:	     ${srcdir}
	Compiler:		     ${CC} 
	GNOME frontend:		     ${use_gnome}
	Using GtkSourceView:	     ${use_gtksourceview}
"
