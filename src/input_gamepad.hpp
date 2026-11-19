#ifndef NBT_DOOM_INPUT_GAMEPAD_HPP
#define NBT_DOOM_INPUT_GAMEPAD_HPP
#include "input_actions.hpp"
#include "lifecycle.hpp"
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

 private:
  struct Delivery {
    GamepadDecoder* self;
    uint64_t now;
    bool menu, always_run;
    TurnSensitivity turn_sens;
  };
  static void receive(void* context, const nbt_gamepad_wire_state& state);
  void emit(InputAction::Type type, int value = 0);
  void key(unsigned index, bool down, int value);
  void tap(int value);
  nbt::gamepad::Input reader_;
  nbt::gamepad::Navigation navigation_;
  InputSink* sink_;
  bool held_[7], fire_, run_, menu_;
  uint64_t previous_;
  int64_t turn_;
};
}  // namespace nbt_doom
#endif
