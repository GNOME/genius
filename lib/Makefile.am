Librarydir = $(datadir)/genius/gel
SUBDIRS = calculus combinatorics equation_solving functions \
	linear_algebra misc number_theory statistics sets symbolic \
	commutative_algebra

Library_DATA = lib.cgel loader.gel

noinst_DATA = library-strings.c

MOSTLYCLEANFILES = lib.cgel temp.cgel loader.gel library-strings.c

EXTRA_DIST = make_loader_gel.sh library-strings.c

lib.cgel: loader.gel $(top_builddir)/src/genius
	cd $(top_builddir)/lib && $(top_builddir)/src/genius --maxerrors=0 --compile loader.gel > temp.cgel && mv -f temp.cgel lib.cgel

loader.gel: $(SUBDIRS:%=%/loader.gel)
	cd $(top_builddir)/lib && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel

library-strings.c: loader.gel $(top_builddir)/src/genius
	cd $(top_builddir)/lib && $(top_builddir)/src/genius --maxerrors=0 --gettext loader.gel > temp.c && mv -f temp.c library-strings.c
