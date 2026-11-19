// SPDX-License-Identifier: GPL-2.0-or-later
#include "input_idrive.hpp"

#include <stdio.h>

#include "nbt/app/log.hpp"

namespace nbt_doom {

namespace {

constexpr uint8_t kTiltUp = 0x01;
constexpr uint8_t kTiltRight = 0x02;
constexpr uint8_t kTiltDown = 0x04;
constexpr uint8_t kTiltLeft = 0x08;

/* The manager decodes the tilt (CCS35up's 12-step clock, the older CCS's
 * cluster 0x1d); a diagonal holds two directions. */
uint8_t tilt_mask(uint32_t keys) {
  return static_cast<uint8_t>(((keys & NBT_CONTROLLER_TILT_UP) ? kTiltUp : 0) |
                              ((keys & NBT_CONTROLLER_TILT_RIGHT) ? kTiltRight : 0) |
                              ((keys & NBT_CONTROLLER_TILT_DOWN) ? kTiltDown : 0) |
                              ((keys & NBT_CONTROLLER_TILT_LEFT) ? kTiltLeft : 0));
}

uint64_t strafe_step_ms(TurnSensitivity sens) {
  if (sens == kTurnLow) return 55u;
  if (sens == kTurnHigh) return 120u;
  return 85u;
}

constexpr uint64_t kStrafeMaxHoldMs = 220u;

}  // namespace

int prompt_answer_key(int key, bool yes_no_prompt) {
  if (!yes_no_prompt) return key;
  if (key == kDoomKeyEnter) return 'y';
  if (key == kDoomKeyBackspace) return 'n';
  return key;
}

IdriveDecoder::IdriveDecoder()
    : sink_(nullptr),
      input_(),
      back_(),
      poll_now_ms_(0),
      poll_menu_mode_(false),
      held_up_(false),
      held_down_(false),
      held_left_(false),
      held_right_(false),
      held_strafe_left_(false),
      held_strafe_right_(false),
      strafe_until_ms_(0),
      held_fire_(false),
      held_menu_(false),
      prev_push_(false),
      exit_requested_(false),
      tilt_mask_(0),
      last_menu_repeat_ms_(0) {
  settings_defaults(&settings_);
}

void IdriveDecoder::emit(int type, int value) {
  if (sink_) {
    const InputAction act = {type, value};
    sink_->on_action(act);
  }
}

void IdriveDecoder::emit_tap(int key) {
  emit(InputAction::kKeyDown, key);
  emit(InputAction::kKeyUp, key);
}

void IdriveDecoder::update_held_key(bool* state, bool want, int key) {
  if (*state == want) return;
  *state = want;
  emit(want ? InputAction::kKeyDown : InputAction::kKeyUp, key);
}

void IdriveDecoder::release_strafe() {
  update_held_key(&held_strafe_left_, false, kDoomKeyStrafeLeft);
  update_held_key(&held_strafe_right_, false, kDoomKeyStrafeRight);
  strafe_until_ms_ = 0;
}

void IdriveDecoder::apply_rotation(int delta, uint64_t now_ms, bool menu_mode) {
  if (delta == 0) return;

  if (menu_mode) {
    release_strafe();
    if (settings_.invert_turn) delta = -delta;
    int steps = delta > 0 ? delta : -delta;
    if (steps > 4) steps = 4;
    int key = (delta > 0) ? kDoomKeyDown : kDoomKeyUp;
    for (int i = 0; i < steps; ++i) emit_tap(key);
  } else {
    int abs_delta = delta > 0 ? delta : -delta;
    if (abs_delta > 4) abs_delta = 4;
    uint64_t step_ms = static_cast<uint64_t>(abs_delta) * strafe_step_ms(settings_.turn_sens);
    bool want_right = delta > 0;
    if (want_right) {
      update_held_key(&held_strafe_left_, false, kDoomKeyStrafeLeft);
      uint64_t base = (held_strafe_right_ && strafe_until_ms_ > now_ms) ? strafe_until_ms_ : now_ms;
      uint64_t next_until = base + step_ms;
      if (next_until > now_ms + kStrafeMaxHoldMs) next_until = now_ms + kStrafeMaxHoldMs;
      strafe_until_ms_ = next_until;
      update_held_key(&held_strafe_right_, true, kDoomKeyStrafeRight);
    } else {
      update_held_key(&held_strafe_right_, false, kDoomKeyStrafeRight);
      uint64_t base = (held_strafe_left_ && strafe_until_ms_ > now_ms) ? strafe_until_ms_ : now_ms;
      uint64_t next_until = base + step_ms;
      if (next_until > now_ms + kStrafeMaxHoldMs) next_until = now_ms + kStrafeMaxHoldMs;
      strafe_until_ms_ = next_until;
      update_held_key(&held_strafe_left_, true, kDoomKeyStrafeLeft);
    }
  }
}

void IdriveDecoder::apply_push(bool down, uint64_t now_ms, bool menu_mode) {
  (void)now_ms;
  if (menu_mode) {
    update_held_key(&held_fire_, false, kDoomKeyRCtrl);
    if (down && !prev_push_) emit_tap(kDoomKeyEnter);
  } else {
    update_held_key(&held_fire_, down, kDoomKeyRCtrl);
  }
  prev_push_ = down;
}

void IdriveDecoder::apply_tilt(uint8_t dir_mask, uint64_t now_ms, bool menu_mode) {
  if (menu_mode) {
    update_held_key(&held_up_, false, kDoomKeyMoveForward);
    update_held_key(&held_down_, false, kDoomKeyMoveBackward);
    update_held_key(&held_left_, false, kDoomKeyLeft);
    update_held_key(&held_right_, false, kDoomKeyRight);
    release_strafe();
    uint8_t pressed = static_cast<uint8_t>(dir_mask & ~tilt_mask_);
    if (pressed & kTiltUp) emit_tap(kDoomKeyUp);
    if (pressed & kTiltDown) emit_tap(kDoomKeyDown);
    if (pressed & kTiltLeft) emit_tap(kDoomKeyLeft);
    if (pressed & kTiltRight) emit_tap(kDoomKeyRight);
    if (dir_mask == 0)
      last_menu_repeat_ms_ = 0;
    else if (pressed != 0)
      last_menu_repeat_ms_ = now_ms;
  } else {
    update_held_key(&held_up_, (dir_mask & kTiltUp) != 0, kDoomKeyMoveForward);
    update_held_key(&held_down_, (dir_mask & kTiltDown) != 0, kDoomKeyMoveBackward);
    bool want_left = (dir_mask & kTiltLeft) != 0;
    bool want_right = (dir_mask & kTiltRight) != 0;
    if (settings_.invert_turn) {
      bool tmp = want_left;
      want_left = want_right;
      want_right = tmp;
    }
    update_held_key(&held_left_, want_left, kDoomKeyLeft);
    update_held_key(&held_right_, want_right, kDoomKeyRight);
  }
  tilt_mask_ = dir_mask;
}

void IdriveDecoder::apply_back(bool down, uint64_t now_ms, bool menu_mode) {
  switch (back_.update(down, now_ms, !menu_mode)) {
    case nbt::app::BackGesture::kTap:
      emit_tap(menu_mode ? kDoomKeyBackspace : kDoomKeySpace);
      break;
    case nbt::app::BackGesture::kDoubleTap:
      emit(kActionNextWeapon, 1);
      break;
    default:
      break;
  }
}

void IdriveDecoder::apply_menu_key(bool down) {
  if (down && !held_menu_) emit_tap(kDoomKeyEscape);
  held_menu_ = down;
}

void IdriveDecoder::update_timers(uint64_t now_ms, bool menu_mode) {
  if (menu_mode || (strafe_until_ms_ != 0 && now_ms >= strafe_until_ms_)) {
    release_strafe();
  }
  /* The long hold exits cleanly before NBT Manager's own 2000 ms BACK escape. */
  switch (back_.poll(now_ms)) {
    case nbt::app::BackGesture::kLongHold:
      if (!exit_requested_) {
        exit_requested_ = true;
        emit(kActionExit, 0);
      }
      break;
    case nbt::app::BackGesture::kHold:
      emit_tap(kDoomKeyEscape);
      break;
    default:
      break;
  }
  if (menu_mode && tilt_mask_ != 0 && last_menu_repeat_ms_ != 0 && now_ms >= last_menu_repeat_ms_ &&
      (now_ms - last_menu_repeat_ms_) >= 180u) {
    last_menu_repeat_ms_ = now_ms;
    if (tilt_mask_ & kTiltUp) emit_tap(kDoomKeyUp);
    if (tilt_mask_ & kTiltDown) emit_tap(kDoomKeyDown);
    if (tilt_mask_ & kTiltLeft) emit_tap(kDoomKeyLeft);
    if (tilt_mask_ & kTiltRight) emit_tap(kDoomKeyRight);
  }
}

void IdriveDecoder::apply(const nbt_controller_wire_state& state,
                          const nbt::controller::Event& event, uint64_t now_ms, bool menu_mode) {
  if (event.pressed || event.released) {
    nbt::app::log_debug("controller keys=%08x pressed=%08x released=%08x link=%u menu=%d",
                        static_cast<unsigned>(state.keys), static_cast<unsigned>(event.pressed),
                        static_cast<unsigned>(event.released),
                        static_cast<unsigned>(state.connected), menu_mode ? 1 : 0);
  }
  apply_rotation(event.rotation, now_ms, menu_mode);
  apply_push((state.keys & NBT_CONTROLLER_PUSH) != 0, now_ms, menu_mode);
  apply_tilt(tilt_mask(state.keys), now_ms, menu_mode);
  apply_menu_key((state.keys & NBT_CONTROLLER_MENU) != 0);
  apply_back((state.keys & NBT_CONTROLLER_BACK) != 0, now_ms, menu_mode);
  /* OPTION, which iDrive no longer sees while Doom is shown: the automap. */
  if (event.pressed & NBT_CONTROLLER_OPTION) emit(kActionAutomap, 0);
  /* Favourite keys 1-7: weapons 1-7 (PrBoom's default number keys). */
  for (int i = 0; i < 7; ++i) {
    if (event.pressed & NBT_CONTROLLER_FAVOURITE(i)) emit_tap('1' + i);
  }
}

void IdriveDecoder::deliver(void* context, const nbt_controller_wire_state& state,
                            const nbt::controller::Event& event) {
  IdriveDecoder* self = static_cast<IdriveDecoder*>(context);
  self->apply(state, event, self->poll_now_ms_, self->poll_menu_mode_);
}

void IdriveDecoder::poll_socket(int controller_fd, uint64_t now_ms, bool menu_mode) {
  poll_now_ms_ = now_ms;
  poll_menu_mode_ = menu_mode;
  input_.poll(controller_fd, &IdriveDecoder::deliver, this);
  update_timers(now_ms, menu_mode);
}

}  // namespace nbt_doom
