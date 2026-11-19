#ifndef NBT_SDL_H
#define NBT_SDL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "SDL_stdinc.h"
#include "SDL_endian.h"
#include "SDL_version.h"
#include "SDL_mutex.h"
#include "SDL_thread.h"
#include "SDL_audio.h"
#include "SDL_timer.h"

#define SDL_INIT_VIDEO 0x00000020u
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_INIT_TIMER 0x00000001u

typedef struct SDL_Color {
  Uint8 r;
  Uint8 g;
  Uint8 b;
  Uint8 a;
} SDL_Color;

typedef struct SDL_Palette {
  int ncolors;
  SDL_Color* colors;
  Uint32 version;
  int refcount;
} SDL_Palette;

typedef struct SDL_PixelFormat {
  Uint32 format;
  SDL_Palette* palette;
  Uint8 BitsPerPixel;
  Uint8 BytesPerPixel;
  Uint8 padding[2];
  Uint32 Rmask;
  Uint32 Gmask;
  Uint32 Bmask;
  Uint32 Amask;
  Uint8 Rloss;
  Uint8 Gloss;
  Uint8 Bloss;
  Uint8 Aloss;
  Uint8 Rshift;
  Uint8 Gshift;
  Uint8 Bshift;
  Uint8 Ashift;
  int refcount;
  struct SDL_PixelFormat* next;
} SDL_PixelFormat;

typedef struct SDL_Surface {
  Uint32 flags;
  SDL_PixelFormat* format;
  int w;
  int h;
  int pitch;
  void* pixels;
  void* userdata;
  int locked;
  void* lock_data;
  int refcount;
} SDL_Surface;

typedef struct SDL_RWops {
  const Uint8* mem;
  size_t size;
  size_t pos;
} SDL_RWops;

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Cursor SDL_Cursor;
typedef void* SDL_GLContext;

typedef struct SDL_Event {
  Uint32 type;
} SDL_Event;

#define SDL_PIXELFORMAT_RGBA8888 0x16462004u

const char* SDL_GetError(void);
void SDL_ClearError(void);

SDL_RWops* SDL_RWFromConstMem(const void* mem, int size);
void SDL_FreeRW(SDL_RWops* area);
SDL_Surface* SDL_LoadBMP_RW(SDL_RWops* src, int freesrc);
SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask,
                                  Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
SDL_Surface* SDL_CreateRGBSurfaceFrom(void* pixels, int width, int height, int depth, int pitch,
                                      Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
void SDL_FreeSurface(SDL_Surface* surface);
int SDL_LockSurface(SDL_Surface* surface);
void SDL_UnlockSurface(SDL_Surface* surface);
SDL_Surface* SDL_ConvertSurface(SDL_Surface* src, const SDL_PixelFormat* fmt, Uint32 flags);
SDL_PixelFormat* SDL_AllocFormat(Uint32 pixel_format);
void SDL_FreeFormat(SDL_PixelFormat* format);
int SDL_SetSurfacePalette(SDL_Surface* surface, SDL_Palette* palette);
int SDL_SetPaletteColors(SDL_Palette* palette, const SDL_Color* colors, int firstcolor,
                         int ncolors);

int SDL_SetWindowGammaRamp(SDL_Window* window, const Uint16* red, const Uint16* green,
                           const Uint16* blue);
int SDL_GetWindowGammaRamp(SDL_Window* window, Uint16* red, Uint16* green, Uint16* blue);
void SDL_CalculateGammaRamp(float gamma, Uint16* ramp);

typedef enum SDL_GLattr {
  SDL_GL_RED_SIZE = 0,
  SDL_GL_GREEN_SIZE,
  SDL_GL_BLUE_SIZE,
  SDL_GL_ALPHA_SIZE,
  SDL_GL_BUFFER_SIZE,
  SDL_GL_DOUBLEBUFFER,
  SDL_GL_DEPTH_SIZE,
  SDL_GL_STENCIL_SIZE,
  SDL_GL_MULTISAMPLEBUFFERS,
  SDL_GL_MULTISAMPLESAMPLES
} SDL_GLattr;

int SDL_GL_SetAttribute(SDL_GLattr attr, int value);
int SDL_GL_GetAttribute(SDL_GLattr attr, int* value);
void* SDL_GL_GetProcAddress(const char* proc);
void SDL_GL_SwapWindow(SDL_Window* window);
void SDL_PumpEvents(void);
int SDL_PollEvent(SDL_Event* event);
char* SDL_GetPrefPath(const char* org, const char* app);

/* Stubs for unguarded SDL2_image references in gl_hires.c when HAVE_LIBSDL2_IMAGE is off */
SDL_Surface* IMG_Load(const char* file);
SDL_Surface* IMG_Load_RW(SDL_RWops* src, int freesrc);
SDL_Surface* IMG_LoadTyped_RW(SDL_RWops* src, int freesrc, const char* type);

#ifdef __cplusplus
}
#endif

#endif /* NBT_SDL_H */
