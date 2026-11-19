#ifndef NBT_DOOM_VIDEO_EGL_HPP
#define NBT_DOOM_VIDEO_EGL_HPP

#include <EGL/egl.h>
#include <stdint.h>

#include "lifecycle.hpp"
#include "video_qnx.hpp"

namespace nbt_doom {

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

void compute_logical_resolution(int win_w, int win_h, int* out_w, int* out_h);

class VideoEgl {
 public:
  VideoEgl();
  ~VideoEgl();

  bool init(int surface_id, const AppSettings& settings);
  void apply_settings(const AppSettings& settings);
  void begin_frame();
  void swap();
  void shutdown();

  void poll_host_events(HostInputState* out);

  bool ready() const { return ready_; }
  int window_width() const { return win_w_; }
  int window_height() const { return win_h_; }
  int logical_width() const { return log_w_; }
  int logical_height() const { return log_h_; }

 private:
  VideoEgl(const VideoEgl&);
  VideoEgl& operator=(const VideoEgl&);

  bool open_x11_window();
  bool init_x11_egl();
  void update_viewport_mapping(const AppSettings& settings);

  bool ready_;
  int surface_id_;
  int win_w_;
  int win_h_;
  int log_w_;
  int log_h_;

  EGLDisplay dpy_;
  EGLSurface surf_;
  EGLContext ctx_;

#ifdef __QNXNTO__
  QnxVideoState qnx_;
#else
  void* x_dpy_;
  uintptr_t x_win_;
#endif
};

}  // namespace nbt_doom

#endif /* NBT_DOOM_VIDEO_EGL_HPP */
