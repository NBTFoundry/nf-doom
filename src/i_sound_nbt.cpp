#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbt_log.hpp"
#include "nbt_platform.hpp"
#include "sound_qnx.hpp"

extern "C" {
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_main.h"
#include "i_sound.h"
#include "i_system.h"
#include "lprintf.h"
#include "m_misc.h"
#include "memio.h"
#include "mus2mid.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "MUSIC/musicplayer.h"
#include "MUSIC/oplplayer.h"
}

namespace {

struct SfxPcmCache {
  uint8_t* data;
  uint32_t samples;
  int sample_rate;
  int bits;
};

SfxPcmCache* g_sfx_cache = NULL;
int g_sfx_cache_count = 0;

bool g_sound_inited = false;
bool g_music_inited = false;
const void* g_music_handle = NULL;
void* g_song_buf = NULL;
int g_music_looping = 1;
int g_music_volume = 15;
int g_steptable[256];

nbt_doom::SoundMixer* get_mixer() {
  nbt_doom::PlatformState* st = nbt_doom::platform_state();
  return st ? st->sound : NULL;
}

uint16_t read_le16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t read_le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

bool ensure_sfx_decoded(int id, SfxPcmCache* out) {
  if (id < 0 || id >= NUMSFX) return false;
  if (!g_sfx_cache || g_sfx_cache_count < NUMSFX) {
    SfxPcmCache* nc = static_cast<SfxPcmCache*>(
        realloc(g_sfx_cache, static_cast<size_t>(NUMSFX) * sizeof(SfxPcmCache)));
    if (!nc) return false;
    if (NUMSFX > g_sfx_cache_count) {
      memset(nc + g_sfx_cache_count, 0,
             static_cast<size_t>(NUMSFX - g_sfx_cache_count) * sizeof(SfxPcmCache));
    }
    g_sfx_cache = nc;
    g_sfx_cache_count = NUMSFX;
  }

  SfxPcmCache& c = g_sfx_cache[id];
  if (c.data) {
    *out = c;
    return true;
  }

  int lump = S_sfx[id].lumpnum;
  if (lump < 0) return false;
  size_t raw_len = static_cast<size_t>(W_LumpLength(lump));
  if (raw_len <= 8) return false;

  const uint8_t* raw = static_cast<const uint8_t*>(W_LockLumpNum(lump));
  if (!raw) return false;

  /* Check for RIFF/WAVE vs Doom DMX SFX format (0x0003, u16 rate, u32 len). */
  const uint8_t* pcm_src = NULL;
  uint32_t pcm_bytes = 0;
  uint32_t sample_rate = 11025;
  int bits = 8;

  if (raw_len >= 44 && memcmp(raw, "RIFF", 4) == 0 && memcmp(raw + 8, "WAVE", 4) == 0) {
    size_t pos = 12;
    uint16_t channels = 1;
    uint16_t bps = 8;
    while (pos + 8 <= raw_len) {
      uint32_t chunk_sz = read_le32(raw + pos + 4);
      if (pos + 8 + chunk_sz > raw_len) break;
      if (memcmp(raw + pos, "fmt ", 4) == 0 && chunk_sz >= 16) {
        channels = read_le16(raw + pos + 10);
        sample_rate = read_le32(raw + pos + 12);
        bps = read_le16(raw + pos + 22);
      } else if (memcmp(raw + pos, "data", 4) == 0) {
        pcm_src = raw + pos + 8;
        pcm_bytes = chunk_sz;
        break;
      }
      pos += 8 + chunk_sz + (chunk_sz & 1u);
    }
    if (channels == 1 && (bps == 8 || bps == 16))
      bits = bps;
    else
      pcm_src = NULL;
  } else if (read_le16(raw) == 3 && raw_len > 24) {
    sample_rate = read_le16(raw + 2);
    uint32_t dmx_len = read_le32(raw + 4);
    if (sample_rate < 4000 || sample_rate > 48000) sample_rate = 11025;
    size_t avail = raw_len - 8;
    if (dmx_len > avail) dmx_len = static_cast<uint32_t>(avail);
    if (dmx_len > 32) {
      pcm_src = raw + 24; /* skip 8-byte header + 16-byte DMX pre-pad */
      pcm_bytes = dmx_len - 32;
    } else if (avail > 0) {
      pcm_src = raw + 8;
      pcm_bytes = static_cast<uint32_t>(avail);
    }
    bits = 8;
  }

  if (pcm_src && pcm_bytes > 0) {
    c.data = static_cast<uint8_t*>(malloc(pcm_bytes));
    if (c.data) {
      memcpy(c.data, pcm_src, pcm_bytes);
      c.bits = bits;
      c.sample_rate = static_cast<int>(sample_rate);
      c.samples = (bits == 16) ? (pcm_bytes / 2u) : pcm_bytes;
    }
  }
  W_UnlockLumpNum(lump);

  if (!c.data) return false;
  *out = c;
  return true;
}

void calc_volumes(int vol, int sep, int* left, int* right) {
  int s = sep + 1;
  int l = vol - ((vol * s * s) >> 16);
  s -= 257;
  int r = vol - ((vol * s * s) >> 16);
  if (l < 0) l = 0;
  if (l > 127) l = 127;
  if (r < 0) r = 0;
  if (r > 127) r = 127;
  *left = l;
  *right = r;
}

void render_music_cb(void* dest, unsigned nsamp) {
  if (g_music_inited && g_music_handle && opl_synth_player.render) {
    opl_synth_player.render(dest, nsamp);
  } else {
    memset(dest, 0, static_cast<size_t>(nsamp) * 4);
  }
}

void free_song_buf(void) {
  if (g_song_buf) {
    free(g_song_buf);
    g_song_buf = NULL;
  }
}

}  // namespace

