/*
 * crashedit - Message area editor for AmigaOS
 *
 * This file is part of the crashedit project.
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet 2:341/207@fidonet
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This program uses JAMLIB, which is licensed under the GNU Lesser
 * General Public License v2.1. See src/jamlib/LICENSE for details.
 */

/* portable.c -- C89, one function per platform, no scope blocks */

#if !defined(PLATFORM_WIN32) && !defined(PLATFORM_AMIGA)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif

#include "portable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WIN32)
#include <direct.h>
#include <windows.h>
#elif defined(PLATFORM_AMIGA)
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/memory.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif

struct PfLockFile
{
    char path[300];
#if defined(PLATFORM_WIN32)
    HANDLE handle;
#elif defined(PLATFORM_AMIGA)
    BPTR fileLock;
#else
    int fd;
#endif
};

int port_mkdir_one(const char *p)
{
#ifdef PLATFORM_AMIGA
    BPTR l;

    l = CreateDir((STRPTR)p);

    if (l)
    {
        UnLock(l);
        return 0;
    }

    return -1;

#elif defined(PLATFORM_WIN32)
    return _mkdir(p);
#else
    return mkdir(p, 0755);
#endif
}

int port_file_create_empty(const char *p)
{
#ifdef PLATFORM_AMIGA
    BPTR fh;

    fh = Open((STRPTR)p, MODE_NEWFILE);

    if (fh)
    {
        Close(fh);
        return 0;
    }

    return -1;

#else
    FILE *f = NULL;

    f = fopen(p, "w");

    if (f)
    {
        fclose(f);
        return 0;
    }

    return -1;
#endif
}

void port_get_config_dir(char *buf, size_t bufsz)
{
    char *home = NULL;

#if defined(PLATFORM_AMIGA)
    snprintf(buf, bufsz, "ENVARC:crashedit");
#elif defined(PLATFORM_WIN32)
    home = getenv("APPDATA");

    if (home && home[0])
        snprintf(buf, bufsz, "%s\\crashedit", home);
    else
        snprintf(buf, bufsz, "crashedit");
#else
    home = getenv("HOME");

    if (home && home[0])
        snprintf(buf, bufsz, "%s/.crashedit", home);
    else
        snprintf(buf, bufsz, ".crashedit");
#endif
}

#if defined(PLATFORM_WIN32)

struct PfDir
{
    HANDLE h;
    WIN32_FIND_DATAA fd;
    int started;
    int eod;
    char name[260];
};

PfDir *pf_dir_open(const char *path)
{
    PfDir *d = NULL;
    char pattern[300];

    if (!path || !path[0])
        return NULL;

    if ((size_t)snprintf(pattern, sizeof(pattern), "%s\\*", path) >= sizeof(pattern))
        return NULL;

    d = (PfDir *)malloc(sizeof(*d));

    if (!d)
        return NULL;

    d->h = FindFirstFileA(pattern, &d->fd);

    if (d->h == INVALID_HANDLE_VALUE)
    {
        free(d);
        return NULL;
    }

    d->started = 0;
    d->eod = 0;
    d->name[0] = '\0';
    return d;
}

const char *pf_dir_next(PfDir *d)
{
    if (!d || d->eod)
        return NULL;

    for (;;)
    {
        if (d->started)
        {
            if (!FindNextFileA(d->h, &d->fd))
            {
                d->eod = 1;
                return NULL;
            }
        }
        else
        {
            d->started = 1;
        }

        if (d->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        if (d->fd.cFileName[0] == '.' && (d->fd.cFileName[1] == '\0' || (d->fd.cFileName[1] == '.' && d->fd.cFileName[2] == '\0')))
            continue;

        strncpy(d->name, d->fd.cFileName, sizeof(d->name) - 1);
        d->name[sizeof(d->name) - 1] = '\0';

        return d->name;
    }
}

void pf_dir_close(PfDir *d)
{
    if (!d)
        return;

    if (d->h != INVALID_HANDLE_VALUE)
        FindClose(d->h);

    free(d);
}

#elif defined(PLATFORM_AMIGA)

struct PfDir
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char name[108];
};

PfDir *pf_dir_open(const char *path)
{
    PfDir *d = NULL;

    if (!path)
        return NULL;

    d = (PfDir *)AllocMem((LONG)sizeof(*d), MEMF_CLEAR);

    if (!d)
        return NULL;

    d->fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);

    if (!d->fib)
    {
        FreeMem(d, (LONG)sizeof(*d));
        return NULL;
    }

    d->lock = Lock((STRPTR)(path[0] ? path : ""), ACCESS_READ);

    if (!d->lock)
    {
        FreeDosObject(DOS_FIB, d->fib);
        FreeMem(d, (LONG)sizeof(*d));
        return NULL;
    }

    if (!Examine(d->lock, d->fib) || d->fib->fib_DirEntryType <= 0)
    {
        UnLock(d->lock);
        FreeDosObject(DOS_FIB, d->fib);
        FreeMem(d, (LONG)sizeof(*d));
        return NULL;
    }

    d->name[0] = '\0';

    return d;
}

