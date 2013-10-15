Librarydir = $(datadir)/genius/gel/equation_solving
SUBDIRS =

GELFILES = find_root.gel diffeqs.gel formulas.gel newton.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/equation_solving && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
