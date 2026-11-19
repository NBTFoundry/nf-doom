// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "nbt/app/clock.hpp"
#include "nbt/frontend/hints.hpp"
#include "nbt_platform.hpp"
#include <stdio.h>
extern "C" {
#include "d_event.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "i_main.h"
#include "i_system.h"
#include "i_video.h"
extern int messageToPrint;
extern dboolean messageNeedsInput;
}
namespace {
void post_key_event(bool down, int key) {
  event_t ev;
  ev.type = down ? ev_keydown : ev_keyup;
  ev.data1 = key;
  ev.data2 = 0;
  ev.data3 = 0;
  D_PostEvent(&ev);
}

void post_key_tap(int key) {
  post_key_event(true, key);
  post_key_event(false, key);
}

void post_mouse_turn(int delta) {
  if (delta == 0) return;
  event_t ev;
  ev.type = ev_mousemotion;
  ev.data1 = 0;
  ev.data2 = delta;
  ev.data3 = 0;
  D_PostEvent(&ev);
}

int translated_keys[512];  // POD; remember prompt translation until the matching release
class NbtInputSink : public nbt_doom::InputSink {
 public:
  void on_action(const nbt_doom::InputAction& action) {
    const bool prompt = messageToPrint && messageNeedsInput;
    switch (action.type) {
      case nbt_doom::InputAction::kKeyDown:
        if (action.value >= 0 && action.value < 512) {
          translated_keys[action.value] = nbt_doom::prompt_answer_key(
              action.value == nbt_doom::kDoomKeySpeed ? key_speed : action.value, prompt);
          post_key_event(true, translated_keys[action.value]);
        }
        break;
      case nbt_doom::InputAction::kKeyUp:
        if (action.value >= 0 && action.value < 512) {
          const int translated = translated_keys[action.value];
          post_key_event(false, translated ? translated : action.value);
          translated_keys[action.value] = 0;
        }
        break;
      case nbt_doom::kActionTurn:
        post_mouse_turn(action.value * 8);
        break;
      case nbt_doom::kActionNextWeapon:
        post_key_tap(key_nextweapon);
        break;
      case nbt_doom::kActionPreviousWeapon:
        post_key_tap(key_prevweapon);
        break;
      case nbt_doom::kActionAutomap:
        post_key_tap(key_map);
        break;
      case nbt_doom::kActionSmoothTurn:
        post_mouse_turn(action.value);
        break;
      default: /* kActionExit: the decoder stops the application */
        break;
    }
  }
};

nbt::app::InputMixer* active_mixer = NULL;
void keyboard_action(bool down, int key) {
  if (!active_mixer) return;
  const nbt_doom::InputAction action = {
      down ? nbt_doom::InputAction::kKeyDown : nbt_doom::InputAction::kKeyUp, key};
  active_mixer->action(nbt_doom::kSourceKeyboard, action);
}
void keyboard_tap(int key) {
  keyboard_action(true, key);
  keyboard_action(false, key);
}
void sync_key(bool* prev, bool now, int doom_key) {
  if (*prev != now) {
    *prev = now;
    keyboard_action(now, doom_key);
  }
}

bool g_prev_fwd = false;
bool g_prev_back = false;
bool g_prev_sl = false;
bool g_prev_sr = false;
bool g_prev_fire = false;
/* File-scope POD: static storage duration zero-initializes every field cleanly. */
nbt_doom::HostInputState g_host_in;

void forward_host_input(const nbt_doom::HostInputState& in, nbt::app::Lifecycle* lc) {
  sync_key(&g_prev_fwd, in.forward, nbt_doom::kDoomKeyMoveForward);
  sync_key(&g_prev_back, in.backward, nbt_doom::kDoomKeyMoveBackward);
  sync_key(&g_prev_sl, in.strafe_left, nbt_doom::kDoomKeyStrafeLeft);
  sync_key(&g_prev_sr, in.strafe_right, nbt_doom::kDoomKeyStrafeRight);
  sync_key(&g_prev_fire, in.fire, nbt_doom::kDoomKeyRCtrl);
  if (in.turn_delta != 0) post_mouse_turn(in.turn_delta * 12);
  if (in.use_tap) keyboard_tap(nbt_doom::kDoomKeySpace);
  if (in.escape_tap) keyboard_tap(nbt_doom::kDoomKeyEscape);
  if (in.enter_tap) keyboard_tap(nbt_doom::kDoomKeyEnter);
  if (in.backspace_tap) keyboard_tap(nbt_doom::kDoomKeyBackspace);
  if (in.menu_up_tap) keyboard_tap(nbt_doom::kDoomKeyUp);
  if (in.menu_down_tap) keyboard_tap(nbt_doom::kDoomKeyDown);
  if (in.menu_left_tap) keyboard_tap(nbt_doom::kDoomKeyLeft);
  if (in.menu_right_tap) keyboard_tap(nbt_doom::kDoomKeyRight);
  if (in.weapon_next_tap) keyboard_tap(key_nextweapon);
  if (in.closed && lc) lc->request_stop();
}

void format_status_line(char* out, size_t out_sz) {
  if (gamestate == GS_LEVEL) {
    snprintf(out, out_sz, "E%dM%d H:%d%%", gameepisode, gamemap, players[consoleplayer].health);
  } else if (menuactive != mnact_inactive) {
    snprintf(out, out_sz, "Menu");
  } else {
    snprintf(out, out_sz, "DOOM Shareware");
  }
}

}  // namespace
extern "C" {
void I_StartTic(void) {
  nbt_doom::PlatformState* st = nbt_doom::platform_state();
  if (!st) return;

  uint64_t now = nbt::app::monotonic_ms();
  bool menu_mode = (menuactive != mnact_inactive) || (gamestate == GS_DEMOSCREEN);

  NbtInputSink output;
  if (!st->input) return;
  st->input->set_sink(&output);
  active_mixer = st->input;
  if (st->idrive) {
    nbt_doom::SourceSink sink(*st->input, nbt_doom::kSourceIdrive);
    st->idrive->set_sink(&sink);
    if (st->lifecycle) st->idrive->poll_socket(st->lifecycle->controller_fd(), now, menu_mode);
    st->idrive->update_timers(now, menu_mode);
    st->idrive->set_sink(NULL);
    if (st->idrive->exit_requested() && st->lifecycle) {
      st->lifecycle->request_stop();
    }
  }

  if (st->gamepad && st->lifecycle) {
    nbt_doom::SourceSink sink(*st->input, nbt_doom::kSourceGamepad);
    st->gamepad->set_sink(&sink);
    st->gamepad->poll_socket(st->lifecycle->gamepad_fd(), now, menu_mode,
                             st->applied_settings.always_run, st->applied_settings.turn_sens);
    st->gamepad->set_sink(NULL);
    /* The pad's own name when it is plugged in (bridge 1.5 gives its family). */
    if (st->gamepad->take_pad_change() && st->video) {
      char text[64];
      const int family = st->gamepad->pad_family();
      if (family >= 0) {
        snprintf(text, sizeof(text), "%s connected", nbt::frontend::input_name(family));
      } else {
        snprintf(text, sizeof(text), "Gamepad unplugged: the iDrive controller plays");
      }
      st->video->notice(text, now);
    }
  }
  if (st->video) {
    st->video->poll_host_events(&g_host_in);
    forward_host_input(g_host_in, st->lifecycle);
  }

  if (st->lifecycle) {
    char status[96];
    format_status_line(status, sizeof(status));
    bool settings_changed = false;
    st->lifecycle->tick(now, status, &settings_changed);
    if (settings_changed) {
      nbt_doom::platform_apply_settings(nbt_doom::doom_settings(*st->lifecycle));
    }
    if (st->lifecycle->consume_show_requested() && gamestate == GS_LEVEL &&
        menuactive == mnact_inactive) {
      post_key_tap(nbt_doom::kDoomKeyEscape);
    }
    if (st->lifecycle->stopping()) {
      st->input->release(nbt_doom::kSourceGamepad);
      st->input->release(nbt_doom::kSourceIdrive);
      st->input->release(nbt_doom::kSourceKeyboard);
      I_SafeExit(0);
    }
  }
  active_mixer = NULL;
  st->input->set_sink(NULL);
}
}
