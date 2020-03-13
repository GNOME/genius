Librarydir = $(datadir)/genius/gel
Plugindescdir = $(datadir)/genius/plugins
Langdescdir = $(datadir)/genius/gtksourceview/

plugindir = $(libdir)/genius

plugin_includedir = $(includedir)/genius

AM_YFLAGS = -d -t

AM_CPPFLAGS = \
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DG_LOG_DOMAIN=\"Genius\"				\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	-DBUILDDIR=\""@abs_top_builddir@"\"			\
	-I$(srcdir)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/ve					\
	-I$(top_srcdir)/gtkextra				\
	-I$(top_builddir)					\
	-I$(top_builddir)/ve					\
	-I$(top_builddir)/gtkextra				\
	$(WARN_CFLAGS)						\
	$(BINRELOC_CFLAGS)					\
	$(GMP_INCLUDEDIR)					\
	$(GENIUS_CFLAGS)					\
	$(GSV_CFLAGS)

AM_CFLAGS = $(PGO_CFLAGS)

bin_PROGRAMS = @GNOME_GENIUS@ genius

libexec_PROGRAMS = @GENIUS_READLINE_HELPER_FIFO@

EXTRA_PROGRAMS = gnome-genius genius-readline-helper-fifo


gnome_genius_SOURCES = 	\
	binreloc.c	\
	binreloc.h	\
	genius-i18n.h	\
	calc.c		\
	calc.h		\
	eval.c		\
	eval.h		\
	gnome-genius.c	\
	gnome-genius.h	\
	util.c		\
	util.h		\
	dict.c		\
	dict.h		\
	funclib.c	\
	funclib.h	\
	symbolic.h	\
	symbolic.c	\
	mpwrap.c	\
	mpwrap.h	\
	mpzextra.c	\
	mpzextra.h	\
	matrix.c	\
	matrix.h	\
	matrixw.c	\
	matrixw.h	\
	matop.c		\
	matop.h		\
	compil.c	\
	compil.h	\
	structs.h	\
	lexer.l		\
	lexer.h		\
	parseutil.h	\
	parseutil.c	\
	inter.c		\
	inter.h		\
	examples.c	\
	examples.h	\
	plugin.c	\
	plugin.h	\
	plugread.c	\
	plugread.h	\
	plug_api.h	\
	parse.h		\
	parse.y		\
 	genius_lists.c	\
 	utype.c		\
	utype.h		\
	extra.h		\
	geloutput.c	\
	geloutput.h	\
	graphing.h	\
	graphing.c	\
	funclibhelper.cP

gnome_genius_LDADD = \
	$(top_builddir)/ve/libviciousui.a	\
	$(top_builddir)/gtkextra/libgtkextra-genius.a	\
	-lm					\
	@GMP_LIB@				\
	-lmpfr					\
	@LEXLIB@				\
	$(INTLLIBS)				\
	$(GENIUS_LIBS)				\
	$(GENIUS_NOGUI_LIBS)			\
	$(GSV_LIBS)				\
	@READLINE_LIB@				\
	@TERMCAP_LIB@

genius_readline_helper_fifo_SOURCES = \
 	genius_lists.c			\
	genius-readline-helper.c

# FIXME: no need for so many libs
genius_readline_helper_fifo_LDADD = \
	$(top_builddir)/ve/libvicious.a		\
	-lm					\
	$(GENIUS_NOGUI_LIBS)			\
	$(INTLLIBS)				\
	@READLINE_LIB@				\
	@TERMCAP_LIB@


genius_SOURCES = 	\
	binreloc.c	\
	binreloc.h	\
	genius-i18n.h	\
	calc.c		\
	calc.h		\
	eval.c		\
	eval.h		\
	genius.c	\
	util.c		\
	util.h		\
	dict.c		\
	dict.h		\
	funclib.c	\
	funclib.h	\
	symbolic.h	\
	symbolic.c	\
	mpwrap.c	\
	mpwrap.h	\
	mpzextra.c	\
	mpzextra.h	\
	matrix.c	\
	matrix.h	\
	matrixw.c	\
	matrixw.h	\
	matop.c		\
	matop.h		\
	compil.c	\
	compil.h	\
	structs.h	\
	parseutil.h	\
	parseutil.c	\
	inter.c		\
	inter.h		\
	lexer.l		\
	lexer.h		\
	plugin.c	\
	plugin.h	\
	plugread.c	\
	plugread.h	\
	plug_api.h	\
	parse.h		\
	parse.y		\
 	genius_lists.c	\
 	utype.c		\
	utype.h		\
	extra.h		\
	geloutput.c	\
	geloutput.h	\
	funclibhelper.cP

genius_LDADD = \
	$(top_builddir)/ve/libvicious.a		\
	-lm					\
	@GMP_LIB@				\
	-lmpfr					\
	@LEXLIB@				\
	$(INTLLIBS)				\
	$(GENIUS_NOGUI_LIBS)			\
	@READLINE_LIB@				\
	@TERMCAP_LIB@

plugin_include_HEADERS = \
	genius-i18n.h	\
	plug_api.h	\
	geloutput.h	\
	mpwrap.h	\
	mpzextra.h	\
	structs.h	\
	matrix.h	\
	matrixw.h	\
	matop.h		\
	compil.h	\
	utype.h		\
	dict.h		\
	lexer.h		\
	eval.h

plugin_LTLIBRARIES = libtestplugin.la

libtestplugin_la_LDFLAGS = -module -avoid-version

libtestplugin_la_SOURCES =	\
	plug_api.h		\
	testplugin.c

BUILT_SOURCES = parse.h parse.c lexer.c

CLEANFILES = $(BUILT_SOURCES)

Desktopdir = $(datadir)/applications
Desktop_in_files = \
	@GNOME_GENIUS_DESKTOP_IN@	
Desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

DISTCLEANFILES = $(Desktop_DATA) test.plugin

EXTRA_DIST = \
	geniustest.pl \
	geniustests.txt \
	testfourier.gel \
	testscope.gel \
	testprec.gel \
	longtest.gel \
	nullspacetest.gel \
	gnome-genius.desktop.in \
	$(Desktop_in_files) \
	$(Desktop_DATA) \
	test.plugin.in \
	genius.lang

Plugindesc_DATA = test.plugin

Langdesc_DATA = genius.lang

parse.h: parse.y
	$(YACC) $(YFLAGS) $(AM_YFLAGS) $<
	test -f y.tab.c && mv -f y.tab.c parse.c
	test -f y.tab.h && mv -f y.tab.h parse.h

test.plugin: test.plugin.in Makefile
## Why do a sed and then an mv?  To avoid problems if the user
## interrupts.
	sed -e 's,\@libdir\@,$(libdir),g' < $(srcdir)/test.plugin.in \
	  > test.plugin.tmp && mv -f test.plugin.tmp test.plugin

