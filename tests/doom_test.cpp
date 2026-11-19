// SPDX-License-Identifier: GPL-2.0-or-later
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "doom_input.hpp"
#include "doom_log.hpp"
#include "doom_settings.hpp"
#include "doom_video.hpp"
#include "input_gamepad.hpp"
#include "input_idrive.hpp"
#include "nbt/app/log.hpp"
#include "nbt/app/settings.hpp"
#include "prboom_compat/SDL.h"
#include "wad_vfs.h"

namespace {

int g_tests_run = 0;

#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      exit(1);                                                        \
    }                                                                 \
  } while (0)

struct RecordedActions : public nbt_doom::InputSink {
  nbt_doom::InputAction items[64];
  int count;

  RecordedActions() : count(0) {}
  void clear() { count = 0; }
  void on_action(const nbt_doom::InputAction& action) {
    if (count < 64) items[count++] = action;
  }
};

/* Sends controller states as NBT Manager's host does (capability `controller`). */
struct ControllerStates {
  int fds[2];
  uint32_t sequence;
  int32_t rotation;
  ControllerStates() : sequence(0), rotation(0) {
    CHECK(!socketpair(AF_UNIX, SOCK_DGRAM, 0, fds));
    CHECK(fcntl(fds[1], F_SETFL, O_NONBLOCK) != -1);
  }
  ~ControllerStates() {
    close(fds[0]);
    close(fds[1]);
  }
  void send(uint32_t keys, int turn = 0, bool connected = true) {
    rotation += turn;
    nbt_controller_wire_state st;
    memset(&st, 0, sizeof(st));
    st.sequence = ++sequence;
    st.keys = connected ? keys : 0;
    st.rotation = rotation;
    st.connected = connected ? 1 : 0;
    uint8_t out[NBT_CONTROLLER_WIRE_SIZE];
    CHECK(nbt_controller_wire_encode(&st, out, sizeof(out)) == NBT_CONTROLLER_WIRE_SIZE);
    CHECK(::send(fds[0], out, sizeof(out), 0) == static_cast<ssize_t>(sizeof(out)));
  }
};

void test_logical_resolution() {
  using namespace nbt_doom;
  int lw = 0, lh = 0;
  compute_logical_resolution(1440, 540, &lw, &lh);
  CHECK(lw == 1440 && lh == 540);

  compute_logical_resolution(1280, 480, &lw, &lh);
  CHECK(lw == 1280 && lh == 480);

  compute_logical_resolution(0, 0, &lw, &lh);
  CHECK(lw == 640 && lh == 480);
  g_tests_run++;
}

