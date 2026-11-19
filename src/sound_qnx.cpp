#include "sound_qnx.hpp"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __QNXNTO__
#include <sys/asoundlib.h>
#endif

#include "nbt_log.hpp"

namespace nbt_doom {

namespace {

#ifdef __QNXNTO__

typedef int (*QnxOpenPreferredFn)(snd_pcm_t** handle, int* card, int* device, int mode);
typedef int (*QnxPluginInfoFn)(snd_pcm_t* handle, snd_pcm_channel_info_t* info);
typedef int (*QnxPluginParamsFn)(snd_pcm_t* handle, snd_pcm_channel_params_t* params);
typedef int (*QnxPluginPrepareFn)(snd_pcm_t* handle, int channel);
typedef int (*QnxPluginStatusFn)(snd_pcm_t* handle, snd_pcm_channel_status_t* status);
typedef ssize_t (*QnxPluginWriteFn)(snd_pcm_t* handle, const void* buffer, size_t size);
typedef int (*QnxPluginFlushFn)(snd_pcm_t* handle, int channel);
typedef int (*QnxCloseFn)(snd_pcm_t* handle);

struct AudioApi {
  QnxOpenPreferredFn open_preferred;
  QnxPluginInfoFn plugin_info;
  QnxPluginParamsFn plugin_params;
  QnxPluginPrepareFn plugin_prepare;
  QnxPluginStatusFn plugin_status;
  QnxPluginWriteFn plugin_write;
  QnxPluginFlushFn plugin_flush;
  QnxCloseFn close;
};

#else

enum { kAlsaPcmStreamPlayback = 0, kAlsaPcmFormatS16Le = 2, kAlsaPcmAccessRwInterleaved = 3 };

typedef int (*AlsaOpenFn)(void** pcm, const char* name, int stream, int mode);
typedef int (*AlsaSetParamsFn)(void* pcm, int format, int access, unsigned channels, unsigned rate,
                               int soft_resample, unsigned latency_us);
typedef long (*AlsaWriteiFn)(void* pcm, const void* buffer, unsigned long frames);
typedef int (*AlsaRecoverFn)(void* pcm, int err, int silent);
typedef int (*AlsaDropFn)(void* pcm);
typedef int (*AlsaCloseFn)(void* pcm);

struct AudioApi {
  AlsaOpenFn open;
  AlsaSetParamsFn set_params;
  AlsaWriteiFn writei;
  AlsaRecoverFn recover;
  AlsaDropFn drop;
  AlsaCloseFn close;
};

#endif

AudioApi g_audio_api;

int clamp_s16(int32_t v) {
  if (v < -32768) return -32768;
  if (v > 32767) return 32767;
  return v;
}

}  // namespace

int clamp_qnx_rate(int preferred, int min_rate, int max_rate) {
  int rate = (preferred >= 8000 && preferred <= 48000) ? preferred : kDefaultSampleRate;
  if (min_rate > 0 && rate < min_rate) rate = min_rate;
  if (max_rate > 0 && rate > max_rate) rate = max_rate;
  return rate;
}

int choose_qnx_frag_size(int preferred_bytes, int min_frag, int max_frag) {
  int lo = (min_frag > 256) ? min_frag : 256;
  int hi = (max_frag > 0 && max_frag < 8192) ? max_frag : 8192;
  if (hi < lo) hi = lo;
  int target = (preferred_bytes > 0) ? preferred_bytes : (kAudioFragFrames * 4);
  if (target < lo) target = lo;
  if (target > hi) target = hi;

  int pow2 = 256;
  while (pow2 * 2 <= target && pow2 * 2 <= hi) {
    pow2 *= 2;
  }
  if (pow2 < lo) {
    int up = 256;
    while (up < lo && up < 8192) up *= 2;
    if (up <= hi) return up;
    return lo;
  }
  return pow2;
}

SoundMixer::SoundMixer()
    : initialized_(false),
      muted_(false),
      sample_rate_(kDefaultSampleRate),
      frag_frames_(kAudioFragFrames),
      seq_(0),
      music_fn_(NULL),
      dl_handle_(NULL),
      pcm_handle_(NULL),
      thread_running_(false),
      stop_requested_(false) {
  pthread_mutex_init(&mu_, NULL);
  memset(slots_, 0, sizeof(slots_));
}

SoundMixer::~SoundMixer() {
  shutdown();
  pthread_mutex_destroy(&mu_);
}

bool SoundMixer::init(int sample_rate, bool open_audio_device) {
  if (initialized_) return true;
  sample_rate_ = (sample_rate >= 8000 && sample_rate <= 48000) ? sample_rate : kDefaultSampleRate;
  stop_all_channels();
  stop_requested_ = false;
  initialized_ = true;

  const char* no_audio = getenv("NBT_NO_AUDIO");
  if (open_audio_device && !(no_audio && no_audio[0] == '1')) {
    if (open_audio()) {
      if (pthread_create(&thread_, NULL, &SoundMixer::thread_entry, this) == 0) {
        thread_running_ = true;
        if (route_.configure()) {
          route_.start();
          route_.set_wanted(!is_muted());
        }
      } else {
        close_audio();
      }
    }
  }
  return true;
}

void SoundMixer::shutdown() {
  if (!initialized_) return;
  stop_requested_ = true;
  if (thread_running_) {
    pthread_join(thread_, NULL);
    thread_running_ = false;
  }
  route_.stop();
  close_audio();
  stop_all_channels();
  initialized_ = false;
}

void SoundMixer::set_muted(bool muted) {
  pthread_mutex_lock(&mu_);
  muted_ = muted;
  pthread_mutex_unlock(&mu_);
  if (audio_active()) {
    route_.set_wanted(!muted);
  }
}

bool SoundMixer::is_muted() const {
  pthread_mutex_lock(&mu_);
  bool m = muted_;
  pthread_mutex_unlock(&mu_);
  return m;
}

bool SoundMixer::audio_active() const {
  return initialized_ && pcm_handle_ != NULL && thread_running_;
}

void SoundMixer::set_music_renderer(MusicRenderFn fn) {
  pthread_mutex_lock(&mu_);
  music_fn_ = fn;
  pthread_mutex_unlock(&mu_);
}

MusicRenderFn SoundMixer::music_renderer() const {
  pthread_mutex_lock(&mu_);
  MusicRenderFn fn = music_fn_;
  pthread_mutex_unlock(&mu_);
  return fn;
}

void SoundMixer::lock() {
  pthread_mutex_lock(&mu_);
}
void SoundMixer::unlock() {
  pthread_mutex_unlock(&mu_);
}

int SoundMixer::start_channel(int preferred_slot, const void* pcm_data, uint32_t num_samples,
                              int src_rate, int bits, int left_vol, int right_vol,
                              uint32_t step_fp) {
  if (!pcm_data || num_samples == 0 || src_rate <= 0) return -1;
  pthread_mutex_lock(&mu_);
  int slot = preferred_slot;
  if (slot < 0 || slot >= kMaxSoundChannels) {
    slot = 0;
    uint64_t oldest = ~0ULL;
    for (int i = 0; i < kMaxSoundChannels; i++) {
      if (!slots_[i].active) {
        slot = i;
        break;
      }
      if (slots_[i].start_seq < oldest) {
        oldest = slots_[i].start_seq;
        slot = i;
      }
    }
  }

  SoundSlot& s = slots_[slot];
  memset(&s, 0, sizeof(s));
  s.active = true;
  s.samples8 = (bits == 8) ? static_cast<const uint8_t*>(pcm_data) : NULL;
  s.samples16 = (bits == 16) ? static_cast<const int16_t*>(pcm_data) : NULL;
  if (!s.samples8 && !s.samples16) {
    s.active = false;
    pthread_mutex_unlock(&mu_);
    return -1;
  }
  s.length = num_samples;
  s.sample_rate = src_rate;
  s.pos_fp = 0;
  s.step_fp = step_fp ? step_fp : static_cast<uint32_t>(((uint64_t)src_rate << 16) / sample_rate_);
  s.left_vol = (left_vol < 0) ? 0 : (left_vol > 127 ? 127 : left_vol);
  s.right_vol = (right_vol < 0) ? 0 : (right_vol > 127 ? 127 : right_vol);
  s.start_seq = ++seq_;
  pthread_mutex_unlock(&mu_);
  return slot;
}

void SoundMixer::update_channel(int slot, int left_vol, int right_vol, uint32_t step_fp) {
  if (slot < 0 || slot >= kMaxSoundChannels) return;
  pthread_mutex_lock(&mu_);
  if (slots_[slot].active) {
    slots_[slot].left_vol = (left_vol < 0) ? 0 : (left_vol > 127 ? 127 : left_vol);
    slots_[slot].right_vol = (right_vol < 0) ? 0 : (right_vol > 127 ? 127 : right_vol);
    if (step_fp != 0) slots_[slot].step_fp = step_fp;
  }
  pthread_mutex_unlock(&mu_);
}

void SoundMixer::stop_channel(int slot) {
  if (slot < 0 || slot >= kMaxSoundChannels) return;
  pthread_mutex_lock(&mu_);
  slots_[slot].active = false;
  pthread_mutex_unlock(&mu_);
}

void SoundMixer::stop_all_channels() {
  pthread_mutex_lock(&mu_);
  for (int i = 0; i < kMaxSoundChannels; i++) slots_[i].active = false;
  pthread_mutex_unlock(&mu_);
}

bool SoundMixer::is_channel_playing(int slot) const {
  if (slot < 0 || slot >= kMaxSoundChannels) return false;
  pthread_mutex_lock(&mu_);
  bool active = slots_[slot].active;
  pthread_mutex_unlock(&mu_);
  return active;
}

bool SoundMixer::any_channel_playing() const {
  pthread_mutex_lock(&mu_);
  bool any = false;
  for (int i = 0; i < kMaxSoundChannels; i++) {
    if (slots_[i].active) {
      any = true;
      break;
    }
  }
  pthread_mutex_unlock(&mu_);
  return any;
}

int SoundMixer::mix_frames(int16_t* out_s16_lr, int num_frames) {
  if (!out_s16_lr || num_frames <= 0) return 0;

  pthread_mutex_lock(&mu_);
  if (muted_) {
    memset(out_s16_lr, 0, static_cast<size_t>(num_frames) * 2 * sizeof(int16_t));
    pthread_mutex_unlock(&mu_);
    return 0;
  }

  if (music_fn_) {
    music_fn_(out_s16_lr, static_cast<unsigned>(num_frames));
  } else {
    memset(out_s16_lr, 0, static_cast<size_t>(num_frames) * 2 * sizeof(int16_t));
  }

  int active_count = 0;
  for (int c = 0; c < kMaxSoundChannels; c++) {
    if (slots_[c].active) active_count++;
  }

  for (int f = 0; f < num_frames; f++) {
    int32_t acc_l = out_s16_lr[f * 2 + 0];
    int32_t acc_r = out_s16_lr[f * 2 + 1];

    for (int c = 0; c < kMaxSoundChannels; c++) {
      SoundSlot& s = slots_[c];
      if (!s.active) continue;
      uint32_t idx = static_cast<uint32_t>(s.pos_fp >> 16);
      if (idx >= s.length) {
        s.active = false;
        continue;
      }
      int32_t sample = 0;
      if (s.samples8) {
        sample = (static_cast<int32_t>(s.samples8[idx]) - 128) * 256;
      } else if (s.samples16) {
        sample = static_cast<int32_t>(s.samples16[idx]);
      }
      acc_l += (sample * s.left_vol) >> 8;
      acc_r += (sample * s.right_vol) >> 8;
      s.pos_fp += s.step_fp;
      if ((s.pos_fp >> 16) >= s.length) {
        s.active = false;
      }
    }

    out_s16_lr[f * 2 + 0] = static_cast<int16_t>(clamp_s16(acc_l));
    out_s16_lr[f * 2 + 1] = static_cast<int16_t>(clamp_s16(acc_r));
  }

  pthread_mutex_unlock(&mu_);
  return active_count;
}

bool SoundMixer::open_audio() {
#ifdef __QNXNTO__
  static const char* const kMmAlsa = "/opt/mm/lib/libasound/libasound.so";
  if (!dl_handle_) {
    dl_handle_ = dlopen(kMmAlsa, RTLD_LAZY);
    if (!dl_handle_) {
      if (access(kMmAlsa, F_OK) == 0) {
        nbt_log("qnx dlopen %s failed: %s", kMmAlsa, dlerror());
        return false;
      }
      dl_handle_ = dlopen("libasound.so.2", RTLD_LAZY);
    }
  }
  if (!dl_handle_) return false;

  g_audio_api.open_preferred =
      reinterpret_cast<QnxOpenPreferredFn>(dlsym(dl_handle_, "snd_pcm_open_preferred"));
  g_audio_api.plugin_info =
      reinterpret_cast<QnxPluginInfoFn>(dlsym(dl_handle_, "snd_pcm_plugin_info"));
  g_audio_api.plugin_params =
      reinterpret_cast<QnxPluginParamsFn>(dlsym(dl_handle_, "snd_pcm_plugin_params"));
  g_audio_api.plugin_prepare =
      reinterpret_cast<QnxPluginPrepareFn>(dlsym(dl_handle_, "snd_pcm_plugin_prepare"));
  g_audio_api.plugin_status =
      reinterpret_cast<QnxPluginStatusFn>(dlsym(dl_handle_, "snd_pcm_plugin_status"));
  g_audio_api.plugin_write =
      reinterpret_cast<QnxPluginWriteFn>(dlsym(dl_handle_, "snd_pcm_plugin_write"));
  g_audio_api.plugin_flush =
      reinterpret_cast<QnxPluginFlushFn>(dlsym(dl_handle_, "snd_pcm_plugin_flush"));
  g_audio_api.close = reinterpret_cast<QnxCloseFn>(dlsym(dl_handle_, "snd_pcm_close"));

  if (!g_audio_api.open_preferred || !g_audio_api.plugin_params || !g_audio_api.plugin_prepare ||
      !g_audio_api.plugin_write || !g_audio_api.close) {
    close_audio();
    return false;
  }

  int card = 0;
  int dev = 0;
  snd_pcm_t* pcm = NULL;
  if (g_audio_api.open_preferred(&pcm, &card, &dev, SND_PCM_OPEN_PLAYBACK) < 0 || !pcm) {
    nbt_log("qnx snd_pcm_open_preferred failed");
    close_audio();
    return false;
  }
  pcm_handle_ = pcm;

  int rate = sample_rate_;
  int frag_bytes = kAudioFragFrames * 4;
  if (g_audio_api.plugin_info) {
    snd_pcm_channel_info_t info;
    memset(&info, 0, sizeof(info));
    info.channel = SND_PCM_CHANNEL_PLAYBACK;
    if (g_audio_api.plugin_info(pcm, &info) == 0) {
      rate = clamp_qnx_rate(rate, info.min_rate, info.max_rate);
      frag_bytes = choose_qnx_frag_size(frag_bytes, info.min_fragment_size, info.max_fragment_size);
      nbt_log("qnx audio info rate=[%u..%u] frag=[%u..%u] -> rate=%d frag=%d", info.min_rate,
              info.max_rate, info.min_fragment_size, info.max_fragment_size, rate, frag_bytes);
    }
  } else {
    frag_bytes = choose_qnx_frag_size(frag_bytes, 0, 0);
  }

  static const int kFragsTries[3][2] = {{0, 5}, {0, 10}, {1, 8}};
  bool params_ok = false;
  unsigned int last_why = 0;
  for (int t = 0; t < 3; t++) {
    int try_frag = (kFragsTries[t][0] && frag_bytes <= 4096) ? (frag_bytes * 2) : frag_bytes;
    snd_pcm_channel_params_t params;
    memset(&params, 0, sizeof(params));
    params.channel = SND_PCM_CHANNEL_PLAYBACK;
    params.mode = SND_PCM_MODE_BLOCK;
    params.start_mode = SND_PCM_START_FULL;
    params.stop_mode = SND_PCM_STOP_STOP;
    params.buf.block.frag_size = try_frag;
    params.buf.block.frags_min = 1;
    params.buf.block.frags_max = kFragsTries[t][1];
    params.format.interleave = 1;
    params.format.format = SND_PCM_SFMT_S16_LE;
    params.format.rate = rate;
    params.format.voices = 2;
    if (g_audio_api.plugin_params(pcm, &params) >= 0) {
      if (params.buf.block.frag_size >= 256 && params.buf.block.frag_size <= 8192) {
        frag_bytes = params.buf.block.frag_size;
      }
      nbt_debug("qnx snd_pcm_plugin_params ok try=%d frag=%d frags=[%d..%d]", t,
                params.buf.block.frag_size, params.buf.block.frags_min, params.buf.block.frags_max);
      params_ok = true;
      break;
    }
    last_why = params.why_failed;
  }
  if (!params_ok || g_audio_api.plugin_prepare(pcm, SND_PCM_CHANNEL_PLAYBACK) < 0) {
    nbt_log("qnx snd_pcm_plugin_params/prepare failed why=%u", last_why);
    close_audio();
    return false;
  }
  sample_rate_ = rate;
  frag_frames_ = frag_bytes / 4;
  if (frag_frames_ < 64) frag_frames_ = 64;
  if (frag_frames_ > 2048) frag_frames_ = 2048;
  nbt_log("qnx audio ok card=%d dev=%d rate=%d frag_frames=%d", card, dev, sample_rate_,
          frag_frames_);
  return true;
#else
  dl_handle_ = dlopen("libasound.so.2", RTLD_LAZY);
  if (!dl_handle_) return false;
  g_audio_api.open = reinterpret_cast<AlsaOpenFn>(dlsym(dl_handle_, "snd_pcm_open"));
  g_audio_api.set_params =
      reinterpret_cast<AlsaSetParamsFn>(dlsym(dl_handle_, "snd_pcm_set_params"));
  g_audio_api.writei = reinterpret_cast<AlsaWriteiFn>(dlsym(dl_handle_, "snd_pcm_writei"));
  g_audio_api.recover = reinterpret_cast<AlsaRecoverFn>(dlsym(dl_handle_, "snd_pcm_recover"));
  g_audio_api.drop = reinterpret_cast<AlsaDropFn>(dlsym(dl_handle_, "snd_pcm_drop"));
  g_audio_api.close = reinterpret_cast<AlsaCloseFn>(dlsym(dl_handle_, "snd_pcm_close"));

  if (!g_audio_api.open || !g_audio_api.set_params || !g_audio_api.writei || !g_audio_api.close) {
    close_audio();
    return false;
  }

  if (g_audio_api.open(&pcm_handle_, "default", kAlsaPcmStreamPlayback, 0) < 0 || !pcm_handle_) {
    close_audio();
    return false;
  }

  if (g_audio_api.set_params(pcm_handle_, kAlsaPcmFormatS16Le, kAlsaPcmAccessRwInterleaved, 2,
                             static_cast<unsigned>(sample_rate_), 1, 50000) < 0) {
    close_audio();
    return false;
  }
  return true;
#endif
}

void SoundMixer::close_audio() {
  if (pcm_handle_) {
#ifdef __QNXNTO__
    snd_pcm_t* pcm = static_cast<snd_pcm_t*>(pcm_handle_);
    if (g_audio_api.plugin_flush) g_audio_api.plugin_flush(pcm, SND_PCM_CHANNEL_PLAYBACK);
    if (g_audio_api.close) g_audio_api.close(pcm);
#else
    if (g_audio_api.drop) g_audio_api.drop(pcm_handle_);
    if (g_audio_api.close) g_audio_api.close(pcm_handle_);
#endif
    pcm_handle_ = NULL;
  }
  memset(&g_audio_api, 0, sizeof(g_audio_api));
#ifndef __QNXNTO__
  if (dl_handle_) {
    dlclose(dl_handle_);
    dl_handle_ = NULL;
  }
#endif
}

void* SoundMixer::thread_entry(void* arg) {
  static_cast<SoundMixer*>(arg)->playback_loop();
  return NULL;
}

void SoundMixer::playback_loop() {
  const int frames = (frag_frames_ >= 64 && frag_frames_ <= 2048) ? frag_frames_ : kAudioFragFrames;
  int16_t* buf = static_cast<int16_t*>(calloc(static_cast<size_t>(frames * 2), sizeof(int16_t)));
  if (!buf) return;
#ifdef __QNXNTO__
  int consecutive_errors = 0;
  uint64_t writes_ok = 0;
  uint64_t writes_err = 0;
#endif
  while (!stop_requested_) {
    mix_frames(buf, frames);
#ifdef __QNXNTO__
    snd_pcm_t* pcm = static_cast<snd_pcm_t*>(pcm_handle_);
    const size_t bytes = static_cast<size_t>(frames) * 4u;
    ssize_t written = g_audio_api.plugin_write(pcm, buf, bytes);
    if (written == static_cast<ssize_t>(bytes)) {
      consecutive_errors = 0;
      if (++writes_ok == 1) {
        nbt_debug("qnx audio first write ok bytes=%u", static_cast<unsigned>(bytes));
      }
      continue;
    }
    if (stop_requested_) break;
    if (written <= 0) {
      ++writes_err;
      if (++consecutive_errors > 50) {
        nbt_log("qnx audio stopped after repeated write errors (total=%llu)",
                static_cast<unsigned long long>(writes_err));
        break;
      }
      bool need_prepare = true;
      int st_val = -1;
      if (g_audio_api.plugin_status) {
        snd_pcm_channel_status_t st;
        memset(&st, 0, sizeof(st));
        st.channel = SND_PCM_CHANNEL_PLAYBACK;
        if (g_audio_api.plugin_status(pcm, &st) == 0) {
          st_val = st.status;
          need_prepare =
              (st.status == SND_PCM_STATUS_UNDERRUN || st.status == SND_PCM_STATUS_READY ||
               st.status == SND_PCM_STATUS_ERROR || st.status == SND_PCM_STATUS_CHANGE);
        }
      }
      if (writes_err <= 5 || (writes_err % 50) == 0) {
        nbt_debug("qnx audio write rc=%ld status=%d prepare=%d err=%llu",
                  static_cast<long>(written), st_val, need_prepare ? 1 : 0,
                  static_cast<unsigned long long>(writes_err));
      }
      if (need_prepare) {
        g_audio_api.plugin_prepare(pcm, SND_PCM_CHANNEL_PLAYBACK);
      }
      usleep(10000);
    } else {
      consecutive_errors = 0;
    }
#else
    long rc = g_audio_api.writei(pcm_handle_, buf, static_cast<unsigned long>(frames));
    if (rc < 0 && !stop_requested_) {
      if (g_audio_api.recover) {
        g_audio_api.recover(pcm_handle_, static_cast<int>(rc), 1);
      } else {
        usleep(5000);
      }
    }
#endif
  }
  free(buf);
}

}  // namespace nbt_doom
