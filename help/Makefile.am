include $(top_srcdir)/gnome-doc-utils.make

dist-hook: doc-dist-hook

DOC_MODULE = genius
DOC_FIGURES =	genius_window.eps	\
		line_plot_graph.eps	\
		parametric.eps		\
		parametric.png		\
		genius_window.png	\
		line_plot_graph.png	\
		parametric_graph.eps	\
		surface_graph.eps	\
		line_plot.eps		\
		line_plot.png		\
		parametric_graph.png	\
		surface_graph.png

DOC_ENTITIES =	gel-function-list.xml	\
           	legal.xml

DOC_LINGUAS = de es

#
# Text version
#

# Run this by hand for now
#genius.txt: genius.xml $(entities)
#	docbook2txt C/genius.xml
#	dos2unix genius.txt

manualdir = $(datadir)/genius

manual_DATA = genius.txt

EXTRA_DIST = genius.txt
