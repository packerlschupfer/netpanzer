/*
   Copyright (C) 2003 Matthias Braun <matze@braunis.de>,
   Ivo Danihelka <ivo@danihelka.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <SDL3/SDL.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

#include <algorithm>
#include <SDL3_mixer/SDL_mixer.h>

#include <chrono>
#include <random>

#include "Interfaces/GameConfig.hpp"
#include "SDLSound.hpp"
#include "Util/Exception.hpp"
#include "Util/FileSystem.hpp"
#include "Util/Log.hpp"
#include "Util/NTimer.hpp"
#include "Interfaces/MapInterface.hpp"


#define SOUND_REPLAY_PROTECTION_TIME 50

class SoundData {
 private:
  MIX_Audio *chunk;

 public:
  NTimer last_played;

  SoundData() : chunk(0), last_played(SOUND_REPLAY_PROTECTION_TIME) {}
  SoundData(MIX_Audio *c)
      : chunk(c), last_played(SOUND_REPLAY_PROTECTION_TIME) {}
  ~SoundData() {
    if (chunk) {
      MIX_DestroyAudio(chunk);
      chunk = 0;
    }
  }

  MIX_Audio *getData() const { return chunk; }
};

musics_t SDLSound::musicfiles;
musics_t::iterator SDLSound::currentsong;
MIX_Mixer *SDLSound::mixer = 0;
MIX_Track *SDLSound::music_track = 0;
MIX_Audio *SDLSound::music_audio = 0;

//-----------------------------------------------------------------
SDLSound::SDLSound() : Sound(), m_chunks(), effects_gain(1.0f) {
  // SDL3 returns true on success here, where SDL2 returned 0.
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    throw Exception("SDL_Init audio error: %s", SDL_GetError());

  if (!MIX_Init()) throw Exception("Couldn't init mixer: %s", SDL_GetError());

  // Worth a line in the log: if SDL was built without a real backend it falls
  // back to the "dummy" driver, where every call succeeds and nothing is
  // audible. Without this the only symptom is silence.
  const char *driver = SDL_GetCurrentAudioDriver();
  LOGGER.info("Audio driver: %s", driver ? driver : "(none)");
  if (driver && SDL_strcmp(driver, "dummy") == 0)
    LOGGER.warning(
        "Audio driver is 'dummy' -- there will be no sound. SDL was built "
        "without a working audio backend, or no sound server is running.");

  // SDL3_mixer opens a device and hands back a mixer, rather than keeping a
  // single global one. Passing a null spec lets it pick the device's format.
  mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
  if (mixer == 0) {
    MIX_Quit();
    throw Exception("Couldn't open audio device: %s", SDL_GetError());
  }

  // The channel pool becomes a pool of tracks, created once and reused.
  for (int i = 0; i < SOUND_TRACK_COUNT; i++) {
    sound_tracks[i] = MIX_CreateTrack(mixer);
    if (sound_tracks[i] == 0) {
      LOGGER.info("Couldn't create sound track %d: %s", i, SDL_GetError());
    }
  }
  music_track = MIX_CreateTrack(mixer);

  loadSound("sound/");
}
//-----------------------------------------------------------------
SDLSound::~SDLSound() {
  stopMusic();

  for (int i = 0; i < SOUND_TRACK_COUNT; i++) {
    if (sound_tracks[i]) MIX_DestroyTrack(sound_tracks[i]);
    sound_tracks[i] = 0;
  }
  if (music_track) {
    MIX_DestroyTrack(music_track);
    music_track = 0;
  }
  if (music_audio) {
    MIX_DestroyAudio(music_audio);
    music_audio = 0;
  }

  for (chunks_t::iterator i = m_chunks.begin(); i != m_chunks.end(); i++) {
    delete i->second;
  }

  if (mixer) {
    MIX_DestroyMixer(mixer);
    mixer = 0;
  }
  MIX_Quit();

  SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

//-----------------------------------------------------------------
int SDLSound::findFreeTrack() {
  for (int i = 0; i < SOUND_TRACK_COUNT; i++) {
    if (sound_tracks[i] && !MIX_TrackPlaying(sound_tracks[i])) return i;
  }
  return -1;
}
//-----------------------------------------------------------------
/**
 * Find a chunk for this name.
 * @param name sound name
 * @return the chunk or NULL
 */
SoundData *SDLSound::findChunk(const char *name) {
  chunks_t::size_type count = m_chunks.count(name);
  if (count == 0) {
    LOG(("Silent sound '%s'", name));
    return 0;
  }

  chunks_t::iterator it = m_chunks.find(name);
  for (int i = rand() % count; i > 0; i--) {
    it++;
  }

  return it->second;
}
//-----------------------------------------------------------------
/**
 * Play sound once.
 * @param name sound name
 */
