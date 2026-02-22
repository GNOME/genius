help_langs = ['C', 'cs', 'de', 'el', 'es', 'fr', 'pt_BR', 'ru', 'sv']

foreach lang : help_langs
  install_data(lang / 'genius.xml',
    install_dir: join_paths(get_option('datadir'), 'genius/help', lang),
  )
  if lang == 'C'
    install_data(lang / 'legal.xml',
      install_dir: join_paths(get_option('datadir'), 'genius/help', lang),
    )
  endif
  install_subdir(lang / 'figures',
    install_dir: join_paths(get_option('datadir'), 'genius/help', lang),
  )
  install_subdir(lang / 'html',
    install_dir: join_paths(get_option('datadir'), 'genius/help', lang),
  )
endforeach

install_data('genius.txt',
  install_dir: join_paths(get_option('datadir'), 'genius'),
)