void test_wad_vfs() {
  nbt_vfs_reset();
  nbt_vfs_register_embedded_wads();

  const char* p = nbt_vfs_resolve_wad("prboom-plus", ".wad");
  CHECK(p != NULL && strcmp(p, "/embedded/prboom-plus.wad") == 0);
  int fd = nbt_vfs_open(p, O_RDONLY);
  CHECK(fd >= 0);
  int len = nbt_vfs_filelength(fd);
  CHECK(len > 10000);

  char hdr[12];
  CHECK(nbt_vfs_read(fd, hdr, 12) == 12);
  CHECK(memcmp(hdr, "PWAD", 4) == 0);
  CHECK(nbt_vfs_lseek(fd, -4, SEEK_END) == len - 4);
  CHECK(nbt_vfs_lseek(fd, -1, SEEK_SET) == -1);
  CHECK(nbt_vfs_close(fd) == 0);
  CHECK(nbt_vfs_read(fd, hdr, 1) == -1);

  /* Custom memory fixture */
  const char custom[] = "IWAD\x01\x00\x00\x00\x0c\x00\x00\x00";
  CHECK(nbt_vfs_register_memory("/embedded/CUSTOM.WAD", custom, sizeof(custom) - 1) == 0);
  const char* p_custom = nbt_vfs_resolve_wad("custom", ".wad");
  const char* p_prb = nbt_vfs_resolve_wad("prboom-plus", ".wad");
  CHECK(p_custom != NULL && strcmp(p_custom, "/embedded/CUSTOM.WAD") == 0);
  CHECK(p_prb != NULL && strcmp(p_prb, "/embedded/prboom-plus.wad") == 0);
  /* Resolving prboom-plus must not overwrite the previously returned pointer. */
  CHECK(strcmp(p_custom, "/embedded/CUSTOM.WAD") == 0);
  CHECK(nbt_vfs_access(p_custom, R_OK) == 0);
  CHECK(nbt_vfs_access(p_custom, W_OK) == -1);
  struct stat st;
  CHECK(nbt_vfs_stat(p_custom, &st) == 0 && st.st_size == static_cast<off_t>(sizeof(custom) - 1));
  CHECK(nbt_vfs_fopen(p_custom, "wb") == NULL);
  FILE* fp = nbt_vfs_fopen(p_custom, "rb");
  CHECK(fp != NULL);
  char fbuf[4] = {};
  CHECK(fread(fbuf, 1, 4, fp) == 4 && memcmp(fbuf, "IWAD", 4) == 0);
  CHECK(fseek(fp, 0, SEEK_END) == 0 && ftell(fp) == static_cast<long>(sizeof(custom) - 1));
  fclose(fp);
#ifndef NBT_NO_EMBEDDED_DOOM1
  const char* p_doom1 = nbt_vfs_resolve_wad("doom1.wad", ".wad");
  CHECK(p_doom1 != NULL);
  FILE* dfp = nbt_vfs_fopen(p_doom1, "rb");
  CHECK(dfp != NULL);
  char dsig[4] = {};
  CHECK(fread(dsig, 1, 4, dfp) == 4 && memcmp(dsig, "IWAD", 4) == 0);
  fclose(dfp);
#endif
  fd = nbt_vfs_open("/embedded/custom.wad", O_RDONLY);
  CHECK(fd >= 0 && nbt_vfs_filelength(fd) == static_cast<int>(sizeof(custom) - 1));
  CHECK(nbt_vfs_close(fd) == 0);

  nbt_vfs_reset();
  g_tests_run++;
}

