// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NBT_DOOM_PRBOOM_CONFIG_H
#define NBT_DOOM_PRBOOM_CONFIG_H

#define PACKAGE_NAME "PrBoom+"
#define PACKAGE_TARNAME "prboom-plus"
#define PACKAGE_VERSION "2.6.66"
#define PACKAGE_HOMEPAGE "https://github.com/coelckers/prboom-plus"
#define PACKAGE_STRING "PrBoom+ 2.6.66"

#define DOOMWADDIR "/app/data"
#define PRBOOMDATADIR "/app/data"

/*#undef WORDS_BIGENDIAN*/

#include <strings.h>
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif

/* Use POSIX unistd / dirent / usleep */
#define HAVE_UNISTD_H 1
#define HAVE_DIRENT_H 1
#define HAVE_USLEEP 1
#define HAVE_STRSIGNAL 1
#define HAVE_MKSTEMP 1
/*#undef HAVE_GETOPT*/
/*#undef HAVE_MMAP*/
/*#undef HAVE_CREATE_FILE_MAPPING*/
/*#undef HAVE_SCHED_SETAFFINITY*/
/*#undef HAVE_SYS_WAIT_H*/
/*#undef HAVE_ASM_BYTEORDER_H*/

/* GL_DOOM enabled via our GLES2 translation layer */
#define GL_DOOM 1

/* Explicitly disable desktop-only GLU / ARB shader / FBO extensions */
/*#undef USE_GLU_TESS*/
/*#undef USE_GLU_MIPMAP*/
/*#undef USE_GLU_IMAGESCALE*/
/*#undef USE_SHADERS*/
/*#undef USE_FBO_TECHNIQUE*/

/* Disable optional external codecs/libs */
/*#undef HAVE_SDL_JOYSTICKGETAXIS*/
/*#undef HAVE_LIBSDL2_IMAGE*/
/*#undef HAVE_LIBSDL2_MIXER*/
/*#undef HAVE_NET*/
/*#undef USE_SDL_NET*/
/*#undef HAVE_LIBPCREPOSIX*/
/*#undef HAVE_LIBZ*/
/*#undef HAVE_LIBMAD*/
/*#undef HAVE_LIBFLUIDSYNTH*/
/*#undef HAVE_LIBDUMB*/
/*#undef HAVE_LIBVORBISFILE*/
/*#undef HAVE_LIBPORTMIDI*/
/*#undef HAVE_ALSA*/

#define SIMPLECHECKS 1
#define ZONEIDCHECK 1
#define RANGECHECK 1

#endif /* NBT_DOOM_PRBOOM_CONFIG_H */
