#ifndef NBT_DOOM_PLATFORM_HPP
#define NBT_DOOM_PLATFORM_HPP

#include "doom_settings.hpp"
#include "doom_video.hpp"
#include "input_gamepad.hpp"
#include "input_idrive.hpp"
#include "nbt/app/audio_mixer.hpp"
#include "nbt/app/lifecycle.hpp"

namespace nbt_doom {

struct PlatformState {
  nbt::app::Lifecycle* lifecycle;
  DoomVideo* video;
  IdriveDecoder* idrive;
  GamepadDecoder* gamepad;
  nbt::app::InputMixer* input;
  nbt::app::AudioMixer* sound;
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