extern "C" {

int snd_pcspeaker = 0;
int lowpass_filter = 0;
int snd_card = 1;
int mus_card = 1;
int snd_samplerate = nbt_doom::kDefaultSampleRate;
int snd_samplecount = nbt_doom::kAudioFragFrames;
int use_experimental_music = 1;

const char* snd_soundfont = "";
const char* snd_mididev = "";
char music_player_order[6][200] = {"opl2 synth player", "", "", "", "", ""};

int mus_fluidsynth_chorus = 0;
int mus_fluidsynth_reverb = 0;
int mus_fluidsynth_gain = 50;
int mus_opl_gain = 50;
const char* mus_portmidi_reset_type = "none";
int mus_portmidi_reset_delay = 0;
int mus_portmidi_filter_sysex = 1;
int mus_portmidi_reverb_level = 0;
int mus_portmidi_chorus_level = 0;

const char* snd_midiplayer = "opl2";
const char* midiplayers[] = {"sdl", "fluidsynth", "opl2", "portmidi", "alsa", NULL};

void I_SetChannels(void) {
  int* mid = g_steptable + 128;
  for (int i = -128; i < 128; i++) {
    mid[i] = static_cast<int>(pow(1.2, static_cast<double>(i) / 64.0) * 65536.0);
  }
}

void I_InitSound(void) {
  if (g_sound_inited) return;
  snd_samplerate = nbt_doom::kDefaultSampleRate;
  snd_samplecount = nbt_doom::kAudioFragFrames;
  I_SetChannels();

  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) {
    mixer->init(snd_samplerate, true);
    const char* no_audio = getenv("NBT_NO_AUDIO");
    if (!mixer->audio_active() && !(no_audio && no_audio[0] == '1')) {
      /* No device (1.0.5 in the car: libasound did not load, then a crash
       * right after S_Init). Sound off, as the Sound setting's -nosound,
       * which runs in the car. */
      nbt_doom::nbt_log("no audio device; sound off");
      mixer->shutdown();
      nosfxparm = nomusicparm = true;
      return;
    }
    snd_samplerate = mixer->sample_rate();
    nbt_doom::nbt_debug("I_InitSound rate=%d frag=%d", snd_samplerate, snd_samplecount);
    I_InitMusic();
  }
  I_AtExit(I_ShutdownSound, true);
  g_sound_inited = true;
}

