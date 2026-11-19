// SPDX-License-Identifier: GPL-2.0-or-later
#include "doom_settings.hpp"

#include <string.h>

#include "nbt/app/parse.hpp"

namespace nbt_doom {

namespace {

void apply_switch(bool* field, const char* value) {
  if (strcmp(value, "on") == 0)
    *field = true;
  else if (strcmp(value, "off") == 0)
    *field = false;
}

}  // namespace

void settings_defaults(AppSettings* out) {
  if (!out) return;
  memset(out, 0, sizeof(*out));
  out->turn_sens = kTurnMedium;
  out->invert_turn = false;
  out->brightness = 2;
  out->always_run = true;
  out->sound = true;
  out->show_fps = false;
  out->debug_log = false;
}

void settings_apply_kv(AppSettings* s, const char* key, const char* value) {
  if (!s || !key || !value) return;
  if (strcmp(key, "turn_sens") == 0) {
    if (strcmp(value, "low") == 0)
      s->turn_sens = kTurnLow;
    else if (strcmp(value, "medium") == 0)
      s->turn_sens = kTurnMedium;
    else if (strcmp(value, "high") == 0)
      s->turn_sens = kTurnHigh;
  } else if (strcmp(key, "invert_turn") == 0) {
    apply_switch(&s->invert_turn, value);
  } else if (strcmp(key, "brightness") == 0) {
    int b = 0;
    if (nbt::app::parse_int_range(value, 0, 4, &b)) s->brightness = b;
  } else if (strcmp(key, "always_run") == 0) {
    apply_switch(&s->always_run, value);
  } else if (strcmp(key, "sound") == 0) {
    apply_switch(&s->sound, value);
  } else if (strcmp(key, "show_fps") == 0) {
    apply_switch(&s->show_fps, value);
  } else if (strcmp(key, "debug_log") == 0) {
    apply_switch(&s->debug_log, value);
  }
}

void settings_from(const nbt::app::Settings& values, AppSettings* out) {
  settings_defaults(out);
  for (size_t i = 0; i < values.count(); ++i) {
    settings_apply_kv(out, values.at(i).key, values.at(i).value);
  }
}

AppSettings doom_settings(const nbt::app::Lifecycle& lifecycle) {
  AppSettings s;
  settings_from(lifecycle.settings(), &s);
  return s;
}

}  // namespace nbt_doom
