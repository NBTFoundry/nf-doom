// SPDX-License-Identifier: GPL-2.0-or-later
#include "SDL.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char g_sdl_error[128] = "";
static int g_gl_attrs[16] = {0};

const char* SDL_GetError(void) {
  return g_sdl_error;
}
void SDL_ClearError(void) {
  g_sdl_error[0] = '\0';
}

Uint32 SDL_GetTicks(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
  return (Uint32)((Uint64)ts.tv_sec * 1000u + (Uint64)ts.tv_nsec / 1000000u);
}

void SDL_Delay(Uint32 ms) {
  struct timespec req;
  req.tv_sec = (time_t)(ms / 1000u);
  req.tv_nsec = (long)((ms % 1000u) * 1000000u);
  while (nanosleep(&req, &req) == -1 && errno == EINTR) {
  }
}

struct SDL_mutex {
  pthread_mutex_t m;
};

struct SDL_sem {
  sem_t s;
};

struct SDL_Thread {
  pthread_t t;
  SDL_ThreadFunction fn;
  void* data;
  int rc;
};

SDL_mutex* SDL_CreateMutex(void) {
  SDL_mutex* mtx = (SDL_mutex*)calloc(1, sizeof(*mtx));
  if (!mtx) return NULL;
  if (pthread_mutex_init(&mtx->m, NULL) != 0) {
    free(mtx);
    return NULL;
  }
  return mtx;
}

void SDL_DestroyMutex(SDL_mutex* mutex) {
  if (!mutex) return;
  pthread_mutex_destroy(&mutex->m);
  free(mutex);
}

int SDL_LockMutex(SDL_mutex* mutex) {
  if (!mutex) return -1;
  return pthread_mutex_lock(&mutex->m) == 0 ? 0 : -1;
}

int SDL_UnlockMutex(SDL_mutex* mutex) {
  if (!mutex) return -1;
  return pthread_mutex_unlock(&mutex->m) == 0 ? 0 : -1;
}

SDL_sem* SDL_CreateSemaphore(Uint32 initial_value) {
  SDL_sem* s = (SDL_sem*)calloc(1, sizeof(*s));
  if (!s) return NULL;
  if (sem_init(&s->s, 0, initial_value) != 0) {
    free(s);
    return NULL;
  }
  return s;
}

void SDL_DestroySemaphore(SDL_sem* sem) {
  if (!sem) return;
  sem_destroy(&sem->s);
  free(sem);
}

int SDL_SemWait(SDL_sem* sem) {
  if (!sem) return -1;
  while (sem_wait(&sem->s) == -1) {
    if (errno != EINTR) return -1;
  }
  return 0;
}

int SDL_SemPost(SDL_sem* sem) {
  if (!sem) return -1;
  return sem_post(&sem->s) == 0 ? 0 : -1;
}

static void* thread_trampoline(void* arg) {
  SDL_Thread* th = (SDL_Thread*)arg;
  th->rc = th->fn(th->data);
  return NULL;
}

SDL_Thread* SDL_CreateThread(SDL_ThreadFunction fn, const char* name, void* data) {
  SDL_Thread* th = (SDL_Thread*)calloc(1, sizeof(*th));
  (void)name;
  if (!th || !fn) {
    free(th);
    return NULL;
  }
  th->fn = fn;
  th->data = data;
  if (pthread_create(&th->t, NULL, thread_trampoline, th) != 0) {
    free(th);
    return NULL;
  }
  return th;
}

void SDL_WaitThread(SDL_Thread* thread, int* status) {
  if (!thread) return;
  pthread_join(thread->t, NULL);
  if (status) *status = thread->rc;
  free(thread);
}

SDL_RWops* SDL_RWFromConstMem(const void* mem, int size) {
  SDL_RWops* rw;
  if (!mem || size < 0) return NULL;
  rw = (SDL_RWops*)calloc(1, sizeof(*rw));
  if (!rw) return NULL;
  rw->mem = (const Uint8*)mem;
  rw->size = (size_t)size;
  rw->pos = 0;
  return rw;
}

