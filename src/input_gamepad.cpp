#include "input_gamepad.hpp"
namespace nbt_doom {
namespace {
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
      turn_(0) {}
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
  const nbt::gamepad::NavigationEvent e = navigation_.update(s, now);
  const uint32_t b = s.connected ? s.buttons : 0;
  fire_ = s.connected && s.rt >= (fire_ ? 13107 : 16384);
  run_ = s.connected && s.lt >= (run_ ? 13107 : 16384);
  const int x = s.connected ? nbt_gamepad_axis(s.lx, 6553) : 0;
  const int y = s.connected ? nbt_gamepad_axis(s.ly, 6553) : 0;
  key(0, !menu && (y > 0 || (b & NBT_GAMEPAD_UP)), kDoomKeyMoveForward);
  key(1, !menu && (y < 0 || (b & NBT_GAMEPAD_DOWN)), kDoomKeyMoveBackward);
  key(2, !menu && (x < 0 || (b & NBT_GAMEPAD_LEFT)), kDoomKeyStrafeLeft);
  key(3, !menu && (x > 0 || (b & NBT_GAMEPAD_RIGHT)), kDoomKeyStrafeRight);
  key(4, !menu && (fire_ || (b & NBT_GAMEPAD_WEST)), kDoomKeyRCtrl);
  key(5, !menu && (b & NBT_GAMEPAD_SOUTH), kDoomKeySpace);
  key(6, !menu && !always_run && (run_ || (b & NBT_GAMEPAD_LS)), kDoomKeySpeed);
  if (menu) {
    const uint32_t nav = e.pressed | e.repeated;
    if (nav & NBT_GAMEPAD_UP) tap(kDoomKeyUp);
    if (nav & NBT_GAMEPAD_DOWN) tap(kDoomKeyDown);
    if (nav & NBT_GAMEPAD_LEFT) tap(kDoomKeyLeft);
    if (nav & NBT_GAMEPAD_RIGHT) tap(kDoomKeyRight);
    if (e.pressed & NBT_GAMEPAD_SOUTH) tap(kDoomKeyEnter);
    if (e.pressed & NBT_GAMEPAD_EAST) tap(kDoomKeyBackspace);
  } else {
    if (e.pressed & NBT_GAMEPAD_LB) emit(kActionPreviousWeapon);
    if (e.pressed & NBT_GAMEPAD_RB) emit(kActionNextWeapon);
    if (e.pressed & (NBT_GAMEPAD_NORTH | NBT_GAMEPAD_VIEW)) emit(kActionAutomap);
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
