// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NBT_DOOM_VIDEO_HPP
#define NBT_DOOM_VIDEO_HPP

#include <stdint.h>

#include "doom_settings.hpp"
#include "nbt/app/video.hpp"
#include "nbt/frontend/overlay.hpp"

namespace nbt_doom {

/* The host keyboard as Doom controls (desktop runs only). */
struct HostInputState {
  int turn_delta;
  bool forward;
  bool backward;
  bool strafe_left;
  bool strafe_right;
  bool fire;
  bool use_tap;
  bool escape_tap;
  bool weapon_next_tap;
  bool enter_tap;
  bool backspace_tap;
  bool menu_up_tap;
  bool menu_down_tap;
  bool menu_left_tap;
  bool menu_right_tap;
  bool closed;
};

/* PrBoom+'s render size for a window: the window itself, at least 320x200. */
void compute_logical_resolution(int win_w, int win_h, int* out_w, int* out_h);

/* The SDK's window with Doom's rendering set-up: the GLES2 shim's batch,
 * viewport mapping and gamma from the brightness setting; the front end's
 * overlay over each frame for notices and the Show FPS counter. */
class DoomVideo {
 public:
  DoomVideo();
  ~DoomVideo();

  bool init(int surface_id, const AppSettings& settings);
  void apply_settings(const AppSettings& settings);
  void begin_frame() {}
  /* The overlay over Doom's frame, then the swap. */
  void swap();
  void shutdown();
  /* A line at the bottom of the screen for 3 seconds (a pad plugged in). */
  void notice(const char* text, uint64_t now_ms);

  void poll_host_events(HostInputState* out);

  bool ready() const { return video_.ready() && batch_ready_; }
  int window_width() const { return video_.window_width(); }
  int window_height() const { return video_.window_height(); }
  int logical_width() const { return log_w_; }
  int logical_height() const { return log_h_; }

 private:
  DoomVideo(const DoomVideo&);
  DoomVideo& operator=(const DoomVideo&);

  void update_viewport_mapping(const AppSettings& settings);
  void draw_overlay(uint64_t now_ms);

  nbt::app::VideoEgl video_;
  nbt::frontend::Overlay overlay_;
  bool batch_ready_;
  bool overlay_ready_;
  bool show_fps_;
  int log_w_;
  int log_h_;
  unsigned frames_, fps_;
  uint64_t fps_since_ms_;
  char notice_[96];
  uint64_t notice_until_ms_;
};

}  // namespace nbt_doom

#endif /* NBT_DOOM_VIDEO_HPP */
