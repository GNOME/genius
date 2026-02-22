gel_subdirs = [
  'calculus',
  'combinatorics',
  'equation_solving',
  'functions',
  'linear_algebra',
  'misc',
  'number_theory',
  'statistics',
  'sets',
  'symbolic',
  'commutative_algebra',
]

make_loader_gel = find_program('make_loader_gel.sh')
make_top_loader_gel = find_program('make_top_loader_gel.sh')

all_sub_loaders = []

foreach d : gel_subdirs
  subdir(d)
endforeach

# Top-level loader.gel
loader_gel = custom_target('loader.gel',
  input: all_sub_loaders,
  output: 'loader.gel',
  command: [make_top_loader_gel, '@INPUT@'],
  capture: true,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'genius/gel'),
)

# lib.cgel
lib_cgel = custom_target('lib.cgel',
  input: loader_gel,
  output: 'lib.cgel',
  command: [genius_target, '--maxerrors=0', '--compile', '@INPUT@'],
  capture: true,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'genius/gel'),
)

# library-strings.c
custom_target('library-strings.c',
  input: loader_gel,
  output: 'library-strings.c',
  command: [genius_target, '--maxerrors=0', '--gettext', '@INPUT@'],
  capture: true,
)
