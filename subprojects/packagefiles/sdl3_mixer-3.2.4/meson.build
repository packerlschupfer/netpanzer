# Meson build for SDL3_mixer, carried in-tree.
#
# SDL_mixer ships CMake only, and SDL3_mixer is not in wrapdb at all, so
# unlike sdl3 and sdl3_ttf there is no port to start from. This one is
# written against the 3.2.4 CMakeLists.
#
# netPanzer plays WAV effects and OGG music and nothing else, so only those
# two decoders are enabled. Every other decoder_*.c is gated behind its own
# DECODER_* macro and compiles to nothing without it, which is why they can
# all stay in the source list.

project(
  'sdl3_mixer',
  'c',
  version: '3.2.4',
  license: 'zlib',
  default_options: ['c_std=c99'],
)

cc = meson.get_compiler('c')

c_args = [
  '-DDECODER_WAV',           # sound effects
  '-DDECODER_OGGVORBIS_STB', # music, via the bundled stb_vorbis
  '-DDECODER_RAW',
  '-DDECODER_AU',
  '-DDECODER_AIFF',
  '-DDECODER_VOC',
]
if get_option('default_library') != 'static' and host_machine.system() == 'windows'
  c_args += '-DDLL_EXPORT'
endif

m_dep = cc.find_library('m', required: false)
sdl3_dep = dependency('sdl3')

sources = files(
  'src/SDL_mixer.c',
  'src/SDL_mixer_metadata_tags.c',
  'src/SDL_mixer_spatialization.c',
  'src/decoder_aiff.c',
  'src/decoder_au.c',
  'src/decoder_drflac.c',
  'src/decoder_drmp3.c',
  'src/decoder_flac.c',
  'src/decoder_fluidsynth.c',
  'src/decoder_gme.c',
  'src/decoder_mpg123.c',
  'src/decoder_opus.c',
  'src/decoder_raw.c',
  'src/decoder_sinewave.c',
  'src/decoder_stb_vorbis.c',
  'src/decoder_timidity.c',
  'src/decoder_voc.c',
  'src/decoder_vorbis.c',
  'src/decoder_wav.c',
  'src/decoder_wavpack.c',
  'src/decoder_xmp.c',
)

incdirs = include_directories('include', 'src')

sdl3_mixer_lib = library(
  'sdl3mixer',
  sources,
  c_args: c_args,
  include_directories: incdirs,
  dependencies: [sdl3_dep, m_dep],
)

sdl3_mixer_dep = declare_dependency(
  include_directories: incdirs,
  link_with: sdl3_mixer_lib,
)
