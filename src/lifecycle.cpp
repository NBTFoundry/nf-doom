#include "lifecycle.hpp"
#include "bridge_input_fd.hpp"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

namespace nbt_doom {

namespace {

volatile sig_atomic_t g_stopping = 0;
volatile sig_atomic_t g_show_requested = 0;

void handle_signal(int signum) {
  if (signum == SIGUSR1) {
    g_show_requested = 1;
  } else {
    g_stopping = 1;
  }
}

bool has_bridge_cap(const char* caps, const char* cap) {
  if (!caps || !caps[0] || !cap || !cap[0]) return false;
  size_t cap_len = strlen(cap);
  const char* p = caps;
  while (*p) {
    const char* comma = strchr(p, ',');
    size_t tok_len = comma ? static_cast<size_t>(comma - p) : strlen(p);
    if (tok_len == cap_len && strncmp(p, cap, cap_len) == 0) return true;
    if (!comma) break;
    p = comma + 1;
  }
  return false;
}

}  // namespace

uint64_t monotonic_ms() {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
  return static_cast<uint64_t>(ts.tv_sec) * 1000u + static_cast<uint64_t>(ts.tv_nsec) / 1000000u;
}

Lifecycle::Lifecycle()
    : ready_fd_(-1),
      lease_fd_(-1),
      status_fd_(-1),
      controller_fd_(-1),
      gamepad_fd_(-1),
      surface_id_(0),
      check_mode_(false),
      user_mode_(true),
      can_show_(false),
      has_live_settings_(false),
      last_heartbeat_ms_(0) {
  settings_file_[0] = '\0';
  settings_defaults(&settings_);
}

Lifecycle::~Lifecycle() {
  close_fds();
}

int Lifecycle::init(int argc, char** argv) {
  g_stopping = 0;
  g_show_requested = 0;
  ready_fd_ = -1;
  lease_fd_ = -1;
  status_fd_ = -1;
  controller_fd_ = -1;
  gamepad_fd_ = -1;
  surface_id_ = 0;
  settings_file_[0] = '\0';
  settings_defaults(&settings_);

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--ready-fd") == 0 && i + 1 < argc) {
      if (!parse_int(argv[++i], 3, &ready_fd_)) return 2;
    } else if (strcmp(argv[i], "--lease-fd") == 0 && i + 1 < argc) {
      if (!parse_int(argv[++i], 3, &lease_fd_)) return 2;
    }
  }
  if ((ready_fd_ >= 0) != (lease_fd_ >= 0) || (ready_fd_ >= 0 && ready_fd_ == lease_fd_)) {
    return 2;
  }

  if (lease_fd_ >= 0) {
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_RDLCK;
    fl.l_whence = SEEK_SET;
    if (fcntl(lease_fd_, F_SETLK, &fl) < 0) return 1;
  }

  const char* launch = getenv("NBT_LAUNCH");
  check_mode_ = (launch && strcmp(launch, "check") == 0);
  user_mode_ = (!check_mode_ && (ready_fd_ < 0 || (launch && strcmp(launch, "user") == 0)));

  const char* surf = getenv("NBT_SURFACE");
  if (user_mode_ && surf && surf[0]) {
    if (!parse_int(surf, 1, &surface_id_)) return 1;
  }

  const char* caps = getenv("NBT_BRIDGE_CAPS");
  can_show_ = user_mode_ && has_bridge_cap(caps, "show");
  has_live_settings_ = has_bridge_cap(caps, "live-settings");

  if (user_mode_ && has_bridge_cap(caps, "status")) {
    const char* sfd = getenv("NBT_STATUS_FD");
    int fd = -1;
    if (sfd && parse_int(sfd, 3, &fd)) status_fd_ = fd;
  }
  if (user_mode_ && has_bridge_cap(caps, "controller")) {
    const int excluded[] = {ready_fd_, lease_fd_, status_fd_};
    controller_fd_ = datagram_input_fd(getenv("NBT_CONTROLLER_FD"), excluded, 3);
  }
  if (user_mode_ && has_bridge_cap(caps, "gamepad")) {
    const int excluded[] = {ready_fd_, lease_fd_, status_fd_, controller_fd_};
    gamepad_fd_ = datagram_input_fd(getenv("NBT_GAMEPAD_FD"), excluded, 4);
  }
  settings_load_env(&settings_);
  if (has_live_settings_) {
    const char* sf = getenv("NBT_SETTINGS_FILE");
    if (sf && sf[0] && strlen(sf) < sizeof(settings_file_)) {
      snprintf(settings_file_, sizeof(settings_file_), "%s", sf);
      settings_load_snapshot_file(settings_file_, &settings_);
    }
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGUSR1, &sa, nullptr);
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, nullptr);
  return 0;
}

void Lifecycle::close_fds() {
  if (gamepad_fd_ >= 0) {
    close(gamepad_fd_);
    gamepad_fd_ = -1;
  }
  if (ready_fd_ >= 0) {
    close(ready_fd_);
    ready_fd_ = -1;
  }
  if (lease_fd_ >= 0) {
    close(lease_fd_);
    lease_fd_ = -1;
  }
}

bool Lifecycle::send_ready() {
  if (ready_fd_ < 0) return true;
  ssize_t n = write(ready_fd_, "R", 1);
  if (n == 1) return true;
  if (n < 0 && (errno == EAGAIN || errno == EINTR)) return true;
  return false;
}

void Lifecycle::send_status(uint64_t now_ms, const char* text) {
  if (status_fd_ < 0 || !text || !text[0]) return;
  char clean[25];
  size_t len = 0;
  while (text[len] && len < 24) {
    unsigned char ch = static_cast<unsigned char>(text[len]);
    clean[len] = (ch >= 0x20 && ch <= 0x7e) ? static_cast<char>(ch) : ' ';
    ++len;
  }
  clean[len] = '\0';
  if (len == 0) return;

  char line[64];
  int n = snprintf(line, sizeof(line), "%llu %s\n", static_cast<unsigned long long>(now_ms), clean);
  if (n <= 0 || n >= static_cast<int>(sizeof(line))) return;
  if (lseek(status_fd_, 0, SEEK_SET) == 0) {
    if (write(status_fd_, line, static_cast<size_t>(n)) == n) {
      if (ftruncate(status_fd_, static_cast<off_t>(n)) < 0) {
      }
    }
  }
}

void Lifecycle::tick(uint64_t now_ms, const char* status_text, bool* settings_changed) {
  if (settings_changed) *settings_changed = false;
  if (last_heartbeat_ms_ != 0 && now_ms - last_heartbeat_ms_ < 1000u) return;
  last_heartbeat_ms_ = now_ms;

  if (!send_ready()) {
    g_stopping = 1;
    return;
  }
  if (settings_file_[0]) {
    AppSettings next = settings_;
    if (settings_load_snapshot_file(settings_file_, &next)) {
      if (memcmp(&next, &settings_, sizeof(next)) != 0) {
        settings_ = next;
        if (settings_changed) *settings_changed = true;
      }
    }
  }
  if (status_text) send_status(now_ms, status_text);
}

bool Lifecycle::stopping() const {
  return g_stopping != 0;
}

bool Lifecycle::consume_show_requested() {
  if (!g_show_requested) return false;
  g_show_requested = 0;
  return true;
}

void Lifecycle::request_stop() {
  g_stopping = 1;
}

}  // namespace nbt_doom