void SDL_FreeRW(SDL_RWops* area) {
  free(area);
}

static Uint16 read_le16(const Uint8* p) {
  return (Uint16)(p[0] | ((Uint16)p[1] << 8));
}
static Uint32 read_le32(const Uint8* p) {
  return (Uint32)p[0] | ((Uint32)p[1] << 8) | ((Uint32)p[2] << 16) | ((Uint32)p[3] << 24);
}
static Sint32 read_le32s(const Uint8* p) {
  return (Sint32)read_le32(p);
}

SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask,
                                  Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
  SDL_Surface* s;
  SDL_PixelFormat* fmt;
  int bpp;
  int pitch;
  void* pixels;
  (void)flags;
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return NULL;
  bpp = (depth == 8) ? 1 : (depth == 24) ? 3 : 4;
  pitch = width * bpp;
  pixels = calloc((size_t)height, (size_t)pitch);
  fmt = (SDL_PixelFormat*)calloc(1, sizeof(*fmt));
  s = (SDL_Surface*)calloc(1, sizeof(*s));
  if (!pixels || !fmt || !s) {
    free(pixels);
    free(fmt);
    free(s);
    return NULL;
  }
  fmt->BitsPerPixel = (Uint8)(bpp * 8);
  fmt->BytesPerPixel = (Uint8)bpp;
  fmt->Rmask = Rmask;
  fmt->Gmask = Gmask;
  fmt->Bmask = Bmask;
  fmt->Amask = Amask;
  if (bpp == 1) {
    fmt->palette = (SDL_Palette*)calloc(1, sizeof(SDL_Palette));
    if (fmt->palette) {
      fmt->palette->ncolors = 256;
      fmt->palette->colors = (SDL_Color*)calloc(256, sizeof(SDL_Color));
    }
  }
  s->format = fmt;
  s->w = width;
  s->h = height;
  s->pitch = pitch;
  s->pixels = pixels;
  s->refcount = 1;
  return s;
}

SDL_Surface* SDL_CreateRGBSurfaceFrom(void* pixels, int width, int height, int depth, int pitch,
                                      Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
  SDL_Surface* s = SDL_CreateRGBSurface(0, width, height, depth, Rmask, Gmask, Bmask, Amask);
  if (!s) return NULL;
  if (pixels && pitch > 0) {
    int y;
    int copy_bytes = width * s->format->BytesPerPixel;
    if (pitch < copy_bytes) copy_bytes = pitch;
    for (y = 0; y < height; y++) {
      memcpy((Uint8*)s->pixels + y * s->pitch, (const Uint8*)pixels + y * pitch,
             (size_t)copy_bytes);
    }
  }
  return s;
}

void SDL_FreeSurface(SDL_Surface* surface) {
  if (!surface) return;
  if (surface->format) {
    if (surface->format->palette) {
      free(surface->format->palette->colors);
      free(surface->format->palette);
    }
    free(surface->format);
  }
  free(surface->pixels);
  free(surface);
}

int SDL_LockSurface(SDL_Surface* surface) {
  return surface ? 0 : -1;
}
void SDL_UnlockSurface(SDL_Surface* surface) {
  (void)surface;
}

