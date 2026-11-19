/*
 * Doom's messages go to stderr only. NBT Manager pipes an application's stdout and stderr
 * into the application's own log on the car (state/<app>/output.log), rotates it, notes how
 * the application ended (exit status or signal), and lets a paired phone read and clear it.
 * Doom keeps no log file of its own: older versions wrote doom.log in the data folder, which
 * nbt_log_reset removes.
 */
#include "nbt_log.hpp"

#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if defined(__QNXNTO__)
#include <dlfcn.h>
#include <ucontext.h>
#endif
#include <unistd.h>

#include "wad_vfs.h"

namespace nbt_doom {

namespace {

const char* volatile g_stage = "startup";
bool g_debug_enabled = false;

void write_all_fd(int fd, const char* buf, size_t len) {
  size_t off = 0;
  while (off < len) {
    ssize_t w = write(fd, buf + off, len - off);
    if (w <= 0) break;
    off += static_cast<size_t>(w);
  }
}

size_t append_str(char* dst, size_t pos, size_t cap, const char* s) {
  if (!s) s = "(null)";
  while (*s && pos + 1 < cap) {
    dst[pos++] = *s++;
  }
  return pos;
}

size_t append_hex(char* dst, size_t pos, size_t cap, uintptr_t v) {
  static const char kHex[] = "0123456789abcdef";
  char tmp[2 * sizeof(uintptr_t) + 2];
  size_t n = 0;
  if (v == 0) {
    tmp[n++] = '0';
  } else {
    while (v != 0 && n < sizeof(tmp)) {
      tmp[n++] = kHex[v & 0xfu];
      v >>= 4;
    }
  }
  pos = append_str(dst, pos, cap, "0x");
  while (n > 0 && pos + 1 < cap) {
    dst[pos++] = tmp[--n];
  }
  return pos;
}

size_t append_dec(char* dst, size_t pos, size_t cap, int v) {
  char tmp[24];
  size_t n = 0;
  unsigned int u;
  if (v < 0) {
    if (pos + 1 < cap) dst[pos++] = '-';
    u = static_cast<unsigned int>(-(v + 1)) + 1u;
  } else {
    u = static_cast<unsigned int>(v);
  }
  if (u == 0) {
    tmp[n++] = '0';
  } else {
    while (u != 0 && n < sizeof(tmp)) {
      tmp[n++] = static_cast<char>('0' + (u % 10u));
      u /= 10u;
    }
  }
  while (n > 0 && pos + 1 < cap) {
    dst[pos++] = tmp[--n];
  }
  return pos;
}

#if defined(__QNXNTO__) && defined(__ARM__)
/* "[doom] pc in /lib/libc.so.3+0x3e8dc (memcpy+0x1c)" */
void log_code_address(const char* name, uintptr_t address) {
  char msg[256];
  size_t pos = append_str(msg, 0, sizeof(msg), "[doom] ");
  pos = append_str(msg, pos, sizeof(msg), name);
  Dl_info info;
  memset(&info, 0, sizeof(info));
  if (!dladdr(reinterpret_cast<void*>(address), &info) || !info.dli_fname) {
    pos = append_str(msg, pos, sizeof(msg), " in no loaded object");
  } else {
    pos = append_str(msg, pos, sizeof(msg), " in ");
    pos = append_str(msg, pos, sizeof(msg), info.dli_fname);
    pos = append_str(msg, pos, sizeof(msg), "+");
    pos = append_hex(msg, pos, sizeof(msg), address - reinterpret_cast<uintptr_t>(info.dli_fbase));
    if (info.dli_sname) {
      pos = append_str(msg, pos, sizeof(msg), " (");
      pos = append_str(msg, pos, sizeof(msg), info.dli_sname);
      pos = append_str(msg, pos, sizeof(msg), "+");
      pos =
          append_hex(msg, pos, sizeof(msg), address - reinterpret_cast<uintptr_t>(info.dli_saddr));
      pos = append_str(msg, pos, sizeof(msg), ")");
    }
  }
  if (pos + 1 < sizeof(msg)) msg[pos++] = '\n';
  write_all_fd(STDERR_FILENO, msg, pos);
}
#endif

void crash_signal_handler(int sig, siginfo_t* info, void* uctx) {
  char msg[256];
  size_t pos = 0;
  pos = append_str(msg, pos, sizeof(msg), "[doom] FATAL sig=");
  pos = append_dec(msg, pos, sizeof(msg), sig);
  if (info) {
    pos = append_str(msg, pos, sizeof(msg), " code=");
    pos = append_dec(msg, pos, sizeof(msg), info->si_code);
    pos = append_str(msg, pos, sizeof(msg), " addr=");
    pos = append_hex(msg, pos, sizeof(msg), reinterpret_cast<uintptr_t>(info->si_addr));
  }
#if defined(__QNXNTO__) && defined(__ARM__)
  if (uctx) {
    const ucontext_t* uc = static_cast<const ucontext_t*>(uctx);
    pos = append_str(msg, pos, sizeof(msg), " pc=");
    pos = append_hex(msg, pos, sizeof(msg),
                     static_cast<uintptr_t>(uc->uc_mcontext.cpu.gpr[ARM_REG_PC]));
    pos = append_str(msg, pos, sizeof(msg), " lr=");
    pos = append_hex(msg, pos, sizeof(msg),
                     static_cast<uintptr_t>(uc->uc_mcontext.cpu.gpr[ARM_REG_LR]));
    pos = append_str(msg, pos, sizeof(msg), " sp=");
    pos = append_hex(msg, pos, sizeof(msg),
                     static_cast<uintptr_t>(uc->uc_mcontext.cpu.gpr[ARM_REG_SP]));
  }
#else
  (void)uctx;
#endif
  pos = append_str(msg, pos, sizeof(msg), " stage=");
  pos = append_str(msg, pos, sizeof(msg), g_stage);
  if (pos + 1 < sizeof(msg)) msg[pos++] = '\n';
  msg[pos] = '\0';

  write_all_fd(STDERR_FILENO, msg, pos);
#if defined(__QNXNTO__) && defined(__ARM__)
  /* Which library pc and lr are in, after the line above is out: dladdr is not
   * async-signal-safe, but the process is ending (1.0.5 crashed in a library
   * after S_Init; which one was not known). QNX 6.5's libc exports dladdr. */
  if (uctx) {
    const ucontext_t* uc = static_cast<const ucontext_t*>(uctx);
    log_code_address("pc", static_cast<uintptr_t>(uc->uc_mcontext.cpu.gpr[ARM_REG_PC]));
    log_code_address("lr", static_cast<uintptr_t>(uc->uc_mcontext.cpu.gpr[ARM_REG_LR]));
  }
#endif

  struct sigaction dfl;
  memset(&dfl, 0, sizeof(dfl));
  dfl.sa_handler = SIG_DFL;
  sigemptyset(&dfl.sa_mask);
  sigaction(sig, &dfl, NULL);
  raise(sig);
}

void emit_log_line(const char* prefix, const char* fmt, va_list ap) {
  char msg[384];
  vsnprintf(msg, sizeof(msg), fmt, ap);

  fprintf(stderr, "%s %s\n", prefix, msg);
  fflush(stderr);
}

}  // namespace

void nbt_log_reset() {
  const char* dir = nbt_data_dir();
  if (dir && dir[0]) {
    char path[512];
    snprintf(path, sizeof(path), "%s/doom.log", dir);
    unlink(path); /* the log file of older versions; the manager keeps the log now */
  }
  nbt_log("=== nbt-doom start ===");
}

void nbt_log_set_debug(bool enabled) {
  g_debug_enabled = enabled;
}

bool nbt_log_debug_enabled() {
  return g_debug_enabled;
}

void nbt_log_stage(const char* stage) {
  g_stage = stage ? stage : "(null)";
  nbt_log("stage: %s", g_stage);
}

const char* nbt_log_current_stage() {
  return g_stage;
}

void nbt_log_install_crash_handlers() {
  static const int kSignals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL};
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = crash_signal_handler;
  sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
  sigemptyset(&sa.sa_mask);
  for (size_t i = 0; i < sizeof(kSignals) / sizeof(kSignals[0]); i++) {
    sigaction(kSignals[i], &sa, NULL);
  }
}

void nbt_log(const char* fmt, ...) {
  if (!fmt) return;
  va_list ap;
  va_start(ap, fmt);
  emit_log_line("[doom]", fmt, ap);
  va_end(ap);
}

void nbt_debug(const char* fmt, ...) {
  if (!fmt || !g_debug_enabled) return;
  va_list ap;
  va_start(ap, fmt);
  emit_log_line("[doom][dbg]", fmt, ap);
  va_end(ap);
}

}  // namespace nbt_doom
