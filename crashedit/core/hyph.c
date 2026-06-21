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

#include "hyph.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>

#ifdef HAVE_HYPHEN
#include <hyphen.h>
#endif

/* Per-platform directory listing */
#ifdef PLATFORM_WIN32
#include <windows.h>
#elif defined(PLATFORM_AMIGA)
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef struct
{
    char word[HYPH_CACHE_KEY_MAX]; /* '\0' = empty slot */
    unsigned char nbreaks;
    unsigned short breaks[HYPH_MAX_BREAKS];
    short prev;
    short next;
} HyphEntry;

struct HyphDict
{
#ifdef HAVE_HYPHEN
    HyphenDict *dict;
#endif
    short head;
    short tail;
    short count;
    HyphEntry cache[HYPH_CACHE_N];
};

#ifdef HAVE_HYPHEN

static void hcache_init(HyphDict *h)
{
    int i;

    h->head = -1;
    h->tail = -1;
    h->count = 0;

    for (i = 0; i < HYPH_CACHE_N; i++)
    {
        h->cache[i].word[0] = '\0';
        h->cache[i].nbreaks = 0;
        h->cache[i].prev = -1;
        h->cache[i].next = -1;
    }
}

static void hcache_unlink(HyphDict *h, int idx)
{
    HyphEntry *e = &h->cache[idx];

    if (e->prev != -1)
        h->cache[e->prev].next = e->next;
    else
        h->head = e->next;

    if (e->next != -1)
        h->cache[e->next].prev = e->prev;
    else
        h->tail = e->prev;

    e->prev = -1;
    e->next = -1;
}

static void hcache_push_front(HyphDict *h, int idx)
{
    HyphEntry *e = &h->cache[idx];

    e->prev = -1;
    e->next = h->head;

    if (h->head != -1)
        h->cache[h->head].prev = (short)idx;

    h->head = (short)idx;

    if (h->tail == -1)
        h->tail = (short)idx;
}

static int hcache_find(const HyphDict *h, const char *word)
{
    int i;

    for (i = h->head; i != -1; i = h->cache[i].next)
    {
        if (strcmp(h->cache[i].word, word) == 0)
            return i;
    }

    return -1;
}

static int hcache_acquire_slot(HyphDict *h)
{
    int i;

    if (h->count < HYPH_CACHE_N)
    {
        for (i = 0; i < HYPH_CACHE_N; i++)
        {
            if (h->cache[i].word[0] == '\0')
            {
                h->count++;
                return i;
            }
        }
    }

    i = h->tail;
    hcache_unlink(h, i);

    return i;
}

static void hcache_put(HyphDict *h, const char *word, const unsigned short *breaks, int nbreaks)
{
    int idx, i;
    size_t wlen;

    wlen = strlen(word);

    /* Reject empty word */
    if (wlen == 0)
        return;

    if (wlen >= HYPH_CACHE_KEY_MAX)
        return;

    if (nbreaks > HYPH_MAX_BREAKS)
        nbreaks = HYPH_MAX_BREAKS;

    idx = hcache_acquire_slot(h);

    memcpy(h->cache[idx].word, word, wlen + 1);

    h->cache[idx].nbreaks = (unsigned char)nbreaks;

    for (i = 0; i < nbreaks; i++)
        h->cache[idx].breaks[i] = breaks[i];

    hcache_push_front(h, idx);
}

/* Run libhyphen and return break positions */
static int hyph_compute(HyphDict *h, const char *word, int word_len, unsigned short *out_pos, int *out_count)
{
    /* libhyphen requires larger buffers: word+5 and 5*word+5 */
    char hyphens[HYPH_CACHE_KEY_MAX + 5];
    char hyphword[5 * HYPH_CACHE_KEY_MAX + 5];
    char **rep;
    int *pos;
    int *cut;
    int i, n;

    *out_count = 0;

    rep = NULL;
    pos = NULL;
    cut = NULL;

    /* Too short or too long */
    if (word_len < 4 || word_len >= HYPH_CACHE_KEY_MAX)
        return 0;

    if (hnj_hyphen_hyphenate2(h->dict, word, word_len, hyphens, hyphword, &rep, &pos, &cut) != 0)
    {
        *out_count = 0;
    }
    else
    {
        /* Odd values mark break points */
        n = 0;

        for (i = 0; i < word_len; i++)
        {
            if ((hyphens[i] & 1) && n < HYPH_MAX_BREAKS)
                out_pos[n++] = (unsigned short)(i + 1);
        }

        *out_count = n;
    }

    /* Free libhyphen allocations */
    if (rep)
    {
        for (i = 0; i < word_len; i++)
        {
            if (rep[i])
                free(rep[i]);
        }

        free(rep);
    }

    if (pos)
        free(pos);

    if (cut)
        free(cut);

    return (*out_count > 0) ? 1 : 0;
}

