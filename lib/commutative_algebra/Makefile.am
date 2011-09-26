Librarydir = $(datadir)/genius/gel/commutative_algebra
SUBDIRS =

GELFILES = macaulay.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/commutative_algebra && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
