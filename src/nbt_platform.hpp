#ifndef NBT_DOOM_PLATFORM_HPP
#define NBT_DOOM_PLATFORM_HPP

#include "input_idrive.hpp"
#include "input_gamepad.hpp"
#include "lifecycle.hpp"
#include "sound_qnx.hpp"
#include "video_egl.hpp"

namespace nbt_doom {

struct PlatformState {
  Lifecycle* lifecycle;
  VideoEgl* video;
  IdriveDecoder* idrive;
  GamepadDecoder* gamepad;
  InputMixer* input;
  SoundMixer* sound;
  AppSettings applied_settings;
  bool settings_applied;
  bool shutdown_done;
};

PlatformState* platform_state();
void set_platform_state(PlatformState* state);
void platform_apply_settings(const AppSettings& settings);
void platform_shutdown();

}  // namespace nbt_doom

#endif /* NBT_DOOM_PLATFORM_HPP */
