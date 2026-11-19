#include "video_egl.hpp"

#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbt/gles2/gles2_compat.h"
#include "nbt_log.hpp"

#ifndef __QNXNTO__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif

namespace nbt_doom {

namespace {

float brightness_to_gamma(int b) {
  static const float kTable[5] = {0.85f, 1.0f, 1.15f, 1.32f, 1.50f};
  if (b < 0) b = 0;
  if (b > 4) b = 4;
  return kTable[b];
}

}  // namespace

void compute_logical_resolution(int win_w, int win_h, int* out_w, int* out_h) {
  if (win_w <= 0 || win_h <= 0) {
    *out_w = 640;
    *out_h = 480;
    return;
  }
  int w = win_w;
  int h = win_h;
  if (h < 200) {
    h = 200;
    w = (h * win_w) / win_h;
  }
  if (w < 320) w = 320;
  w &= ~3;
  h &= ~1;
  *out_w = w;
  *out_h = h;
}

VideoEgl::VideoEgl()
    : ready_(false),
      surface_id_(0),
      win_w_(1280),
      win_h_(480),
      log_w_(640),
      log_h_(480),
      dpy_(EGL_NO_DISPLAY),
      surf_(EGL_NO_SURFACE),
      ctx_(EGL_NO_CONTEXT) {
#ifdef __QNXNTO__
  memset(&qnx_, 0, sizeof(qnx_));
  qnx_.dpy = EGL_NO_DISPLAY;
  qnx_.surf = EGL_NO_SURFACE;
  qnx_.ctx = EGL_NO_CONTEXT;
#else
  x_dpy_ = nullptr;
  x_win_ = 0;
#endif
}

VideoEgl::~VideoEgl() {
  shutdown();
}

#ifndef __QNXNTO__
bool VideoEgl::open_x11_window() {
  win_w_ = 1280;
  win_h_ = 480;
  Display* xdpy = XOpenDisplay(nullptr);
  if (!xdpy) {
    nbt_log("XOpenDisplay failed (set DISPLAY)");
    return false;
  }
  int screen = DefaultScreen(xdpy);
  Window root = RootWindow(xdpy, screen);
  Window xwin = XCreateSimpleWindow(xdpy, root, 0, 0, static_cast<unsigned>(win_w_),
                                    static_cast<unsigned>(win_h_), 0, 0, 0);
  XStoreName(xdpy, xwin, "NBT Doom (Host GLES2)");
  XSelectInput(xdpy, xwin, KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask);
  Atom wm_del = XInternAtom(xdpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(xdpy, xwin, &wm_del, 1);
  XMapWindow(xdpy, xwin);
  XFlush(xdpy);
  x_dpy_ = xdpy;
  x_win_ = static_cast<uintptr_t>(xwin);
  return true;
}

bool VideoEgl::init_x11_egl() {
  Display* xdpy = static_cast<Display*>(x_dpy_);
  typedef EGLDisplay(EGLAPIENTRY * FnGetPlatformDisplay)(EGLenum, void*, const EGLint*);
  FnGetPlatformDisplay get_plat =
      reinterpret_cast<FnGetPlatformDisplay>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
  static const EGLenum kEglPlatformX11 = 0x31D5;
  dpy_ = get_plat ? get_plat(kEglPlatformX11, xdpy, nullptr)
                  : eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(xdpy));
  if (dpy_ == EGL_NO_DISPLAY) dpy_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (dpy_ == EGL_NO_DISPLAY) {
    nbt_log("eglGetDisplay failed err=0x%x", eglGetError());
    return false;
  }
  if (!eglInitialize(dpy_, nullptr, nullptr)) {
    nbt_log("eglInitialize failed err=0x%x", eglGetError());
    return false;
  }
  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    nbt_log("eglBindAPI failed err=0x%x", eglGetError());
    return false;
  }

