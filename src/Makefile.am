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
	-DICONDIR=\""$(datadir)/pixmaps/genius"\"		\
	-DLIBRARY_DIR=\""$(datadir)/genius/"\"			\
	-I$(srcdir)						\
	-I$(includedir)						\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/libeog					\
	-I$(top_builddir)/libeog				\
	$(GMP_INCLUDEDIR)					\
	$(GENIUS_CFLAGS)					\
	$(EXTRA_WARNING_CFLAGS)


bin_PROGRAMS = gnome-genius genius

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
	structs.h	\
	calc.h		\
	eval.h

plugin_LTLIBRARIES = libtestplugin.la

libtestplugin_la_SOURCES =	\
	plug_api.h		\
	testplugin.c

BUILT_SOURCES = parse.h parse.c lexer.c

CLEANFILES += $(BUILT_SOURCES)

EXTRA_DIST = geniustest.pl geniustests.txt gnome-genius.desktop test.plugin.in # lib.gel 

Utilities_DATA = gnome-genius.desktop

# Library_DATA = lib.gel lib.cgel

Plugindesc_DATA = test.plugin

# MOSTLYCLEANFILES = lib.cgel

# lib.cgel: lib.gel genius
#	$(top_builddir)/src/genius --compile lib.gel > temp.cgel && mv -f temp.cgel lib.cgel

parse.h: parse.y
	$(YACC) $(YFLAGS) $<
	test -f y.tab.c && mv -f y.tab.c parse.c
	test -f y.tab.h && mv -f y.tab.h parse.h

test.plugin: test.plugin.in Makefile
## Why do a sed and then an mv?  To avoid problems if the user
## interrupts.
	sed -e 's,\@libdir\@,$(libdir),g' < $(srcdir)/test.plugin.in \
	  > test.plugin.tmp && mv -f test.plugin.tmp test.plugin

