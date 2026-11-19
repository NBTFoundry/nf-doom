#ifndef NBT_SDL_AUDIO_H
#define NBT_SDL_AUDIO_H

#define AUDIO_S16SYS 0x8010

typedef struct SDL_AudioSpec {
  int freq;
  Uint16 format;
  Uint8 channels;
  Uint8 silence;
  Uint16 samples;
  Uint16 padding;
  Uint32 size;
  void (*callback)(void* userdata, Uint8* stream, int len);
  void* userdata;
} SDL_AudioSpec;

#endif
