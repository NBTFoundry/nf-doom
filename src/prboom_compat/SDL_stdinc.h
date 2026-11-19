// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NBT_SDL_STDINC_H
#define NBT_SDL_STDINC_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;
typedef int64_t Sint64;

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

#define SDL_free(p) free(p)
#define SDL_malloc(s) malloc(s)
#define SDL_calloc(n, s) calloc((n), (s))
#define SDL_realloc(p, s) realloc((p), (s))

#endif
