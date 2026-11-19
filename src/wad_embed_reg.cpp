// SPDX-License-Identifier: GPL-2.0-or-later
#include "wad_vfs.h"

extern "C" {
extern const unsigned char nbt_prboom_wad_start[];
extern const unsigned char nbt_prboom_wad_end[];
#ifndef NBT_NO_EMBEDDED_DOOM1
extern const unsigned char nbt_doom1_wad_start[];
extern const unsigned char nbt_doom1_wad_end[];
#endif

void nbt_vfs_register_embedded_wads(void) {
  size_t prboom_sz = static_cast<size_t>(nbt_prboom_wad_end - nbt_prboom_wad_start);
  if (prboom_sz > 0) {
    nbt_vfs_register_memory("/embedded/prboom-plus.wad", nbt_prboom_wad_start, prboom_sz);
  }
#ifndef NBT_NO_EMBEDDED_DOOM1
  size_t doom1_sz = static_cast<size_t>(nbt_doom1_wad_end - nbt_doom1_wad_start);
  if (doom1_sz > 0) {
    nbt_vfs_register_memory("/embedded/doom1.wad", nbt_doom1_wad_start, doom1_sz);
  }
#endif
}
}
