src_inc = include_directories('.', '..', '../ve', '../gtkextra')

lgen = generator(flex, output: '@BASENAME@.c', arguments: ['-o', '@OUTPUT@', '@INPUT@'])
ygen = generator(bison, output: ['@BASENAME@.c', '@BASENAME@.h'], 
                 arguments: ['--defines=@OUTPUT1@', '--output=@OUTPUT0@', '@INPUT@'])

bison_src = ygen.process('parse.y')
flex_src = lgen.process('lexer.l')

common_sources = [
  'binreloc.c',
  'calc.c',
  'eval.c',
  'util.c',
  'dict.c',
  'funclib.c',
  'symbolic.c',
  'mpwrap.c',
  'mpzextra.c',
  'matrix.c',
  'matrixw.c',
  'matop.c',
  'compil.c',
  'parseutil.c',
  'inter.c',
  'plugin.c',
  'plugread.c',
  'genius_lists.c',
  'utype.c',
  'geloutput.c',
]

genius_sources = common_sources + [
  'genius.c',
  bison_src,
  flex_src,
]

gnome_genius_sources = common_sources + [
  'gnome-genius.c',
  'examples.c',
  'graphing.c',
  bison_src,
  flex_src,
]

readline_dep_combined = declare_dependency(dependencies: [readline_dep] + readline_deps)

genius_target = executable('genius',
  genius_sources,
  include_directories: src_inc,
  dependencies: [glib_dep, gmodule_dep, gmp_dep, mpfr_dep, m_dep, readline_dep_combined],
  link_with: libvicious,
  install: true,
)

if use_gnome
  executable('gnome-genius',
    gnome_genius_sources,
    include_directories: src_inc,
    dependencies: [glib_dep, gmodule_dep, gio_dep, gtk_dep, vte_dep, gmp_dep, mpfr_dep, m_dep, readline_dep_combined] + (use_gtksourceview ? [gsv_dep] : []),
    link_with: [libviciousui, libgtkextra],
    install: true,
  )
endif

executable('genius-readline-helper-fifo',
  ['genius_lists.c', 'genius-readline-helper.c'],
  include_directories: src_inc,
  dependencies: [glib_dep, gmodule_dep, readline_dep_combined],
  link_with: libvicious,
  install: true,
  install_dir: libexecdir,
)

# Plugin
shared_module('testplugin',
  'testplugin.c',
  include_directories: src_inc,
  dependencies: [glib_dep],
  install: true,
  install_dir: join_paths(get_option('libdir'), 'genius'),
)

configure_file(
  input: 'test.plugin.in',
  output: 'test.plugin',
  configuration: conf_data, # it only needs @libdir@
  install: true,
  install_dir: join_paths(get_option('datadir'), 'genius/plugins'),
)

# Desktop file
if use_gnome
  i18n.merge_file(
    input: 'gnome-genius.desktop.in',
    output: 'gnome-genius.desktop',
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: join_paths(get_option('datadir'), 'applications'),
  )
endif

# Language file
install_data('genius.lang',
  install_dir: join_paths(get_option('datadir'), 'genius/gtksourceview/'),
)

install_headers([
  'genius-i18n.h',
  'plug_api.h',
  'geloutput.h',
  'mpwrap.h',
  'mpzextra.h',
  'structs.h',
  'matrix.h',
  'matrixw.h',
  'matop.h',
  'compil.h',
  'utype.h',
  'dict.h',
  'lexer.h',
  'eval.h',
], install_dir: join_paths(get_option('includedir'), 'genius'))
