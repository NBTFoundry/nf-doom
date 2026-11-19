// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NBT_DOOM_INPUT_GAMEPAD_HPP
#define NBT_DOOM_INPUT_GAMEPAD_HPP
#include "doom_input.hpp"
#include "doom_settings.hpp"
#include "nbt/gamepad/input.hpp"
namespace nbt_doom {
class GamepadDecoder {
 public:
  GamepadDecoder();
  void set_sink(InputSink* sink) { sink_ = sink; }
  void poll_socket(int fd, uint64_t now, bool menu, bool always_run,
                   TurnSensitivity turn_sens = kTurnMedium);
  void update(const nbt_gamepad_wire_state& state, uint64_t now, bool menu, bool always_run,
              TurnSensitivity turn_sens = kTurnMedium);
  /* The pad in use: its NBT_GAMEPAD_FAMILY_* (bridge 1.5), -1 with none. */
  int pad_family() const { return family_; }
  /* True once after a pad was connected, changed family or was unplugged. */
  bool take_pad_change();

 private:
  struct Delivery {
    GamepadDecoder* self;
    uint64_t now;
    bool menu, always_run;
    TurnSensitivity turn_sens;
  };
  static void receive(void* context, const nbt_gamepad_wire_state& state);
  void emit(int type, int value = 0);
  void key(unsigned index, bool down, int value);
  void tap(int value);
  nbt::gamepad::Input reader_;
  nbt::gamepad::Navigation navigation_;
  InputSink* sink_;
  bool held_[7], fire_, run_, menu_;
  uint64_t previous_;
  int64_t turn_;
  int family_;
  bool family_changed_;
};
}  // namespace nbt_doom
#endif
