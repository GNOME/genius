Librarydir = $(datadir)/genius/gel/calculus
SUBDIRS =

GELFILES = differentiation.gel integration.gel limits.gel sums_products.gel fourier.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/calculus && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
