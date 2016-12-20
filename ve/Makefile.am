## Process this file with automake to produce makefile.in
SUBDIRS=.

AM_CPPFLAGS = \
 	-I.					\
	-I..					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(WARN_CFLAGS)				\
	$(VICIOUS_CFLAGS)

AM_CFLAGS = $(PGO_CFLAGS)

# Use these only really for checking for deprecated use, not all the time
#	-DG_DISABLE_DEPRECATED			\
#	-DGDK_DISABLE_DEPRECATED		\
#	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
#	-DGTK_DISABLE_DEPRECATED		\
#	-DGNOME_DISABLE_DEPRECATED		\
#

noinst_LIBRARIES = \
	@VICIOUS_UI_LIBS@ \
	libvicious.a

EXTRA_LIBRARIES = \
	libviciousui.a	

noinst_PROGRAMS = test-ve-config

libvicious_a_SOURCES =	\
	ve-i18n.h	\
	ve-misc.c	\
	ve-misc.h	\
	ve-config.c	\
	ve-config.h	\
	ve-nongnome.c	\
	vicious.h

libviciousui_a_SOURCES = \
	$(libvicious_a_SOURCES) \
	ve-miscui.c	\
	ve-miscui.h	\
	viciousui.h

#	Genius does not use glade, if you want to use ve in your project
#	and want to use glade-helper make sure to compile it in
#	glade-helper.c	\
#	glade-helper.h

test_ve_config_SOURCES = \
	test-ve-config.c

test_ve_config_LDADD = \
	libvicious.a				\
	$(VICIOUS_LIBS)				\
	$(INTLLIBS)				\
	$(GLIB_LIBS)
