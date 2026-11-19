// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "doomtype.h"
#include "lprintf.h"
#include "wad_vfs.h"

#define fopen nbt_vfs_fopen
#define open nbt_vfs_open
#define access nbt_vfs_access
#define stat(p, b) nbt_vfs_stat((p), (b))

#include "../../third_party/prboom-plus/prboom2/src/m_io.c"
