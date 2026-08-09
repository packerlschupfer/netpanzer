#!/bin/sh
# Test for installed data

# As of now, this can be run manually or in the CI, not by 'meson test'.
# The data must be installed to get an accurate test. See support/Notes.md for
# details about installing data for testing.

set -ev

# SDL3 reads SDL_VIDEO_DRIVER / SDL_AUDIO_DRIVER; SDL2 read the unseparated
# SDL_VIDEODRIVER / SDL_AUDIODRIVER. Set both so this works either way --
# with only the SDL2 spelling the game tries to open a real display, finds
# none on a CI runner, and exits before the check below.
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
HOME=$PWD ./netpanzer &
./netpanzer &
sleep 10s

if pgrep netpanzer > /dev/null; then
  pkill netpanzer
  exit $?
else
  exit 1
fi

