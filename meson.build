project('genius', 'c',
  version: '1.0.28',
  meson_version: '>= 1.0',
  default_options: [
    'warning_level=1',
    'c_std=gnu99',
  ],
)

gnome = import('gnome')
i18n = import('i18n')

cc = meson.get_compiler('c')

conf_data = configuration_data()
conf_data.set_quoted('PACKAGE', meson.project_name())
conf_data.set_quoted('VERSION', meson.project_version())
conf_data.set_quoted('PACKAGE_NAME', meson.project_name())
conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())
conf_data.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
conf_data.set_quoted('GETTEXT_PACKAGE', meson.project_name())

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
localedir = join_paths(prefix, get_option('localedir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
includedir = join_paths(prefix, get_option('includedir'))
libdir = join_paths(prefix, get_option('libdir'))

conf_data.set_quoted('DATADIR', datadir)
conf_data.set_quoted('LIBEXECDIR', libexecdir)
conf_data.set_quoted('GNOMELOCALEDIR', localedir)
conf_data.set_quoted('BUILDDIR', meson.project_build_root())
conf_data.set('libdir', libdir) # used in test.plugin.in

# Dependencies
glib_dep = dependency('glib-2.0', version: '>= 2.41.1')
gmodule_dep = dependency('gmodule-2.0')
gio_dep = dependency('gio-2.0', version: '>= 2.16.0')
intl_dep = dependency('intl')

use_gnome = get_option('gnome')
if use_gnome
  gtk_dep = dependency('gtk+-3.0', version: '>= 3.21.4')
  vte_dep = dependency('vte-2.91', version: '>= 0.50.0')
endif

use_gtksourceview = get_option('gtksourceview')
if use_gnome and use_gtksourceview
  gsv_dep = dependency('gtksourceview-4', version: '>= 3.99.7', required: false)
  if gsv_dep.found()
    conf_data.set('HAVE_GTKSOURCEVIEW', 1)
  else
    use_gtksourceview = false
  endif
else
  use_gtksourceview = false
endif

# Math and other libraries
m_dep = cc.find_library('m', required: false)
gmp_dep = cc.find_library('gmp', required: true)
mpfr_dep = cc.find_library('mpfr', required: true)

# readline and termcap/ncurses
ncurses_dep = dependency('ncurses', required: false)
if not ncurses_dep.found()
  termcap_dep = cc.find_library('termcap', required: false)
  if termcap_dep.found()
    readline_deps = [termcap_dep]
  else
    error('Neither ncurses nor termcap found')
  endif
else
  readline_deps = [ncurses_dep]
  conf_data.set('USE_NCURSES', 1)
  if cc.has_header('ncurses/curses.h')
    conf_data.set('INC_NCURSES', 1)
  endif
endif

readline_dep = cc.find_library('readline', required: true)
readline_deps += [readline_dep]

# Lex and Yacc
flex = find_program('flex', required: true)
bison = find_program('bison', required: true)

# Functions
if cc.has_function('wordexp')
  conf_data.set('HAVE_WORDEXP', 1)
endif

# Binary relocation
if get_option('binreloc')
  conf_data.set('ENABLE_BINRELOC', 1)
endif

# Extra optimizations
if get_option('extra_gcc_optimization') and cc.get_id() == 'gcc'
  add_project_arguments('-finline-functions', '-frename-registers', language: 'c')
endif

configure_file(output: 'config.h', configuration: conf_data)

add_project_arguments('-I' + meson.current_build_dir(), language: 'c')
add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

subdir('ve')
subdir('gtkextra')
subdir('src')
subdir('pixmaps')
subdir('examples')
subdir('lib')
subdir('po')
subdir('help')

# Root files
install_data('genius.keys', 'genius.mime',
  install_dir: join_paths(get_option('datadir'), 'mime-info'),
)

install_data('genius.applications',
  install_dir: join_paths(get_option('datadir'), 'application-registry'),
)

install_data('genius.xml',
  install_dir: join_paths(get_option('datadir'), 'mime/packages'),
)

# update-mime-database
if get_option('update_mimedb')
  meson.add_install_script('sh', '-c', 'update-mime-database "${DESTDIR:-}' + join_paths(datadir, 'mime') + '"')
endif
