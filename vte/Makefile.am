ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

SUBDIRS = src termcaps

EXTRA_DIST = \
	MAINTAINERS \
	HACKING \
	ChangeLog.pre-git

DISTCLEANFILES =

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--disable-silent-rules

MAINTAINERCLEANFILES = \
		ChangeLog \
		$(srcdir)/INSTALL \
		$(srcdir)/aclocal.m4 \
		$(srcdir)/autoscan.log \
		$(srcdir)/compile \
		$(srcdir)/config.guess \
		$(srcdir)/config.h.in \
		$(srcdir)/config.sub \
		$(srcdir)/configure \
		$(srcdir)/configure.scan \
		$(srcdir)/depcomp \
		$(srcdir)/install-sh \
		$(srcdir)/ltmain.sh \
		$(srcdir)/missing \
		$(srcdir)/mkinstalldirs \
		$(srcdir)/omf.make \
		$(srcdir)/xmldocs.make \
		$(srcdir)/gtk-doc.make \
		`find "$(srcdir)" -type f -name Makefile.in -print`
