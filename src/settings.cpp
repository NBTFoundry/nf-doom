#include "lifecycle.hpp"
#include "bridge_input_fd.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
namespace nbt_doom {
namespace {
void trim_line(char* s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
    s[--n] = '\0';
  }
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
    if (strcmp(value, "on") == 0)
      s->invert_turn = true;
    else if (strcmp(value, "off") == 0)
      s->invert_turn = false;
  } else if (strcmp(key, "brightness") == 0) {
    int b = 0;
    if (parse_int(value, 0, &b) && b <= 4) s->brightness = b;
  } else if (strcmp(key, "always_run") == 0) {
    if (strcmp(value, "on") == 0)
      s->always_run = true;
    else if (strcmp(value, "off") == 0)
      s->always_run = false;
  } else if (strcmp(key, "sound") == 0) {
    if (strcmp(value, "on") == 0)
      s->sound = true;
    else if (strcmp(value, "off") == 0)
      s->sound = false;
  } else if (strcmp(key, "show_fps") == 0) {
    if (strcmp(value, "on") == 0)
      s->show_fps = true;
    else if (strcmp(value, "off") == 0)
      s->show_fps = false;
  } else if (strcmp(key, "debug_log") == 0) {
    if (strcmp(value, "on") == 0)
      s->debug_log = true;
    else if (strcmp(value, "off") == 0)
      s->debug_log = false;
  }
}

void settings_load_env(AppSettings* s) {
  static const struct {
    const char* env;
    const char* key;
  } kMap[] = {
      {"NBT_SETTING_TURN_SENS", "turn_sens"},   {"NBT_SETTING_INVERT_TURN", "invert_turn"},
      {"NBT_SETTING_BRIGHTNESS", "brightness"}, {"NBT_SETTING_ALWAYS_RUN", "always_run"},
      {"NBT_SETTING_SOUND", "sound"},           {"NBT_SETTING_SHOW_FPS", "show_fps"},
      {"NBT_SETTING_DEBUG_LOG", "debug_log"},
  };
  for (size_t i = 0; i < sizeof(kMap) / sizeof(kMap[0]); ++i) {
    const char* v = getenv(kMap[i].env);
    if (v && v[0]) settings_apply_kv(s, kMap[i].key, v);
  }
}

bool settings_load_snapshot_file(const char* path, AppSettings* s) {
  if (!path || !path[0] || !s) return false;
  FILE* fp = fopen(path, "r");
  if (!fp) return false;
  char line[256];
  while (fgets(line, sizeof(line), fp)) {
    trim_line(line);
    if (!line[0] || line[0] == '#') continue;
    char* eq = strchr(line, '=');
    if (!eq) continue;
    *eq = '\0';
    settings_apply_kv(s, line, eq + 1);
  }
  fclose(fp);
  return true;
}

}  // namespace nbt_doom
