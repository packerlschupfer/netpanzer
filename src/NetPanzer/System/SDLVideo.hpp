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
#ifndef __SDLVideo_hpp__
#define __SDLVideo_hpp__

#include <SDL3/SDL.h>

// DirectDraw class declarations
//---------------------------------------------------------------------------
class SDLVideo {
 private:
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Surface* surface;
  SDL_Texture* texture;
  bool is_fullscreen;

  /// Expanded ARGB copy of the screen, kept between frames so only the rows
  /// that changed have to be converted and uploaded.
  Uint32* argb_buffer;
  /// Last frame's indexed pixels, to detect what changed.
  Uint8* prev_indexed;
  /// Last frame's palette, since identical indices can still mean new colours.
  Uint32 prev_lut[256];
  size_t argb_pixels;
  size_t prev_indexed_bytes;
  bool have_prev_frame;

  void releaseFrameCache();

 public:
  SDLVideo();
  virtual ~SDLVideo();

  bool setVideoMode(int width, int height, int bpp, bool fullscreen);
  /// Warp the cursor to a point in game coordinates, converting to window
  /// coordinates on the way. Always use this rather than
  /// SDL_WarpMouseInWindow, which does not know about the logical size.
  void warpMouse(int logical_x, int logical_y);
  void setPalette(SDL_Color* color);
  SDL_Surface* getSurface();
  SDL_Window* getWindow();
  void render();
  void doScreenshot();
};  // end DirectDraw

extern SDLVideo* Screen;

#endif  // end __UIDraw_hpp__
