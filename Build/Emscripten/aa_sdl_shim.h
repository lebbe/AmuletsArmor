/* Emscripten-only shim, force-included (-include) into every source file.
 *
 * CMake defines WIN32=1 for the whole game, which makes SDL 1.2's headers
 * think they are being compiled for Windows (__declspec, windows.h config).
 * Hide the macro while the real SDL headers are parsed. */
#ifndef AA_EMSCRIPTEN_SDL_SHIM_H
#define AA_EMSCRIPTEN_SDL_SHIM_H

#pragma push_macro("WIN32")
#undef WIN32
#include <SDL/SDL.h>
#pragma pop_macro("WIN32")

#endif
