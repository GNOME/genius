ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = ve gtkextra src pixmaps examples lib po help

mimeinfodir = $(datadir)/mime-info
mimeinfo_DATA = genius.keys genius.mime

appregdir = $(datadir)/application-registry
appreg_DATA = genius.applications

packagesdir = $(datadir)/mime/packages
packages_DATA = genius.xml

install-data-hook: 
if ENABLE_UPDATE_MIMEDB
	-update-mime-database "$(DESTDIR)${datadir}/mime"
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	-update-mime-database "$(DESTDIR)${datadir}/mime"
endif

profile-generate:
	make clean
	PGO_CFLAGS="-fprofile-generate" make

profile-use:
	make clean
	PGO_CFLAGS="-fprofile-use" make

EXTRA_DIST = \
	genius.spec \
	genius.spec.in \
	genius.xml \
	genius.mime \
	genius.keys \
	genius.applications \
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	INSTALL.MacOSX		\
	m4/NOTES

DISTCLEANFILES = \
	intltool-extract	\
	intltool-merge		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --disable-update-mimedb

# This doesn't add the graphing functions
#genius-function-list.txt: src/genius
#	LANG=C src/genius --version > genius-function-list.txt
#	echo "------------------------------------------" >> genius-function-list.txt
#	echo help | src/genius >> genius-function-list.txt

distuninstallcheck_listfiles = find . -type f -print | fgrep -v '/var/scrollkeeper' | fgrep -v 'omf' | fgrep -v 'figures' | fgrep -v '/share/mime'
