Librarydir = $(datadir)/genius/gel/sets
SUBDIRS =

GELFILES = basic.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/sets && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