#endif /* HAVE_HYPHEN */

void hyph_cache_clear(HyphDict *h)
{
#ifdef HAVE_HYPHEN
    if (h)
        hcache_init(h);
#endif
}

HyphDict *hyph_new(const char *dict_path)
{
#ifdef HAVE_HYPHEN
    HyphDict *h;
    FILE *fp;

    if (!dict_path)
        return NULL;

    /* Probe file first to avoid stderr logging */
    fp = fopen(dict_path, "rb");

    if (!fp)
        return NULL;

    fclose(fp);

    h = (HyphDict *)calloc(1, sizeof(*h));

    if (!h)
        return NULL;

    h->dict = hnj_hyphen_load(dict_path);

    if (!h->dict)
    {
        free(h);
        return NULL;
    }

    hcache_init(h);

    return h;
#else

    return NULL;
#endif
}

void hyph_free(HyphDict *h)
{
    if (!h)
        return;

#ifdef HAVE_HYPHEN
    if (h->dict)
        hnj_hyphen_free(h->dict);
#endif

    free(h);
}

int hyph_breakpoints(HyphDict *h, const char *word, int word_len, int *out_pos, int *out_count)
{
#ifdef HAVE_HYPHEN
    int i, n;
    unsigned short tmp[HYPH_MAX_BREAKS];

    if (out_count)
        *out_count = 0;

    if (!h || !h->dict || !word || word_len <= 0 || !out_pos || !out_count)
        return 0;

    /* Reject zero-length word */
    if (word[0] == '\0')
        return 0;

    /* Cache fast path */
    if (word_len < HYPH_CACHE_KEY_MAX)
    {
        int idx = hcache_find(h, word);

        if (idx != -1)
        {
            hcache_unlink(h, idx);
            hcache_push_front(h, idx);

            n = h->cache[idx].nbreaks;

            for (i = 0; i < n; i++)
                out_pos[i] = h->cache[idx].breaks[i];

            *out_count = n;

            return n > 0 ? 1 : 0;
        }
    }

    if (!hyph_compute(h, word, word_len, tmp, &n))
    {
        /* Cache negative result to avoid re-querying */
        hcache_put(h, word, tmp, 0);
        return 0;
    }

    hcache_put(h, word, tmp, n);

    for (i = 0; i < n; i++)
        out_pos[i] = tmp[i];

    *out_count = n;

    return 1;
#else

    if (out_count)
        *out_count = 0;

    return 0;
#endif
}

int hyph_is_available(void)
{
#ifdef HAVE_HYPHEN
    return 1;
#else
    return 0;
#endif
}

/* Check if filename ends with .dic */
static int hyph_ends_with_dic(const char *name)
{
    size_t len;

    if (!name)
        return 0;

    len = strlen(name);

    if (len < 4)
        return 0;

    return (tolower((unsigned char)name[len - 4]) == '.' &&
            tolower((unsigned char)name[len - 3]) == 'd' &&
            tolower((unsigned char)name[len - 2]) == 'i' &&
            tolower((unsigned char)name[len - 1]) == 'c');
}

/* Extract base name without .dic extension */
static char *hyph_extract_name(const char *name)
{
    size_t len;
    char *base;

    if (!name)
        return NULL;

    len = strlen(name);

    if (len < 4)
        return NULL;

    base = (char *)malloc(len - 3);

    if (!base)
        return NULL;

    memcpy(base, name, len - 4);
    base[len - 4] = '\0';

    return base;
}

/* Dedupe dictionary names */
static int hyph_name_exists(char **dicts, int count, const char *name)
{
    int i;

    for (i = 0; i < count; i++)
    {
        if (dicts[i] && strcmp(dicts[i], name) == 0)
            return 1;
    }

    return 0;
}