  static const EGLint kCfgAttrs[] = {EGL_SURFACE_TYPE,
                                     EGL_WINDOW_BIT,
                                     EGL_RENDERABLE_TYPE,
                                     EGL_OPENGL_ES2_BIT,
                                     EGL_RED_SIZE,
                                     8,
                                     EGL_GREEN_SIZE,
                                     8,
                                     EGL_BLUE_SIZE,
                                     8,
                                     EGL_DEPTH_SIZE,
                                     16,
                                     EGL_STENCIL_SIZE,
                                     8,
                                     EGL_NONE};
  EGLConfig cfg = nullptr;
  EGLint num_cfg = 0;
  if (!eglChooseConfig(dpy_, kCfgAttrs, &cfg, 1, &num_cfg) || num_cfg < 1) {
    nbt_log("eglChooseConfig failed err=0x%x", eglGetError());
    return false;
  }

  surf_ = eglCreateWindowSurface(dpy_, cfg, static_cast<EGLNativeWindowType>(x_win_), nullptr);
  if (surf_ == EGL_NO_SURFACE) {
    nbt_log("eglCreateWindowSurface failed err=0x%x", eglGetError());
    return false;
  }

  static const EGLint kCtxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  ctx_ = eglCreateContext(dpy_, cfg, EGL_NO_CONTEXT, kCtxAttrs);
  if (ctx_ == EGL_NO_CONTEXT) {
    nbt_log("eglCreateContext failed err=0x%x", eglGetError());
    return false;
  }
  if (!eglMakeCurrent(dpy_, surf_, surf_, ctx_)) {
    nbt_log("eglMakeCurrent failed err=0x%x", eglGetError());
    return false;
  }
  eglSwapInterval(dpy_, 1);
  return true;
}
#endif

void VideoEgl::update_viewport_mapping(const AppSettings& settings) {
  gles2_set_viewport_mapping(win_w_, win_h_, log_w_, log_h_, 0, 0, win_w_, win_h_);
  gles2_set_gamma(brightness_to_gamma(settings.brightness));
}

bool VideoEgl::init(int surface_id, const AppSettings& settings) {
  shutdown();
  surface_id_ = surface_id;
#ifdef __QNXNTO__
  if (!qnx_video_open(surface_id, &qnx_)) {
    shutdown();
    return false;
  }
  dpy_ = qnx_.dpy;
  surf_ = qnx_.surf;
  ctx_ = qnx_.ctx;
  win_w_ = qnx_.win_w;
  win_h_ = qnx_.win_h;
#else
  if (!open_x11_window() || !init_x11_egl()) {
    shutdown();
    return false;
  }
#endif
  compute_logical_resolution(win_w_, win_h_, &log_w_, &log_h_);
  update_viewport_mapping(settings);
  {
    const char* egl_vendor = eglQueryString(dpy_, EGL_VENDOR);
    const char* egl_ver = eglQueryString(dpy_, EGL_VERSION);
    const char* gl_vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* gl_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* gl_ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    nbt_debug("EGL vendor=%s version=%s", egl_vendor ? egl_vendor : "?", egl_ver ? egl_ver : "?");
    nbt_debug("GL vendor=%s renderer=%s version=%s", gl_vendor ? gl_vendor : "?",
              gl_renderer ? gl_renderer : "?", gl_ver ? gl_ver : "?");
  }

  nbt_log_stage("video.shaders");
  if (!gles2_batch_init()) {
    nbt_log("gles2_batch_init failed");
    shutdown();
    return false;
  }

  /* Post an initial black frame immediately so LayerManager binds surface_id_. */
  glViewport(0, 0, win_w_, win_h_);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  eglSwapBuffers(dpy_, surf_);

  ready_ = true;
  return true;
}

void VideoEgl::apply_settings(const AppSettings& settings) {
  if (!ready_) return;
  update_viewport_mapping(settings);
}

void VideoEgl::begin_frame() {
  (void)ready_;
}

void VideoEgl::swap() {
  if (ready_ && dpy_ != EGL_NO_DISPLAY && surf_ != EGL_NO_SURFACE) {
    eglSwapBuffers(dpy_, surf_);
  }
}

