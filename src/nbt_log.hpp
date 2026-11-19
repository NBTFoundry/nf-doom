#ifndef NBT_DOOM_LOG_HPP
#define NBT_DOOM_LOG_HPP

namespace nbt_doom {

/* Messages go to stderr, which NBT Manager keeps as this application's log (see
 * nbt_log.cpp). nbt_debug lines only while the debug_log setting is on. */

void nbt_log_reset();
void nbt_log_set_debug(bool enabled);
bool nbt_log_debug_enabled();
void nbt_log(const char* fmt, ...);
void nbt_debug(const char* fmt, ...);
void nbt_log_stage(const char* stage);
const char* nbt_log_current_stage();
void nbt_log_install_crash_handlers();

}  // namespace nbt_doom

#endif /* NBT_DOOM_LOG_HPP */
