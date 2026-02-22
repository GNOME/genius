icon_sizes = ['8x8', '16x16', '22x22', '24x24', '32x32', '48x48', '256x256']

foreach size : icon_sizes
  install_data(size / 'gnome-genius.png',
    install_dir: join_paths(get_option('datadir'), 'icons/hicolor', size, 'apps'),
  )
endforeach

install_data('scalable/gnome-genius.svg',
  install_dir: join_paths(get_option('datadir'), 'icons/hicolor/scalable/apps'),
)

install_data('genius-graph.png',
  install_dir: join_paths(get_option('datadir'), 'genius'),
)

# Icon cache update
gnome.post_install(
  gtk_update_icon_cache: true,
)