void test_idrive_controller_states() {
  using namespace nbt_doom;
  IdriveDecoder dec;
  RecordedActions rec;
  dec.set_sink(&rec);
  ControllerStates c;
  const int fd = c.fds[1];

  AppSettings s;
  settings_defaults(&s);
  s.turn_sens = kTurnMedium;
  s.invert_turn = false;
  dec.set_settings(s);

  /* All keys up: the decoder is armed and connected; nothing happens. */
  c.send(0);
  dec.poll_socket(fd, 100, false);
  CHECK(rec.count == 0 && dec.connected());

  /* Three detents clockwise, push and tilt up at once. */
  c.send(NBT_CONTROLLER_PUSH | NBT_CONTROLLER_TILT_UP, 3);
  dec.poll_socket(fd, 110, false);
  CHECK(rec.count == 3);
  CHECK(rec.items[0].type == InputAction::kKeyDown && rec.items[0].value == kDoomKeyStrafeRight);
  CHECK(rec.items[1].type == InputAction::kKeyDown && rec.items[1].value == kDoomKeyRCtrl);
  CHECK(rec.items[2].type == InputAction::kKeyDown && rec.items[2].value == kDoomKeyMoveForward);

  /* Strafe stays held briefly, reverses cleanly on CCW, and auto-releases when timer expires. */
  rec.clear();
  dec.update_timers(120, false);
  CHECK(rec.count == 0);
  c.send(NBT_CONTROLLER_PUSH | NBT_CONTROLLER_TILT_UP, -1);
  dec.poll_socket(fd, 125, false);
  CHECK(rec.count == 2);
  CHECK(rec.items[0].type == InputAction::kKeyUp && rec.items[0].value == kDoomKeyStrafeRight);
  CHECK(rec.items[1].type == InputAction::kKeyDown && rec.items[1].value == kDoomKeyStrafeLeft);
  rec.clear();
  dec.update_timers(220, false);
  CHECK(rec.count == 1);
  CHECK(rec.items[0].type == InputAction::kKeyUp && rec.items[0].value == kDoomKeyStrafeLeft);

  /* Release push/tilt while turning one more detent. */
  rec.clear();
  c.send(0, 1);
  dec.poll_socket(fd, 240, false);
  CHECK(rec.count == 3);
  CHECK(rec.items[0].type == InputAction::kKeyDown && rec.items[0].value == kDoomKeyStrafeRight);
  CHECK(rec.items[1].type == InputAction::kKeyUp && rec.items[1].value == kDoomKeyRCtrl);
  CHECK(rec.items[2].type == InputAction::kKeyUp && rec.items[2].value == kDoomKeyMoveForward);
  dec.update_timers(400, false);

  /* Tilts: forward/back + view turning (left/right arrows), each held until centred. */
  const struct {
    uint32_t tilt;
    int key;
  } tilts[] = {{NBT_CONTROLLER_TILT_UP, kDoomKeyMoveForward},
               {NBT_CONTROLLER_TILT_RIGHT, kDoomKeyRight},
               {NBT_CONTROLLER_TILT_DOWN, kDoomKeyMoveBackward},
               {NBT_CONTROLLER_TILT_LEFT, kDoomKeyLeft}};
  for (size_t i = 0; i < sizeof(tilts) / sizeof(tilts[0]); ++i) {
    uint64_t t = 410u + static_cast<uint64_t>(i) * 40u;
    rec.clear();
    c.send(tilts[i].tilt);
    dec.poll_socket(fd, t, false);
    c.send(0);
    dec.poll_socket(fd, t + 10u, false);
    CHECK(rec.count >= 2);
    CHECK(rec.items[rec.count - 2].type == InputAction::kKeyDown &&
          rec.items[rec.count - 2].value == tilts[i].key);
    CHECK(rec.items[rec.count - 1].type == InputAction::kKeyUp &&
          rec.items[rec.count - 1].value == tilts[i].key);
  }
  dec.update_timers(900, false);
  /* Menu mode still emits arrow taps on tilt. */
  rec.clear();
  c.send(NBT_CONTROLLER_TILT_UP);
  dec.poll_socket(fd, 910, true);
  CHECK(rec.count == 2);
  CHECK(rec.items[0].type == InputAction::kKeyDown && rec.items[0].value == kDoomKeyUp);
  CHECK(rec.items[1].type == InputAction::kKeyUp && rec.items[1].value == kDoomKeyUp);
  c.send(0);
  dec.poll_socket(fd, 920, true);

  /* BACK short tap -> Space (Use); quick second tap -> NextWeapon */
  rec.clear();
  c.send(NBT_CONTROLLER_BACK);
  dec.poll_socket(fd, 950, false);
  c.send(0);
  dec.poll_socket(fd, 990, false);
  CHECK(rec.count == 2 && rec.items[0].type == InputAction::kKeyDown &&
        rec.items[0].value == kDoomKeySpace);
  rec.clear();
  c.send(NBT_CONTROLLER_BACK);
  dec.poll_socket(fd, 1050, false);
  c.send(0);
  dec.poll_socket(fd, 1090, false);
  CHECK(rec.count == 1 && rec.items[0].type == kActionNextWeapon);

  /* Long hold BACK (>=600ms) -> Escape; Very long hold (>=1400ms, <2000ms manager escape) -> Exit
   */
  rec.clear();
  c.send(NBT_CONTROLLER_BACK);
  dec.poll_socket(fd, 2000, false);
  dec.update_timers(2650, false);
  CHECK(rec.count == 2 && rec.items[0].type == InputAction::kKeyDown &&
        rec.items[0].value == kDoomKeyEscape);
  CHECK(!dec.exit_requested());
  dec.update_timers(3300, false);
  CHECK(!dec.exit_requested());
  dec.update_timers(3450, false);
  CHECK(dec.exit_requested());
  CHECK(rec.items[2].type == kActionExit);
  c.send(0);
  dec.poll_socket(fd, 3500, false);

  /* invert_turn swaps the tilt's view turning, not the knob's strafing. */
  s.invert_turn = true;
  dec.set_settings(s);
  rec.clear();
  c.send(0, 2);
  dec.poll_socket(fd, 4020, false);
  CHECK(rec.count == 1 && rec.items[0].type == InputAction::kKeyDown &&
        rec.items[0].value == kDoomKeyStrafeRight);
  dec.update_timers(4500, false);
  rec.clear();
  c.send(NBT_CONTROLLER_TILT_RIGHT);
  dec.poll_socket(fd, 4510, false);
  CHECK(rec.count == 1 && rec.items[0].type == InputAction::kKeyDown &&
        rec.items[0].value == kDoomKeyLeft);
  c.send(0);
  dec.poll_socket(fd, 4520, false);

  /* MENU -> Escape; OPTION -> automap; favourite keys 1-7 -> weapons 1-7, 8 nothing. */
  rec.clear();
  c.send(NBT_CONTROLLER_MENU);
  dec.poll_socket(fd, 4600, false);
  CHECK(rec.count == 2 && rec.items[0].value == kDoomKeyEscape);
  rec.clear();
  c.send(NBT_CONTROLLER_OPTION);
  dec.poll_socket(fd, 4610, false);
  CHECK(rec.count == 1 && rec.items[0].type == kActionAutomap);
  rec.clear();
  c.send(NBT_CONTROLLER_FAVOURITE(2) | NBT_CONTROLLER_FAVOURITE(7));
  dec.poll_socket(fd, 4620, false);
  CHECK(rec.count == 2 && rec.items[0].type == InputAction::kKeyDown && rec.items[0].value == '3');
  c.send(0);
  dec.poll_socket(fd, 4630, false);

  /* A lost link (or the manager pausing Doom) releases what was held. */
  rec.clear();
  c.send(NBT_CONTROLLER_PUSH);
  dec.poll_socket(fd, 4700, false);
  c.send(0, 0, false);
  dec.poll_socket(fd, 4710, false);
  CHECK(!dec.connected());
  CHECK(rec.count == 2 && rec.items[1].type == InputAction::kKeyUp &&
        rec.items[1].value == kDoomKeyRCtrl);
  /* Back again with the push still held (the one that brought Doom back): no fire
   * until every key was seen up. */
  rec.clear();
  c.send(NBT_CONTROLLER_PUSH);
  dec.poll_socket(fd, 4720, false);
  CHECK(rec.count == 0);
  c.send(0);
  c.send(NBT_CONTROLLER_PUSH);
  dec.poll_socket(fd, 4730, false);
  CHECK(rec.count == 1 && rec.items[0].value == kDoomKeyRCtrl);

  /* A yes/no prompt (quit): push answers y, BACK n; other keys and no prompt unchanged. */
  CHECK(prompt_answer_key(kDoomKeyEnter, true) == 'y');
  CHECK(prompt_answer_key(kDoomKeyBackspace, true) == 'n');
  CHECK(prompt_answer_key(kDoomKeyEscape, true) == kDoomKeyEscape);
  CHECK(prompt_answer_key(kDoomKeyUp, true) == kDoomKeyUp);
  CHECK(prompt_answer_key(kDoomKeyEnter, false) == kDoomKeyEnter);
  CHECK(prompt_answer_key(kDoomKeyBackspace, false) == kDoomKeyBackspace);
  g_tests_run++;
}

