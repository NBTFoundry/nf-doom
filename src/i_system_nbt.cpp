#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "nbt_log.hpp"
#include "nbt_platform.hpp"
#include "wad_vfs.h"

extern "C" {
#include "doomdef.h"
#include "doomtype.h"
#include "d_main.h"
#include "e6y.h"
#include "g_game.h"
#include "i_main.h"
#include "protocol.h"
#include "i_network.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "lprintf.h"
#include "m_argv.h"
#include "m_fixed.h"
#include "r_fps.h"
#include "z_zone.h"
}

namespace nbt_doom {
static PlatformState* g_platform = NULL;

PlatformState* platform_state() {
  return g_platform;
}
void set_platform_state(PlatformState* state) {
  g_platform = state;
}
}  // namespace nbt_doom

extern "C" {

int ms_to_next_tick = 0;
int realtic_clock_rate = 100;
int displaytime = 0;
static uint64_t g_start_ms = 0;

static uint64_t now_monotonic_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000ULL +
         static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
}

int I_GetTime_MS(void) {
  uint64_t now = now_monotonic_ms();
  if (g_start_ms == 0) g_start_ms = now;
  return static_cast<int>(now - g_start_ms);
}

int I_GetTime_RealTime(void) {
  int64_t t = I_GetTime_MS();
  int64_t i = t * TICRATE / 1000;
  ms_to_next_tick = static_cast<int>((i + 1) * 1000 / TICRATE - t);
  if (ms_to_next_tick < 0) ms_to_next_tick = 0;
  if (ms_to_next_tick > (1000 / TICRATE)) {
    ms_to_next_tick = 1000 / TICRATE;
  }
  return static_cast<int>(i);
}

static int I_GetTime_FastDemo(void) {
  ms_to_next_tick = 0;
  static int fasttic = 0;
  return fasttic++;
}

static int I_TickElapsedTime_FastDemo(void) {
  return 0;
}

static int I_TickElapsedTime_RealTime(void) {
  return static_cast<int>(static_cast<int64_t>(I_GetTime_MS()) * TICRATE % 1000 * FRACUNIT / 1000);
}

int (*I_GetTime)(void) = I_GetTime_RealTime;
int (*I_TickElapsedTime)(void) = I_TickElapsedTime_RealTime;

fixed_t I_GetTimeFrac(void) {
  if (fastdemo) return FRACUNIT;
  int ms = I_GetTime_MS();
  int frac = static_cast<int>(static_cast<int64_t>(ms) * TICRATE % 1000 * FRACUNIT / 1000);
  return BETWEEN(0, FRACUNIT, frac);
}

dboolean realframe = false;
static dboolean g_in_display = false;
static int g_saved_gametic = -1;

dboolean I_StartDisplay(void) {
  if (g_in_display) return false;
  realframe = (!movement_smooth) || (gametic > g_saved_gametic);
  if (realframe) g_saved_gametic = gametic;
  g_in_display = true;
  displaytime = I_GetTime_MS();
  return true;
}
void I_EndDisplay(void) {
  g_in_display = false;
  displaytime = 0;
}

unsigned long I_GetRandomTimeSeed(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<unsigned long>(ts.tv_sec ^ ts.tv_nsec ^ getpid());
}

void I_uSleep(unsigned long usecs) {
  usleep(static_cast<useconds_t>(usecs));
}

const char* I_GetVersionString(char* buf, size_t sz) {
  snprintf(buf, sz, "%s v%s (NBT GLES2)", PACKAGE_NAME, PACKAGE_VERSION);
  return buf;
}

const char* I_SigString(char* buf, size_t sz, int signum) {
  snprintf(buf, sz, "signal %d", signum);
  return buf;
}

const char* I_GetTempDir(void) {
  return "/tmp";
}

const char* I_DoomExeDir(void) {
  return nbt_data_dir();
}

dboolean HasTrailingSlash(const char* dn) {
  if (!dn || !dn[0]) return false;
  size_t len = strlen(dn);
  return dn[len - 1] == '/' ? true : false;
}

char* I_FindFile(const char* wfname, const char* ext) {
  const char* resolved = nbt_vfs_resolve_wad(wfname, ext);
  if (!resolved) return NULL;
  size_t len = strlen(resolved) + 1;
  char* copy = static_cast<char*>(malloc(len));
  if (copy) memcpy(copy, resolved, len);
  return copy;
}

char* I_FindFileEx(const char* wfname, const char* ext) {
  return I_FindFile(wfname, ext);
}

const char* I_FindFile2(const char* wfname, const char* ext) {
  return nbt_vfs_resolve_wad(wfname, ext);
}

