Librarydir = $(datadir)/genius/gel/number_theory
SUBDIRS =

GELFILES = factoring.gel misc.gel modulus.gel primes.gel quadratic.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/number_theory && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
