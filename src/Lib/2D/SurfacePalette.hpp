/*
Copyright (C) 2026 The netPanzer Project

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
#ifndef _SURFACEPALETTE_HPP
#define _SURFACEPALETTE_HPP

#include <SDL.h>

/**
 * The palette of an indexed SDL_Surface.
 *
 * Reaching through surface->format->palette does not survive SDL3: there
 * SDL_Surface::format is an enumerated value rather than a pointer to a
 * structure, and the palette is reached with SDL_GetSurfacePalette() instead.
 * Indexed surfaces there also do not get a palette implicitly -- one is
 * attached with SDL_CreateSurfacePalette().
 *
 * Everything that wants the palette goes through here, so that change is one
 * edit rather than a hunt through the drawing code.
 */
inline SDL_Palette *getSurfacePalette(SDL_Surface *surface) {
  if (surface == 0 || surface->format == 0) return 0;
  return surface->format->palette;
}

inline const SDL_Palette *getSurfacePalette(const SDL_Surface *surface) {
  if (surface == 0 || surface->format == 0) return 0;
  return surface->format->palette;
}

#endif  // _SURFACEPALETTE_HPP
