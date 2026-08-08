# Meson build for SDL2_ttf, carried in-tree.
#
# SDL_ttf ships no meson build of its own at any version -- upstream has
# CMakeLists.txt and configure.ac only -- so wrapdb carries a hand-written
# port and someone has to redo it for each release. That is why wrapdb sits
# on 2.20.1 while 2.24.0 is the final SDL2-series release.
#
# Distribution builds link the system SDL2_ttf and never see this. The
# bundled builds -- Windows, macOS and the AppImage -- do, which is why it is
# worth keeping current: SDL_ttf parses font files, i.e. untrusted input.
#
# Derived from the wrapdb 2.20.1-4 port. The library is still a single
# translation unit and the optional features are unchanged, so the port
# carries over almost verbatim.

project(
  'sdl2_ttf',
  'c',
  version: '2.24.0',
  license: 'zlib',
  default_options: ['c_std=gnu99'],
)

cc = meson.get_compiler('c')

c_args = []
if get_option('default_library') != 'static' and host_machine.system() == 'windows'
  c_args += '-DDLL_EXPORT'
endif

m_dep = cc.find_library('m', required: false)
freetype_dep = dependency('freetype2')
sdl2_dep = dependency('sdl2')

sdl2_ttf_deps = [freetype_dep, m_dep, sdl2_dep]

# Complex text shaping. netPanzer does not use it, and enabling it would pull
# HarfBuzz into every bundled build, so it stays off unless asked for.
harfbuzz_dep = dependency('harfbuzz', required: get_option('harfbuzz'))
if harfbuzz_dep.found()
  c_args += '-DTTF_USE_HARFBUZZ=1'
  sdl2_ttf_deps += harfbuzz_dep
endif

sdl2_ttf = library(
  'sdl2_ttf',
  'SDL_ttf.c',
  c_args: c_args,
  dependencies: sdl2_ttf_deps,
)

depinc = include_directories('.')
sdl2_ttf_dep = declare_dependency(
  link_with: sdl2_ttf,
  version: meson.project_version(),
  include_directories: depinc,
)
