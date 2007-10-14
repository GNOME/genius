figdir = figures
docname = genius
lang = C
omffile = genius-C.omf
entities = gel-function-list.xml \
           legal.xml

include $(top_srcdir)/help/xmldocs.make
dist-hook: app-dist-hook

DISTCLEANFILES = genius-C.omf.out

#
# Text version
#

# Run this by hand for now
#genius.txt: genius.xml $(entities)
#	docbook2txt genius.xml
#	dos2unix -a genius.txt # for svn

manualdir = $(datadir)/genius

manual_DATA = genius.txt

EXTRA_DIST += genius.txt
