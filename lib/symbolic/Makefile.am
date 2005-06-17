Librarydir = $(datadir)/genius/gel/symbolic
SUBDIRS =

GELFILES = differentiation.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/symbolic && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
