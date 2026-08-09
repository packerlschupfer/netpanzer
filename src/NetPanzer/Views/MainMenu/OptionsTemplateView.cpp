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

#include "Views/MainMenu/OptionsTemplateView.hpp"

#include "Interfaces/GameConfig.hpp"
#include "Interfaces/GameManager.hpp"
#include "System/DummySound.hpp"
#include "System/SDLSound.hpp"
#include "System/Sound.hpp"
#include "Util/Exception.hpp"
#include "Util/Log.hpp"
#include "Views/Components/Button.hpp"
#include "Views/Components/Desktop.hpp"
#include "Views/Components/Label.hpp"
#include "Views/GameViewGlobals.hpp"

class Separator : public Component {
 public:
  Separator(int x, int y, int endx, std::string t, PIX color) {
    text = t;
    foreground = color;
    position.x = x;
    position.y = y;
    xend = endx;
    text_y = y - (Surface::getFontHeight() / 3);
  }

  void draw(Surface &dest);

  virtual void render() {
    // nothing
  }

  void actionPerformed(const mMouseEvent &me) {
    // nothing
  }

 private:
  std::string text;
  int xend;
  int text_y;
};

void Separator::draw(Surface &dest) {
  dest.drawLine(position.x, position.y + 3, position.x + 20, position.y + 3,
                foreground);
  // dest.drawLine(position.x, position.y+2, position.x+20, position.y+2,
  // Color::gray);
  dest.bltStringShadowed(position.x + 25, text_y, text.c_str(), foreground,
                         componentBodyColor);
  int lentxt = 30 + dest.getTextLength(text);
  dest.drawLine(position.x + lentxt, position.y + 3, xend, position.y + 3,
                foreground);
  // dest.drawLine(position.x+lentxt, position.y+2, xend, position.y+2,
  // Color::gray);
}

static void bIncreaseScrollRate() {
  if (GameConfig::interface_scrollrate + 100 <= 10000)
    GameConfig::interface_scrollrate = GameConfig::interface_scrollrate + 100;
}

static void bDecreaseScrollRate() {
  if (GameConfig::interface_scrollrate - 100 >= 500)
    GameConfig::interface_scrollrate = GameConfig::interface_scrollrate - 100;
}

static int getScrollRate() { return (int)GameConfig::interface_scrollrate; }

static void bDecreaseSoundVolume() {
  unsigned int v = GameConfig::sound_effectsvol;
  if (v) {
    --v;
    GameConfig::sound_effectsvol = v;
    sound->setSoundVolume(v);
  }
}

static void bIncreaseSoundVolume() {
  unsigned int v = GameConfig::sound_effectsvol;
  if (v < 100) {
    ++v;
    GameConfig::sound_effectsvol = v;
    sound->setSoundVolume(v);
  }
}

static int getSoundVolume() { return GameConfig::sound_effectsvol; }

static void bDecreaseMusicVolume() {
  unsigned int v = GameConfig::sound_musicvol;
  if (v) {
    --v;
    GameConfig::sound_musicvol = v;
    sound->setMusicVolume(v);
  }
}

static void bIncreaseMusicVolume() {
  unsigned int v = GameConfig::sound_musicvol;
  if (v < 100) {
    ++v;
    GameConfig::sound_musicvol = v;
    sound->setMusicVolume(v);
  }
}

static int getMusicVolume() { return GameConfig::sound_musicvol; }

// OptionsTemplateView
//---------------------------------------------------------------------------
OptionsTemplateView::OptionsTemplateView() : MenuTemplateView() {
  setSearchName("OptionsView");
  setTitle("OptionsTemplate");
  setSubTitle("");

  initButtons();

}  // end OptionsTemplateView::OptionsTemplateView

bool hasSize(const std::vector<SDL_DisplayMode> &modes, int w, int h) {
  for (const SDL_DisplayMode &existingMode : modes) {
    if (existingMode.w == w && existingMode.h == h) {
      return true;
    }
  }
  return false;
}

// The smallest resolution the game supports; also the fallback when the
// display reports nothing usable.
static const int MIN_DISPLAY_WIDTH = 800;
static const int MIN_DISPLAY_HEIGHT = 600;

static SDL_DisplayMode makeFallbackDisplayMode(SDL_DisplayID display_id) {
  SDL_DisplayMode mode;
  SDL_zero(mode);

  const SDL_DisplayMode *current = SDL_GetCurrentDisplayMode(display_id);
  if (current != nullptr) {
    mode = *current;
  } else {
    mode.w = GameConfig::video_width;
    mode.h = GameConfig::video_height;
  }

  if (mode.w < MIN_DISPLAY_WIDTH) mode.w = MIN_DISPLAY_WIDTH;
  if (mode.h < MIN_DISPLAY_HEIGHT) mode.h = MIN_DISPLAY_HEIGHT;
  return mode;
}

std::vector<SDL_DisplayMode> OptionsTemplateView::getUsableDisplayModes() {
  if (!usableDisplayModes.empty()) {
    return usableDisplayModes;
  }
  // SDL3 returns the whole list at once for a display id, rather than a
  // count plus indexed lookups.
  const SDL_DisplayID display_id = SDL_GetPrimaryDisplay();
  int display_mode_count = 0;
  SDL_DisplayMode **modes =
      SDL_GetFullscreenDisplayModes(display_id, &display_mode_count);
  if (modes == nullptr || display_mode_count < 1) {
    SDL_free(modes);
    // No fullscreen modes at all. That is normal headless -- the dummy video
    // driver reports none -- and it used to abort the whole game from here,
    // even though this list only fills the resolution selector in the options
    // menu. Fall back to what the display reports, or the configured size.
    LOGGER.warning("No fullscreen display modes available (%s); using a fallback resolution.",
                   SDL_GetError());
    usableDisplayModes.push_back(makeFallbackDisplayMode(display_id));
    return usableDisplayModes;
  }
  for (int i = 0; i < display_mode_count; i++) {
    SDL_DisplayMode mode = *modes[i];

    if (mode.w > 799 && mode.h > 599) {
      if (!hasSize(usableDisplayModes, mode.w, mode.h)) {
        usableDisplayModes.push_back(mode);
      }
    }

    //        SDL_Log("Mode %i\tbpp %i\t%s\t%i x %i",
    //                i, SDL_BITSPERPIXEL(mode.format),
    //                SDL_GetPixelFormatName(mode.format),
    //                mode.w, mode.h);
  }
  // The array is ours to release; the SDL_DisplayMode values were copied out
  // of it above.
  SDL_free(modes);

  // Every reported mode can still be below the minimum the game accepts, which
  // would leave the selector empty and the callers indexing into nothing.
  if (usableDisplayModes.empty()) {
    LOGGER.warning("No display mode is at least 800x600; using a fallback resolution.");
    usableDisplayModes.push_back(makeFallbackDisplayMode(display_id));
  }
  return usableDisplayModes;
}

// initButtons
//---------------------------------------------------------------------------
void OptionsTemplateView::initButtons() {
  removeAllButtons();
  removeComponents();

  MenuTemplateView::initButtons();

  int xTextStart = bodyTextRect.min.x;
  int x = xTextStart;
  int y = bodyTextRect.min.y;
  int yOffset = 17;
  int arrowButtonWidth = 16;
  int col = 200;
  // Settings
  //----------------------------------------------------------------------
  int minWidth = 19 * 8;

  add(new Separator(bodyTextRect.min.x, y, bodyTextRect.max.x, "VISUAL",
                    Color::gray));
  y += yOffset;
  x = xTextStart;
  checkBoxFullscreen = new CheckBox();
  checkBoxFullscreen->setLabel("Fullscreen");
  checkBoxFullscreen->setLocation(x + col, y);
  checkBoxFullscreen->setStateChangedCallback(this);
  add(checkBoxFullscreen);
  y += yOffset;
  x = xTextStart;
  checkBoxDrawAllShadows = new CheckBox();
  checkBoxDrawAllShadows->setLabel("Draw All Shadows");
  checkBoxDrawAllShadows->setState(GameConfig::video_shadows);
  checkBoxDrawAllShadows->setLocation(x + col, y);
  checkBoxDrawAllShadows->setStateChangedCallback(this);
  add(checkBoxDrawAllShadows);

  x = xTextStart;
  choiceResolution = new Choice();
  choiceResolution->setName("Resolution");
  choiceResolution->addItem("current");

  std::vector<SDL_DisplayMode> displayModes = getUsableDisplayModes();
  for (SDL_DisplayMode mode : displayModes) {
    std::string optionName =
        std::to_string(mode.w) + "x" + std::to_string(mode.h);
    choiceResolution->addItem(optionName);
  }

  choiceResolution->setLocation(x, y - 3);
  choiceResolution->select(0);
  choiceResolution->setMinWidth(minWidth);
  choiceResolution->setStateChangedCallback(this);
  add(choiceResolution);
  current_width = 0;
  current_height = 0;
  y += yOffset;

  x = xTextStart;
  checkBoxBlendSmoke = new CheckBox();
  checkBoxBlendSmoke->setLabel("Blend Smoke");
  checkBoxBlendSmoke->setState(GameConfig::video_blendsmoke);
  checkBoxBlendSmoke->setLocation(x + col, y);
  checkBoxBlendSmoke->setStateChangedCallback(this);
  add(checkBoxBlendSmoke);
  y += yOffset;

#if defined _WIN32 || defined __MINGW32__
  x = xTextStart;
  checkBoxUseDirectX = new CheckBox();
  checkBoxUseDirectX->setLabel("Use DirectX");
  checkBoxUseDirectX->setState(GameConfig::video_usedirectx);
  checkBoxUseDirectX->setLocation(x + col, y);
  checkBoxUseDirectX->setStateChangedCallback(this);
  add(checkBoxUseDirectX);
#endif

  x = xTextStart;
  choiceMiniMapUnitSize = new Choice();
  choiceMiniMapUnitSize->setName("Mini Map Unit Size");
  choiceMiniMapUnitSize->addItem("Small");
  choiceMiniMapUnitSize->addItem("Large");
  choiceMiniMapUnitSize->setLocation(x, y);
  choiceMiniMapUnitSize->select(gameconfig->radar_unitsize);
  choiceMiniMapUnitSize->setMinWidth(minWidth);
  choiceMiniMapUnitSize->setStateChangedCallback(this);
  add(choiceMiniMapUnitSize);
  y += yOffset * 2;

  // VISUAL OPTIONS
  add(new Separator(bodyTextRect.min.x, y, bodyTextRect.max.x, "INTERFACE",
                    Color::gray));

  y += yOffset;
  x = xTextStart;
  add(new Label(x, y + 3, "Scroll Rate:", windowTextColor, componentBodyColor));
  x += optionsMeterStartX;
  addButtonCenterText(iXY(x - 1, y), arrowButtonWidth, "<", "",
                      bDecreaseScrollRate);
  x += optionsMeterWidth + arrowButtonWidth;
  addButtonCenterText(iXY(x + 1, y), arrowButtonWidth, ">", "",
                      bIncreaseScrollRate);
  y += yOffset * 3;

  // SOUND OPTIONS

  add(new Separator(bodyTextRect.min.x, y, bodyTextRect.max.x, "SOUND",
                    Color::gray));

  y += yOffset;
  x = xTextStart;
  add(new Label(x, y, "Sound Status:", windowTextColor, componentBodyColor));
  checkBoxSoundEnabled = new CheckBox();
  checkBoxSoundEnabled->setLabel(GameConfig::sound_enable ? "Enabled"
                                                          : "Disabled");
  checkBoxSoundEnabled->setState(GameConfig::sound_enable);
  x += optionsMeterStartX;
  checkBoxSoundEnabled->setLocation(x, y - 2);
  checkBoxSoundEnabled->setStateChangedCallback(this);
  add(checkBoxSoundEnabled);
  y += yOffset;

  x = xTextStart;
  add(new Label(x, y + 3, "Sound Volume:", windowTextColor, componentBodyColor));
  x += optionsMeterStartX;
  addButtonCenterText(iXY(x - 1, y), arrowButtonWidth, "<", "",
                      bDecreaseSoundVolume);
  x += optionsMeterWidth + arrowButtonWidth;
  addButtonCenterText(iXY(x + 1, y), arrowButtonWidth, ">", "",
                      bIncreaseSoundVolume);
  y += yOffset;

  y += yOffset;
  x = xTextStart;
  add(new Label(x, y, "Music Status:", windowTextColor, componentBodyColor));
  checkBoxMusicEnabled = new CheckBox();
  checkBoxMusicEnabled->setLabel(GameConfig::sound_music ? "Enabled"
                                                         : "Disabled");
  checkBoxMusicEnabled->setState(GameConfig::sound_music);
  x += optionsMeterStartX;
  checkBoxMusicEnabled->setLocation(x, y - 2);
  checkBoxMusicEnabled->setStateChangedCallback(this);
  add(checkBoxMusicEnabled);
  y += yOffset;

  x = xTextStart;
  add(new Label(x, y + 3, "Music Volume:", windowTextColor, componentBodyColor));
  x += optionsMeterStartX;
  addButtonCenterText(iXY(x - 1, y), arrowButtonWidth, "<", "",
                      bDecreaseMusicVolume);
  x += optionsMeterWidth + arrowButtonWidth;
  addButtonCenterText(iXY(x + 1, y), arrowButtonWidth, ">", "",
                      bIncreaseMusicVolume);
  y += yOffset;
}  // end OptionsTemplateView::initButtons

