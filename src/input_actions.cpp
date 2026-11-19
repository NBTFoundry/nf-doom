#include "input_actions.hpp"
namespace nbt_doom {
InputMixer::InputMixer() : held_(), sink_(NULL) {}
void InputMixer::action(Source source, const InputAction& action) {
  if (source < 0 || source >= SourceCount || !sink_) return;
  if (action.type != InputAction::kKeyDown && action.type != InputAction::kKeyUp) {
    sink_->on_action(action);
    return;
  }
  if (action.value < 0 || action.value >= 512) return;
  bool before = false, after = false;
  for (unsigned s = 0; s < SourceCount; ++s) before = before || held_[s][action.value];
  held_[source][action.value] = action.type == InputAction::kKeyDown;
  for (unsigned s = 0; s < SourceCount; ++s) after = after || held_[s][action.value];
  if (before != after) sink_->on_action(action);
}
void InputMixer::release(Source source) {
  if (source < 0 || source >= SourceCount) return;
  for (int key = 0; key < 512; ++key) {
    if (held_[source][key]) {
      const InputAction a = {InputAction::kKeyUp, key};
      action(source, a);
    }
  }
}
}  // namespace nbt_doom
