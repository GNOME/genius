Utilitiesdir = $(datadir)/apps/Utilities
Librarydir = $(datadir)/genius/gel
Plugindescdir = $(datadir)/genius/plugins

plugindir = $(libdir)/genius

plugin_includedir = $(includedir)/genius

#YACCFLAGS += -d -t
YFLAGS += -d -t

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DG_LOG_DOMAIN=\"Genius\"				\
	-DDATADIR=\""$(datadir)"\"				\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	-DICONDIR=\""$(datadir)/pixmaps/genius"\"		\
	-DLIBRARY_DIR=\""$(datadir)/genius/"\"			\
	-I$(srcdir)						\
	-I$(includedir)						\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	$(GMP_INCLUDEDIR)					\
	$(GENIUS_CFLAGS)					\
	$(EXTRA_WARNING_CFLAGS)


bin_PROGRAMS = gnome-genius genius

libexec_PROGRAMS = genius-readline-helper-fifo

gnome_genius_SOURCES = 	\
	calc.c		\
	calc.h		\
	eval.c		\
	eval.h		\
	gnome-genius.c	\
	util.c		\
	util.h		\
	dict.c		\
	dict.h		\
	funclib.c	\
	funclib.h	\
	mpwrap.c	\
	mpwrap.h	\
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
	geloutput.h

gnome_genius_LDADD = \
	-lm					\
	@GMP_LIB@				\
	@LEXLIB@				\
	$(GENIUS_LIBS)				\
	@READLINE_LIB@				\
	@TERMCAP_LIB@

genius_readline_helper_fifo_SOURCES = \
 	genius_lists.c			\
	genius-readline-helper.c

# FIXME: no need for so many libs
genius_readline_helper_fifo_LDADD = \
	-lm					\
	$(GENIUS_LIBS)				\
	@READLINE_LIB@				\
	@TERMCAP_LIB@


genius_SOURCES = 	\
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
	mpwrap.c	\
	mpwrap.h	\
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
	plugread_nongnome.c	\
	plugread.h	\
	plug_api.h	\
	parse.h		\
	parse.y		\
 	genius_lists.c	\
 	utype.c		\
	utype.h		\
	extra.h		\
	geloutput.c	\
	geloutput.h

genius_LDADD = \
	-lm					\
	@GMP_LIB@				\
	@LEXLIB@				\
	$(GENIUS_LIBS)				\
	@READLINE_LIB@				\
	@TERMCAP_LIB@

plugin_include_HEADERS = \
	plug_api.h	\
	geloutput.h	\
	mpwrap.h	\
	structs.h	\
	matrix.h	\
	matrixw.h	\
	matop.h		\
	compil.h	\
	utype.h		\
	dict.h		\
	eval.h

plugin_LTLIBRARIES = libtestplugin.la

libtestplugin_la_SOURCES =	\
	plug_api.h		\
	testplugin.c

BUILT_SOURCES = parse.h parse.c lexer.c

CLEANFILES += $(BUILT_SOURCES)

Desktopdir = $(datadir)/applications
Desktop_in_files = \
	gnome-genius.desktop.in	
Desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	geniustest.pl \
	geniustests.txt \
	$(Desktop_in_files) \
	$(Desktop_DATA) \
	test.plugin.in

Plugindesc_DATA = test.plugin

parse.h: parse.y
	$(YACC) $(YFLAGS) $<
	test -f y.tab.c && mv -f y.tab.c parse.c
	test -f y.tab.h && mv -f y.tab.h parse.h

test.plugin: test.plugin.in Makefile
## Why do a sed and then an mv?  To avoid problems if the user
## interrupts.
	sed -e 's,\@libdir\@,$(libdir),g' < $(srcdir)/test.plugin.in \
	  > test.plugin.tmp && mv -f test.plugin.tmp test.plugin

