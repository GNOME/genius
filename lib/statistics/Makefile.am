Librarydir = $(datadir)/genius/gel/statistics
SUBDIRS =

GELFILES = basic.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/statistics && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
