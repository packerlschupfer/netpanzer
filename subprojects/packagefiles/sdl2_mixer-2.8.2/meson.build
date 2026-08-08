# Meson build for SDL2_mixer, carried in-tree.
#
# SDL_mixer ships no meson build of its own -- upstream has CMakeLists.txt and
# configure.ac only -- so wrapdb carries a hand-written port that has to be
# redone for each release. That is why wrapdb sits on 2.6.2 while 2.8.2 is the
# final SDL2-series release.
#
# Distribution builds link the system SDL2_mixer and never see this. The
# bundled builds -- Windows, macOS and the AppImage -- do.
#
# Derived from the wrapdb 2.6.2-4 port. Relative to that, 2.8.2 drops
# music_drmp3.c in favour of music_minimp3.c and adds music_gme.c,
# music_wavpack.c and remap_channels.c. Only the last of those compiles to
# anything here: the codec files are gated on their own MUSIC_* macros and
# become empty translation units when the feature is off.
#
# The wrapdb port also built the playmus sample, which pulled in SDL2main.
# Nothing consumes it, so it is left out.

project(
  'sdl2 mixer',
  'c',
  version: '2.8.2',
)

cc = meson.get_compiler('c')

# netPanzer only plays WAV effects and OGG music, so everything else stays
# off and the corresponding codec files compile away to nothing.
feature_args = ['-DMUSIC_WAV', '-DMUSIC_OGG']
if get_option('default_library') != 'static' and host_machine.system() == 'windows'
  feature_args += '-DDLL_EXPORT'
endif

sdl2_dep = dependency('sdl2')
ogg_dep = dependency('ogg')
vorbis_dep = dependency('vorbis')
vorbisfile_dep = dependency('vorbisfile')

flac_dep = dependency('libflac', required: get_option('flac'))
if flac_dep.found()
  feature_args += '-DMUSIC_FLAC_LIBFLAC'
endif

mpg123_dep = dependency('mpg123', required: get_option('mpg123'))
if mpg123_dep.found()
  feature_args += '-DMUSIC_MP3_MPG123'
endif

opus_dep = dependency('opusfile', required: get_option('opus'))
if opus_dep.found()
  feature_args += '-DMUSIC_OPUS'
endif

incdirs = include_directories('include', 'src', 'src/codecs')

sources = files(
  'src/codecs/load_aiff.c',
  'src/codecs/load_voc.c',
  'src/codecs/mp3utils.c',
  'src/codecs/music_cmd.c',
  'src/codecs/music_drflac.c',
  'src/codecs/music_flac.c',
  'src/codecs/music_fluidsynth.c',
  'src/codecs/music_gme.c',
  'src/codecs/music_minimp3.c',
  'src/codecs/music_modplug.c',
  'src/codecs/music_mpg123.c',
  'src/codecs/music_nativemidi.c',
  'src/codecs/music_ogg.c',
  'src/codecs/music_ogg_stb.c',
  'src/codecs/music_opus.c',
  'src/codecs/music_timidity.c',
  'src/codecs/music_wav.c',
  'src/codecs/music_wavpack.c',
  'src/codecs/music_xmp.c',
  'src/codecs/remap_channels.c',
  'src/effect_position.c',
  'src/effect_stereoreverse.c',
  'src/effects_internal.c',
  'src/mixer.c',
  'src/music.c',
  'src/utils.c',
)

sdl2_mixer_lib = library(
  'sdl2mixer',
  sources,
  c_args: feature_args,
  include_directories: incdirs,
  dependencies: [
    vorbisfile_dep,
    vorbis_dep,
    ogg_dep,
    opus_dep,
    mpg123_dep,
    flac_dep,
    sdl2_dep,
  ],
)

sdl2_mixer_dep = declare_dependency(
  include_directories: incdirs,
  link_with: sdl2_mixer_lib,
)
