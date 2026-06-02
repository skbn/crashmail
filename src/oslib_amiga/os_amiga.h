#ifndef OS_AMIGA_H
#define OS_AMIGA_H

#include <stddef.h>
#include <stdint.h>

typedef uint16_t UINT16; /* Unsigned 16-bit integer */

#define OS_EXIT_ERROR   10
#define OS_EXIT_OK      0

#define OS_PLATFORM_NAME "AmigaOS"
#define OS_PATH_CHARS "/:"
#define OS_CURRENT_DIR ""

#define OS_CONFIG_NAME "crashmail.prefs"
#define OS_CONFIG_VAR "CMCONFIGFILE"

/* AmigaOS doesn't have syslog */
#undef OS_HAS_SYSLOG

/*
   OS_PATH_CHARS is used by MakeFullPath. If path doesn't end with one of these characters,
   the first character will be appended to it.

   Example:

   OS_PATH_CHARS = "/:"

   "inbound" + "file"  --> "inbound/file"
   "inbound/" + "file" --> "inbound/file"
   "inbound:" + "file" --> "inbound:file"
   "RAM:" + "file"     --> "RAM:file"
*/

/* AmigaOS case-insensitive string functions */
#define stricmp strcasecmp
#define strnicmp strncasecmp

#endif /* OS_AMIGA_H */
