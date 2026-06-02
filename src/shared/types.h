#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#ifdef PLATFORM_WIN32
#include <stdint.h>
typedef uint32_t ulong;
typedef long off_t;
#endif

#ifndef NO_TYPEDEF_BOOL
typedef int bool;
#endif

#define FALSE 0
#define TRUE 1

#endif
