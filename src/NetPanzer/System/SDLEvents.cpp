/*
   Copyright (C) 2004 Matthias Braun <matze@braunis.de>,

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
#include <ctype.h>

#include "2D/Palette.hpp"
#include "Interfaces/GameConfig.hpp"
#include "Interfaces/GameManager.hpp"
#include "Interfaces/KeyboardInterface.hpp"
#include "Interfaces/MouseInterface.hpp"
#include "SDLVideo.hpp"
#include "Util/Log.hpp"

bool handleSDLEvents() {
  static SDL_Event event;
  KeyboardInterface::sampleKeyboard();
  while (SDL_PollEvent(&event)) {
    // SDL2 scaled mouse coordinates into the logical space itself, as part of
    // delivering the event. SDL3 does not: it hands out window coordinates and
    // expects this call. Without it every click is off by the letterbox offset
    // and the render scale, which is invisible whenever the window happens to
    // match the game resolution exactly -- and wrong at every other size.
    if (Screen != 0) Screen->convertEventCoordinates(&event);

    switch (event.type) {
      case SDL_EVENT_QUIT:
        return true;
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        MouseInterface::onMouseButtonDown(&event.button);
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        MouseInterface::onMouseButtonUp(&event.button);
        break;
      case SDL_EVENT_MOUSE_MOTION:
        MouseInterface::onMouseMoved(&event.motion);
        break;
      case SDL_EVENT_TEXT_INPUT: {
        size_t text_length = strlen(event.text.text);

        for (size_t i = 0; i < text_length; i++) {
          KeyboardInterface::putChar(event.text.text[i]);
        }
        break;
      }
      case SDL_EVENT_KEY_DOWN: {
        //                LOGGER.info("Pressed key : scancode[%d] keycode[%d]",
        //                event.key.scancode, event.key.key);
        KeyboardInterface::keyPressed(event.key.key);

        SDL_Keycode c = event.key.key;
        switch (c) {
          // see cInputField
          case SDLK_HOME:
          case SDLK_LEFT:
          case SDLK_RIGHT:
          case SDLK_END:
          case SDLK_INSERT:
          case SDLK_DELETE:
          case SDLK_BACKSPACE:
          case SDLK_KP_ENTER:
          case SDLK_RETURN:
            // extended chars, first push a 0
            KeyboardInterface::putChar(0);
            KeyboardInterface::putChar(c);
            break;
          default:
            // international character ignored for now
            break;
        }

        break;
      }
      case SDL_EVENT_KEY_UP:
        //                LOGGER.debug("Released key : scancode[%d]
        //                keycode[%d]", event.key.scancode,
        //                event.key.key);
        KeyboardInterface::keyReleased(event.key.key);
        break;
    }
  }

  return false;
}
