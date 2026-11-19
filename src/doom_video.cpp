#include "doom_video.hpp"

#include <string.h>

#include "nbt/app/log.hpp"
#include "nbt/gles2/gles2_compat.h"

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

DoomVideo::DoomVideo() : batch_ready_(false), log_w_(640), log_h_(480) {}

DoomVideo::~DoomVideo() {
  shutdown();
}

void DoomVideo::update_viewport_mapping(const AppSettings& settings) {
  const int w = video_.window_width();
  const int h = video_.window_height();
  gles2_set_viewport_mapping(w, h, log_w_, log_h_, 0, 0, w, h);
  gles2_set_gamma(brightness_to_gamma(settings.brightness));
}

bool DoomVideo::init(int surface_id, const AppSettings& settings) {
  shutdown();
  const nbt::app::VideoConfig config = {"NBT Doom (Host GLES2)", 1280, 480};
  if (!video_.init(surface_id, &config)) return false;
  compute_logical_resolution(video_.window_width(), video_.window_height(), &log_w_, &log_h_);
  update_viewport_mapping(settings);
  nbt::app::log_stage("video.shaders");
  if (!gles2_batch_init()) {
    nbt::app::log_info("gles2_batch_init failed");
    shutdown();
    return false;
  }
  batch_ready_ = true;
  return true;
}

void DoomVideo::apply_settings(const AppSettings& settings) {
  if (!ready()) return;
  update_viewport_mapping(settings);
}

void DoomVideo::shutdown() {
  if (batch_ready_) {
    gles2_batch_shutdown(); /* while the context is still current */
    batch_ready_ = false;
  }
  video_.shutdown();
}

void DoomVideo::poll_host_events(HostInputState* out) {
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

  nbt::app::HostEvent ev;
  while (video_.poll_event(&ev)) {
    if (ev.type == nbt::app::HostEvent::kClose) {
      out->closed = true;
      continue;
    }
    if (ev.type == nbt::app::HostEvent::kResize) {
      gles2_set_viewport_mapping(ev.width, ev.height, log_w_, log_h_, 0, 0, ev.width, ev.height);
      continue;
    }
    const bool down = (ev.type == nbt::app::HostEvent::kKeyDown);
    const int k = ev.key;
    if (k == nbt::app::kHostKeyUp || k == 'w') {
      out->forward = down;
      if (down) out->menu_up_tap = true;
    } else if (k == nbt::app::kHostKeyDown || k == 's') {
      out->backward = down;
      if (down) out->menu_down_tap = true;
    } else if (k == 'a') {
      out->strafe_left = down;
      if (down) out->menu_left_tap = true;
    } else if (k == 'd') {
      out->strafe_right = down;
      if (down) out->menu_right_tap = true;
    } else if (k == nbt::app::kHostKeyLeft || k == 'q') {
      if (down) {
        out->turn_delta -= 64;
        out->menu_up_tap = true;
      }
    } else if (k == nbt::app::kHostKeyRight || k == 'e') {
      if (down) {
        out->turn_delta += 64;
        out->menu_down_tap = true;
      }
    } else if (k == nbt::app::kHostKeyLeftCtrl || k == nbt::app::kHostKeyRightCtrl || k == 'f') {
      out->fire = down;
    } else if (k == nbt::app::kHostKeyReturn) {
      out->fire = down;
      if (down) out->enter_tap = true;
    } else if (k == ' ') {
      if (down) out->use_tap = true;
    } else if (k == nbt::app::kHostKeyTab) {
      if (down) out->weapon_next_tap = true;
    } else if (k == nbt::app::kHostKeyEscape) {
      if (down) out->escape_tap = true;
    } else if (k == nbt::app::kHostKeyBackspace) {
      if (down) out->backspace_tap = true;
    }
  }
}

}  // namespace nbt_doom
