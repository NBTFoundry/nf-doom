#ifndef NBT_DOOM_VIDEO_HPP
#define NBT_DOOM_VIDEO_HPP

#include "doom_settings.hpp"
#include "nbt/app/video.hpp"

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
 * viewport mapping and gamma from the brightness setting. */
class DoomVideo {
 public:
  DoomVideo();
  ~DoomVideo();

  bool init(int surface_id, const AppSettings& settings);
  void apply_settings(const AppSettings& settings);
  void begin_frame() {}
  void swap() { video_.swap(); }
  void shutdown();

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

  nbt::app::VideoEgl video_;
  bool batch_ready_;
  int log_w_;
  int log_h_;
};

}  // namespace nbt_doom

#endif /* NBT_DOOM_VIDEO_HPP */
