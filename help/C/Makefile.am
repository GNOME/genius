figdir = figures
docname = genius
lang = C
omffile = genius-C.omf
entities = about.xml \
           examples.xml \
           gel-absolute-value-modulus.xml \
           gel-comparison-operators.xml \
           gel-conditionals.xml \
           gel-error-handling.xml \
           gel-function-list.xml \
           gel-functions.xml \
           gel-loading-programs.xml \
           gel-loops.xml \
           gel-lvalues.xml \
           gel-matrix-support.xml \
           gel-matrix-transpose.xml \
           gel-modular-evaluation.xml \
           gel-operator-list.xml \
           gel-polynomials.xml \
           gel-previous-result.xml \
           gel-references.xml \
           gel-returning.xml \
           gel-separator.xml \
           gel-startup-procedure.xml \
           gel-sums-products.xml \
           gel-values.xml \
           gel-variables.xml \
           legal.xml \
           prefs-display.xml \
           prefs-precision.xml \
           prefs-terminal.xml \
           starting-to-start.xml \
           starting-when-start.xml \
           usage-create-program.xml \
           usage-open-program.xml \
           usage-workarea.xml

include $(top_srcdir)/help/xmldocs.make
dist-hook: app-dist-hook

DISTCLEANFILES = genius-C.omf.out

#
# Text version
#

# Run this by hand for now
#genius.txt: genius.xml $(entities)
#	docbook2txt genius.xml

manualdir = $(datadir)/genius

manual_DATA = genius.txt

EXTRA_DIST += genius.txt