void test_gamepad() {
  using namespace nbt_doom;
  uint8_t bytes[NBT_GAMEPAD_WIRE_SIZE + 1];
  nbt_gamepad_wire_state st = {};
  st.connected = 1;
  GamepadDecoder pad;
  RecordedActions rec;
  pad.set_sink(&rec);
  st.lx = 0;
  st.ly = 6553;
  pad.update(st, 100, false, false);
  CHECK(rec.count == 0);
  st.ly = 6554;
  pad.update(st, 110, false, false);
  CHECK(rec.count == 1 && rec.items[0].value == kDoomKeyMoveForward);
  st.rx = 32767;
  pad.update(st, 140, false, false);
  CHECK(rec.items[rec.count - 1].type == kActionSmoothTurn &&
        rec.items[rec.count - 1].value >= 950 && rec.items[rec.count - 1].value <= 1020);
  rec.clear();
  pad.update(st, 240, false, false, kTurnLow);
  const int low_turn = rec.items[rec.count - 1].value;
  rec.clear();
  pad.update(st, 340, false, false, kTurnHigh);
  const int high_turn = rec.items[rec.count - 1].value;
  CHECK(low_turn >= 2100 && high_turn >= 4300 && high_turn > low_turn);
  rec.clear();
  pad.update(st, 140, false, false);
  rec.clear();
  st.ly = 0;
  st.rx = 0;
  st.rt = 16384;
  st.lt = 16384;
  pad.update(st, 150, false, false);
  CHECK(rec.count == 3);  // movement release, fire, sprint
  CHECK(rec.items[1].value == kDoomKeyRCtrl && rec.items[2].value == kDoomKeySpeed);
  rec.clear();
  st.rt = 14000;
  st.lt = 14000;
  pad.update(st, 160, false, false);
  CHECK(rec.count == 0);
  st.rt = 13000;
  st.lt = 13000;
  pad.update(st, 170, false, false);
  CHECK(rec.count == 2);
  rec.clear();
  st.lt = 65535;
  pad.update(st, 180, false, true);
  CHECK(rec.count == 0);  // already running
  st.buttons = NBT_GAMEPAD_LB | NBT_GAMEPAD_RB | NBT_GAMEPAD_VIEW;
  pad.update(st, 190, false, true);
  CHECK(rec.count == 3 && rec.items[0].type == kActionPreviousWeapon &&
        rec.items[1].type == kActionNextWeapon && rec.items[2].type == kActionAutomap);
  rec.clear();
  st.buttons = NBT_GAMEPAD_SOUTH | NBT_GAMEPAD_UP;
  st.lt = 0;
  pad.update(st, 200, true, true);
  CHECK(rec.count == 4 && rec.items[0].value == kDoomKeyUp && rec.items[2].value == kDoomKeyEnter);
  rec.clear();
  pad.update(st, 599, true, true);
  CHECK(rec.count == 0);
  pad.update(st, 600, true, true);
  CHECK(rec.count == 2 && rec.items[0].value == kDoomKeyUp);
  rec.clear();
  st.connected = 0;
  st.buttons = 0;
  pad.update(st, 610, false, false);
  CHECK(rec.count == 0);
  int sockets[2];
  CHECK(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) == 0);
  // Preserve a short menu press/release queued between game tics.
  GamepadDecoder queued;
  rec.clear();
  queued.set_sink(&rec);
  st.connected = 1;
  st.buttons = 0;
  st.lx = st.ly = st.rx = st.ry = 0;
  st.lt = st.rt = 0;
  for (unsigned sequence = 1; sequence <= 3; ++sequence) {
    st.sequence = sequence;
    st.buttons = sequence == 2 ? NBT_GAMEPAD_SOUTH : 0;
    CHECK(nbt_gamepad_wire_encode(&st, bytes, sizeof(bytes)) == 24);
    CHECK(send(sockets[0], bytes, 24, 0) == 24);
  }
  queued.poll_socket(sockets[1], 700, true, true);
  CHECK(rec.count == 2 && rec.items[0].value == kDoomKeyEnter);
  // Entering menu while Menu and Left Stick are still held must not disconnect or double-Escape.
  GamepadDecoder transition;
  rec.clear();
  transition.set_sink(&rec);
  st.sequence = 10;
  st.buttons = 0;
  st.ly = 0;
  CHECK(nbt_gamepad_wire_encode(&st, bytes, sizeof(bytes)) == 24);
  CHECK(send(sockets[0], bytes, 24, 0) == 24);
  st.sequence = 11;
  st.buttons = NBT_GAMEPAD_MENU;
  st.ly = 20000;
  CHECK(nbt_gamepad_wire_encode(&st, bytes, sizeof(bytes)) == 24);
  CHECK(send(sockets[0], bytes, 24, 0) == 24);
  transition.poll_socket(sockets[1], 800, false, true);
  CHECK(rec.count == 3 && rec.items[0].value == kDoomKeyMoveForward &&
        rec.items[1].value == kDoomKeyEscape);
  rec.clear();
  transition.poll_socket(sockets[1], 830, true, true);
  CHECK(rec.count == 1 && rec.items[0].type == InputAction::kKeyUp &&
        rec.items[0].value == kDoomKeyMoveForward);
  rec.clear();
  st.sequence = 12;
  st.buttons = NBT_GAMEPAD_MENU | NBT_GAMEPAD_DOWN;
  CHECK(nbt_gamepad_wire_encode(&st, bytes, sizeof(bytes)) == 24);
  CHECK(send(sockets[0], bytes, 24, 0) == 24);
  transition.poll_socket(sockets[1], 860, true, true);
  CHECK(rec.count == 2 && rec.items[0].value == kDoomKeyDown);
  rec.clear();
  close(sockets[0]);
  close(sockets[1]);
  /* The pad's family (bridge 1.5): its profile, by position, and one change
   * for the notice when it is plugged in or out. */
  GamepadDecoder family;
  rec.clear();
  family.set_sink(&rec);
  CHECK(family.pad_family() == -1 && !family.take_pad_change());
  st = nbt_gamepad_wire_state();
  st.connected = 1;
  st.family = NBT_GAMEPAD_FAMILY_DUALSENSE;
  st.buttons = NBT_GAMEPAD_WEST; /* Square */
  family.update(st, 1000, false, false);
  CHECK(family.take_pad_change() && !family.take_pad_change());
  CHECK(family.pad_family() == static_cast<int>(NBT_GAMEPAD_FAMILY_DUALSENSE));
  CHECK(rec.count == 1 && rec.items[0].value == kDoomKeyRCtrl);
  st.connected = 0;
  st.buttons = 0;
  family.update(st, 1010, false, false);
  CHECK(family.take_pad_change() && family.pad_family() == -1);
  CHECK(rec.count == 2 && rec.items[1].type == InputAction::kKeyUp);
  g_tests_run++;
}

