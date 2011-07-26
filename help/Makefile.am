include $(top_srcdir)/gnome-doc-utils.make

dist-hook: doc-dist-hook

DOC_MODULE = genius
DOC_FIGURES =	figures/genius_window.eps	\
		figures/line_plot_graph.eps	\
		figures/parametric.eps		\
		figures/parametric.png		\
		figures/genius_window.png	\
		figures/line_plot_graph.png	\
		figures/parametric_graph.eps	\
		figures/surface_graph.eps	\
		figures/line_plot.eps		\
		figures/line_plot.png		\
		figures/parametric_graph.png	\
		figures/surface_graph.png

DOC_ENTITIES =	gel-function-list.xml	\
           	legal.xml

DOC_LINGUAS = cs de es

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
