// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NBT_DOOM_WAD_VFS_H
#define NBT_DOOM_WAD_VFS_H

#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void nbt_vfs_reset(void);
int nbt_vfs_register_memory(const char* virtual_path, const void* data, size_t size);
void nbt_vfs_register_embedded_wads(void);

const char* nbt_vfs_resolve_wad(const char* name, const char* ext);
const char* nbt_data_dir(void);

int nbt_vfs_open(const char* path, int oflag, ...);
off_t nbt_vfs_lseek(int fd, off_t offset, int whence);
ssize_t nbt_vfs_read(int fd, void* buf, size_t count);
int nbt_vfs_filelength(int fd);
int nbt_vfs_close(int fd);
int nbt_vfs_access(const char* path, int mode);
int nbt_vfs_stat(const char* path, struct stat* buf);
FILE* nbt_vfs_fopen(const char* path, const char* mode);

#ifdef __cplusplus
}
#endif

#endif /* NBT_DOOM_WAD_VFS_H */
