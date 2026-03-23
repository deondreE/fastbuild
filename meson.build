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
