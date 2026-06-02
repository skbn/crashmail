#include <stddef.h>
#include <stdint.h>

typedef uint16_t UINT16; /* Unsigned 16-bit integer */

#define OS_EXIT_ERROR   10
#define OS_EXIT_OK      0

#if defined(__linux__)
#define OS_PLATFORM_NAME "Linux"
#elif defined(__FreeBSD__)
#define OS_PLATFORM_NAME "FreeBSD"
#elif defined(__OpenBSD__)
#define OS_PLATFORM_NAME "OpenBSD"
#elif defined(__NetBSD__)
#define OS_PLATFORM_NAME "NetBSD"
#elif defined(__DragonFly__)
#define OS_PLATFORM_NAME "DragonFlyBSD"
#elif defined(__APPLE__) && defined(__MACH__)
#define OS_PLATFORM_NAME "MacOS"
#elif defined(__unix__) || defined(__unix)
#define OS_PLATFORM_NAME "Unix"
#else
#define OS_PLATFORM_NAME "unknown"
#endif

#define OS_PATH_CHARS "/"
#define OS_CURRENT_DIR "."

#define OS_CONFIG_NAME "crashmail.prefs"
#define OS_CONFIG_VAR "CMCONFIGFILE"

#define OS_HAS_SYSLOG

/*
   OS_PATH_CHARS is used by MakeFullPath. If path doesn't end with one of these characters,
   the first character will be appended to it.

   Example:

   OS_PATH_CHARS = "/:"

   "inbound" + "file"  --> "inbound/file"
   "inbound/" + "file" --> "inbound/file"
   "inbound:" + "file" --> "inbound/file"
*/

#define stricmp strcasecmp
#define strnicmp strncasecmp

