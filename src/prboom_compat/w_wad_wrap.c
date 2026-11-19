#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <unistd.h>

#include "m_io.h"
#include "wad_vfs.h"

#define M_open nbt_vfs_open
#define lseek nbt_vfs_lseek
#define close nbt_vfs_close

#include "../../third_party/prboom-plus/prboom2/src/w_wad.c"
