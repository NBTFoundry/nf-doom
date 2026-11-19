#include "wad_vfs.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr int kVfsFdBase = 0x40000000;
constexpr int kMaxVfsFiles = 16;
constexpr int kMaxVfsHandles = 32;

struct VfsFile {
  char path[128];
  const unsigned char* data;
  size_t size;
  bool used;
};

struct VfsHandle {
  int file_idx;
  size_t offset;
  bool open;
};

VfsFile g_files[kMaxVfsFiles];
VfsHandle g_handles[kMaxVfsHandles];
char g_data_dir[512];
bool g_data_dir_ready = false;

bool eq_ignore_case(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

const char* basename_of(const char* path) {
  if (!path) return "";
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

int find_vfs_file(const char* path) {
  if (!path) return -1;
  for (int i = 0; i < kMaxVfsFiles; ++i) {
    if (g_files[i].used && eq_ignore_case(g_files[i].path, path)) {
      return i;
    }
  }
  return -1;
}

int handle_index(int fd) {
  if (fd < kVfsFdBase || fd >= kVfsFdBase + kMaxVfsHandles) return -1;
  int idx = fd - kVfsFdBase;
  if (!g_handles[idx].open) return -1;
  int fi = g_handles[idx].file_idx;
  if (fi < 0 || fi >= kMaxVfsFiles || !g_files[fi].used) return -1;
  return idx;
}

void ensure_dir(const char* dir) {
  struct stat st;
  if (!dir || !dir[0]) return;
  if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) return;
  mkdir(dir, 0755);
}

}  // namespace

