## Process this file with automake to produce makefile.in
SUBDIRS=.

INCLUDES = \
 	-I.					\
	-I..					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GENIUS_CFLAGS)

# Use these only really for checking for deprecated use, not all the time
#	-DG_DISABLE_DEPRECATED			\
#	-DGDK_DISABLE_DEPRECATED		\
#	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
#	-DGTK_DISABLE_DEPRECATED		\
#	-DGNOME_DISABLE_DEPRECATED		\
#


# For a gnome build set this in configure.in
#  VICIOUS_GNOME_LIBS="libviciousui.a libviciousui-gnome.a \
#       libviciousui-minimal.a libvicious-gnome.a"
#  AC_SUBST(VICIOUS_GNOME_LIBS)

noinst_LIBRARIES = \
	@GTKEXTRA_LIBS@

EXTRA_LIBRARIES = \
	libgtkextra-genius.a

EXTRA_DIST=gtkextra-marshal.list

public_h_sources =	\
	gtkextra.h		\
	gtkpsfont.h		\
	gtkplot.h		\
	gtkplot3d.h		\
	gtkplotbar.h		\
	gtkplotbox.h		\
	gtkplotcandle.h		\
	gtkplotcanvas.h		\
	gtkplotcsurface.h	\
	gtkplotdata.h		\
	gtkplotdt.h		\
	gtkplotdtriangle.h	\
	gtkplotflux.h		\
	gtkplotgdk.h		\
	gtkplotpc.h		\
	gtkplotpixmap.h		\
	gtkplotpolar.h		\
	gtkplotprint.h		\
	gtkplotps.h		\
	gtkplotsurface.h

libgtkextra_genius_a_SOURCES =	\
	$(public_h_sources)	\
	gtkextra-marshal.c	\
	gtkextra-marshal.h	\
	gtkextratypebuiltins.c	\
	gtkextratypebuiltins.h	\
	gtkextra.c		\
	gtkpsfont.c		\
	gtkplot.c		\
	gtkplot3d.c		\
	gtkplotbar.c		\
	gtkplotbox.c		\
	gtkplotcandle.c		\
	gtkplotcanvas.c		\
	gtkplotcsurface.c	\
	gtkplotdata.c		\
	gtkplotdt.c		\
	gtkplotdtriangle.c	\
	gtkplotflux.c		\
	gtkplotgdk.c		\
	gtkplotpc.c		\
	gtkplotpixmap.c		\
	gtkplotpolar.c		\
	gtkplotprint.c		\
	gtkplotps.c		\
	gtkplotsurface.c

$(srcdir)/gtkextra-marshal.h: gtkextra-marshal.list
        ( @GLIB_GENMARSHAL@ --prefix=gtkextra gtkextra-marshal.list --header > gtkextra-marshal.tmp \
        && mv gtkextra-marshal.tmp gtkextra-marshal.h ) \
        || ( rm -f gtkextra-marshal.tmp && exit 1 )
$(srcdir)/gtkextra-marshal.c: gtkextra-marshal.h
        ( @GLIB_GENMARSHAL@ --prefix=gtkextra gtkextra-marshal.list --body > gtkextra-marshal.tmp \
        && mv gtkextra-marshal.tmp gtkextra-marshal.c ) \
        || ( rm -f gtkextra-marshal.tmp && exit 1 )


$(srcdir)/gtkextratypebuiltins.h: $(public_h_sources)
	( cd $(srcdir) && glib-mkenums \
		--fhead "#ifndef __GTKEXTRA_TYPE_BUILTINS_H__\n#define __GTKEXTRA_TYPE_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void);\n#define GTK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n#endif /* __GTKEXTRA_TYPE_BUILTINS_H__ */" \
		$(public_h_sources) ) > gtkextratypebuiltins.h


$(srcdir)/gtkextratypebuiltins.c: $(public_h_sources)
	( cd $(srcdir) && glib-mkenums \
		--fhead "#define GTKEXTRA_ENABLE_BROKEN\n#include \"gtkextra.h\"" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(public_h_sources) ) > $(srcdir)/gtkextratypebuiltins.c