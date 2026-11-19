// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NBT_DOOM_CONTROLS_HPP
#define NBT_DOOM_CONTROLS_HPP

#include <stddef.h>

#include "doom_input.hpp"
#include "nbt/frontend/pad_profiles.hpp"

namespace nbt_doom {

/* Doom's controls in the front end's profiles (libnbt-frontend): what each
 * control does in a level, per pad family and for the iDrive controller,
 * with Doom's key codes and actions. The decoders (input_gamepad,
 * input_idrive) add what tables do not hold: the menus, the right stick's
 * turn, the knob's strafe timing and the BACK gestures. */

/* A pad in a level: `held` holds Doom keys (buttons, the left stick, the
 * triggers); `actions` sends Doom's own actions (DoomAction) once a press. */
struct DoomPadProfile {
  nbt::frontend::PadProfile held;
  const nbt::frontend::Binding* actions;
  size_t action_count;
};

/* By NBT_GAMEPAD_FAMILY_*: every family by position, an unknown one too. */
const DoomPadProfile& doom_pad_profile(unsigned family);

/* The iDrive controller in a level: `keys` holds Doom keys (push, tilts),
 * the knob strafes (turn_left, turn_right; the length from Turn
 * Sensitivity), a BACK tap and MENU; `actions` (DoomAction) and `taps` (key
 * codes) go once a press. */
struct DoomControllerProfile {
  nbt::frontend::ControllerProfile keys;
  const nbt::frontend::Binding* actions;
  size_t action_count;
  const nbt::frontend::Binding* taps;
  size_t tap_count;
};

const DoomControllerProfile& doom_controller_profile();

/* The action a profile's table gives `control`, or nbt::frontend::kNoAction. */
int binding_action(const nbt::frontend::Binding* table, size_t count, uint32_t control);

}  // namespace nbt_doom

#endif /* NBT_DOOM_CONTROLS_HPP */
