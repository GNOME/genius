Librarydir = $(datadir)/genius/gel/combinatorics
SUBDIRS =

GELFILES = factorial.gel misc.gel recursive_sequences.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/combinatorics && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