SDL_Surface* SDL_LoadBMP_RW(SDL_RWops* src, int freesrc) {
  SDL_Surface* surf = NULL;
  if (src && src->mem && src->size >= 54 && src->mem[0] == 'B' && src->mem[1] == 'M') {
    Uint32 data_off = read_le32(src->mem + 10);
    Uint32 dib_size = read_le32(src->mem + 14);
    Sint32 w = read_le32s(src->mem + 18);
    Sint32 h = read_le32s(src->mem + 22);
    Uint16 bpp = read_le16(src->mem + 28);
    Uint32 comp = read_le32(src->mem + 30);
    int top_down = 0;
    if (h < 0) {
      top_down = 1;
      h = -h;
    }
    if (dib_size >= 40 && w > 0 && h > 0 && w <= 2048 && h <= 2048 && comp == 0 &&
        (bpp == 8 || bpp == 24 || bpp == 32) && data_off < src->size) {
      size_t row_stride = (((size_t)w * bpp + 31u) / 32u) * 4u;
      if (data_off + row_stride * (size_t)h <= src->size) {
        surf = SDL_CreateRGBSurface(0, (int)w, (int)h, bpp == 32 ? 32 : bpp, 0x000000FFu,
                                    0x0000FF00u, 0x00FF0000u, bpp == 32 ? 0xFF000000u : 0u);
        if (surf) {
          int y;
          if (bpp == 8 && surf->format->palette && 14u + dib_size + 256u * 4u <= data_off) {
            const Uint8* pal = src->mem + 14u + dib_size;
            int i;
            for (i = 0; i < 256; i++) {
              surf->format->palette->colors[i].b = pal[i * 4 + 0];
              surf->format->palette->colors[i].g = pal[i * 4 + 1];
              surf->format->palette->colors[i].r = pal[i * 4 + 2];
              surf->format->palette->colors[i].a = 255;
            }
          }
          for (y = 0; y < (int)h; y++) {
            int sy = top_down ? y : ((int)h - 1 - y);
            const Uint8* row = src->mem + data_off + (size_t)sy * row_stride;
            Uint8* dst = (Uint8*)surf->pixels + y * surf->pitch;
            if (bpp == 8) {
              memcpy(dst, row, (size_t)w);
            } else if (bpp == 24) {
              int x;
              for (x = 0; x < (int)w; x++) {
                dst[x * 3 + 0] = row[x * 3 + 2];
                dst[x * 3 + 1] = row[x * 3 + 1];
                dst[x * 3 + 2] = row[x * 3 + 0];
              }
            } else {
              int x;
              for (x = 0; x < (int)w; x++) {
                dst[x * 4 + 0] = row[x * 4 + 2];
                dst[x * 4 + 1] = row[x * 4 + 1];
                dst[x * 4 + 2] = row[x * 4 + 0];
                dst[x * 4 + 3] = row[x * 4 + 3];
              }
            }
          }
        }
      }
    }
  }
  if (freesrc && src) SDL_FreeRW(src);
  return surf;
}

