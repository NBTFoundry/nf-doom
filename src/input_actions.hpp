#ifndef NBT_DOOM_INPUT_ACTIONS_HPP
#define NBT_DOOM_INPUT_ACTIONS_HPP
#include <stddef.h>
namespace nbt_doom {
enum DoomKeyCode {
  kDoomKeyRight = 0xae,
  kDoomKeyLeft = 0xac,
  kDoomKeyUp = 0xad,
  kDoomKeyDown = 0xaf,
  kDoomKeyEscape = 27,
  kDoomKeyEnter = 13,
  kDoomKeyBackspace = 127,
  kDoomKeySpace = ' ',
  kDoomKeyRCtrl = 0x9d,
  kDoomKeySpeed = 0xb6,
  kDoomKeyMoveForward = 'w',
  kDoomKeyMoveBackward = 's',
  kDoomKeyStrafeLeft = 'a',
  kDoomKeyStrafeRight = 'd'
};
struct InputAction {
  enum Type { kKeyDown, kKeyUp, kTurn, kNextWeapon, kExit, kPreviousWeapon, kAutomap, kSmoothTurn };
  Type type;
  int value;
};
class InputSink {
 public:
  virtual ~InputSink() {}
  virtual void on_action(const InputAction& action) = 0;
};
typedef InputAction IdriveAction;
typedef InputSink IdriveSink;
/* Held keys are ORed across sources, so one release cannot cancel another pad/knob/key. */
class InputMixer {
 public:
  enum Source { Idrive, Gamepad, Keyboard, SourceCount };
  InputMixer();
  void set_sink(InputSink* sink) { sink_ = sink; }
  void action(Source source, const InputAction& action);
  void release(Source source);

 private:
  bool held_[SourceCount][512];
  InputSink* sink_;
};
class SourceSink : public InputSink {
 public:
  SourceSink(InputMixer& mixer, InputMixer::Source source) : mixer_(mixer), source_(source) {}
  void on_action(const InputAction& action) { mixer_.action(source_, action); }

 private:
  InputMixer& mixer_;
  InputMixer::Source source_;
};
}  // namespace nbt_doom
#endif
