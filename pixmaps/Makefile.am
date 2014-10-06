SUBDIRS = \
	8x8 16x16 22x22 24x24 32x32 48x48 256x256 scalable

imagedir = \
	$(datadir)/genius/
image_DATA = \
	genius-graph.png
EXTRA_DIST = \
	$(image_DATA)

run_gtk_update_icon_cache = $(GTK_UPDATE_ICON_CACHE) -f -t $(datadir)/icons/hicolor

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating GTK icon cache."; \
		$(run_gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(run_gtk_update_icon_cache)"; \
	fi

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache


