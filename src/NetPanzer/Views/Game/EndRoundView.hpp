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

#ifndef __EndRoundView_hpp__
#define __EndRoundView_hpp__

#include <vector>

#include "2D/Surface.hpp"
#include "Views/MainMenu/SpecialButtonView.hpp"

class PlayerState;

//---------------------------------------------------------------------------
class EndRoundView : public SpecialButtonView {
 private:
  int viewableMessageCount;
  enum { ITEM_GAP_SPACE = 10 };
  void drawPlayerStats(Surface &dest, unsigned int flagHeight, PIX winnerBlendColor, PIX tableHeaderBlendColor, PIX nextGameBlendColor);

  Surface allyImage;
  Surface allyRequestImage;
  Surface allyOtherImage;
  Surface noAllyImage;
  Surface colorImage;
  std::vector<const PlayerState *> states;
  int selected_line;
  iRect RectWinner;
  iRect RectStates;

 public:
  EndRoundView();
  virtual ~EndRoundView() {}

  virtual void doDraw(Surface &windowArea, Surface &clientArea);
  void checkResolution(iXY oldResolution, iXY newResolution);

 protected:
  virtual void lMouseDown(const iXY &pos);
  virtual void mouseMove(const iXY &prevPos, const iXY &newPos);
  virtual void doActivate();
  virtual void doDeactivate();
  // No drawTitle override here. View::drawTitle is not virtual, so this class
  // could never have replaced it; the empty body only looked like it
  // suppressed the title. The view sets setBordered(false), which is what
  // actually keeps the title from being drawn.
  //    virtual void processEvents();
};  // end _WIN

#endif  // end __EndRoundView_hpp__
