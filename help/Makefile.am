
# After updating docs run ./update-xml-to-txt-html.sh
# Read that script for what it does.  I'll run this script before releasing.
# The output is stored in git and it is not run on make or make dist or some
# such.
#
# to update an existing po file, you can run ./update-po.sh LANG
# or just use xml2po manually


# FIXME: this should be some sort of a loop
# DOC_LINGUAS = cs de el es fr pt_BR ru sv
# If adding languages make sure to also update the update-xml-to-txt-html.sh

#C
THE_CFIGURES =	C/figures/parametric.png	\
		C/figures/genius_window.png	\
		C/figures/line_plot_graph.png	\
		C/figures/line_plot.png		\
		C/figures/parametric_graph.png	\
		C/figures/surface_graph.png

manualxmlCdir = $(datadir)/genius/help/C
manualxmlC_DATA = C/genius.xml C/legal.xml
manualxmlCfiguresdir = $(datadir)/genius/help/C/figures
manualxmlCfigures_DATA = $(THE_CFIGURES)

manualhtmlCdir = $(datadir)/genius/help/C/html
manualhtmlC_DATA = C/html/*.html
manualhtmlCfiguresdir = $(datadir)/genius/help/C/html/figures
manualhtmlCfigures_DATA = $(THE_CFIGURES)

#cs
THE_csFIGURES =	cs/figures/parametric.png	\
		cs/figures/genius_window.png	\
		cs/figures/line_plot_graph.png	\
		cs/figures/line_plot.png		\
		cs/figures/parametric_graph.png	\
		cs/figures/surface_graph.png

manualxmlcsdir = $(datadir)/genius/help/cs
manualxmlcs_DATA = cs/genius.xml
manualxmlcsfiguresdir = $(datadir)/genius/help/cs/figures
manualxmlcsfigures_DATA = $(THE_csFIGURES)

manualhtmlcsdir = $(datadir)/genius/help/cs/html
manualhtmlcs_DATA = cs/html/*.html
manualhtmlcsfiguresdir = $(datadir)/genius/help/cs/html/figures
manualhtmlcsfigures_DATA = $(THE_csFIGURES)

#de
THE_deFIGURES =	de/figures/parametric.png	\
		de/figures/genius_window.png	\
		de/figures/line_plot_graph.png	\
		de/figures/line_plot.png		\
		de/figures/parametric_graph.png	\
		de/figures/surface_graph.png

manualxmldedir = $(datadir)/genius/help/de
manualxmlde_DATA = de/genius.xml
manualxmldefiguresdir = $(datadir)/genius/help/de/figures
manualxmldefigures_DATA = $(THE_deFIGURES)

manualhtmldedir = $(datadir)/genius/help/de/html
manualhtmlde_DATA = de/html/*.html
manualhtmldefiguresdir = $(datadir)/genius/help/de/html/figures
manualhtmldefigures_DATA = $(THE_deFIGURES)

#el
manualxmleldir = $(datadir)/genius/help/el
manualxmlel_DATA = el/genius.xml
manualxmlelfiguresdir = $(datadir)/genius/help/el/figures
manualxmlelfigures_DATA = $(THE_CFIGURES)

manualhtmleldir = $(datadir)/genius/help/el/html
manualhtmlel_DATA = el/html/*.html
manualhtmlelfiguresdir = $(datadir)/genius/help/el/html/figures
manualhtmlelfigures_DATA = $(THE_CFIGURES)

#es
manualxmlesdir = $(datadir)/genius/help/es
manualxmles_DATA = es/genius.xml
manualxmlesfiguresdir = $(datadir)/genius/help/es/figures
manualxmlesfigures_DATA = $(THE_esFIGURES)

manualhtmlesdir = $(datadir)/genius/help/es/html
manualhtmles_DATA = es/html/*.html
manualhtmlesfiguresdir = $(datadir)/genius/help/es/html/figures
manualhtmlesfigures_DATA = $(THE_esFIGURES)

#fr
THE_frFIGURES =	fr/figures/parametric.png	\
		fr/figures/genius_window.png	\
		fr/figures/line_plot_graph.png	\
		fr/figures/line_plot.png		\
		fr/figures/parametric_graph.png	\
		fr/figures/surface_graph.png

manualxmlfrdir = $(datadir)/genius/help/fr
manualxmlfr_DATA = fr/genius.xml
manualxmlfrfiguresdir = $(datadir)/genius/help/fr/figures
manualxmlfrfigures_DATA = $(THE_frFIGURES)

manualhtmlfrdir = $(datadir)/genius/help/fr/html
manualhtmlfr_DATA = fr/html/*.html
manualhtmlfrfiguresdir = $(datadir)/genius/help/fr/html/figures
manualhtmlfrfigures_DATA = $(THE_frFIGURES)

#pt_BR
manualxmlpt_BRdir = $(datadir)/genius/help/pt_BR
manualxmlpt_BR_DATA = pt_BR/genius.xml
manualxmlpt_BRfiguresdir = $(datadir)/genius/help/pt_BR/figures
manualxmlpt_BRfigures_DATA = $(THE_CFIGURES)

manualhtmlpt_BRdir = $(datadir)/genius/help/pt_BR/html
manualhtmlpt_BR_DATA = pt_BR/html/*.html
manualhtmlpt_BRfiguresdir = $(datadir)/genius/help/pt_BR/html/figures
manualhtmlpt_BRfigures_DATA = $(THE_CFIGURES)

#ru
manualxmlrudir = $(datadir)/genius/help/ru
manualxmlru_DATA = ru/genius.xml
manualxmlrufiguresdir = $(datadir)/genius/help/ru/figures
manualxmlrufigures_DATA = $(THE_CFIGURES)

manualhtmlrudir = $(datadir)/genius/help/ru/html
manualhtmlru_DATA = ru/html/*.html
manualhtmlrufiguresdir = $(datadir)/genius/help/ru/html/figures
manualhtmlrufigures_DATA = $(THE_CFIGURES)

#sv
THE_svFIGURES =	sv/figures/parametric.png	\
		sv/figures/genius_window.png	\
		sv/figures/line_plot_graph.png	\
		sv/figures/line_plot.png		\
		sv/figures/parametric_graph.png	\
		sv/figures/surface_graph.png

manualxmlsvdir = $(datadir)/genius/help/sv
manualxmlsv_DATA = sv/genius.xml
manualxmlsvfiguresdir = $(datadir)/genius/help/sv/figures
manualxmlsvfigures_DATA = $(THE_svFIGURES)

manualhtmlsvdir = $(datadir)/genius/help/sv/html
manualhtmlsv_DATA = sv/html/*.html
manualhtmlsvfiguresdir = $(datadir)/genius/help/sv/html/figures
manualhtmlsvfigures_DATA = $(THE_svFIGURES)

#
# Text version

manualdir = $(datadir)/genius
manual_DATA = genius.txt

EXTRA_DIST = genius.txt \
	     C/legal.xml \
	     C/genius.xml \
	     C/html/*.html \
	     cs/genius.xml \
	     cs/html/*.html \
	     de/genius.xml \
	     de/html/*.html \
	     el/genius.xml \
	     el/html/*.html \
	     es/genius.xml \
	     es/html/*.html \
	     fr/genius.xml \
	     fr/html/*.html \
	     pt_BR/genius.xml \
	     pt_BR/html/*.html \
	     ru/genius.xml \
	     ru/html/*.html \
	     sv/genius.xml \
	     sv/html/*.html \
	     $(THE_CFIGURES) \
	     $(THE_csFIGURES) \
	     $(THE_deFIGURES) \
	     $(THE_frFIGURES) \
	     $(THE_svFIGURES) \
	     update-po.sh \
	     update-xml-to-txt-html.sh
