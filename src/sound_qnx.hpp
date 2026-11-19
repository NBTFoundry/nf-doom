#ifndef NBT_DOOM_SOUND_QNX_HPP
#define NBT_DOOM_SOUND_QNX_HPP

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_route_qnx.hpp"

namespace nbt_doom {

static const int kMaxSoundChannels = 16;
static const int kDefaultSampleRate = 22050;
static const int kAudioFragFrames = 512;

int clamp_qnx_rate(int preferred, int min_rate, int max_rate);
int choose_qnx_frag_size(int preferred_bytes, int min_frag, int max_frag);

typedef void (*MusicRenderFn)(void* dest_s16_stereo, unsigned nsamp);

struct SoundSlot {
  bool active;
  const uint8_t* samples8;
  const int16_t* samples16;
  uint32_t length;
  int sample_rate;
  uint64_t pos_fp;
  uint32_t step_fp;
  int left_vol;
  int right_vol;
  uint64_t start_seq;
};

class SoundMixer {
 public:
  SoundMixer();
  ~SoundMixer();

  bool init(int sample_rate, bool open_audio_device);
  void shutdown();

  void set_muted(bool muted);
  bool is_muted() const;
  bool audio_active() const;
  int sample_rate() const { return sample_rate_; }

  void set_music_renderer(MusicRenderFn fn);
  MusicRenderFn music_renderer() const;

  void lock();
  void unlock();

  int start_channel(int preferred_slot, const void* pcm_data, uint32_t num_samples, int src_rate,
                    int bits, int left_vol, int right_vol, uint32_t step_fp);
  void update_channel(int slot, int left_vol, int right_vol, uint32_t step_fp);
  void stop_channel(int slot);
  void stop_all_channels();
  bool is_channel_playing(int slot) const;
  bool any_channel_playing() const;

  /* Mixes `num_frames` stereo S16 samples into `out_s16_lr` (2 * num_frames int16_t values).
   * Returns count of active channels during this mix. */
  int mix_frames(int16_t* out_s16_lr, int num_frames);

 private:
  SoundMixer(const SoundMixer&);
  SoundMixer& operator=(const SoundMixer&);

  static void* thread_entry(void* arg);
  void playback_loop();
  bool open_audio();
  void close_audio();

  mutable pthread_mutex_t mu_;
  bool initialized_;
  bool muted_;
  int sample_rate_;
  int frag_frames_;
  uint64_t seq_;
  MusicRenderFn music_fn_;
  SoundSlot slots_[kMaxSoundChannels];

  void* dl_handle_;
  void* pcm_handle_;
  bool thread_running_;
  volatile bool stop_requested_;
  pthread_t thread_;
  AudioRoute route_;
};

}  // namespace nbt_doom

#endif /* NBT_DOOM_SOUND_QNX_HPP */
