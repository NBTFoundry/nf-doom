// SPDX-License-Identifier: GPL-2.0-or-later
#include "input_gamepad.hpp"

#include "doom_controls.hpp"

namespace nbt_doom {
namespace {
/* The keys a pad holds in a level, in the order their changes are sent. */
const int kHeld[] = {kDoomKeyMoveForward, kDoomKeyMoveBackward, kDoomKeyStrafeLeft,
                     kDoomKeyStrafeRight, kDoomKeyRCtrl,        kDoomKeySpace,
                     kDoomKeySpeed};
const unsigned kHeldCount = sizeof(kHeld) / sizeof(kHeld[0]);

void hold(bool (&want)[kHeldCount], int key) {
  for (unsigned i = 0; i < kHeldCount; ++i) {
    if (kHeld[i] == key) want[i] = true;
  }
}

int64_t turn_rate_units_per_sec(TurnSensitivity sens) {
  /* cmd->angleturn maps 65536 units to 360 deg (angleturn[0]=640, [1]=1280 per 35Hz tic). */
  if (sens == kTurnLow) return 22400;
  if (sens == kTurnHigh) return 44800;
  return 32768;
}
}  // namespace

GamepadDecoder::GamepadDecoder()
    : reader_(),
      navigation_(),
      sink_(NULL),
      held_(),
      fire_(false),
      run_(false),
      menu_(false),
      previous_(0),
      turn_(0),
      family_(-1),
      family_changed_(false) {}
bool GamepadDecoder::take_pad_change() {
  const bool changed = family_changed_;
  family_changed_ = false;
  return changed;
}
void GamepadDecoder::emit(int type, int value) {
  if (sink_) {
    const InputAction a = {type, value};
    sink_->on_action(a);
  }
}
void GamepadDecoder::key(unsigned i, bool down, int value) {
  if (held_[i] == down) return;
  held_[i] = down;
  emit(down ? InputAction::kKeyDown : InputAction::kKeyUp, value);
}
void GamepadDecoder::tap(int value) {
  emit(InputAction::kKeyDown, value);
  emit(InputAction::kKeyUp, value);
}
void GamepadDecoder::poll_socket(int fd, uint64_t now, bool menu, bool always_run,
                                 TurnSensitivity turn_sens) {
  Delivery delivery = {this, now, menu, always_run, turn_sens};
  reader_.poll(fd, now, &receive, &delivery);
  update(reader_.state(), now, menu, always_run, turn_sens);
}
void GamepadDecoder::receive(void* context, const nbt_gamepad_wire_state& state) {
  Delivery* d = static_cast<Delivery*>(context);
  d->self->update(state, d->now, d->menu, d->always_run, d->turn_sens);
}
void GamepadDecoder::update(const nbt_gamepad_wire_state& s, uint64_t now, bool menu,
                            bool always_run, TurnSensitivity turn_sens) {
  const uint64_t elapsed =
      previous_ && now >= previous_ ? (now - previous_ > 100 ? 100 : now - previous_) : 0;
  previous_ = now;
  if (menu != menu_) {
    turn_ = 0;
  }
  menu_ = menu;
  const int family = s.connected ? static_cast<int>(s.family) : -1;
  if (family != family_) {
    family_ = family;
    family_changed_ = true;
  }
  const DoomPadProfile& p = doom_pad_profile(s.connected ? s.family : 0);
  const nbt::gamepad::NavigationEvent e = navigation_.update(s, now);
  const uint32_t b = s.connected ? s.buttons : 0;
  fire_ = s.connected && s.rt >= (fire_ ? 13107 : 16384);
  run_ = s.connected && s.lt >= (run_ ? 13107 : 16384);
  const int x = s.connected ? nbt_gamepad_axis(s.lx, 6553) : 0;
  const int y = s.connected ? nbt_gamepad_axis(s.ly, 6553) : 0;
  /* In a level, the profile's keys: buttons, the left stick, the triggers. */
  bool want[kHeldCount] = {false, false, false, false, false, false, false};
  if (!menu) {
    for (size_t i = 0; i < p.held.button_count; ++i) {
      if (b & p.held.buttons[i].control) hold(want, p.held.buttons[i].action);
    }
    if (y > 0) hold(want, p.held.stick_up);
    if (y < 0) hold(want, p.held.stick_down);
    if (x < 0) hold(want, p.held.stick_left);
    if (x > 0) hold(want, p.held.stick_right);
    if (run_) hold(want, p.held.left_trigger);
    if (fire_) hold(want, p.held.right_trigger);
  }
  for (unsigned i = 0; i < kHeldCount; ++i) {
    key(i, want[i] && !(always_run && kHeld[i] == kDoomKeySpeed), kHeld[i]);
  }
  if (menu) {
    const uint32_t nav = e.pressed | e.repeated;
    if (nav & NBT_GAMEPAD_UP) tap(kDoomKeyUp);
    if (nav & NBT_GAMEPAD_DOWN) tap(kDoomKeyDown);
    if (nav & NBT_GAMEPAD_LEFT) tap(kDoomKeyLeft);
    if (nav & NBT_GAMEPAD_RIGHT) tap(kDoomKeyRight);
    if (e.pressed & NBT_GAMEPAD_SOUTH) tap(kDoomKeyEnter);
    if (e.pressed & NBT_GAMEPAD_EAST) tap(kDoomKeyBackspace);
  } else {
    for (size_t i = 0; i < p.action_count; ++i) {
      if (e.pressed & p.actions[i].control) emit(p.actions[i].action);
    }
    const int right = s.connected ? nbt_gamepad_axis(s.rx, 6553) : 0;
    turn_ += static_cast<int64_t>(right) * static_cast<int64_t>(elapsed) *
             turn_rate_units_per_sec(turn_sens);
    const int delta = static_cast<int>(turn_ / 32767000);
    turn_ %= 32767000;
    if (delta) emit(kActionSmoothTurn, delta);
    if (!right) turn_ = 0;
  }
  if (e.pressed & NBT_GAMEPAD_MENU || (!menu && (e.pressed & NBT_GAMEPAD_EAST)))
    tap(kDoomKeyEscape);
}
}  // namespace nbt_doom
