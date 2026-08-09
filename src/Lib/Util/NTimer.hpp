/*
Copyright (C) 2007 by Aaron Perez <aaronps@gmail.com>

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

#ifndef _NTIMER_HPP_
#define _NTIMER_HPP_

#include <SDL.h>

// Times are 64-bit and read through SDL_GetTicks64 (SDL >= 2.0.18, which this
// project already requires). SDL_GetTicks() wraps after 49.7 days, and
// "starttime + timeout" wrapped with it, so on a server up that long
// isTimeOut() started returning true immediately and kept doing so. 64 bits
// pushes that out beyond any plausible uptime, and it is also what
// SDL_GetTicks() itself returns in SDL3.
class NTimer {
 public:
  NTimer() : starttime(0), timeout(0) {}
  NTimer(Uint64 t) : starttime(0), timeout(t) {}

  inline void reset() { starttime = SDL_GetTicks64(); }
  inline void reset(Uint64 t) { starttime = t; }

  inline Uint64 getStartTime() { return starttime; }

  inline void setTimeOut(Uint64 t) { timeout = t; }

  inline Uint64 getTimeOut() { return timeout; }

  inline bool isTimeOut() { return (starttime + timeout) < SDL_GetTicks64(); }
  inline bool isTimeOut(Uint64 t) { return (starttime + timeout) < t; }
  inline bool checkWithTimeOut(Uint64 tout) {
    return (starttime + tout) < SDL_GetTicks64();
  }

 private:
  Uint64 starttime;
  Uint64 timeout;
};

#endif  // _NTIMER_HPP_
