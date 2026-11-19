#ifndef NBT_DOOM_AUDIO_ROUTE_QNX_HPP
#define NBT_DOOM_AUDIO_ROUTE_QNX_HPP

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

namespace nbt_doom {

/* Manages ConnectionMaster routing for QNX dev-libasound (A2I input_stereo_3,
 * connection 0xb8 = eSRC_BROWSER -> eSNK_SPEAKERS).
 * Serialized so at most one audioconn child runs at a time, with LD_PRELOAD
 * stripped to avoid loading the HMI graphics stack. */
class AudioRoute {
 public:
  AudioRoute();
  ~AudioRoute();

  bool configure();
  void start();
  void set_wanted(bool wanted);
  void stop();

  bool configured() const;
  bool built() const;
  uint32_t conn_id() const { return conn_id_; }
  const char* binary_path() const { return path_; }

  /* Exposed for unit tests. */
  static int build_child_env(char** const src_env, char* out[], size_t max_out);
  static uint32_t parse_conn_env(const char* val, uint32_t fallback);
  /* The connection named in <data_dir>/audio-conn (one number, e.g. 0x9a), or
   * `fallback`: a test hook for the car's other audio sources, whose minimum
   * volume may differ from the browser source's 25. Regular files only. */
  static uint32_t conn_file(const char* data_dir, uint32_t fallback);

 private:
  AudioRoute(const AudioRoute&);
  AudioRoute& operator=(const AudioRoute&);

  static void* worker_entry(void* arg);
  void worker_loop();
  bool sleep_ms_interruptible_locked(unsigned ms);
  int run_audioconn(const char* path, char op, uint32_t conn_id, char* out_buf, size_t out_size);

  mutable pthread_mutex_t mu_;
  pthread_cond_t cond_;
  pthread_t thread_;
  char path_[128];
  uint32_t conn_id_;
  bool configured_;
  bool thread_running_;
  bool stop_requested_;
  bool wanted_;
  bool built_;
  int fail_streak_;
};

}  // namespace nbt_doom

#endif /* NBT_DOOM_AUDIO_ROUTE_QNX_HPP */
