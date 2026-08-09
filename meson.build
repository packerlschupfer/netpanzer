project(
  'netpanzer',
  ['c', 'cpp'],
  version: '0.9.0',
  meson_version : '>= 0.59.0',
  default_options: [
    'warning_level=2',
    'cpp_std=c++20',
    # Sanitizers are opt-in, not opt-out: a plain "meson setup" must produce
    # the build we ship. Enable them explicitly with
    #   -Db_sanitize=address,undefined
    # which CI does for the Linux native job. b_lundef stays disabled because
    # the sanitizer runtimes need it when that option is turned on.
    'b_lundef=false',
    ]
  )

# Setup compiler.
cxx = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
extra_flags = [
  '-fno-common',
#  '-fstack-protector-strong',
#  '-Wformat-security',
#  '-Wformat-overflow=2',
  '-Wno-shadow',
  '-Werror=odr',
  '-Werror=lto-type-mismatch',
#  '-Werror=strict-aliasing',
#  '-Wstrict-overflow=5'
# '-Wconversion'
  ]

ldflags = []

mingw_flags = [
    '-static-libgcc',
    '-static-libstdc++',
  ]

host_sys = host_machine.system()
platform_is_windows = (host_sys == 'windows')

add_project_arguments(cc.get_supported_arguments(extra_flags), language: 'c')
add_project_arguments(cxx.get_supported_arguments(extra_flags), language: 'cpp')

if platform_is_windows
  ldflags += mingw_flags
endif

add_project_link_arguments(cxx.get_supported_link_arguments(ldflags), language: 'cpp')

deps = []

if platform_is_windows == true
    deps += cxx.find_library('ws2_32')
    deps += cxx.find_library('winpthread', static: true)
endif

conf = configuration_data()
subdir('po')
localedir = join_paths(get_option('prefix'), get_option('localedir'))
conf.set_quoted('NP_LOCALEDIR', localedir)
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('NP_DATADIR', join_paths(
  get_option('prefix'),
  get_option('datadir'),
  meson.project_name())
  )

config_h = configure_file(output : 'config.h', configuration : conf)

meson.add_devenv({'NETPANZER_DATADIR': join_paths(meson.project_source_root(), 'data')})
meson.add_devenv({'NETPANZER_LOCALEDIR': join_paths(meson.project_build_root(), 'po')})

# this won't use fallback defined in subprojects/lua.wrap
lua_dep = dependency('lua-5.1', required: false)
if not lua_dep.found()
  lua_dep = cc.find_library('lua', has_headers: 'lua.h', required: false)
  if not lua_dep.found()
    # This will use the fallback
    lua_dep = dependency(
      'lua-5.1',
      required: true,
      fallback: 'lua',
      default_options: ['default_library=static']
      )
  endif
endif
deps += lua_dep

phyfs_dep = dependency('physfs', required: false)
if not phyfs_dep.found()
  phyfs_dep = cc.find_library('physfs', has_headers: 'physfs.h', required: false)
  if not phyfs_dep.found()
    # This will use the fallback
    phyfs_dep = dependency(
      'physfs',
      required: true,
      fallback: 'physfs',
      default_options: ['default_library=static']
      )
  endif
endif
deps += phyfs_dep

sdl3_dep = dependency('sdl3', version: '>=3.2.0', required: false)
if not sdl3_dep.found()
  sdl3_dep = dependency(
    'sdl3',
    version: '>=3.2.0',
    required: true,
    fallback: 'sdl3',
    default_options: ['default_library=static'])
endif
deps += sdl3_dep
sdl2_dep = sdl3_dep

# SDL3 has no separate SDL2main library.
sdl2main_dep = []

# Don't add to deps, but add it to netpanzer_dep when the dependency is declared

sdl2_ttf_dep = dependency('sdl3-ttf', required: false)
if not sdl2_ttf_dep.found()
  sdl2_ttf_dep = dependency(
    'sdl3_ttf',
    required: true,
    fallback: 'sdl3_ttf',
    default_options: ['default_library=static'])
endif
deps += sdl2_ttf_dep

# SDL3_mixer is not in wrapdb and its API is a redesign rather than a rename,
# so audio is stubbed out in this pass and SDLSound is not built. The game
# falls back to DummySound.

inc_dirs = [include_directories('.')]

subdir('src')

subdir('src/tools')
exe = executable(
  'netpanzer',
  'src/NetPanzer/Core/main.cpp',
  win_subsystem: 'windows',
  dependencies: netpanzer_dep_static,
  link_args: ldflags,
  install : true
)

test(
  'test_' + meson.project_name(),
  exe,
  args: '--help',
  )

install_subdir(
  'data',
  install_dir : join_paths(get_option('datadir'), meson.project_name()),
  strip_directory : true
  )

# Check the Dockerfile when updating this
install_data(
  files(
    'README.md',
    'CONTRIBUTING.md',
    'COPYING.txt',
    'docs/SERVER-HOWTO.md',
    ),
  install_dir : get_option('docdir')
  )

if get_option('build_tests')
  subdir('tests')
endif

message('\n\n        == Important Note ==\n')
message('In the build directory, you will need to enter "meson devenv"')
message('to set the environmental variable NETPANZER_DATADIR to the absolute')
message('path of the "data" directory in the source root; or you may set the')
message('variable by entering "export NETPANZER_DATADIR=$PWD/data" while in the')
message('source root directory.\n')
