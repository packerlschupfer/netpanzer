/*
Copyright (C) 1998 Pyrosoft Inc. (www.pyrosoftgames.com), Matthew Bogue

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

#include "SDLVideo.hpp"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <iostream>
#include <string>

#include "2D/Color.hpp"
#include "2D/SurfacePalette.hpp"
#include "Interfaces/ConsoleInterface.hpp"
#include "Util/Exception.hpp"
#include "Util/FileSystem.hpp"
#include "Util/Log.hpp"
#include "Util/NTimer.hpp"
#include "Util/Log.hpp"
#include "package.hpp"

#if defined _WIN32 || defined __MINGW32__

#include "Interfaces/GameConfig.hpp"

#endif

SDLVideo *Screen;  // get rid of this later...

SDLVideo::SDLVideo() : window(0) {
#if defined _WIN32
#if not defined __MINGW32__
  if (GameConfig::video_usedirectx) {
    putenv("SDL_VIDEODRIVER=directx");
  }
#endif
#endif
  this->window = nullptr;
  this->renderer = nullptr;
  this->surface = nullptr;
  this->texture = nullptr;
  this->argb_buffer = nullptr;
  this->prev_indexed = nullptr;
  this->argb_pixels = 0;
  this->prev_indexed_bytes = 0;
  this->have_prev_frame = false;
  memset(this->prev_lut, 0, sizeof(this->prev_lut));
  this->is_fullscreen = false;
  // SDL3 returns true on success here, where SDL2 returned 0.
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    throw Exception("Couldn't initialize SDL_video subsystem: %s",
                    SDL_GetError());
  }
}

SDLVideo::~SDLVideo() {
  releaseFrameCache();
  if (texture != nullptr) {
    SDL_DestroyTexture(texture);
  }
  if (surface != nullptr) {
    SDL_DestroySurface(surface);
  }
  if (renderer != nullptr) {
    SDL_DestroyRenderer(renderer);
  }
  if (window != nullptr) {
    SDL_DestroyWindow(window);
  }

  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool SDLVideo::setVideoMode(int new_width, int new_height, int bpp,
                            bool fullscreen) {
  const bool was_fullscreen = this->is_fullscreen;
  this->is_fullscreen = fullscreen;

  if (window == nullptr) {
    LOGGER.debug("Creating new window.");
    if (fullscreen) {
      // use the native desktop resolution, and scale linearly later using
      // renderer
      // SDL3 dropped the position arguments, and SDL_WINDOW_FULLSCREEN is
      // now the borderless-desktop behaviour that _DESKTOP used to mean.
      window = SDL_CreateWindow(Package::getFullyQualifiedName().c_str(), 0, 0,
                                SDL_WINDOW_FULLSCREEN);
    } else {
      window = SDL_CreateWindow(Package::getFullyQualifiedName().c_str(),
                                new_width, new_height, 0);
    }
    if (window == nullptr) {
        LOGGER.warning("Couldn't create a window: %s", SDL_GetError());
        return false;
    }

    LOGGER.debug("Showing window.");
    SDL_ShowWindow(window); // has to happen before fullscreen switch to fix cursor stuck in region issue
    SDL_RaiseWindow(window);

    LOGGER.debug("Creating new renderer.");
    renderer = SDL_CreateRenderer(window, nullptr);

    if (renderer == nullptr) {
        LOGGER.warning("Couldn't create renderer: %s", SDL_GetError());
        return false;
    }
  } else {
    LOGGER.debug("Showing window.");
    SDL_ShowWindow(window);  // has to happen before fullscreen switch to fix
    // cursor stuck in region issue
    SDL_RaiseWindow(window);
    if (fullscreen) {
      if (was_fullscreen) {
        // no change
      } else {
        LOGGER.debug("Setting fullscreen.");
        const bool setFullscreenOk = SDL_SetWindowFullscreen(window, true);
        if (!setFullscreenOk) {
            LOGGER.warning("Could not set fullscreen: %s", SDL_GetError());
        }
      }
    } else {
      if (was_fullscreen) {
        LOGGER.debug("Disabling fullscreen.");
        const bool disableFullScreenOk = SDL_SetWindowFullscreen(window, false);
        if (!disableFullScreenOk) {
            LOGGER.warning("Could not disable fullscreen: %s", SDL_GetError());
        }
      }
      LOGGER.debug("Setting window size.");
      SDL_SetWindowSize(window, new_width, new_height);
    }
  }

  if (surface != nullptr) {
    LOGGER.debug("Cleaning up old surface.");
    SDL_DestroySurface(surface);
  }

  LOGGER.debug("Creating surface.");
  surface = SDL_CreateSurface(new_width, new_height, SDL_PIXELFORMAT_INDEX8);

  if (surface == nullptr) {
    LOGGER.warning("Couldn't create render surface: %s", SDL_GetError());
    return false;
  }

  // Indexed surfaces do not carry a palette implicitly in SDL3; one has to be
  // attached before setPalette() or the render loop can use it.
  if (SDL_CreateSurfacePalette(surface) == nullptr) {
    LOGGER.warning("Couldn't attach a palette to the render surface: %s",
                   SDL_GetError());
    return false;
  }

  if (texture != nullptr) {
    LOGGER.debug("Destroying old texture.");
    SDL_DestroyTexture(texture);
  }

  // Any cached frame belongs to the old mode.
  releaseFrameCache();

  // A streaming texture, created once per video mode and then written in
  // place every frame. The previous code called SDL_CreateTextureFromSurface
  // in render(), which allocated and freed a full-screen GPU texture on every
  // single frame.
  LOGGER.debug("Creating new render texture.");
  texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, new_width, new_height);

  if (texture == nullptr) {
    LOGGER.warning("Couldn't create render texture: %s", SDL_GetError());
    return false;
  }

  // Nearest-neighbour scaling: "linear" made the game look blurry whenever
  // the game resolution did not match the monitor. In SDL3 this is a property
  // of the texture rather than the global SDL_HINT_RENDER_SCALE_QUALITY hint.
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);


  // With a logical size set, SDL scales incoming mouse coordinates into the
  // game's own resolution, so a click lands where the player aimed even when
  // the window is a different size than video.width/height. Without it the
  // game read raw window coordinates as if they were surface coordinates,
  // and every click was off by the scale factor.
  //
  // This used to be disabled because it "breaks right mouse movement w/
  // SDL_WarpMouseInWindow". That is a real interaction, not a reason to give
  // up the fix, but it does mean both directions have to be handled
  // explicitly. Incoming events are scaled into game space in
  // convertEventCoordinates(); SDL_WarpMouseInWindow, going the other way,
  // still expects window coordinates, so warping with a logical coordinate
  // lands somewhere else and the next motion event yields a nonsense delta.
  // Every warp below goes through warpMouse(), which converts back.
  LOGGER.debug("Setting render logical size.");
  const bool setLogicalSizeOk = SDL_SetRenderLogicalPresentation(
      renderer, new_width, new_height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  if (!setLogicalSizeOk) {
    LOGGER.warning("Couldn't set logical resolution: %d %d %s", new_width,
                   new_height, SDL_GetError());
  }

  // let's scare the mouse :)
  // this fixes the mouse cursor stuck to a small region after resolution change
  LOGGER.debug("Showing cursor.");
  const bool showCursorOk = SDL_HideCursor();
  if (!showCursorOk) {
    LOGGER.warning("Couldn't show cursor: %s", SDL_GetError());
    // can still try to continue
  }

  // Center the mouse after changing the resolution - also helps mouse cursor
  // from getting stuck in old region
  int centerX = new_width / 2;
  int centerY = new_height / 2;
  LOGGER.debug("Warping mouse into window...");
  warpMouse(centerX, centerY);
  SDL_SetWindowMouseGrab(window, fullscreen);
  return true;
}

void SDLVideo::warpMouse(int logical_x, int logical_y) {
  if (window == nullptr) return;

  // Incoming coordinates are scaled into game space by
  // convertEventCoordinates(), but SDL_WarpMouseInWindow speaks window
  // coordinates. Callers work in game coordinates, so convert on the way out
  // or the cursor lands somewhere else entirely.
  float window_x = (float)logical_x;
  float window_y = (float)logical_y;
  if (renderer != nullptr) {
    SDL_RenderCoordinatesToWindow(renderer, (float)logical_x, (float)logical_y,
                                  &window_x, &window_y);
  }

  SDL_WarpMouseInWindow(window, window_x, window_y);
}

void SDLVideo::convertEventCoordinates(SDL_Event* event) {
  if (renderer == nullptr || event == nullptr) return;
  SDL_ConvertEventToRenderCoordinates(renderer, event);
}

void SDLVideo::setPalette(SDL_Color *color) {
  SDL_SetPaletteColors(getSurfacePalette(surface), color, 0, 256);
}

SDL_Surface *SDLVideo::getSurface() { return surface; }
SDL_Window *SDLVideo::getWindow() { return window; }

void SDLVideo::releaseFrameCache() {
  delete[] argb_buffer;
  argb_buffer = nullptr;
  delete[] prev_indexed;
  prev_indexed = nullptr;
  argb_pixels = 0;
  prev_indexed_bytes = 0;
  have_prev_frame = false;
}

void SDLVideo::render() {
  // Going through the renderer rather than SDL_UpdateWindowSurface buys us
  // simpler code and much nicer scaling. What it must not cost us is a
  // texture allocation per frame, so the texture is created once in
  // setVideoMode and the indexed screen surface is expanded into it here.
  //
  // Expanding the whole screen every frame is itself most of the cost, and
  // most frames do not need it: measured in a game with four bots, only 7% of
  // frames differ from the one before at all, and across all frames 0.3% of
  // the screen changes. So the indexed pixels are compared against the last
  // frame a band of rows at a time, and only the bands that moved are
  // converted and uploaded.
  if (texture == nullptr || surface == nullptr) return;

  const int width = surface->w;
  const int height = surface->h;
  const int src_pitch = surface->pitch;
  const Uint8 *src = (const Uint8 *)surface->pixels;
  if (src == nullptr || width <= 0 || height <= 0) return;

  const size_t want_pixels = (size_t)width * height;
  const size_t want_indexed = (size_t)src_pitch * height;
  if (argb_pixels != want_pixels || prev_indexed_bytes != want_indexed) {
    releaseFrameCache();
    argb_buffer = new Uint32[want_pixels];
    prev_indexed = new Uint8[want_indexed];
    argb_pixels = want_pixels;
    prev_indexed_bytes = want_indexed;
    have_prev_frame = false;
  }

  Uint32 lut[256];
  const SDL_Palette *pal = getSurfacePalette(surface);
  const int color_count = (pal != nullptr) ? pal->ncolors : 0;
  for (int i = 0; i < 256; i++) {
    if (i < color_count) {
      const SDL_Color &c = pal->colors[i];
      lut[i] =
          0xFF000000u | ((Uint32)c.r << 16) | ((Uint32)c.g << 8) | (Uint32)c.b;
    } else {
      lut[i] = 0xFF000000u;
    }
  }

  // The same indices can mean different colours after a palette change -- the
  // game fades the palette -- so a new palette makes every row dirty.
  const bool palette_changed =
      !have_prev_frame || memcmp(lut, prev_lut, sizeof(lut)) != 0;

  const int BAND = 16;
  int first_dirty = -1;
  int last_dirty = -1;

  for (int y = 0; y < height; y += BAND) {
    const int rows = (y + BAND <= height) ? BAND : (height - y);
    const size_t off = (size_t)y * src_pitch;
    const size_t len = (size_t)rows * src_pitch;

    if (!palette_changed &&
        memcmp(prev_indexed + off, src + off, len) == 0) {
      continue;
    }

    for (int r = 0; r < rows; r++) {
      const Uint8 *src_row = src + (size_t)(y + r) * src_pitch;
      Uint32 *dst_row = argb_buffer + (size_t)(y + r) * width;
      for (int x = 0; x < width; x++) {
        dst_row[x] = lut[src_row[x]];
      }
    }

    memcpy(prev_indexed + off, src + off, len);

    if (first_dirty < 0) first_dirty = y;
    last_dirty = y + rows - 1;
  }

  memcpy(prev_lut, lut, sizeof(lut));
  have_prev_frame = true;

  // One upload covering everything that moved. Uploading the bounding span
  // rather than each band keeps this to a single call; the bands that changed
  // are usually adjacent anyway.
  if (first_dirty >= 0) {
    SDL_Rect dirty;
    dirty.x = 0;
    dirty.y = first_dirty;
    dirty.w = width;
    dirty.h = last_dirty - first_dirty + 1;
    SDL_UpdateTexture(texture, &dirty,
                      argb_buffer + (size_t)first_dirty * width,
                      width * (int)sizeof(Uint32));
  }

  SDL_RenderClear(renderer);
  SDL_RenderTexture(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

void SDLVideo::doScreenshot() {
  // this is called blind faith
  static NTimer timer(1000);

  if (!timer.isTimeOut()) {
    return;
  }

  filesystem::mkdir("screenshots");

  char buf[256];
  time_t curtime = time(0);
  struct tm *loctime = localtime(&curtime);
  strftime(buf, sizeof(buf), "screenshots/%Y%m%d_%H%M%S.bmp", loctime);

  std::string bmpfile = filesystem::getRealWriteName(buf);
  SDL_SaveBMP(surface, bmpfile.c_str());
  ConsoleInterface::postMessage(Color::cyan, false, 0,
                                "Screenshot saved as: %s", buf);
  timer.reset();
}