const char *pf_dir_next(PfDir *d)
{
    if (!d)
        return NULL;

    while (ExNext(d->lock, d->fib))
    {
        if (d->fib->fib_DirEntryType > 0)
            continue;

        strncpy(d->name, d->fib->fib_FileName, sizeof(d->name) - 1);
        d->name[sizeof(d->name) - 1] = '\0';

        return d->name;
    }
    return NULL;
}

void pf_dir_close(PfDir *d)
{
    if (!d)
        return;

    if (d->lock)
        UnLock(d->lock);

    if (d->fib)
        FreeDosObject(DOS_FIB, d->fib);

    FreeMem(d, (LONG)sizeof(*d));
}

#else /* POSIX */

struct PfDir
{
    DIR *dp;
    char base[300];
    char name[300];
};

PfDir *pf_dir_open(const char *path)
{
    PfDir *d = NULL;

    if (!path)
        return NULL;

    d = (PfDir *)malloc(sizeof(*d));

    if (!d)
        return NULL;

    d->dp = opendir(path[0] ? path : ".");

    if (!d->dp)
    {
        free(d);
        return NULL;
    }

    strncpy(d->base, path, sizeof(d->base) - 1);
    d->base[sizeof(d->base) - 1] = '\0';
    d->name[0] = '\0';

    return d;
}

