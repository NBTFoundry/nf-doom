// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doom_settings.hpp"
#include "nbt/app/log.hpp"
#include "nbt/gles2/gles2_compat.h"
#include "nbt_platform.hpp"

extern "C" {
#include "d_event.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "f_wipe.h"
#include "g_game.h"
#include "i_joy.h"
#include "i_main.h"
#include "i_system.h"
#include "i_video.h"
#include "lprintf.h"
#include "m_argv.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_things.h"
#include "v_video.h"
#include "gl_struct.h"
extern int messageToPrint;         /* m_menu.c: a message is on screen */
extern dboolean messageNeedsInput; /* m_menu.c: it waits for y or n */
}

namespace {

int g_gl_inited = 0;

}  // namespace

namespace nbt_doom {

void platform_apply_settings(const AppSettings& settings) {
  PlatformState* st = platform_state();
  if (!st) return;
  bool prev_dbg = nbt::app::log_debug_enabled();
  nbt::app::log_set_debug(settings.debug_log);
  if (settings.debug_log != prev_dbg) {
    nbt::app::log_info("debug_log=%s", settings.debug_log ? "on" : "off");
  }
  nbt::app::log_debug("apply_settings turn=%d invert=%d bright=%d run=%d sound=%d fps=%d",
                      static_cast<int>(settings.turn_sens), settings.invert_turn ? 1 : 0,
                      settings.brightness, settings.always_run ? 1 : 0, settings.sound ? 1 : 0,
                      settings.show_fps ? 1 : 0);
  st->applied_settings = settings;
  st->settings_applied = true;

  autorun = settings.always_run ? 1 : 0;
  rendering_stats = 0; /* Show FPS: the overlay's counter (DoomVideo::apply_settings) */

  if (st->sound) st->sound->set_muted(!settings.sound);
  if (st->idrive) st->idrive->set_settings(settings);
  if (st->video && st->video->ready()) {
    int prev_w = st->video->logical_width();
    int prev_h = st->video->logical_height();
    st->video->apply_settings(settings);
    if (st->video->logical_width() != prev_w || st->video->logical_height() != prev_h) {
      V_ChangeScreenResolution();
    }
  }
}

void platform_shutdown() {
  PlatformState* st = platform_state();
  if (!st || st->shutdown_done) return;
  st->shutdown_done = true;
  if (st->sound) st->sound->shutdown();
  if (st->video) st->video->shutdown();
  if (st->lifecycle) st->lifecycle->close_fds();
}

}  // namespace nbt_doom

extern "C" {

int render_vsync = 1;
int render_screen_multiply = 1;
int integer_scaling = 0;

SDL_Window* sdl_window = NULL;
SDL_Renderer* sdl_renderer = NULL;
SDL_Surface* screen = NULL;

const char* screen_resolutions_list[] = {"1440x540", NULL};
const char* screen_resolution = "1440x540";
const char* sdl_video_window_pos = "center";

int use_fullscreen = 1;
int desired_fullscreen = 1;
int exclusive_fullscreen = 0;
int renderW = 1440;
int renderH = 540;
int process_affinity_mask = 0;
int process_priority = 0;
int vanilla_keymap = 0;
dboolean window_focused = true;

int gl_colorbuffer_bits = 32;
int gl_depthbuffer_bits = 24;

/* Joystick globals & stubs (single-controller iDrive replaces SDL joystick) */
int joyleft = 0;
int joyright = 0;
int joyup = 0;
int joydown = 0;
int usejoystick = 0;
void I_InitJoystick(void) {}
void I_PollJoystick(void) {}

void SDL_GL_SwapWindow(SDL_Window* window) {
  (void)window;
  static int s_swapped = 0;
  nbt_doom::PlatformState* st = nbt_doom::platform_state();
  if (st && st->video) {
    st->video->swap();
    if (!s_swapped) {
      s_swapped = 1;
      nbt::app::log_info("first frame swapped");
    }
  }
}

void I_PreInitGraphics(void) {}

void I_InitScreenResolution(void) {
  static char s_resolution[32];
  nbt_doom::PlatformState* st = nbt_doom::platform_state();
  int w = 1440;
  int h = 540;
  if (st && st->video && st->video->ready()) {
    w = st->video->logical_width();
    h = st->video->logical_height();
  }
  snprintf(s_resolution, sizeof(s_resolution), "%dx%d", w, h);
  screen_resolutions_list[0] = s_resolution;
  screen_resolution = s_resolution;
  V_InitMode(VID_MODEGL);
  SCREENWIDTH = w;
  SCREENHEIGHT = h;
  SCREENPITCH = w * V_GetPixelDepth();
  renderW = w;
  renderH = h;

  V_DestroyUnusedTrueColorPalettes();
  V_FreeScreens();
  for (int i = 0; i < 3; i++) {
    screens[i].width = SCREENWIDTH;
    screens[i].height = SCREENHEIGHT;
    screens[i].byte_pitch = SCREENPITCH;
    screens[i].short_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE16);
    screens[i].int_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE32);
  }
  screens[4].width = SCREENWIDTH;
  screens[4].height = SCREENHEIGHT;
  screens[4].byte_pitch = SCREENPITCH;
  screens[4].short_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE16);
  screens[4].int_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE32);
  R_InitMeltRes();
  R_InitSpritesRes();
  R_InitBuffersRes();
  R_InitPlanesRes();
  R_InitVisplanesRes();
}

void I_SetWindowCaption(void) {}
void I_SetWindowIcon(void) {}
void I_UpdateRenderSize(void) {}
void UpdateGrab(void) {}

void I_UpdateVideoMode(void) {
  nbt_doom::PlatformState* st = nbt_doom::platform_state();
  if (!st || !st->video) I_Error("Video backend unavailable");

  if (!st->video->ready()) {
    nbt_doom::AppSettings cfg;
    if (st->lifecycle) {
      cfg = nbt_doom::doom_settings(*st->lifecycle);
    } else {
      nbt_doom::settings_defaults(&cfg);
    }
    if (!st->video->init(st->lifecycle ? st->lifecycle->surface_id() : 0, cfg)) {
      I_Error("Failed to initialize Screen/EGL GLES2 surface");
    }
  }

  I_InitScreenResolution();

  if (!g_gl_inited) {
    nbt::app::log_stage("gld_Init");
    gld_Init(SCREENWIDTH, SCREENHEIGHT);
    g_gl_inited = 1;
    nbt::app::log_stage("running");
  } else {
    glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);
  }

  if (st->lifecycle && !st->settings_applied) {
    nbt_doom::platform_apply_settings(nbt_doom::doom_settings(*st->lifecycle));
  }
  st->video->begin_frame();
}

void I_InitGraphics(void) {
  static int first = 1;
  if (!first) return;
  first = 0;
  I_AtExit(I_ShutdownGraphics, true);
  I_UpdateVideoMode();
}

void I_ShutdownGraphics(void) {
  nbt_doom::PlatformState* st = nbt_doom::platform_state();
  if (st && st->video) st->video->shutdown();
}

void I_SetPalette(int pal) {
  (void)pal;
}
void I_UpdateNoBlit(void) {}

void I_FinishUpdate(void) {
  if (V_GetMode() == VID_MODEGL) {
    gld_Finish();
  }
}

void I_StartFrame(void) {
  nbt_doom::PlatformState* st = nbt_doom::platform_state();
  if (st && st->video && st->video->ready()) {
    st->video->begin_frame();
  }
}

}  // extern "C"