// doDraw
//---------------------------------------------------------------------------
void OptionsTemplateView::doDraw(Surface &viewArea, Surface &clientArea) {
  checkBoxFullscreen->setState(GameConfig::video_fullscreen);
  checkBoxBlendSmoke->setState(GameConfig::video_blendsmoke);
  checkBoxDrawAllShadows->setState(GameConfig::video_shadows);

  MenuTemplateView::doDraw(viewArea, clientArea);

  Surface tempSurface(optionsMeterWidth, Surface::getFontHeight(), 1);

  // Scroll Rate
  tempSurface.fill(meterColor);
  tempSurface.drawButtonBorder(meterTopLeftBorderColor,
                               meterBottomRightBorderColor);

  char strBuf[256];
  const int arrowButtonWidth = 16;
  int x = bodyTextRect.min.x + optionsMeterStartX + arrowButtonWidth;
  int y = bodyTextRect.min.y;
  int yOffset = 17;

  y += yOffset * 7;

  sprintf(strBuf, "%d %%", int((float(getScrollRate()) / 10000.0f * 100.0f)));
  tempSurface.bltStringCenter(strBuf, meterTextColor, Color::white);
  tempSurface.blt(clientArea, x, y);
  y += yOffset * 2;

  // Sound Volume
  y += yOffset * 3;
  tempSurface.fill(meterColor);
  tempSurface.drawButtonBorder(meterTopLeftBorderColor,
                               meterBottomRightBorderColor);
  sprintf(strBuf, "%d %%", getSoundVolume());
  tempSurface.bltStringCenter(strBuf, meterTextColor, Color::white);
  tempSurface.blt(clientArea, x, y);

  // Music Volume
  y += yOffset * 3;
  tempSurface.fill(meterColor);
  tempSurface.drawButtonBorder(meterTopLeftBorderColor,
                               meterBottomRightBorderColor);
  sprintf(strBuf, "%d %%", getMusicVolume());
  tempSurface.bltStringCenter(strBuf, meterTextColor, Color::white);
  tempSurface.blt(clientArea, x, y);
}  // end VisualsView::doDraw

