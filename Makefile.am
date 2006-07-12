if INTERNAL_MPFR
MAYBE_MPFR = mpfr
endif
SUBDIRS = $(MAYBE_MPFR) ve gtkextra src pixmaps lib po help

mimeinfodir = $(datadir)/mime-info
mimeinfo_DATA = genius.keys genius.mime

appregdir = $(datadir)/application-registry
appreg_DATA = genius.applications

packagesdir = $(datadir)/mime/packages
packages_DATA = genius.xml

install-data-hook: 
	-update-mime-database "$(DESTDIR)${datadir}/mime"

EXTRA_DIST = \
	genius.spec \
	genius.spec.in \
	genius.xml \
	genius.mime \
	genius.keys \
	genius.applications \
	intltool-modules/XML/Parser/Style/OrigTree.pm \
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in

DISTCLEANFILES = \
	intltool-extract	\
	intltool-merge		\
	intltool-update

# This doesn't add the graphing functions
#genius-function-list.txt: src/genius
#	LANG=C src/genius --version > genius-function-list.txt
#	echo "------------------------------------------" >> genius-function-list.txt
#	echo help | src/genius >> genius-function-list.txt

distuninstallcheck_listfiles = find . -type f -print | grep -v '^\./var/scrollkeeper' | grep -v 'omf' | grep -v 'figures'
