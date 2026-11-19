#ifndef NBT_DOOM_INPUT_HPP
#define NBT_DOOM_INPUT_HPP

#include "nbt/app/input_mixer.hpp"

namespace nbt_doom {

using nbt::app::InputAction;
using nbt::app::InputMixer;
using nbt::app::InputSink;
using nbt::app::SourceSink;

/* PrBoom+ key codes the input mappers send. */
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

/* Doom's own actions, passed through the InputMixer unchanged. */
enum DoomAction {
  kActionTurn = InputAction::kAppAction,
  kActionNextWeapon,
  kActionExit,
  kActionPreviousWeapon,
  kActionAutomap,
  kActionSmoothTurn
};

/* The InputMixer's sources. */
enum InputSource { kSourceIdrive = 0, kSourceGamepad = 1, kSourceKeyboard = 2 };

}  // namespace nbt_doom

#endif /* NBT_DOOM_INPUT_HPP */
