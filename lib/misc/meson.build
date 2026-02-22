gels = [
  'misc.gel',
]

foreach f : gels
  configure_file(input: f, output: f, copy: true)
endforeach

loader = custom_target('loader.gel',
  input: gels,
  output: 'loader.gel',
  command: [make_loader_gel, '@INPUT@'],
  capture: true,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'genius/gel/misc'),
)

all_sub_loaders += loader

install_data(gels,
  install_dir: join_paths(get_option('datadir'), 'genius/gel/misc'),
)