const char *pf_dir_next(PfDir *d)
{
    struct dirent *e;
    struct stat st;
    char full[600];
    size_t bl;

    if (!d)
        return NULL;

    while ((e = readdir(d->dp)) != NULL)
    {
        if (e->d_name[0] == '.' && (e->d_name[1] == '\0' || (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;

        bl = strlen(d->base);

        if (bl == 0)
        {
            strncpy(full, e->d_name, sizeof(full) - 1);
            full[sizeof(full) - 1] = '\0';
        }
        else if (d->base[bl - 1] == '/')
        {
            snprintf(full, sizeof(full), "%s%s", d->base, e->d_name);
        }
        else
        {
            snprintf(full, sizeof(full), "%s/%s", d->base, e->d_name);
        }

        if (stat(full, &st) != 0)
            continue;

        if (!S_ISREG(st.st_mode))
            continue;

        strncpy(d->name, e->d_name, sizeof(d->name) - 1);
        d->name[sizeof(d->name) - 1] = '\0';

        return d->name;
    }

    return NULL;
}

void pf_dir_close(PfDir *d)
{
    if (!d)
        return;

    if (d->dp)
        closedir(d->dp);
    free(d);
}

#endif

#if defined(PLATFORM_WIN32)

PfLockFile *pf_lock_create(const char *path)
{
    PfLockFile *lk = NULL;
    char buf[32];
    DWORD wrote;
    int n;

    if (!path)
        return NULL;

    lk = (PfLockFile *)malloc(sizeof(*lk));

    if (!lk)
        return NULL;

    strncpy(lk->path, path, sizeof(lk->path) - 1);
    lk->path[sizeof(lk->path) - 1] = '\0';

    lk->handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    if (lk->handle == INVALID_HANDLE_VALUE)
    {
        free(lk);
        return NULL;
    }

    n = snprintf(buf, sizeof(buf), "%lu\n", (unsigned long)GetCurrentProcessId());
    wrote = 0;

    if (n > 0)
        WriteFile(lk->handle, buf, (DWORD)n, &wrote, NULL);

    return lk;
}

void pf_lock_release(PfLockFile *lk)
{
    if (!lk)
        return;

    if (lk->handle != INVALID_HANDLE_VALUE)
        CloseHandle(lk->handle);

    DeleteFileA(lk->path);
    free(lk);
}

#elif defined(PLATFORM_AMIGA)

PfLockFile *pf_lock_create(const char *path)
{
    PfLockFile *lk = NULL;
    BPTR probe;
    BPTR fh;

    if (!path)
        return NULL;

    probe = Lock((STRPTR)path, ACCESS_READ);

    if (probe)
    {
        UnLock(probe);
        return NULL;
    }

    lk = (PfLockFile *)AllocMem((LONG)sizeof(*lk), MEMF_CLEAR);

    if (!lk)
        return NULL;

    strncpy(lk->path, path, sizeof(lk->path) - 1);
    lk->path[sizeof(lk->path) - 1] = '\0';

    fh = Open((STRPTR)path, MODE_NEWFILE);

    if (!fh)
    {
        FreeMem(lk, (LONG)sizeof(*lk));
        return NULL;
    }

    Write(fh, (APTR) "0\n", 2);
    Close(fh);

    lk->fileLock = Lock((STRPTR)path, EXCLUSIVE_LOCK);

    if (!lk->fileLock)
    {
        DeleteFile((STRPTR)path);
        FreeMem(lk, (LONG)sizeof(*lk));
        return NULL;
    }

    return lk;
}

void pf_lock_release(PfLockFile *lk)
{
    if (!lk)
        return;

    if (lk->fileLock)
        UnLock(lk->fileLock);

    DeleteFile((STRPTR)lk->path);
    FreeMem(lk, (LONG)sizeof(*lk));
}

#else /* POSIX */

PfLockFile *pf_lock_create(const char *path)
{
    PfLockFile *lk = NULL;
    char buf[32];
    int n;
    ssize_t w;

    if (!path)
        return NULL;

    lk = (PfLockFile *)malloc(sizeof(*lk));

    if (!lk)
        return NULL;

    strncpy(lk->path, path, sizeof(lk->path) - 1);
    lk->path[sizeof(lk->path) - 1] = '\0';

    lk->fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0644);

    if (lk->fd < 0)
    {
        free(lk);
        return NULL;
    }

    n = snprintf(buf, sizeof(buf), "%ld\n", (long)getpid());

    if (n > 0)
    {
        w = write(lk->fd, buf, (size_t)n);
        (void)w;
    }

    return lk;
}

void pf_lock_release(PfLockFile *lk)
{
    if (!lk)
        return;

    if (lk->fd >= 0)
        close(lk->fd);

    unlink(lk->path);

    free(lk);
}

#endif

#if defined(PLATFORM_WIN32)

int pf_atomic_rename(const char *from, const char *to)
{
    if (!from || !to)
        return -1;

    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
}
#elif defined(PLATFORM_AMIGA)

int pf_atomic_rename(const char *from, const char *to)
{
    if (!from || !to)
        return -1;

    DeleteFile((STRPTR)to);

    return Rename((STRPTR)from, (STRPTR)to) ? 0 : -1;
}
#else

int pf_atomic_rename(const char *from, const char *to)
{
    if (!from || !to)
        return -1;
    return (rename(from, to) == 0) ? 0 : -1;
}
#endif

#if defined(PLATFORM_WIN32)
int pf_remove_file(const char *path)
{
    return (path && DeleteFileA(path)) ? 0 : -1;
}
#elif defined(PLATFORM_AMIGA)

int pf_remove_file(const char *path)
{
    return (path && DeleteFile((STRPTR)path)) ? 0 : -1;
}
#else

int pf_remove_file(const char *path)
{
    return (path && unlink(path) == 0) ? 0 : -1;
}
#endif

#if defined(PLATFORM_WIN32)
int pf_is_directory(const char *path)
{
    DWORD attr;

    if (!path || !path[0])
        return 0;

    attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return 0;

    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}
#elif defined(PLATFORM_AMIGA)

int pf_is_directory(const char *path)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    int is_dir;

    if (!path || !path[0])
        return 0;

    lock = Lock((STRPTR)path, ACCESS_READ);
    if (!lock)
        return 0;

    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);

    if (!fib)
    {
        UnLock(lock);
        return 0;
    }

    is_dir = 0;

    if (Examine(lock, fib) && fib->fib_DirEntryType > 0)
        is_dir = 1;

    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);

    return is_dir;
}
#else

int pf_is_directory(const char *path)
{
    struct stat st;

    if (!path || !path[0])
        return 0;

    if (stat(path, &st) != 0)
        return 0;

    return S_ISDIR(st.st_mode) ? 1 : 0;
}
#endif

#if defined(PLATFORM_WIN32)
void pf_sleep_ms(unsigned ms)
{
    Sleep((DWORD)ms);
}

#elif defined(PLATFORM_AMIGA)
void pf_sleep_ms(unsigned ms)
{
    LONG ticks = (LONG)((ms * 50UL + 999UL) / 1000UL);

    if (ticks < 1)
        ticks = 1;

    Delay(ticks);
}

#else
void pf_sleep_ms(unsigned ms)
{
    struct timespec ts;

    ts.tv_sec = (time_t)(ms / 1000U);
    ts.tv_nsec = (long)(ms % 1000U) * 1000000L;

    nanosleep(&ts, NULL);
}
#endif
