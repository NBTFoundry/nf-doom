#ifndef NBT_DOOM_INPUT_IDRIVE_HPP
#define NBT_DOOM_INPUT_IDRIVE_HPP

#include <stddef.h>
#include <stdint.h>

#include "lifecycle.hpp"
#include "input_actions.hpp"
#include "nbt/controller/input.hpp"

namespace nbt_doom {

/* PrBoom's yes/no prompts (quit, end game, ...) take only y, n, space or
 * Escape, which the controller has no key for: while one waits, the knob's
 * push (Enter) answers yes and BACK (Backspace) answers no. */
int prompt_answer_key(int key, bool yes_no_prompt);

/* The iDrive controller as NBT Manager decodes it (bridge capability
 * `controller`, NBT_CONTROLLER_FD): knob, push, tilt, the controller's keys
 * and the favourite keys, turned into Doom actions. */
class IdriveDecoder {
 public:
  IdriveDecoder();

  void set_settings(const AppSettings& settings) { settings_ = settings; }
  void set_sink(IdriveSink* sink) { sink_ = sink; }

  /* One change of the controller's state (also what poll_socket delivers). */
  void apply(const nbt_controller_wire_state& state, const nbt::controller::Event& event,
             uint64_t now_ms, bool menu_mode);
  void poll_socket(int controller_fd, uint64_t now_ms, bool menu_mode);
  void update_timers(uint64_t now_ms, bool menu_mode);

  bool exit_requested() const { return exit_requested_; }
  bool connected() const { return input_.state().connected != 0; }

 private:
  static void deliver(void* context, const nbt_controller_wire_state& state,
                      const nbt::controller::Event& event);
  void apply_rotation(int delta, uint64_t now_ms, bool menu_mode);
  void apply_push(bool down, uint64_t now_ms, bool menu_mode);
  void apply_tilt(uint8_t dir_mask, uint64_t now_ms, bool menu_mode);
  void apply_back(bool down, uint64_t now_ms, bool menu_mode);
  void apply_menu_key(bool down);
  void update_held_key(bool* state, bool want, int key);
  void release_strafe();
  void emit_tap(int key);
  void emit(IdriveAction::Type type, int value);

  AppSettings settings_;
  IdriveSink* sink_;
  nbt::controller::Input input_;
  uint64_t poll_now_ms_;
  bool poll_menu_mode_;

  bool held_up_;
  bool held_down_;
  bool held_left_;
  bool held_right_;
  bool held_strafe_left_;
  bool held_strafe_right_;
  uint64_t strafe_until_ms_;
  bool held_fire_;
  bool held_menu_;
  bool prev_push_;

  bool back_down_;
  uint64_t back_down_ms_;
  bool back_menu_fired_;
  uint64_t last_short_tap_ms_;
  bool exit_requested_;
  uint8_t tilt_mask_;
  uint64_t last_menu_repeat_ms_;
};

}  // namespace nbt_doom

#endif /* NBT_DOOM_INPUT_IDRIVE_HPP */
