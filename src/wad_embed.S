/* SPDX-License-Identifier: GPL-2.0-or-later */
.section .rodata

.global nbt_prboom_wad_start
.global nbt_prboom_wad_end
.balign 16
nbt_prboom_wad_start:
.incbin NBT_PRBOOM_WAD_PATH
nbt_prboom_wad_end:

#ifndef NBT_NO_EMBEDDED_DOOM1
.global nbt_doom1_wad_start
.global nbt_doom1_wad_end
.balign 16
nbt_doom1_wad_start:
.incbin NBT_DOOM1_WAD_PATH
nbt_doom1_wad_end:
#endif

#ifdef __linux__
.section .note.GNU-stack,"",%progbits
#endif
