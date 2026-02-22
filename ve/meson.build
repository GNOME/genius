ve_sources = [
  've-misc.c',
  've-misc.h',
  've-config.c',
  've-config.h',
  've-nongnome.c',
  've-i18n.h',
  'vicious.h',
]

ve_ui_sources = ve_sources + [
  've-miscui.c',
  've-miscui.h',
  'viciousui.h',
]

ve_args = [
  '-DGNOMELOCALEDIR="@0@"'.format(localedir),
]

libvicious = static_library('vicious',
  ve_sources,
  include_directories: [include_directories('.'), include_directories('..')],
  dependencies: [glib_dep, gmodule_dep],
  c_args: ve_args,
)

libvicious_dep = declare_dependency(
  link_with: libvicious,
  include_directories: include_directories('.'),
  dependencies: [glib_dep, gmodule_dep],
)

if use_gnome
  libviciousui = static_library('viciousui',
    ve_ui_sources,
    include_directories: [include_directories('.'), include_directories('..')],
    dependencies: [glib_dep, gmodule_dep, gtk_dep],
    c_args: ve_args,
  )

  libviciousui_dep = declare_dependency(
    link_with: libviciousui,
    include_directories: include_directories('.'),
    dependencies: [glib_dep, gmodule_dep, gtk_dep],
  )
else
  libviciousui_dep = libvicious_dep
endif
