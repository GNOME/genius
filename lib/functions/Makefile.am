Librarydir = $(datadir)/genius/gel/functions
SUBDIRS =

GELFILES = delta.gel elementary.gel numerical.gel orthogonal_polynomials.gel kernels.gel complex_numbers.gel lambert.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/functions && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
