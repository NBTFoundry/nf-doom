#ifndef NBT_DOOM_SETTINGS_HPP
#define NBT_DOOM_SETTINGS_HPP

#include "nbt/app/lifecycle.hpp"
#include "nbt/app/settings.hpp"

namespace nbt_doom {

enum TurnSensitivity { kTurnLow = 0, kTurnMedium = 1, kTurnHigh = 2 };

/* Doom's settings, as declared in its package (see the Makefile's doom-package). */
struct AppSettings {
  TurnSensitivity turn_sens;
  bool invert_turn;
  int brightness; /* 0..4 */
  bool always_run;
  bool sound;
  bool show_fps;
  bool debug_log;
};

void settings_defaults(AppSettings* out);
/* Applies one value; unknown keys and values outside the choices are ignored. */
void settings_apply_kv(AppSettings* s, const char* key, const char* value);
/* Defaults, then every value of `values`. */
void settings_from(const nbt::app::Settings& values, AppSettings* out);
/* The settings the manager gave this launch (environment and live file). */
AppSettings doom_settings(const nbt::app::Lifecycle& lifecycle);

}  // namespace nbt_doom

#endif /* NBT_DOOM_SETTINGS_HPP */
