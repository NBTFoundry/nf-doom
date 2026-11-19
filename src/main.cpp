#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "input_idrive.hpp"
#include "lifecycle.hpp"
#include "nbt_log.hpp"
#include "nbt_platform.hpp"
#include "sound_qnx.hpp"
#include "video_egl.hpp"
#include "wad_vfs.h"

extern "C" {
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "e6y.h"
#include "g_game.h"
#include "i_main.h"
#include "i_system.h"
#include "i_video.h"
#include "lprintf.h"
#include "m_argv.h"
#include "m_misc.h"
#include "z_zone.h"
}

namespace {

void ensure_dir_exists(const char* path) {
  if (!path || !path[0]) return;
  mkdir(path, 0755);
}

void on_doom_quit(void) {
  if (demorecording) G_CheckDemoStatus();
  M_SaveDefaults();
  I_DemoExShutdown();
}

int g_clean_exit = 0;

void on_process_exit(void) {
  nbt_doom::PlatformState* st = nbt_doom::platform_state();
  if (!g_clean_exit && st && !st->shutdown_done) {
    nbt_doom::nbt_log("atexit cleanup stage=%s", nbt_doom::nbt_log_current_stage());
  }
  nbt_doom::platform_shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  nbt_doom::Lifecycle lifecycle;
  int init_rc = lifecycle.init(argc, argv);
  if (init_rc != 0) {
    return init_rc;
  }

  nbt_vfs_register_embedded_wads();

  if (lifecycle.is_check_mode()) {
    /* Readiness and health check: keep lease + 1 Hz heartbeats until manager stops us. */
    nbt_doom::nbt_log_set_debug(lifecycle.settings().debug_log);
    nbt_doom::nbt_log_reset();
    nbt_doom::nbt_log_install_crash_handlers();
    nbt_doom::nbt_log_stage("check");
    if (nbt_vfs_resolve_wad("prboom-plus", ".wad") == NULL) {
      nbt_doom::nbt_log("check mode: embedded prboom-plus.wad missing");
      lifecycle.close_fds();
      return 1;
    }
    const char* iwad = nbt_vfs_resolve_wad("doom1.wad", ".wad");
    if (!iwad) iwad = nbt_vfs_resolve_wad("doom.wad", ".wad");
    nbt_doom::nbt_log("check mode ready iwad=%s", iwad ? iwad : "(external)");
    if (!lifecycle.send_ready()) {
      nbt_doom::nbt_log("check mode: send_ready failed");
      lifecycle.close_fds();
      return 1;
    }
    while (!lifecycle.stopping()) {
      usleep(200000);
      lifecycle.tick(nbt_doom::monotonic_ms(), NULL, NULL);
    }
    nbt_doom::nbt_log("check mode exit");
    lifecycle.close_fds();
    return 0;
  }

  nbt_doom::VideoEgl video;
  nbt_doom::IdriveDecoder idrive;
  nbt_doom::GamepadDecoder gamepad;
  nbt_doom::InputMixer input;
  nbt_doom::SoundMixer sound;
  nbt_doom::PlatformState state;
  memset(&state, 0, sizeof(state));
  state.lifecycle = &lifecycle;
  state.video = &video;
  state.idrive = &idrive;
  state.gamepad = &gamepad;
  state.input = &input;
  state.sound = &sound;
  nbt_doom::set_platform_state(&state);
  atexit(on_process_exit);

  /* Line-buffer stdout so PrBoom+ lprintf diagnostics flush to manager output.log before exit. */
  setvbuf(stdout, NULL, _IOLBF, 0);

  const char* data_dir = nbt_data_dir();
  ensure_dir_exists(data_dir);
  nbt_doom::nbt_log_set_debug(lifecycle.settings().debug_log);
  nbt_doom::nbt_log_reset();
  nbt_doom::nbt_log_install_crash_handlers();
  nbt_doom::nbt_log_stage("init");
  nbt_doom::nbt_log("init ok surface_id=%d data_dir=%s sound=%d debug=%d", lifecycle.surface_id(),
                    data_dir, lifecycle.settings().sound ? 1 : 0,
                    lifecycle.settings().debug_log ? 1 : 0);
  nbt_doom::nbt_debug(
      "settings turn=%d invert=%d bright=%d run=%d sound=%d fps=%d controller_fd=%d gamepad_fd=%d",
      static_cast<int>(lifecycle.settings().turn_sens), lifecycle.settings().invert_turn ? 1 : 0,
      lifecycle.settings().brightness, lifecycle.settings().always_run ? 1 : 0,
      lifecycle.settings().sound ? 1 : 0, lifecycle.settings().show_fps ? 1 : 0,
      lifecycle.controller_fd(), lifecycle.gamepad_fd());
  char cfg_path[512];
  snprintf(cfg_path, sizeof(cfg_path), "%s/prboom-plus.cfg", data_dir);

  /* Build myargv without --ready-fd / --lease-fd so PrBoom+ sees clean arguments. */
  const int max_args = argc + 16;
  char** doom_argv = static_cast<char**>(calloc(static_cast<size_t>(max_args), sizeof(char*)));
  if (!doom_argv) return 1;

  int dargc = 0;
  doom_argv[dargc++] = (argc > 0 && argv[0]) ? argv[0] : const_cast<char*>("nbt-doom");

  bool has_iwad = false;
  bool has_vidmode = false;
  bool has_config = false;
  bool has_nosound = false;
  for (int i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "--ready-fd") == 0 || strcmp(argv[i], "--lease-fd") == 0) &&
        i + 1 < argc) {
      i++;
      continue;
    }
    if (strcmp(argv[i], "-iwad") == 0) has_iwad = true;
    if (strcmp(argv[i], "-vidmode") == 0) has_vidmode = true;
    if (strcmp(argv[i], "-config") == 0) has_config = true;
    if (strcmp(argv[i], "-nosound") == 0) has_nosound = true;
    doom_argv[dargc++] = argv[i];
  }

  if (!has_vidmode) {
    doom_argv[dargc++] = const_cast<char*>("-vidmode");
    doom_argv[dargc++] = const_cast<char*>("gl");
  }
  if (!lifecycle.settings().sound && !has_nosound) {
    doom_argv[dargc++] = const_cast<char*>("-nosound");
  }
  if (!has_config) {
    doom_argv[dargc++] = const_cast<char*>("-config");
    doom_argv[dargc++] = cfg_path;
  }
  if (!has_iwad) {
    const char* iwad = nbt_vfs_resolve_wad("doom1.wad", ".wad");
    if (!iwad) iwad = nbt_vfs_resolve_wad("doom.wad", ".wad");
    nbt_doom::nbt_log("resolved iwad=%s", iwad ? iwad : "(none)");
    if (iwad) {
      doom_argv[dargc++] = const_cast<char*>("-iwad");
      doom_argv[dargc++] = const_cast<char*>(iwad);
    }
  }

  /* Initialize native display + EGL + GLES2 shaders before signaling ready so
   * NBT Manager sees a live rendering context immediately after 'R'. */
  nbt_doom::nbt_log_stage("video.init");
  if (!video.init(lifecycle.surface_id(), lifecycle.settings())) {
    nbt_doom::nbt_log("video.init failed");
    g_clean_exit = 1;
    nbt_doom::platform_shutdown();
    free(doom_argv);
    return 1;
  }
  nbt_doom::nbt_log("video.init ok (%dx%d logical %dx%d)", video.window_width(),
                    video.window_height(), video.logical_width(), video.logical_height());
  if (!lifecycle.send_ready()) {
    nbt_doom::nbt_log("lifecycle.send_ready failed");
    g_clean_exit = 1;
    nbt_doom::platform_shutdown();
    free(doom_argv);
    return 1;
  }
  nbt_doom::nbt_log_stage("M_LoadDefaults");

  myargc = dargc;
  myargv = doom_argv;

  ParamsMatchingCheck();
  M_LoadDefaults();
  I_AtExit(Z_Close, true);
  nbt_doom::nbt_log_stage("Z_Init");
  Z_Init();
  I_AtExit(on_doom_quit, false);
  I_PreInitGraphics();

  nbt_doom::nbt_log_stage("D_DoomMain");
  D_DoomMain();

  g_clean_exit = 1;
  nbt_doom::platform_shutdown();
  nbt_doom::set_platform_state(NULL);
  free(doom_argv);
  return 0;
}
