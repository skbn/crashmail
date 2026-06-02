#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <proto/dos.h>
#include <dos/dos.h>

#include <shared/types.h>
#include <shared/jblist.h>

#include <oslib/osmisc.h>

void osSetComment(char *file, char *comment)
{
    /* AmigaOS supports file comments */
    SetComment(file, comment);
}

/* Returns -1 if dir was not found and errorlevel otherwise */

int osChDirExecute(char *dir, char *cmd)
{
    BPTR oldlock;
    BPTR newlock;
    int res;

    if (!(oldlock = CurrentDir(NULL)))
        return (-1);

    CurrentDir(oldlock);

    if (!(newlock = Lock(dir, ACCESS_READ)))
        return (-1);

    oldlock = CurrentDir(newlock);
    res = osExecute(cmd);

    CurrentDir(oldlock);
    UnLock(newlock);

    return (res);
}

int osExecute(char *cmd)
{
    /* AmigaOS Execute() is different from Unix system() */
    /* For now, use the simple approach with system() from libnix */
    int res;

    res = system(cmd);

    return res;
}

bool osExists(char *file)
{
    BPTR lock;

    if ((lock = Lock(file, ACCESS_READ)))
    {
        UnLock(lock);
        return (TRUE);
    }

    return (FALSE);
}

bool osMkDir(char *dir)
{
    BPTR lock;

    if ((lock = CreateDir(dir)))
    {
        UnLock(lock);
        return (TRUE);
    }

    return (FALSE);
}

bool osRename(char *oldfile, char *newfile)
{
    if (Rename(oldfile, newfile))
        return (TRUE);

    return (FALSE);
}

bool osDelete(char *file)
{
    if (DeleteFile(file))
        return (TRUE);

    return (FALSE);
}

void osSleep(int secs)
{
    /* Poll CTRL-C every 10 ticks (0.2s) instead of blocking the full duration */
    int i;
    for (i = 0; i < secs * 5; i++)
    {
        Delay(10);
        if (SetSignal(0L, 0L) & SIGBREAKF_CTRL_C)
            break;
    }
}

char *osErrorMsg(uint32_t errnum)
{
    /* For AmigaOS, we could use Fault() to get error strings,
       but for simplicity we'll use strerror from libnix */
    return (char *)strerror(errnum);
}

uint32_t osError(void)
{
    /* Return DOS error code */
    return (uint32_t)IoErr();
}