void test_sdl_compat_bmp_and_settings() {
  /* Minimal 1x1 24-bit BMP */
  const uint8_t bmp[58] = {
      'B', 'M', 58, 0, 0, 0, 0, 0, 0,  0, 54, 0, 0, 0, 40, 0,  0,  0, 1, 0,
      0,   0,   1,  0, 0, 0, 1, 0, 24, 0, 0,  0, 0, 0, 4,  0,  0,  0, 0, 0,
      0,   0,   0,  0, 0, 0, 0, 0, 0,  0, 0,  0, 0, 0, 10, 20, 30, 0,
  };
  SDL_RWops* rw = SDL_RWFromConstMem(bmp, sizeof(bmp));
  CHECK(rw != NULL);
  SDL_Surface* surf = SDL_LoadBMP_RW(rw, 1);
  CHECK(surf != NULL && surf->w == 1 && surf->h == 1);
  const uint8_t* px = static_cast<const uint8_t*>(surf->pixels);
  CHECK(px[0] == 30 && px[1] == 20 && px[2] == 10); /* BGR->RGB */
  SDL_FreeSurface(surf);
  CHECK(IMG_Load("nonexistent.png") == NULL);

  nbt_doom::AppSettings s;
  nbt_doom::settings_defaults(&s);
  CHECK(!s.debug_log);
  nbt_doom::settings_apply_kv(&s, "turn_sens", "high");
  nbt_doom::settings_apply_kv(&s, "brightness", "4");
  nbt_doom::settings_apply_kv(&s, "brightness", "999"); /* invalid rejected */
  nbt_doom::settings_apply_kv(&s, "aspect", "wide");    /* removed settings safely ignored */
  nbt_doom::settings_apply_kv(&s, "res_scale", "half");
  nbt_doom::settings_apply_kv(&s, "debug_log", "on");
  CHECK(s.turn_sens == nbt_doom::kTurnHigh);
  CHECK(s.brightness == 4);
  CHECK(s.debug_log);
  nbt_doom::settings_apply_kv(&s, "debug_log", "bogus");
  CHECK(s.debug_log);
  nbt_doom::settings_apply_kv(&s, "debug_log", "off");
  CHECK(!s.debug_log);
  /* From the SDK's settings: the environment, then defaults for the rest;
   * values outside Doom's choices are ignored. */
  setenv("NBT_SETTING_DEBUG_LOG", "on", 1);
  setenv("NBT_SETTING_BRIGHTNESS", "9", 1);
  nbt::app::Settings values;
  nbt::app::settings_load_env(&values);
  unsetenv("NBT_SETTING_DEBUG_LOG");
  unsetenv("NBT_SETTING_BRIGHTNESS");
  nbt_doom::settings_from(values, &s);
  CHECK(s.debug_log && s.brightness == 2 && s.turn_sens == nbt_doom::kTurnMedium);
  g_tests_run++;
}

