#ifndef NBT_DOOM_VIDEO_QNX_HPP
#define NBT_DOOM_VIDEO_QNX_HPP

#include <EGL/egl.h>

namespace nbt_doom {

struct QnxVideoState {
  void* screen_ctx;
  void* screen_win;
  void* kd_lib;
  void* kd_win;
  EGLDisplay dpy;
  EGLSurface surf;
  EGLContext ctx;
  int win_w;
  int win_h;
};

#ifdef __QNXNTO__
bool qnx_video_open(int surface_id, QnxVideoState* out);
void qnx_video_close(QnxVideoState* state);
#endif

}  // namespace nbt_doom

#endif /* NBT_DOOM_VIDEO_QNX_HPP */