void I_ShutdownSound(void) {
  if (!g_sound_inited) return;
  nbt_doom::nbt_debug("I_ShutdownSound");
  I_ShutdownMusic();
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) {
    mixer->shutdown();
  }
  if (g_sfx_cache) {
    for (int i = 0; i < g_sfx_cache_count; i++) {
      free(g_sfx_cache[i].data);
    }
    free(g_sfx_cache);
    g_sfx_cache = NULL;
    g_sfx_cache_count = 0;
  }
  g_sound_inited = false;
}

int I_GetSfxLumpNum(sfxinfo_t* sfx) {
  if (!sfx || !sfx->name || !sfx->name[0]) return -1;
  char namebuf[16];
  snprintf(namebuf, sizeof(namebuf), "%s%s", snd_pcspeaker ? "dp" : "ds", sfx->name);
  return W_CheckNumForName(namebuf);
}

int I_StartSound(int id, int channel, int vol, int sep, int pitch, int priority) {
  (void)priority;
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (!mixer || !g_sound_inited || snd_pcspeaker) return -1;
  if (channel < 0 || channel >= nbt_doom::kMaxSoundChannels) return -1;

  SfxPcmCache pcm;
  if (!ensure_sfx_decoded(id, &pcm)) return -1;

  int lvol = 0, rvol = 0;
  calc_volumes(vol, sep, &lvol, &rvol);
  int pidx = (pitch < 0) ? 0 : (pitch > 255 ? 255 : pitch);
  uint32_t step =
      pitched_sounds
          ? static_cast<uint32_t>((static_cast<uint64_t>(pcm.sample_rate) * g_steptable[pidx]) /
                                  snd_samplerate)
          : static_cast<uint32_t>((static_cast<uint64_t>(pcm.sample_rate) << 16) / snd_samplerate);
  return mixer->start_channel(channel, pcm.data, pcm.samples, pcm.sample_rate, pcm.bits, lvol, rvol,
                              step);
}

void I_StopSound(int handle) {
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) mixer->stop_channel(handle);
}

dboolean I_SoundIsPlaying(int handle) {
  nbt_doom::SoundMixer* mixer = get_mixer();
  return (mixer && mixer->is_channel_playing(handle)) ? true : false;
}

dboolean I_AnySoundStillPlaying(void) {
  nbt_doom::SoundMixer* mixer = get_mixer();
  return (mixer && mixer->any_channel_playing()) ? true : false;
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch) {
  (void)pitch;
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (!mixer) return;
  int lvol = 0, rvol = 0;
  calc_volumes(vol, sep, &lvol, &rvol);
  mixer->update_channel(handle, lvol, rvol, 0);
}

void I_ResampleStream(void* dest, unsigned nsamp, void (*proc)(void* dest, unsigned nsamp),
                      unsigned sratein, unsigned srateout) {
  (void)sratein;
  (void)srateout;
  if (proc) proc(dest, nsamp);
}

void I_InitMusic(void) {
  if (g_music_inited || nomusicparm) return;
  if (opl_synth_player.init && opl_synth_player.init(snd_samplerate)) {
    g_music_inited = true;
    nbt_doom::SoundMixer* mixer = get_mixer();
    if (mixer) mixer->set_music_renderer(&render_music_cb);
    I_AtExit(I_ShutdownMusic, true);
  }
}

void I_ShutdownMusic(void) {
  if (!g_music_inited) return;
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) {
    mixer->lock();
    if (g_music_handle) {
      if (opl_synth_player.stop) opl_synth_player.stop();
      if (opl_synth_player.unregistersong) opl_synth_player.unregistersong(g_music_handle);
      g_music_handle = NULL;
    }
    free_song_buf();
    mixer->unlock();
    mixer->set_music_renderer(NULL);
  }
  if (opl_synth_player.shutdown) opl_synth_player.shutdown();
  g_music_inited = false;
}

