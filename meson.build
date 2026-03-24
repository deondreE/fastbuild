project('fastbuild', 'cpp',
  version : '0.1',
  default_options : ['cpp_std=c++20'])

sources = [
  'src/main.cpp',
  'src/project_generator.cpp',
  'src/file_watcher.cpp'
]

executable('fastbuild',
  sources,
  include_directories : include_directories('include'),
  install : true
)

fish_comp_dir = get_option('datadir') / 'fish' / 'vendor_completions.d'

install_data(
    'completions/fastbuild.fish',
    install_dir: fish_comp_dir
)
