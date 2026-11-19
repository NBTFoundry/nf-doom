// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NBT_SDL_THREAD_H
#define NBT_SDL_THREAD_H

typedef struct SDL_Thread SDL_Thread;
typedef int (*SDL_ThreadFunction)(void* data);

SDL_Thread* SDL_CreateThread(SDL_ThreadFunction fn, const char* name, void* data);
void SDL_WaitThread(SDL_Thread* thread, int* status);

#endif