void test_doom_log() {
  /* Messages go to stderr only (the manager keeps the log); a doom.log from an
   * older version is removed. */
  char log_dir[] = "/tmp/nbt-doom-log-test-XXXXXX";
  CHECK(mkdtemp(log_dir) != NULL);
  char old_log[160], log_file[160];
  snprintf(old_log, sizeof(old_log), "%s/doom.log", log_dir);
  snprintf(log_file, sizeof(log_file), "%s/stderr", log_dir);
  FILE* old_fp = fopen(old_log, "w");
  CHECK(old_fp != NULL);
  fputs("left by an older version\n", old_fp);
  fclose(old_fp);
  fflush(stderr);
  const int saved_stderr = dup(STDERR_FILENO);
  const int capture = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  CHECK(saved_stderr >= 0 && capture >= 0);
  dup2(capture, STDERR_FILENO);
  close(capture);
  setenv("NBT_DATA_DIR", log_dir, 1);
  nbt::app::log_init("doom");
  nbt_doom::doom_log_start(false);
  nbt::app::log_debug("suppressed_debug_line");
  nbt::app::log_set_debug(true);
  nbt::app::log_debug("enabled_debug_line");
  unsetenv("NBT_DATA_DIR");
  fflush(stderr);
  dup2(saved_stderr, STDERR_FILENO);
  close(saved_stderr);
  CHECK(access(old_log, F_OK) != 0);
  FILE* lfp = fopen(log_file, "r");
  CHECK(lfp != NULL);
  char lbuf[512];
  size_t lread = fread(lbuf, 1, sizeof(lbuf) - 1, lfp);
  fclose(lfp);
  lbuf[lread] = '\0';
  CHECK(strstr(lbuf, "[doom] === nbt-doom start ===") != NULL);
  CHECK(strstr(lbuf, "suppressed_debug_line") == NULL);
  CHECK(strstr(lbuf, "[doom][dbg] enabled_debug_line") != NULL);
  nbt::app::log_set_debug(false);
  unlink(log_file);
  rmdir(log_dir);
  g_tests_run++;
}

}  // namespace

int main() {
  test_logical_resolution();
  test_wad_vfs();
  test_idrive_controller_states();
  test_gamepad();
  test_sdl_compat_bmp_and_settings();
  test_doom_log();
  printf("ALL %d UNIT TEST SUITES PASSED\n", g_tests_run);
  return 0;
}
