#include "doom_log.hpp"

#include <stdio.h>
#include <unistd.h>

#include "nbt/app/log.hpp"
#include "wad_vfs.h"

namespace nbt_doom {

void doom_log_start(bool debug) {
  nbt::app::log_set_debug(debug);
  const char* dir = nbt_data_dir();
  if (dir && dir[0]) {
    char path[512];
    snprintf(path, sizeof(path), "%s/doom.log", dir);
    unlink(path);
  }
  nbt::app::log_info("=== nbt-doom start ===");
}

}  // namespace nbt_doom
