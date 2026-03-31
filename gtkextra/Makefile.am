## Process this file with automake to produce makefile.in
SUBDIRS=.

AM_CPPFLAGS = \
	-I$(srcdir)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/ve					\
	-I$(top_srcdir)/gtkextra				\
	-I$(top_builddir)					\
	-I$(top_builddir)/ve					\
	-I$(top_builddir)/gtkextra				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"		\
	$(WARN_CFLAGS)						\
	$(GENIUS_CFLAGS)

#	-I$(includedir)

AM_CFLAGS = $(PGO_CFLAGS)

# Use these only really for checking for deprecated use, not all the time
#	-DG_DISABLE_DEPRECATED			\
#	-DGDK_DISABLE_DEPRECATED		\
#	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
#	-DGTK_DISABLE_DEPRECATED		\
#	-DGNOME_DISABLE_DEPRECATED		\
#


# For a gnome build set this in configure.in
#  VICIOUS_GNOME_LIBS="libviciousui.a libviciousui-gnome.a \
#       libviciousui-minimal.a libvicious-gnome.a"
#  AC_SUBST(VICIOUS_GNOME_LIBS)

noinst_LIBRARIES = \
	libgtkextra-genius.a

EXTRA_LIBRARIES = \
	libgtkextra-genius.a

public_h_sources =	\
	gtkextra.h		\
	gtkextra-compat.h	\
	gtkpsfont.h		\
	gtkplot.h		\
	gtkplot3d.h		\
	gtkplotarray.h		\
	gtkplotbar.h		\
	gtkplotbox.h		\
	gtkplotcandle.h		\
	gtkplotcanvas.h		\
	gtkplotcanvasellipse.h	\
	gtkplotcanvasline.h	\
	gtkplotcanvaspixmap.h	\
	gtkplotcanvasplot.h	\
	gtkplotcanvasrectangle.h	\
	gtkplotcanvastext.h	\
	gtkplotcsurface.h	\
	gtkplotdata.h		\
	gtkplotdt.h		\
	gtkplotflux.h		\
	gtkplotcairo.h		\
	gtkplotpc.h		\
	gtkplotpixmap.h		\
	gtkplotpolar.h		\
	gtkplotprint.h		\
	gtkplotps.h		\
	gtkplotsurface.h

libgtkextra_genius_a_SOURCES =	\
	$(public_h_sources)	\
	gtkextra.c		\
	gtkpsfont.c		\
	gtkplot.c		\
	gtkplot3d.c		\
	gtkplotarray.c		\
	gtkplotbar.c		\
	gtkplotbox.c		\
	gtkplotcandle.c		\
	gtkplotcanvas.c		\
	gtkplotcanvasellipse.c	\
	gtkplotcanvasline.c	\
	gtkplotcanvaspixmap.c	\
	gtkplotcanvasplot.c	\
	gtkplotcanvasrectangle.c	\
	gtkplotcanvastext.c	\
	gtkplotcsurface.c	\
	gtkplotdata.c		\
	gtkplotdt.c		\
	gtkplotflux.c		\
	gtkplotcairo.c		\
	gtkplotpc.c		\
	gtkplotpixmap.c		\
	gtkplotpolar.c		\
	gtkplotprint.c		\
	gtkplotps.c		\
	gtkplotsurface.c