void I_SetMusicVolume(int volume) {
  g_music_volume = volume;
  if (!g_music_inited || !opl_synth_player.setvolume) return;
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) mixer->lock();
  opl_synth_player.setvolume(volume);
  if (mixer) mixer->unlock();
}

void I_PauseSong(int handle) {
  (void)handle;
  if (!g_music_inited || !opl_synth_player.pause) return;
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) mixer->lock();
  opl_synth_player.pause();
  if (mixer) mixer->unlock();
}

void I_ResumeSong(int handle) {
  (void)handle;
  if (!g_music_inited || !opl_synth_player.resume) return;
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) mixer->lock();
  opl_synth_player.resume();
  if (mixer) mixer->unlock();
}

int I_RegisterSong(const void* data, size_t len) {
  if (!g_music_inited || !data || len < 4) return 0;
  I_UnRegisterSong(1);

  void* owned = NULL;
  size_t owned_len = 0;

  if (memcmp(data, "MUS\x1a", 4) == 0) {
    MEMFILE* in = mem_fopen_read(const_cast<void*>(data), len);
    MEMFILE* out = mem_fopen_write();
    if (in && out && mus2mid(in, out) == 0) {
      void* buf = NULL;
      size_t blen = 0;
      mem_get_buf(out, &buf, &blen);
      if (buf && blen > 0) {
        owned = malloc(blen);
        if (owned) {
          memcpy(owned, buf, blen);
          owned_len = blen;
        }
      }
    }
    if (in) mem_fclose(in);
    if (out) mem_fclose(out);
  } else {
    owned = malloc(len);
    if (owned) {
      memcpy(owned, data, len);
      owned_len = len;
    }
  }

  if (!owned || owned_len == 0) {
    free(owned);
    return 0;
  }

  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) mixer->lock();
  const void* h = opl_synth_player.registersong(owned, owned_len);
  if (h) {
    g_song_buf = owned;
    g_music_handle = h;
    opl_synth_player.setvolume(g_music_volume);
  } else {
    free(owned);
  }
  if (mixer) mixer->unlock();
  return h ? 1 : 0;
}

int I_RegisterMusic(const char* filename, musicinfo_t* music) {
  (void)filename;
  (void)music;
  return 1; /* external music files unused on head unit */
}

void I_PlaySong(int handle, int looping) {
  (void)handle;
  g_music_looping = looping;
  if (!g_music_inited || !g_music_handle || !opl_synth_player.play) return;
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) mixer->lock();
  opl_synth_player.play(g_music_handle, looping);
  opl_synth_player.setvolume(g_music_volume);
  if (mixer) mixer->unlock();
}

void I_StopSong(int handle) {
  (void)handle;
  if (!g_music_inited || !opl_synth_player.stop) return;
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) mixer->lock();
  opl_synth_player.stop();
  if (mixer) mixer->unlock();
}

void I_UnRegisterSong(int handle) {
  (void)handle;
  if (!g_music_inited) return;
  nbt_doom::SoundMixer* mixer = get_mixer();
  if (mixer) mixer->lock();
  if (g_music_handle) {
    if (opl_synth_player.stop) opl_synth_player.stop();
    if (opl_synth_player.unregistersong) opl_synth_player.unregistersong(g_music_handle);
    g_music_handle = NULL;
  }
  free_song_buf();
  if (mixer) mixer->unlock();
}

void M_ChangeMIDIPlayer(void) {}

int I_PCS_StartSound(int id, int channel, int vol, int sep, int pitch, int priority) {
  (void)id;
  (void)channel;
  (void)vol;
  (void)sep;
  (void)pitch;
  (void)priority;
  return -1;
}
void I_PCS_StopSound(int handle) {
  (void)handle;
}
int I_PCS_SoundIsPlaying(int handle) {
  (void)handle;
  return 0;
}
void I_PCS_InitSound(void) {}

}  // extern "C"