void SDLSound::playSound(const char *name) {
  SoundData *sdata = findChunk(name);
  if (sdata) {
    if (sdata->last_played.isTimeOut()) {
      const int t = findFreeTrack();
      if (t >= 0) {
        MIX_SetTrackGain(sound_tracks[t], effects_gain);
        if (MIX_SetTrackAudio(sound_tracks[t], sdata->getData())) {
          MIX_PlayTrack(sound_tracks[t], 0);
        }
      }
      sdata->last_played.reset();
    } else {
      //            LOGGER.debug("Skipped sound '%s' due to timeout", name);
    }
  }
}
//-----------------------------------------------------------------
/**
 * Play sound once.
 * @param name sound name
 * @param distance mag2 distance
 */
void SDLSound::playAmbientSound(const char *name, long distance) {
//  printf("playAmbientSound %s %ld\n", name, distance);
  SoundData *sdata = findChunk(name);
  if (sdata) {
    if (sdata->last_played.isTimeOut()) {
      // Gain is a property of the track in SDL3_mixer, not of the sample, so
      // the distance attenuation is applied to whichever track this plays on
      // instead of mutating the shared chunk.
      const float distance_gain = getSoundVolume(distance) / 128.0f;
      const float gain =
          distance_gain * ((float)GameConfig::sound_effectsvol / 100.0f);

      const int t = findFreeTrack();
      if (t >= 0) {
        MIX_SetTrackGain(sound_tracks[t], gain);
        if (MIX_SetTrackAudio(sound_tracks[t], sdata->getData())) {
          MIX_PlayTrack(sound_tracks[t], 0);
        }
      }
      sdata->last_played.reset();
    } else {
      //            LOGGER.debug("Skipped ambient sound '%s' due to timeout",
      //            name);
    }
  }
}
//-----------------------------------------------------------------
/**
 * Play sound repeatedly.
 * @param name sound name
 * @return the channel the sample is played on. On any errors, -1 is returned.
 */
