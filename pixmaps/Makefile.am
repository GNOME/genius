SUBDIRS = \
	32x32 48x48

imagedir = \
	$(datadir)/genius/
image_DATA = \
	genius-graph.png
EXTRA_DIST = \
	$(image_DATA)

install-data-hook:
	$(GTK_UPDATE_ICON_CACHE) -q $(datadir)/icons/hicolor
