SUBDIRS = po vicious-extensions gtkextra src pixmaps lib doc

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