void VideoEgl::shutdown() {
  ready_ = false;
  gles2_batch_shutdown();
#ifdef __QNXNTO__
  qnx_video_close(&qnx_);
  ctx_ = EGL_NO_CONTEXT;
  surf_ = EGL_NO_SURFACE;
  dpy_ = EGL_NO_DISPLAY;
#else
  if (dpy_ != EGL_NO_DISPLAY) {
    eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (ctx_ != EGL_NO_CONTEXT) eglDestroyContext(dpy_, ctx_);
    if (surf_ != EGL_NO_SURFACE) eglDestroySurface(dpy_, surf_);
    eglTerminate(dpy_);
  }
  ctx_ = EGL_NO_CONTEXT;
  surf_ = EGL_NO_SURFACE;
  dpy_ = EGL_NO_DISPLAY;

  Display* xdpy = static_cast<Display*>(x_dpy_);
  if (xdpy) {
    if (x_win_) XDestroyWindow(xdpy, static_cast<Window>(x_win_));
    XCloseDisplay(xdpy);
  }
  x_dpy_ = nullptr;
  x_win_ = 0;
#endif
}

void VideoEgl::poll_host_events(HostInputState* out) {
  if (!out) return;
  out->turn_delta = 0;
  out->use_tap = false;
  out->escape_tap = false;
  out->weapon_next_tap = false;
  out->enter_tap = false;
  out->backspace_tap = false;
  out->menu_up_tap = false;
  out->menu_down_tap = false;
  out->menu_left_tap = false;
  out->menu_right_tap = false;
  out->closed = false;

#ifndef __QNXNTO__
  Display* xdpy = static_cast<Display*>(x_dpy_);
  if (!xdpy) return;
  Atom wm_del = XInternAtom(xdpy, "WM_DELETE_WINDOW", False);
  while (XPending(xdpy) > 0) {
    XEvent ev;
    XNextEvent(xdpy, &ev);
    if (ev.type == ClientMessage && static_cast<Atom>(ev.xclient.data.l[0]) == wm_del) {
      out->closed = true;
    } else if (ev.type == ConfigureNotify) {
      if (ev.xconfigure.width > 0 && ev.xconfigure.height > 0 &&
          (ev.xconfigure.width != win_w_ || ev.xconfigure.height != win_h_)) {
        win_w_ = ev.xconfigure.width;
        win_h_ = ev.xconfigure.height;
        gles2_set_viewport_mapping(win_w_, win_h_, log_w_, log_h_, 0, 0, win_w_, win_h_);
      }
    } else if (ev.type == KeyPress || ev.type == KeyRelease) {
      bool down = (ev.type == KeyPress);
      KeySym ks = XLookupKeysym(&ev.xkey, 0);
      if (ks == XK_Up || ks == XK_w) {
        out->forward = down;
        if (down) out->menu_up_tap = true;
      } else if (ks == XK_Down || ks == XK_s) {
        out->backward = down;
        if (down) out->menu_down_tap = true;
      } else if (ks == XK_a) {
        out->strafe_left = down;
        if (down) out->menu_left_tap = true;
      } else if (ks == XK_d) {
        out->strafe_right = down;
        if (down) out->menu_right_tap = true;
      } else if (ks == XK_Left || ks == XK_q) {
        if (down) {
          out->turn_delta -= 64;
          out->menu_up_tap = true;
        }
      } else if (ks == XK_Right || ks == XK_e) {
        if (down) {
          out->turn_delta += 64;
          out->menu_down_tap = true;
        }
      } else if (ks == XK_Control_R || ks == XK_Control_L || ks == XK_f) {
        out->fire = down;
      } else if (ks == XK_Return) {
        out->fire = down;
        if (down) out->enter_tap = true;
      } else if (ks == XK_space) {
        if (down) out->use_tap = true;
      } else if (ks == XK_Tab) {
        if (down) out->weapon_next_tap = true;
      } else if (ks == XK_Escape) {
        if (down) out->escape_tap = true;
      } else if (ks == XK_BackSpace) {
        if (down) out->backspace_tap = true;
      }
    }
  }
#endif
}

}  // namespace nbt_doom