int SDLSound::playSoundRepeatedly(const char *name) {
  int channel = -1;
  SoundData *sdata = findChunk(name);
  if (sdata) {
    channel = findFreeTrack();
    if (channel >= 0) {
      MIX_SetTrackGain(sound_tracks[channel], effects_gain);
      if (MIX_SetTrackAudio(sound_tracks[channel], sdata->getData())) {
        // A negative loop count repeats forever, as the -1 passed to
        // Mix_PlayChannel used to.
        SDL_PropertiesID opts = SDL_CreateProperties();
        SDL_SetNumberProperty(opts, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
        if (!MIX_PlayTrack(sound_tracks[channel], opts)) {
          LOG(("Couldn't play sound '%s': %s", name, SDL_GetError()));
          channel = -1;
        }
        SDL_DestroyProperties(opts);
      } else {
        channel = -1;
      }
    }
  }

  return channel;
}
//-----------------------------------------------------------------
/**
 * Stop playing the channel.
 * @param channel channel to stop
 */
void SDLSound::stopChannel(int channel) {
  if (channel >= 0 && channel < SOUND_TRACK_COUNT && sound_tracks[channel]) {
    MIX_StopTrack(sound_tracks[channel], 0);
  }
}
//-----------------------------------------------------------------
int SDLSound::getSoundVolume(long distance) {
  const int max_distance = MapInterface::getWidth();
  // 0 to 2 800x600 screen widths away--
  if ((distance < 640000)) return 128;

  // 2 to 4 800x600 screen widths away--
  if ((distance < 10240000)) return int(0.7 * 128);

  // 4 to 8 800x600 screen widths away--
  if ((distance < 40960000)) return int(0.5 * 128);

  // 8 to 12 800x600 screen widths away--
  if ((distance < 92760000)) return int(0.2 * 128);

  // 12 to 16 800x600 screen widths away--
  if ((distance < 163840000)) return int(0.1 * 128);

  // anything further away--
  return int(0.05 * 128); // better to have some background noise rather than nothing
}
//-----------------------------------------------------------------
/**
 * Load all *.wav from directory.
 * @param directory path to the directory
 */
void SDLSound::loadSound(const char *directory) {
  char **list = filesystem::enumerateFiles(directory);

  for (char **i = list; *i != NULL; i++) {
    std::string filename = directory;
    filename.append(*i);
    if (!filesystem::isDirectory(filename.c_str())) {
      try {
        filesystem::ReadFile *file = filesystem::openRead(filename.c_str());
        // predecode=true keeps short effects in memory, which is what
        // Mix_LoadWAV did; closeio=true hands the stream's lifetime over.
        MIX_Audio *chunk =
            MIX_LoadAudio_IO(mixer, file->getSDLRWOps(), true, true);
        if (chunk) {
          std::string idName = getIdName(*i);
          m_chunks.insert(std::pair<std::string, SoundData *>(
              idName, new SoundData(chunk)));
        } else {
          LOGGER.info("Couldn't load wav '%s': %s", filename.c_str(),
                      SDL_GetError());
        }
      } catch (Exception &e) {
        LOGGER.info("Couldn't load wav '%s': %s", filename.c_str(), e.what());
      }
    }
  }
  filesystem::freeList(list);
  setSoundVolume(GameConfig::sound_effectsvol);
}
//-----------------------------------------------------------------
/**
 * Hash filename to idName.
 * @return id name
 */
std::string SDLSound::getIdName(const char *filename) {
  std::string name = filename;
  std::string::size_type pos = name.find_first_of("._");

  return name.substr(0, pos);
}

void SDLSound::setSoundVolume(unsigned int volume) {
  if (volume > 100) volume = 100;
  // Gain belongs to the track rather than the sample, so this is remembered
  // and applied whenever a sound starts.
  effects_gain = volume / 100.0f;
  for (int i = 0; i < SOUND_TRACK_COUNT; i++) {
    if (sound_tracks[i]) MIX_SetTrackGain(sound_tracks[i], effects_gain);
  }
}

//---------------------------------------------------------------------------
// Music part
//---------------------------------------------------------------------------

void SDLSound::playMusic(const char *directory) {
  // Part1: scan directory for music files
  char **list = filesystem::enumerateFiles(directory);
  setMusicVolume(GameConfig::sound_musicvol);

  musicfiles.clear();
  for (char **i = list; *i != NULL; i++) {
    std::string filename = directory;
    filename.append(*i);
    if (!filesystem::isDirectory(filename.c_str())) {
      musicfiles.push_back(filename);
    }
  }
  filesystem::freeList(list);

  if (musicfiles.size() == 0) {
    LOGGER.info("Couldn't find any music in '%s'", directory);
    return;
  }

  // Part2: play music :)
  currentsong = musicfiles.end();
  nextSong();
  // The finished hook is per track in SDL3_mixer.
  if (music_track) MIX_SetTrackStoppedCallback(music_track, musicFinished, 0);
}

void SDLSound::stopMusic() {
  // nicely fade the music out
  if (music_track && MIX_TrackPlaying(music_track)) {
    MIX_SetTrackStoppedCallback(music_track, 0, 0);
    // The fade is expressed in sample frames rather than milliseconds.
    MIX_StopTrack(music_track, MIX_TrackMSToFrames(music_track, 500));
  }
}

void SDLCALL SDLSound::musicFinished(void *, MIX_Track *) { nextSong(); }

void SDLSound::setMusicVolume(unsigned int volume) {
  if (volume > 100) volume = 100;
  if (music_track) MIX_SetTrackGain(music_track, volume / 100.0f);
}

void SDLSound::nextSong() {
  if (music_track == 0) return;

  if (music_audio != 0) {
    MIX_StopTrack(music_track, 0);
    MIX_SetTrackAudio(music_track, 0);
    MIX_DestroyAudio(music_audio);
    music_audio = 0;
  }

  if (currentsong == musicfiles.end()) {
    // create a new random playlist
    std::mt19937 rng(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::shuffle(musicfiles.begin(), musicfiles.end(), rng);
    currentsong = musicfiles.begin();
  }

  musics_t::iterator lastsong = currentsong;
  do {
    const char *toplay = currentsong->c_str();
    currentsong++;
    // #ifdef HAS_LOADMUS_RW
    /*
     * use LoadMUS_RW from newer SDL_mixers
     */
    try {
      filesystem::ReadFile *file = filesystem::openRead(toplay);
      // predecode=false: music is streamed rather than held in memory.
      music_audio = MIX_LoadAudio_IO(mixer, file->getSDLRWOps(), false, true);
      if (music_audio) {
        if (MIX_SetTrackAudio(music_track, music_audio) &&
            MIX_PlayTrack(music_track, 0)) {
          LOG(("Start playing song '%s'", toplay));
          break;  // break while cycle
        } else {
          LOG(("Failed to play song '%s': %s", toplay, SDL_GetError()));
        }
      } else {
        LOG(("Failed to load song '%s': %s", toplay, SDL_GetError()));
      }
    } catch (Exception &e) {
      LOG(("Failed to load song '%s': %s", toplay, e.what()));
    }

    if (currentsong == musicfiles.end()) {
      currentsong = musicfiles.begin();
    }
  } while (currentsong != lastsong);
}
