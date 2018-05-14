
EXTRA_DIST = iso2022.txt


# The library

noinst_HEADERS = pty.h reaper.h vte.h vteaccess.h vtepty.h vtetypebuiltins.h vteversion.h vtedeprecated.h

noinst_LIBRARIES = libvte.a

EXTRA_libvte_a_SOURCES = \
	genkeysyms.py \
	keysyms.c \
	marshal.list \
	mkunitables.sh \
	unitable.CNS11643 \
	unitable.CP437 \
	unitable.GB12345 \
	unitable.GB2312 \
	unitable.JIS0201 \
	unitable.JIS0208 \
	unitable.JIS0212 \
	unitable.KSX1001 \
	vteseq-2.gperf \
	vteseq-2.c \
	vteseq-n.gperf \
	vteseq-n.c \
	vtetypebuiltins.c.template \
	vtetypebuiltins.h.template \
	$(NULL)

libvte_a_SOURCES = \
	buffer.h \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	iso2022.c \
	iso2022.h \
	keymap.c \
	keymap.h \
	marshal.c \
	marshal.h \
	matcher.c \
	matcher.h \
	pty.c \
	pty.h \
	reaper.c \
	reaper.h \
	ring.c \
	ring.h \
	table.c \
	table.h \
	trie.c \
	trie.h \
	vte.c \
	vte.h \
	vte-gtk-compat.h \
	vte-private.h \
	vteaccess.c \
	vteaccess.h \
	vtebg.c \
	vtebg.h \
	vteconv.c \
	vteconv.h \
	vtedraw.c \
	vtedraw.h \
	vtedeprecated.h \
	vteint.h \
	vtepty.h \
	vtepty-private.h \
	vteregex.c \
	vteregex.h \
	vterowdata.c \
	vterowdata.h \
	vteseq.c \
	vteseq-list.h \
	vtestream.c \
	vtestream.h \
	vtestream-base.h \
	vtestream-file.h \
	vtetc.c \
	vtetc.h \
	vtetree.c \
	vtetree.h \
	vtetypebuiltins.c \
	vtetypebuiltins.h \
	vteunistr.c \
	vteunistr.h \
	vteversion.h
	$(NULL)

libvte_a_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Genius-Vte\" \
 	-DDATADIR='"$(datadir)"' \
	-DLIBEXECDIR='"$(libexecdir)"' \
	-DLOCALEDIR='"$(localedir)"' \
	-DVTE_COMPILATION \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	$(AM_CPPFLAGS)

libvte_a_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
#libvte_a_LIBADD = $(VTE_LIBS)
#libvte_a_LDFLAGS = \
  #$(VTE_LDFLAGS) \
	#-version-info $(LT_VERSION_INFO) \
	#-export-symbols-regex "^vte_terminal_.*|^vte_pty_.*|^vte_get_.*|^_vte_pty_.*|^vte_reaper_.*|_vte_debug_.*" \
	#@LIBTOOL_EXPORT_OPTIONS@ @LIBTOOL_FLAGS@ \
	#$(AM_LDFLAGS)


# Generated sources

VTESEQ_BUILTSOURCES = vteseq-list.h vteseq-2.c vteseq-n.c
BUILT_SOURCES = marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h $(VTESEQ_BUILTSOURCES)
MAINTAINERCLEANFILES = $(VTESEQ_BUILTSOURCES)
EXTRA_DIST += $(VTESEQ_BUILTSOURCES)
CLEANFILES = marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h stamp-vtetypebuiltins.h

marshal.c: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --body --internal $< > $@
	
marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --internal $< > $@

vtetypebuiltins.h: stamp-vtetypebuiltins.h
	@true
stamp-vtetypebuiltins.h: vtetypebuiltins.h.template vte.h vtepty.h vtedeprecated.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbh \
	&& (cmp -s xgen-vtbh vtetypebuiltins.h || cp xgen-vtbh vtetypebuiltins.h ) \
	&& rm -f xgen-vtbh \
	&& echo timestamp > $(@F)

vtetypebuiltins.c: vtetypebuiltins.c.template vte.h vtepty.h vtedeprecated.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbc \
	&& (cmp -s xgen-vtbc vtetypebuiltins.c || cp xgen-vtbc vtetypebuiltins.c ) \
	&& rm -f xgen-vtbc

vteseq-2.c: vteseq-2.gperf
	$(AM_V_GEN) $(top_srcdir)/missing --run gperf -m 100 --no-strlen $< > $@.tmp && \
	mv -f $@.tmp $@

vteseq-n.c: vteseq-n.gperf
	$(AM_V_GEN) $(top_srcdir)/missing --run gperf -m 100 $< > $@.tmp && \
	mv -f $@.tmp $@

vteseq-list.h: vteseq-2.gperf vteseq-n.gperf
	$(AM_V_GEN) echo '/* Generated file.  Do not edit */' > $@.tmp && \
	cat $^ | grep -v '^#' | grep '\<VTE_SEQUENCE_HANDLER\>'| sed 's/.*, //' | LANG=C sort -u >> $@.tmp && \
	mv -f $@.tmp $@
