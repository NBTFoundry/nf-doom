#ifndef NBT_DOOM_LIFECYCLE_HPP
#define NBT_DOOM_LIFECYCLE_HPP

#include <stdint.h>

namespace nbt_doom {

enum TurnSensitivity { kTurnLow = 0, kTurnMedium = 1, kTurnHigh = 2 };

struct AppSettings {
  TurnSensitivity turn_sens;
  bool invert_turn;
  int brightness; /* 0..4 */
  bool always_run;
  bool sound;
  bool show_fps;
  bool debug_log;
};

void settings_defaults(AppSettings* out);
void settings_apply_kv(AppSettings* s, const char* key, const char* value);
void settings_load_env(AppSettings* s);
bool settings_load_snapshot_file(const char* path, AppSettings* s);

class Lifecycle {
 public:
  Lifecycle();
  ~Lifecycle();

  /* Parses --ready-fd / --lease-fd and acquires F_RDLCK on leaseFd if provided.
   * Returns 0 on valid args, 1 on lease error, 2 on usage error. */
  int init(int argc, char** argv);
  void close_fds();

  bool is_check_mode() const { return check_mode_; }
  bool is_user_mode() const { return user_mode_; }
  int surface_id() const { return surface_id_; }
  int gamepad_fd() const { return gamepad_fd_; }
  int controller_fd() const { return controller_fd_; }
  int status_fd() const { return status_fd_; }
  bool can_show() const { return can_show_; }
  const AppSettings& settings() const { return settings_; }

  bool send_ready();
  void send_status(uint64_t now_ms, const char* text);
  void tick(uint64_t now_ms, const char* status_text, bool* settings_changed);

  bool stopping() const;
  bool consume_show_requested();
  void request_stop();

 private:
  Lifecycle(const Lifecycle&);
  Lifecycle& operator=(const Lifecycle&);

  int ready_fd_;
  int lease_fd_;
  int status_fd_;
  int controller_fd_;
  int gamepad_fd_;
  int surface_id_;
  bool check_mode_;
  bool user_mode_;
  bool can_show_;
  bool has_live_settings_;
  char settings_file_[512];
  uint64_t last_heartbeat_ms_;
  AppSettings settings_;
};

uint64_t monotonic_ms();

}  // namespace nbt_doom

#endif /* NBT_DOOM_LIFECYCLE_HPP */
