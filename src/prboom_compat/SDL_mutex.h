#ifndef NBT_SDL_MUTEX_H
#define NBT_SDL_MUTEX_H

typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_sem SDL_sem;

SDL_mutex* SDL_CreateMutex(void);
void SDL_DestroyMutex(SDL_mutex* mutex);
int SDL_LockMutex(SDL_mutex* mutex);
int SDL_UnlockMutex(SDL_mutex* mutex);

SDL_sem* SDL_CreateSemaphore(Uint32 initial_value);
void SDL_DestroySemaphore(SDL_sem* sem);
int SDL_SemWait(SDL_sem* sem);
int SDL_SemPost(SDL_sem* sem);

#endif