void hyph_free_dictionaries(char **dicts, int n_dicts)
{
    int i;

    if (!dicts)
        return;

    for (i = 0; i < n_dicts; i++)
    {
        if (dicts[i])
            free(dicts[i]);
    }

    free(dicts);
}

char **hyph_list_dictionaries(const char *dir_path, int *n_dicts)
{
    char **dicts;
    int capacity;
    int count;
    char *name;
    char **new_dicts;

#ifdef PLATFORM_WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pattern[300];
#elif defined(PLATFORM_AMIGA)
    BPTR lock;
    struct FileInfoBlock *fib;
#else
    DIR *d;
    struct dirent *e;
#endif

    if (!n_dicts)
        return NULL;

    *n_dicts = 0;

    if (!dir_path || !dir_path[0])
        return NULL;

    capacity = 16;
    count = 0;
    dicts = (char **)malloc(capacity * sizeof(char *));

    if (!dicts)
        return NULL;

#ifdef PLATFORM_WIN32
    snprintf(pattern, sizeof(pattern), "%s\\*.dic", dir_path);
    h = FindFirstFileA(pattern, &fd);

    if (h == INVALID_HANDLE_VALUE)
    {
        free(dicts);
        return NULL;
    }

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        name = hyph_extract_name(fd.cFileName);

        if (!name)
            continue;

        if (!hyph_name_exists(dicts, count, name))
        {
            if (count >= capacity)
            {
                if (capacity > INT_MAX / 2)
                    break;

                capacity *= 2;
                new_dicts = (char **)realloc(dicts, capacity * sizeof(char *));

                if (!new_dicts)
                {
                    free(name);

                    FindClose(h);
                    hyph_free_dictionaries(dicts, count);

                    return NULL;
                }

                dicts = new_dicts;
            }

            dicts[count++] = name;
        }
        else
        {
            free(name);
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);

#elif defined(PLATFORM_AMIGA)
    lock = Lock((STRPTR)dir_path, ACCESS_READ);

    if (!lock)
    {
        free(dicts);
        return NULL;
    }

    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);

    if (!fib)
    {
        UnLock(lock);
        free(dicts);

        return NULL;
    }

    if (Examine(lock, fib))
    {
        while (ExNext(lock, fib))
        {
            if (fib->fib_DirEntryType > 0)
                continue;

            if (!hyph_ends_with_dic(fib->fib_FileName))
                continue;

            name = hyph_extract_name(fib->fib_FileName);
            if (!name)
                continue;

            if (!hyph_name_exists(dicts, count, name))
            {
                if (count >= capacity)
                {
                    if (capacity > INT_MAX / 2)
                        break;

                    capacity *= 2;
                    new_dicts = (char **)realloc(dicts, capacity * sizeof(char *));

                    if (!new_dicts)
                    {
                        free(name);

                        FreeDosObject(DOS_FIB, fib);
                        UnLock(lock);
                        hyph_free_dictionaries(dicts, count);

                        return NULL;
                    }

                    dicts = new_dicts;
                }

                dicts[count++] = name;
            }
            else
            {
                free(name);
            }
        }
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);

#else
    d = opendir(dir_path);

    if (!d)
    {
        free(dicts);
        return NULL;
    }

    while ((e = readdir(d)) != NULL)
    {
        if (!hyph_ends_with_dic(e->d_name))
            continue;

        name = hyph_extract_name(e->d_name);
        if (!name)
            continue;

        if (!hyph_name_exists(dicts, count, name))
        {
            if (count >= capacity)
            {
                if (capacity > INT_MAX / 2)
                    break;

                capacity *= 2;
                new_dicts = (char **)realloc(dicts, capacity * sizeof(char *));

                if (!new_dicts)
                {
                    free(name);

                    closedir(d);
                    hyph_free_dictionaries(dicts, count);

                    return NULL;
                }

                dicts = new_dicts;
            }

            dicts[count++] = name;
        }
        else
        {
            free(name);
        }
    }

    closedir(d);
#endif

    if (count == 0)
    {
        free(dicts);
        return NULL;
    }

    *n_dicts = count;

    return dicts;
}