SDL_Surface* SDL_ConvertSurface(SDL_Surface* src, const SDL_PixelFormat* fmt, Uint32 flags) {
  SDL_Surface* dst;
  int x, y;
  (void)flags;
  if (!src || !fmt) return NULL;
  dst = SDL_CreateRGBSurface(0, src->w, src->h, fmt->BitsPerPixel ? fmt->BitsPerPixel : 32,
                             fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
  if (!dst) return NULL;
  for (y = 0; y < src->h; y++) {
    const Uint8* srow = (const Uint8*)src->pixels + y * src->pitch;
    Uint8* drow = (Uint8*)dst->pixels + y * dst->pitch;
    for (x = 0; x < src->w; x++) {
      Uint8 r = 0, g = 0, b = 0, a = 255;
      if (src->format->BytesPerPixel == 1 && src->format->palette) {
        SDL_Color c = src->format->palette->colors[srow[x]];
        r = c.r;
        g = c.g;
        b = c.b;
        a = 255;
      } else if (src->format->BytesPerPixel == 3) {
        r = srow[x * 3 + 0];
        g = srow[x * 3 + 1];
        b = srow[x * 3 + 2];
      } else if (src->format->BytesPerPixel == 4) {
        r = srow[x * 4 + 0];
        g = srow[x * 4 + 1];
        b = srow[x * 4 + 2];
        a = srow[x * 4 + 3];
      }
      if (dst->format->BytesPerPixel == 3) {
        drow[x * 3 + 0] = r;
        drow[x * 3 + 1] = g;
        drow[x * 3 + 2] = b;
      } else if (dst->format->BytesPerPixel == 4) {
        drow[x * 4 + 0] = r;
        drow[x * 4 + 1] = g;
        drow[x * 4 + 2] = b;
        drow[x * 4 + 3] = a;
      } else if (dst->format->BytesPerPixel == 1) {
        drow[x] = r;
      }
    }
  }
  return dst;
}

SDL_PixelFormat* SDL_AllocFormat(Uint32 pixel_format) {
  SDL_PixelFormat* f = (SDL_PixelFormat*)calloc(1, sizeof(*f));
  if (!f) return NULL;
  f->format = pixel_format;
  f->BitsPerPixel = 32;
  f->BytesPerPixel = 4;
  f->Rmask = 0x000000FFu;
  f->Gmask = 0x0000FF00u;
  f->Bmask = 0x00FF0000u;
  f->Amask = 0xFF000000u;
  return f;
}

void SDL_FreeFormat(SDL_PixelFormat* format) {
  free(format);
}

int SDL_SetSurfacePalette(SDL_Surface* surface, SDL_Palette* palette) {
  if (!surface || !surface->format || !palette) return -1;
  if (surface->format->palette && surface->format->palette->colors && palette->colors) {
    int n = palette->ncolors < 256 ? palette->ncolors : 256;
    memcpy(surface->format->palette->colors, palette->colors, (size_t)n * sizeof(SDL_Color));
  }
  return 0;
}

int SDL_SetPaletteColors(SDL_Palette* palette, const SDL_Color* colors, int firstcolor,
                         int ncolors) {
  int i;
  if (!palette || !palette->colors || !colors || firstcolor < 0) return -1;
  for (i = 0; i < ncolors && (firstcolor + i) < palette->ncolors; i++) {
    palette->colors[firstcolor + i] = colors[i];
  }
  return 0;
}

int SDL_SetWindowGammaRamp(SDL_Window* window, const Uint16* red, const Uint16* green,
                           const Uint16* blue) {
  (void)window;
  (void)red;
  (void)green;
  (void)blue;
  return -1;
}

int SDL_GetWindowGammaRamp(SDL_Window* window, Uint16* red, Uint16* green, Uint16* blue) {
  (void)window;
  (void)red;
  (void)green;
  (void)blue;
  return -1;
}

void SDL_CalculateGammaRamp(float gamma, Uint16* ramp) {
  int i;
  (void)gamma;
  if (!ramp) return;
  for (i = 0; i < 256; i++) ramp[i] = (Uint16)((i << 8) | i);
}

int SDL_GL_SetAttribute(SDL_GLattr attr, int value) {
  if ((int)attr >= 0 && (int)attr < 16) g_gl_attrs[(int)attr] = value;
  return 0;
}

int SDL_GL_GetAttribute(SDL_GLattr attr, int* value) {
  if (value && (int)attr >= 0 && (int)attr < 16) *value = g_gl_attrs[(int)attr];
  return 0;
}

void* SDL_GL_GetProcAddress(const char* proc) {
  (void)proc;
  return NULL;
}

void SDL_PumpEvents(void) {}
int SDL_PollEvent(SDL_Event* event) {
  (void)event;
  return 0;
}

char* SDL_GetPrefPath(const char* org, const char* app) {
  const char* data = getenv("NBT_DATA_DIR");
  const char* home = getenv("HOME");
  char* out = (char*)malloc(512);
  (void)org;
  if (!out) return NULL;
  if (data && data[0]) {
    snprintf(out, 512, "%s/", data);
  } else if (home && home[0]) {
    snprintf(out, 512, "%s/.%s/", home, app ? app : "prboom-plus");
  } else {
    snprintf(out, 512, "/tmp/%s/", app ? app : "prboom-plus");
  }
  return out;
}

SDL_Surface* IMG_Load(const char* file) {
  (void)file;
  return NULL;
}

SDL_Surface* IMG_Load_RW(SDL_RWops* src, int freesrc) {
  if (freesrc && src) SDL_FreeRW(src);
  return NULL;
}

SDL_Surface* IMG_LoadTyped_RW(SDL_RWops* src, int freesrc, const char* type) {
  (void)type;
  if (freesrc && src) SDL_FreeRW(src);
  return NULL;
}
