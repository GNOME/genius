Librarydir = $(datadir)/genius/gel/misc
SUBDIRS =

GELFILES = misc.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/misc && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
