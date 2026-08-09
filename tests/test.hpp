// If NDEBUG is defined as a macro name at the point in the source code where
// <cassert> or <assert.h> is included, the assertion is disabled: assert does
// nothing.
// https://en.cppreference.com/w/cpp/error/assert
// "#undefine" is not a preprocessor directive, so this never did what it says:
// with NDEBUG defined it was a hard error rather than a re-enable. The whole
// suite states its expectations through assert(), so a build that quietly
// compiled them out would pass without checking anything.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#include "Util/FileSystem.hpp"
#include "package.hpp"

// Maybe resolves "undefined reference to SDL_main"
#ifdef WIN32
#include <windows.h>
#endif
// Maybe Resolves "undefined reference to WinMain"
#include <SDL3/SDL.h>