extern "C" {

void nbt_vfs_reset(void) {
  memset(g_files, 0, sizeof(g_files));
  memset(g_handles, 0, sizeof(g_handles));
}

int nbt_vfs_register_memory(const char* virtual_path, const void* data, size_t size) {
  if (!virtual_path || !virtual_path[0] || !data || size == 0) return -1;
  if (strlen(virtual_path) >= sizeof(g_files[0].path)) return -1;
  int existing = find_vfs_file(virtual_path);
  int slot = existing;
  if (slot < 0) {
    for (int i = 0; i < kMaxVfsFiles; ++i) {
      if (!g_files[i].used) {
        slot = i;
        break;
      }
    }
  }
  if (slot < 0) return -1;
  snprintf(g_files[slot].path, sizeof(g_files[slot].path), "%s", virtual_path);
  g_files[slot].data = static_cast<const unsigned char*>(data);
  g_files[slot].size = size;
  g_files[slot].used = true;
  return 0;
}

const char* nbt_vfs_resolve_wad(const char* name, const char* ext) {
  char candidate[256];
  if (!name || !name[0]) return nullptr;
  if (strncmp(name, "/embedded/", 10) == 0) {
    int fi = find_vfs_file(name);
    if (fi >= 0) return g_files[fi].path;
  }
  const char* base = basename_of(name);
  const bool has_dot = strchr(base, '.') != nullptr;
  if (!has_dot && ext && ext[0]) {
    snprintf(candidate, sizeof(candidate), "/embedded/%s%s", base, ext);
  } else {
    snprintf(candidate, sizeof(candidate), "/embedded/%s", base);
  }
  int fi = find_vfs_file(candidate);
  if (fi >= 0) return g_files[fi].path;
  return nullptr;
}

const char* nbt_data_dir(void) {
  if (g_data_dir_ready) return g_data_dir;
  const char* env = getenv("NBT_DATA_DIR");
  if (env && env[0]) {
    snprintf(g_data_dir, sizeof(g_data_dir), "%s", env);
  } else {
    const char* home = getenv("HOME");
    if (home && home[0]) {
      snprintf(g_data_dir, sizeof(g_data_dir), "%s/.nbt-doom", home);
    } else {
      snprintf(g_data_dir, sizeof(g_data_dir), "/tmp/nbt-doom");
    }
  }
  ensure_dir(g_data_dir);
  g_data_dir_ready = true;
  return g_data_dir;
}

int nbt_vfs_open(const char* path, int oflag, ...) {
  int fi = find_vfs_file(path);
  if (fi >= 0) {
    if ((oflag & O_ACCMODE) != O_RDONLY) {
      errno = EROFS;
      return -1;
    }
    for (int i = 0; i < kMaxVfsHandles; ++i) {
      if (!g_handles[i].open) {
        g_handles[i].file_idx = fi;
        g_handles[i].offset = 0;
        g_handles[i].open = true;
        return kVfsFdBase + i;
      }
    }
    errno = EMFILE;
    return -1;
  }
  return open(path, oflag, 0644);
}

off_t nbt_vfs_lseek(int fd, off_t offset, int whence) {
  int idx = handle_index(fd);
  if (idx < 0) return lseek(fd, offset, whence);
  size_t sz = g_files[g_handles[idx].file_idx].size;
  int64_t base = 0;
  if (whence == SEEK_SET)
    base = 0;
  else if (whence == SEEK_CUR)
    base = static_cast<int64_t>(g_handles[idx].offset);
  else if (whence == SEEK_END)
    base = static_cast<int64_t>(sz);
  else {
    errno = EINVAL;
    return -1;
  }
  int64_t target = base + static_cast<int64_t>(offset);
  if (target < 0 || static_cast<uint64_t>(target) > sz) {
    errno = EINVAL;
    return -1;
  }
  g_handles[idx].offset = static_cast<size_t>(target);
  return static_cast<off_t>(target);
}

ssize_t nbt_vfs_read(int fd, void* buf, size_t count) {
  int idx = handle_index(fd);
  if (idx < 0) return read(fd, buf, count);
  if (!buf && count > 0) {
    errno = EFAULT;
    return -1;
  }
  const VfsFile& f = g_files[g_handles[idx].file_idx];
  size_t avail = (g_handles[idx].offset <= f.size) ? (f.size - g_handles[idx].offset) : 0;
  size_t n = (count < avail) ? count : avail;
  if (n > 0) {
    memcpy(buf, f.data + g_handles[idx].offset, n);
    g_handles[idx].offset += n;
  }
  return static_cast<ssize_t>(n);
}

int nbt_vfs_filelength(int fd) {
  int idx = handle_index(fd);
  if (idx >= 0) {
    size_t sz = g_files[g_handles[idx].file_idx].size;
    return sz > 0x7fffffffu ? 0x7fffffff : static_cast<int>(sz);
  }
  struct stat st;
  if (fstat(fd, &st) == -1) return -1;
  return static_cast<int>(st.st_size);
}

int nbt_vfs_close(int fd) {
  int idx = handle_index(fd);
  if (idx >= 0) {
    g_handles[idx].open = false;
    g_handles[idx].file_idx = -1;
    g_handles[idx].offset = 0;
    return 0;
  }
  return close(fd);
}

int nbt_vfs_access(const char* path, int mode) {
  int fi = find_vfs_file(path);
  if (fi >= 0) {
    if (mode & W_OK) {
      errno = EROFS;
      return -1;
    }
    return 0;
  }
  return access(path, mode);
}

int nbt_vfs_stat(const char* path, struct stat* buf) {
  int fi = find_vfs_file(path);
  if (fi >= 0) {
    if (!buf) {
      errno = EFAULT;
      return -1;
    }
    memset(buf, 0, sizeof(*buf));
    buf->st_mode = S_IFREG | 0444;
    buf->st_size = static_cast<off_t>(g_files[fi].size);
    return 0;
  }
  return stat(path, buf);
}

#ifdef __QNXNTO__
static FILE* open_temp_vfs_stream(const char* dir, const unsigned char* data, size_t size) {
  char tmpl[512];
  snprintf(tmpl, sizeof(tmpl), "%s/.nbtvfs_XXXXXX", dir);
  int fd = mkstemp(tmpl);
  if (fd < 0) return nullptr;
  unlink(tmpl);
  size_t off = 0;
  while (off < size) {
    ssize_t w = write(fd, data + off, size - off);
    if (w <= 0) {
      if (w < 0 && errno == EINTR) continue;
      close(fd);
      return nullptr;
    }
    off += static_cast<size_t>(w);
  }
  lseek(fd, 0, SEEK_SET);
  FILE* fp = fdopen(fd, "rb");
  if (!fp) close(fd);
  return fp;
}
#endif

FILE* nbt_vfs_fopen(const char* path, const char* mode) {
  int fi = find_vfs_file(path);
  if (fi < 0) return fopen(path, mode);
  if (!mode || strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+')) {
    errno = EROFS;
    return nullptr;
  }
#ifdef __QNXNTO__
  FILE* fp = open_temp_vfs_stream("/dev/shmem", g_files[fi].data, g_files[fi].size);
  if (!fp) fp = open_temp_vfs_stream("/tmp", g_files[fi].data, g_files[fi].size);
  if (!fp) fp = open_temp_vfs_stream(nbt_data_dir(), g_files[fi].data, g_files[fi].size);
  return fp;
#else
  return fmemopen(const_cast<unsigned char*>(g_files[fi].data), g_files[fi].size, "rb");
#endif
}

}  // extern "C"
