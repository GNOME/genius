Librarydir = $(datadir)/genius/gel
SUBDIRS = calculus combinatorics equation_solving functions \
	linear_algebra misc number_theory statistics

Library_DATA = lib.cgel

MOSTLYCLEANFILES = lib.cgel temp.cgel loader.gel

EXTRA_DIST = make_loader_gel.sh

lib.cgel: loader.gel $(top_builddir)/src/genius
	cd $(top_builddir)/lib && $(top_builddir)/src/genius --maxerrors=0 --compile loader.gel > temp.cgel && mv -f temp.cgel lib.cgel

loader.gel: $(SUBDIRS:%=%/loader.gel)
	cd $(top_builddir)/lib && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
