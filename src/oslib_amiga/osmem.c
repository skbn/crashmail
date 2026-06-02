#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void *osAlloc(size_t size)
{
   return malloc(size);
}

void *osAllocCleared(size_t size)
{
   return calloc(1,size);
}

void osFree(void *buf)
{
   free(buf);
}
