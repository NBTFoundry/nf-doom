#ifndef NBT_DOOM_LOG_HPP
#define NBT_DOOM_LOG_HPP

namespace nbt_doom {

/* Doom's log start: debug lines on or off, the doom.log file older versions
 * kept in the data folder removed (the manager keeps the log now), and the
 * start banner. The tag ("[doom]") is set first thing in main. */
void doom_log_start(bool debug);

}  // namespace nbt_doom

#endif /* NBT_DOOM_LOG_HPP */