dboolean I_FileToBuffer(const char* filename, byte** data, int* size) {
  if (!filename) return false;
  int fd = nbt_vfs_open(filename, 0);
  if (fd < 0) return false;
  int len = nbt_vfs_filelength(fd);
  if (len <= 0 || len > 128 * 1024 * 1024) {
    nbt_vfs_close(fd);
    return false;
  }
  byte* buf = static_cast<byte*>(malloc(static_cast<size_t>(len)));
  if (!buf) {
    nbt_vfs_close(fd);
    return false;
  }
  size_t got = 0;
  while (got < static_cast<size_t>(len)) {
    ssize_t rc = nbt_vfs_read(fd, buf + got, static_cast<size_t>(len) - got);
    if (rc <= 0) {
      free(buf);
      nbt_vfs_close(fd);
      return false;
    }
    got += static_cast<size_t>(rc);
  }
  nbt_vfs_close(fd);
  if (data)
    *data = buf;
  else
    free(buf);
  if (size) *size = len;
  return true;
}

void I_Read(int fd, void* vbuf, size_t sz) {
  unsigned char* buf = static_cast<unsigned char*>(vbuf);
  while (sz > 0) {
    ssize_t rc = nbt_vfs_read(fd, buf, sz);
    if (rc <= 0) {
      I_Error("I_Read: read failed: %s", rc < 0 ? strerror(errno) : "EOF");
    }
    sz -= static_cast<size_t>(rc);
    buf += rc;
  }
}

int I_Filelength(int handle) {
  int len = nbt_vfs_filelength(handle);
  if (len < 0) I_Error("I_Filelength failed on fd %d", handle);
  return len;
}

typedef struct nbt_atexit_entry {
  atexit_func_t func;
  dboolean run_on_error;
  struct nbt_atexit_entry* next;
} nbt_atexit_entry_t;

static nbt_atexit_entry_t* g_exit_funcs = NULL;
static int g_in_exit = 0;

void I_AtExit(atexit_func_t func, dboolean run_on_error) {
  nbt_atexit_entry_t* entry = static_cast<nbt_atexit_entry_t*>(malloc(sizeof(*entry)));
  if (!entry) return;
  entry->func = func;
  entry->run_on_error = run_on_error;
  entry->next = g_exit_funcs;
  g_exit_funcs = entry;
}

void I_SafeExit(int rc) {
  nbt_doom::nbt_log("I_SafeExit rc=%d stage=%s", rc, nbt_doom::nbt_log_current_stage());
  if (!g_in_exit) {
    g_in_exit = 1;
    nbt_atexit_entry_t* entry;
    while ((entry = g_exit_funcs) != NULL) {
      g_exit_funcs = entry->next;
      if (rc == 0 || entry->run_on_error) entry->func();
      free(entry);
    }
    nbt_doom::platform_shutdown();
  }
  exit(rc);
}

int endoom_mode = 0;

ExeptionParam_t ExeptionsParams[EXEPTION_MAX + 1] = {
    {NULL},
    {"OpenGL error"},
    {NULL},
};
void I_ExeptionBegin(ExeptionsList_t exception_index) {
  (void)exception_index;
}
void I_ExeptionEnd(void) {}
void I_ExeptionProcess(void) {}

void I_Init(void) {
  if (fastdemo) {
    I_GetTime = I_GetTime_FastDemo;
    I_TickElapsedTime = I_TickElapsedTime_FastDemo;
  } else {
    I_GetTime = I_GetTime_RealTime;
    I_TickElapsedTime = I_TickElapsedTime_RealTime;
  }

  if (!(nomusicparm && nosfxparm)) {
    nbt_doom::nbt_log_stage("I_InitSound");
    I_InitSound();
  } else {
    nbt_doom::nbt_log("I_InitSound skipped (nosound)");
  }
  nbt_doom::nbt_log_stage("I_Init done");
  R_InitInterpolation();
}

void I_Init2(void) {
  I_Init();
  force_singletics_to = gametic + BACKUPTICS;
}

/* Minimal network and capture stubs (HAVE_NET and capture disabled) */
size_t sentbytes = 0;
size_t recvdbytes = 0;
void I_InitNetwork(void) {}
size_t I_GetPacket(packet_header_t* buffer, size_t buflen) {
  (void)buffer;
  (void)buflen;
  return 0;
}
void I_SendPacket(packet_header_t* packet, size_t len) {
  (void)packet;
  (void)len;
}
void I_WaitForPacket(int ms) {
  (void)ms;
}
void I_SetSoundCap(void) {}
unsigned char* I_GrabSound(int len) {
  (void)len;
  return NULL;
}
int I_ScreenShot(const char* fname) {
  (void)fname;
  return -1;
}
unsigned char* I_GrabScreen(void) {
  return NULL;
}

#ifdef __QNXNTO__
size_t strnlen(const char* s, size_t maxlen) {
  size_t n = 0;
  while (n < maxlen && s[n] != '\0') n++;
  return n;
}
#endif

}  // extern "C"
