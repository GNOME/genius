Librarydir = $(datadir)/genius/gel/linear_algebra
SUBDIRS =

GELFILES = bilinear_forms.gel linear_algebra.gel \
	misc.gel special_matrices.gel subspaces.gel

EXTRA_DIST = $(GELFILES)

Library_DATA = $(GELFILES) loader.gel

MOSTLYCLEANFILES = loader.gel

loader.gel: $(GELFILES)
	cd $(top_builddir)/lib/linear_algebra && $(top_srcdir)/lib/make_loader_gel.sh $^ > loader.gel