// processEvents
//---------------------------------------------------------------------------
// void OptionsView::processEvents()
//{
//    OptionsTemplateView::processEvents();
//} // end VisualsView::processEvents

// actionPerformed
//---------------------------------------------------------------------------
void OptionsTemplateView::stateChanged(Component *source) {
  if (source == checkBoxDrawAllShadows) {
    GameConfig::video_shadows = checkBoxDrawAllShadows->getState();
  } else if (source == checkBoxBlendSmoke) {
    GameConfig::video_blendsmoke = checkBoxBlendSmoke->getState();
  } else if (source == checkBoxFullscreen) {
    GameConfig::video_fullscreen = checkBoxFullscreen->getState();
    GameManager::setVideoMode();
  } else if (source == choiceResolution) {
    int sel_index = choiceResolution->getSelectedIndex() - 1;
    if (sel_index < 0) {
      return;
    }

    std::vector<SDL_DisplayMode> displayModes = getUsableDisplayModes();
    SDL_DisplayMode mode = displayModes[sel_index];

    GameConfig::video_width = mode.w;
    GameConfig::video_height = mode.h;

#ifdef SDL_PLATFORM_APPLE
    if (sel_index == 0 && !GameConfig::video_fullscreen) {
      // on Mac crash if we are in window and we select the biggest
      // resolution (the first one in theory), we make it smaller so it
      // wont crash, it is a SDL error.
      // TODO test w/ SDL2
      GameConfig::video_height -= 50;
    }
#endif

    GameManager::setVideoMode();
  } else if (source == choiceMiniMapUnitSize) {
    if (choiceMiniMapUnitSize->getSelectedIndex() == 0) {
      gameconfig->radar_unitsize = _mini_map_unit_size_small;
    } else if (choiceMiniMapUnitSize->getSelectedIndex() == 1) {
      gameconfig->radar_unitsize = _mini_map_unit_size_large;
    }
  } else if (source == checkBoxMusicEnabled) {
    GameConfig::sound_music = checkBoxMusicEnabled->getState();

    if (checkBoxMusicEnabled->getState()) {
      sound->playMusic("sound/music/");
      checkBoxMusicEnabled->setLabel("Enabled");
    } else {
      sound->stopMusic();
      checkBoxMusicEnabled->setLabel("Disabled");
    }
  } else if (source == checkBoxSoundEnabled) {
    GameConfig::sound_enable = checkBoxSoundEnabled->getState();

    delete sound;

    if (checkBoxSoundEnabled->getState()) {
      sound = new SDLSound();
      checkBoxSoundEnabled->setLabel("Enabled");
      if (GameControlRulesDaemon::getGameState()) {
        sound->playTankIdle();
      }
      if (checkBoxMusicEnabled->getState()) sound->playMusic("sound/music/");
    } else {
      sound = new DummySound();
      checkBoxSoundEnabled->setLabel("Disabled");
    }
  }
#if defined _WIN32 || defined __MINGW32__
  else if (source == checkBoxUseDirectX) {
    GameConfig::video_usedirectx = checkBoxUseDirectX->getState();
  }
#endif
}  // end OptionsTemplateView::doDraw

// loadBackgroundSurface
//---------------------------------------------------------------------------
void OptionsTemplateView::loadBackgroundSurface() {
  // MenuTemplateView::loadBackgroundSurface();
}  // end OptionsTemplateView::loadBackgroundSurface

// loadTitleSurface
//---------------------------------------------------------------------------
void OptionsTemplateView::loadTitleSurface() {
  // doLoadTitleSurface("optionsTitle");

}  // end ControlsView::loadTitleSurface

void OptionsTemplateView::onComponentClicked(Component *c) {
  MenuTemplateView::onComponentClicked(c);
}
