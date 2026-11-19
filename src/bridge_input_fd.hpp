#ifndef NBT_DOOM_BRIDGE_INPUT_FD_HPP
#define NBT_DOOM_BRIDGE_INPUT_FD_HPP
#include <stddef.h>
namespace nbt_doom {
bool parse_int(const char* text, long minimum, int* out);
/* The bridge's datagram input descriptor named by `text` (NBT_GAMEPAD_FD,
 * NBT_CONTROLLER_FD), made non-blocking; -1 unless it is a datagram socket
 * other than the `excluded` descriptors. */
int datagram_input_fd(const char* text, const int* excluded, size_t count);
}  // namespace nbt_doom
#endif
