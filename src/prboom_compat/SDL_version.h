// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NBT_SDL_VERSION_H
#define NBT_SDL_VERSION_H

#define SDL_MAJOR_VERSION 2
#define SDL_MINOR_VERSION 0
#define SDL_PATCHLEVEL 20
#define SDL_VERSION_ATLEAST(X, Y, Z) \
  ((SDL_MAJOR_VERSION >= (X)) && (SDL_MINOR_VERSION >= (Y)) && (SDL_PATCHLEVEL >= (Z)))

typedef struct SDL_version {
  Uint8 major;
  Uint8 minor;
  Uint8 patch;
} SDL_version;

#endif
