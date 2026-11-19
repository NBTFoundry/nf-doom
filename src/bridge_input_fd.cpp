#include "bridge_input_fd.hpp"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
namespace nbt_doom {
bool parse_int(const char* text, long min_val, int* out) {
  if (!text || !text[0] || !out) return false;
  char* end = nullptr;
  errno = 0;
  long v = strtol(text, &end, 10);
  if (errno || !end || end == text || *end != '\0' || v < min_val || v > INT_MAX) return false;
  *out = static_cast<int>(v);
  return true;
}

int datagram_input_fd(const char* text, const int* excluded, size_t count) {
  int fd = -1, type = 0;
  if (!parse_int(text, 3, &fd)) return -1;
  for (size_t i = 0; i < count; ++i)
    if (fd == excluded[i]) return -1;
  socklen_t size = sizeof(type);
  if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_DGRAM) return -1;
  /* POSIX promises only "not -1" on success: QNX's io-pkt sockets answered
   * F_SETFL with other values in the car (NBT Manager dev107). */
  const int flags = fcntl(fd, F_GETFL, 0);
  return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1 ? fd : -1;
}
}  // namespace nbt_doom
