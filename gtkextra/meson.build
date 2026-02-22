public_h_sources = [
  'gtkextra.h',
  'gtkextra-compat.h',
  'gtkpsfont.h',
  'gtkplot.h',
  'gtkplot3d.h',
  'gtkplotarray.h',
  'gtkplotbar.h',
  'gtkplotbox.h',
  'gtkplotcandle.h',
  'gtkplotcanvas.h',
  'gtkplotcanvasellipse.h',
  'gtkplotcanvasline.h',
  'gtkplotcanvaspixmap.h',
  'gtkplotcanvasplot.h',
  'gtkplotcanvasrectangle.h',
  'gtkplotcanvastext.h',
  'gtkplotcsurface.h',
  'gtkplotdata.h',
  'gtkplotdt.h',
  'gtkplotflux.h',
  'gtkplotcairo.h',
  'gtkplotpc.h',
  'gtkplotpixmap.h',
  'gtkplotpolar.h',
  'gtkplotprint.h',
  'gtkplotps.h',
  'gtkplotsurface.h',
]

glib_genmarshal = find_program('glib-genmarshal')

marshal_h = custom_target('gtkextra-marshal.h',
  input: 'gtkextra-marshal.list',
  output: 'gtkextra-marshal.h',
  command: [glib_genmarshal, '--prefix=gtkextra', '@INPUT@', '--header', '--output=@OUTPUT@', '--stdinc'],
)

marshal_c = custom_target('gtkextra-marshal.c',
  input: 'gtkextra-marshal.list',
  output: 'gtkextra-marshal.c',
  command: [glib_genmarshal, '--prefix=gtkextra', '@INPUT@', '--body', '--include-header=gtkextra-marshal.h', '--output=@OUTPUT@', '--stdinc'],
)

typebuiltins = gnome.mkenums('gtkextratypebuiltins',
  sources: public_h_sources,
  c_template: 'gtkextratypebuiltins.c.template',
  h_template: 'gtkextratypebuiltins.h.template',
)

gtkextra_sources = [
  'gtkextra.c',
  'gtkpsfont.c',
  'gtkplot.c',
  'gtkplot3d.c',
  'gtkplotarray.c',
  'gtkplotbar.c',
  'gtkplotbox.c',
  'gtkplotcandle.c',
  'gtkplotcanvas.c',
  'gtkplotcanvasellipse.c',
  'gtkplotcanvasline.c',
  'gtkplotcanvaspixmap.c',
  'gtkplotcanvasplot.c',
  'gtkplotcanvasrectangle.c',
  'gtkplotcanvastext.c',
  'gtkplotcsurface.c',
  'gtkplotdata.c',
  'gtkplotdt.c',
  'gtkplotflux.c',
  'gtkplotcairo.c',
  'gtkplotpc.c',
  'gtkplotpixmap.c',
  'gtkplotpolar.c',
  'gtkplotprint.c',
  'gtkplotps.c',
  'gtkplotsurface.c',
]

libgtkextra = static_library('gtkextra-genius',
  gtkextra_sources, marshal_h, marshal_c, typebuiltins,
  include_directories: [include_directories('.'), include_directories('..'), include_directories('../ve')],
  dependencies: [glib_dep, gtk_dep],
  c_args: ['-DGNOMELOCALEDIR="@0@"'.format(localedir)],
)

libgtkextra_dep = declare_dependency(
  link_with: libgtkextra,
  include_directories: include_directories('.'),
  dependencies: [glib_dep, gtk_dep],
)
