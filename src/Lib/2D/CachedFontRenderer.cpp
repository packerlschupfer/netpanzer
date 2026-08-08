/*
Copyright (C) 2024 NetPanzer (https://github.com/netpanzer/), Devon Winrick, Et
al.

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

#ifndef TEST_LIB

#include "CachedFontRenderer.hpp"

#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include "Interfaces/MenuConfig.hpp"
#include "Util/FileSystem.hpp"
#include "Util/Log.hpp"

TTF_Font *CachedFontRenderer::font = nullptr;
Uint32 CachedFontRenderer::lastCleanedTick = 0;
std::unordered_map<std::string, RenderedText>
        CachedFontRenderer::rendered_surfaces = {};

void CachedFontRenderer::initFont() {
  if (TTF_Init() < 0) {
    LOGGER.warning("Couldn't initialize SDL TTF: %s\n", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  std::string absFontPath = std::string(filesystem::getRealName((*MenuConfig::menu_font).c_str()));
  LOGGER.warning("font path: %s", absFontPath.c_str());
  font = TTF_OpenFont(absFontPath.c_str(), MenuConfig::menu_font_size);
  if (font == NULL) {
    LOGGER.warning("CachedFontRenderer - cannot load font %s.", absFontPath.c_str());
#ifndef TEST_LIB
    exit(EXIT_FAILURE);
#else
    return;
#endif
  }

  TTF_SetFontStyle(CachedFontRenderer::font, TTF_STYLE_BOLD);
  TTF_SetFontHinting(CachedFontRenderer::font, TTF_HINTING_MONO);
}

std::string CachedFontRenderer::create_cache_key(const char *text,
                                                 SDL_Color color,
                                                 SDL_Color blendColor,
                                                 bool wrapped,
                                                 int wrapLength) {
  // Fixed-width hex behind a separator, for two reasons. It is one
  // allocation instead of the nine that a chain of std::to_string costs on
  // every lookup, hit or miss; and it cannot collide. Concatenating decimal
  // numbers made rgb(1,23,4) and rgb(12,3,4) both spell "1234", so two
  // different colours could share one cached surface and the wrong one would
  // be drawn.
  char suffix[32];
  const int suffix_len =
      snprintf(suffix, sizeof(suffix), "|%02X%02X%02X|%02X%02X%02X|%d|%d",
               color.r, color.g, color.b, blendColor.r, blendColor.g,
               blendColor.b, wrapped ? 1 : 0, wrapLength);

  const size_t text_len = strlen(text);
  std::string result;
  result.reserve(text_len + (suffix_len > 0 ? (size_t)suffix_len : 0));
  result.assign(text, text_len);
  if (suffix_len > 0) result.append(suffix, (size_t)suffix_len);
  return result;
}

SDL_Surface *CachedFontRenderer::render(const char *text, SDL_Color color, SDL_Color blendColor, bool wrapped, int wrapLength) {
  std::string key = create_cache_key(text, color, blendColor, wrapped, wrapLength);
  auto it = rendered_surfaces.find(key);
  if (it != rendered_surfaces.end()) {
    // Return the cached surface
    it->second.lastUsedTick = SDL_GetTicks();
    return it->second.sdlSurface;
  }

  // If not found, render the text
  SDL_Surface *rendered_surface = wrapped
          ? TTF_RenderUTF8_Shaded_Wrapped(CachedFontRenderer::font, text, color, blendColor, wrapLength)
          : TTF_RenderUTF8_Shaded(CachedFontRenderer::font, text, color, blendColor);
  if (rendered_surface) {
    // Store the rendered surface in the cache
    RenderedText rendered_text(rendered_surface, SDL_GetTicks());
    rendered_surfaces[key] = rendered_text;
  }

  return rendered_surface;
}

SDL_Surface *CachedFontRenderer::render(const char *text, SDL_Color color, SDL_Color blendColor) {
  return render(text, color, blendColor, false, 0);
}

SDL_Surface *CachedFontRenderer::renderWrapped(const char *text, SDL_Color color, SDL_Color blendColor, int wrapLength) {
  return render(text, color, blendColor, true, wrapLength);
}

void CachedFontRenderer::cleanup() {
  const Uint32 currentTick = SDL_GetTicks();
  const Uint32 cleanupThreshold = 20000;

  if (currentTick - lastCleanedTick < cleanupThreshold) {
    return;
  }

  LOGGER.debug("Cached font cleanup: Begin.");

  // Iterate through the map to remove old RenderedText objects
  for (auto it = rendered_surfaces.begin(); it != rendered_surfaces.end();) {
    Uint32 lastUsedTick = it->second.lastUsedTick;
    if (currentTick - lastUsedTick > cleanupThreshold) {
      SDL_FreeSurface(it->second.sdlSurface);  // Free SDL surface memory
      it = rendered_surfaces.erase(it);        // Remove the entry from the map
    } else {
      ++it;
    }
  }
  lastCleanedTick = currentTick;
  LOGGER.debug("Cached font cleanup: Done.");
}

#else

#include "test.hpp"
#include "CachedFontRenderer.hpp"
#include "Interfaces/MenuConfig.hpp"
#include "Scripts/ScriptManager.hpp"

void CachedFontRenderer::test_openFont(void) {
  CachedFontRenderer::initFont();
  assert(font != NULL);

  return;
}

/**
 * The cache key has to be injective: two draws that differ in any way must
 * not share a cached surface, or one of them is drawn in the wrong colour.
 * The original key concatenated decimal numbers with no separators, so
 * several genuinely different draws collapsed onto the same string.
 */
void CachedFontRenderer::test_cacheKey(void) {
  const SDL_Color black = {0, 0, 0, 255};

  // rgb(1,23,4) and rgb(12,3,4) both spelled "1234" under the old scheme.
  const SDL_Color a = {1, 23, 4, 255};
  const SDL_Color b = {12, 3, 4, 255};
  assert(create_cache_key("hp", a, black, false, 0) !=
         create_cache_key("hp", b, black, false, 0));

  // The text ran straight into the numbers, so a trailing digit in the text
  // was indistinguishable from a leading digit of the first colour channel.
  const SDL_Color c2 = {2, 3, 4, 255};
  const SDL_Color c12 = {12, 3, 4, 255};
  assert(create_cache_key("a1", c2, black, false, 0) !=
         create_cache_key("a", c12, black, false, 0));

  // The blend colour has to separate keys too, not just the foreground.
  assert(create_cache_key("x", a, a, false, 0) !=
         create_cache_key("x", a, b, false, 0));

  // Wrapping and wrap length are part of the rendered result.
  assert(create_cache_key("x", a, black, false, 0) !=
         create_cache_key("x", a, black, true, 0));
  assert(create_cache_key("x", a, black, true, 10) !=
         create_cache_key("x", a, black, true, 100));

  // Same inputs must always give the same key, or nothing ever hits.
  assert(create_cache_key("Player 1", a, b, true, 42) ==
         create_cache_key("Player 1", a, b, true, 42));

  // Empty text is a legal draw and must not read out of bounds.
  assert(!create_cache_key("", a, black, false, 0).empty());

  return;
}

int main(int argc, char *argv[]) {
  (void)argc;

  filesystem::initialize(argv[0], "test_CachedFontRenderer");
  Package::assignDataDir();
  filesystem::addToSearchPath(Package::getDataDir().c_str());

  ScriptManager::initialize();
  MenuConfig::loadConfig();
  CachedFontRenderer::test_openFont();
  CachedFontRenderer::test_cacheKey();

  return 0;
}

#endif
