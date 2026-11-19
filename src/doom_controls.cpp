// SPDX-License-Identifier: GPL-2.0-or-later
#include "doom_controls.hpp"

namespace nbt_doom {

namespace {

using nbt::frontend::Binding;
using nbt::frontend::kNoAction;

/* NBT_GAMEPAD_* name buttons by position, so one layout serves Xbox,
 * DualShock 4 and DualSense pads alike (README, Controls). */
const Binding kPadHeld[] = {
    {NBT_GAMEPAD_UP, kDoomKeyMoveForward},  {NBT_GAMEPAD_DOWN, kDoomKeyMoveBackward},
    {NBT_GAMEPAD_LEFT, kDoomKeyStrafeLeft}, {NBT_GAMEPAD_RIGHT, kDoomKeyStrafeRight},
    {NBT_GAMEPAD_WEST, kDoomKeyRCtrl},      {NBT_GAMEPAD_SOUTH, kDoomKeySpace},
    {NBT_GAMEPAD_LS, kDoomKeySpeed}};
const Binding kPadActions[] = {{NBT_GAMEPAD_LB, kActionPreviousWeapon},
                               {NBT_GAMEPAD_RB, kActionNextWeapon},
                               {NBT_GAMEPAD_NORTH | NBT_GAMEPAD_VIEW, kActionAutomap}};
const DoomPadProfile kPad = {{kPadHeld, sizeof(kPadHeld) / sizeof(kPadHeld[0]),
                              /* left stick */ kDoomKeyMoveForward, kDoomKeyMoveBackward,
                              kDoomKeyStrafeLeft, kDoomKeyStrafeRight,
                              /* LT: run, RT: fire */ kDoomKeySpeed, kDoomKeyRCtrl,
                              /* the menus are Doom's own: Menu, B / Circle */ 0, 0},
                             kPadActions,
                             sizeof(kPadActions) / sizeof(kPadActions[0])};
const DoomPadProfile* const kPads[NBT_GAMEPAD_FAMILY_MAX + 1] = {&kPad, &kPad, &kPad, &kPad};

const Binding kControllerHeld[] = {{NBT_CONTROLLER_PUSH, kDoomKeyRCtrl},
                                   {NBT_CONTROLLER_TILT_UP, kDoomKeyMoveForward},
                                   {NBT_CONTROLLER_TILT_DOWN, kDoomKeyMoveBackward},
                                   {NBT_CONTROLLER_TILT_LEFT, kDoomKeyLeft},
                                   {NBT_CONTROLLER_TILT_RIGHT, kDoomKeyRight}};
/* OPTION, which iDrive no longer sees while Doom is shown: the automap. */
const Binding kControllerActions[] = {{NBT_CONTROLLER_OPTION, kActionAutomap}};
/* Favourite keys 1-7: weapons 1-7 (PrBoom's number keys). */
const Binding kControllerTaps[] = {
    {NBT_CONTROLLER_FAVOURITE(0), '1'}, {NBT_CONTROLLER_FAVOURITE(1), '2'},
    {NBT_CONTROLLER_FAVOURITE(2), '3'}, {NBT_CONTROLLER_FAVOURITE(3), '4'},
    {NBT_CONTROLLER_FAVOURITE(4), '5'}, {NBT_CONTROLLER_FAVOURITE(5), '6'},
    {NBT_CONTROLLER_FAVOURITE(6), '7'}};
const DoomControllerProfile kController = {
    {kControllerHeld, sizeof(kControllerHeld) / sizeof(kControllerHeld[0]), kDoomKeyStrafeLeft,
     kDoomKeyStrafeRight, /* the strafe's length: Turn Sensitivity */ 0, kDoomKeySpace,
     NBT_CONTROLLER_MENU},
    kControllerActions,
    sizeof(kControllerActions) / sizeof(kControllerActions[0]),
    kControllerTaps,
    sizeof(kControllerTaps) / sizeof(kControllerTaps[0])};

}  // namespace

const DoomPadProfile& doom_pad_profile(unsigned family) {
  return *kPads[family <= NBT_GAMEPAD_FAMILY_MAX ? family : 0];
}

const DoomControllerProfile& doom_controller_profile() {
  return kController;
}

int binding_action(const Binding* table, size_t count, uint32_t control) {
  for (size_t i = 0; i < count; ++i) {
    if (table[i].control == control) return table[i].action;
  }
  return kNoAction;
}

}  // namespace nbt_doom
