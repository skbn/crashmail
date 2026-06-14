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

/* freq.c -- write FTN file requests as .req/.clo into a binkd outbound */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "freq.h"

#ifdef PLATFORM_AMIGA
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#define FREQ_PATHSEP '/'
#elif defined(PLATFORM_WIN32)
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#define FREQ_PATHSEP '\\'
#else
#include <sys/types.h>
#include <sys/stat.h>
#define FREQ_PATHSEP '/'
#endif

/* path_exists: does the path resolve to anything? */
static int freq_path_exists(const char *p)
{
#ifdef PLATFORM_AMIGA
    BPTR l;

    if (!p)
        return 0;

    l = Lock((STRPTR)p, ACCESS_READ);

    if (l)
    {
        UnLock(l);
        return 1;
    }

    return 0;
#else
    struct stat st;
    return (p && stat(p, &st) == 0);
#endif
}

/* is_directory: does the path resolve to a directory? */
static int freq_is_directory(const char *p)
{
#ifdef PLATFORM_AMIGA
    BPTR l;
    struct FileInfoBlock *fib;
    int res = 0;

    if (!p)
        return 0;

    l = Lock((STRPTR)p, ACCESS_READ);

    if (l)
    {
        fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);

        if (fib)
        {
            if (Examine(l, fib))
                res = (fib->fib_DirEntryType >= 0) ? 1 : 0;

            FreeDosObject(DOS_FIB, fib);
        }

        UnLock(l);
    }

    return res;
#else
    struct stat st;
    return (p && stat(p, &st) == 0 && (st.st_mode & S_IFDIR));
#endif
}

/* mkdir_one: create a single directory component, 0 on success */
static int freq_mkdir_one(const char *p)
{
#ifdef PLATFORM_AMIGA
    BPTR l = CreateDir((STRPTR)p);

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

static void freq_str_tolower(char *s)
{
    for (; s && *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

/* NUL-terminated bounded copy */
static void safe_copy(char *dst, const char *src, int sz)
{
    int i;

    if (!dst || sz <= 0)
        return;

    for (i = 0; i < sz - 1 && src && src[i]; i++)
        dst[i] = src[i];

    dst[i] = '\0';
}

/* Create directory and parents, 0 on success, -1 on failure */
static int freq_mkdir_recursive(const char *path)
{
    char tmp[FREQ_MAX_PATH];
    size_t len;
    size_t i;

    if (!path || !path[0])
        return -1;

    len = strlen(path);

    if (len >= sizeof(tmp))
        return -1;

    memcpy(tmp, path, len + 1);

    /* Strip trailing separator so final mkdir targets leaf */
    if (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
        tmp[len - 1] = '\0';

    for (i = 1; tmp[i]; i++)
    {
        if (tmp[i] == '/' || tmp[i] == '\\')
        {
            char saved = tmp[i];
            tmp[i] = '\0';

            if (tmp[0] && !freq_path_exists(tmp))
            {
                if (freq_mkdir_one(tmp) != 0 && !freq_path_exists(tmp))
                    return -1;
            }

            tmp[i] = saved;
        }
    }

    if (!freq_path_exists(tmp))
    {
        if (freq_mkdir_one(tmp) != 0 && !freq_path_exists(tmp))
            return -1;
    }

    return 0;
}

/* Reject unsafe filenames (traversal, absolute, separators, control chars) */
static int freq_is_safe_filename(const char *name)
{
    const char *p;

    if (!name || !name[0])
        return 0;

    /* No absolute paths, no parent traversal */
    if (name[0] == '/' || name[0] == '\\')
        return 0;

    if (strstr(name, "..") != NULL)
        return 0;

    for (p = name; *p; p++)
    {
        unsigned char c = (unsigned char)*p;

        /* Reject separators and control chars */
        if (c == '/' || c == '\\')
            return 0;

        if (c < 0x20)
            return 0;
    }

    return 1;
}

/* Address parsing */
int freq_parse_addr(const char *addr, unsigned int *zone, unsigned int *net, unsigned int *node, unsigned int *point)
{
    unsigned int z = 0, n = 0, f = 0, p = 0;
    int parsed;

    if (!addr)
        return -1;

    /* Try full format with point first: z:n/f.p */
    parsed = sscanf(addr, "%u:%u/%u.%u", &z, &n, &f, &p);

    /* If that fails, try without point: z:n/f */
    if (parsed < 3)
        parsed = sscanf(addr, "%u:%u/%u", &z, &n, &f);

    if (parsed < 3)
        return -1;

    if (zone)
        *zone = z;

    if (net)
        *net = n;

    if (node)
        *node = f;

    if (point)
        *point = p;

    return 0;
}

/* Path builders (mirror freq.c build_aso_paths / build_bso_paths) */
static int build_aso_paths(const char *outbound, unsigned int zone, unsigned int net, unsigned int node, unsigned int point, char *req_path, char *clo_path, int pathsize)
{
    char *dot;

    if (freq_mkdir_recursive(outbound) < 0 && !freq_path_exists(outbound))
        return -1;

    if (snprintf(req_path, (size_t)pathsize, "%s%c%u.%u.%u.%u.req", outbound, FREQ_PATHSEP, zone, net, node, point) >= pathsize)
        return -1;

    safe_copy(clo_path, req_path, pathsize);
    dot = strrchr(clo_path, '.');

    if (dot)
        strcpy(dot, ".clo");

    return 0;
}

static int build_bso_paths(const char *outbound, unsigned int zone, unsigned int net, unsigned int node, unsigned int point, int use_zone_ext, char *req_path, char *clo_path, int pathsize)
{
    char zone_dir[FREQ_MAX_PATH];
    char node_dir[FREQ_MAX_PATH];
    const char *base_dir;

    if (use_zone_ext)
    {
        int zr;

        /* FTS-5005: zones above 0xFFF use 4 hex digits, else 3 */
        if (zone > 0xFFF)
            zr = snprintf(zone_dir, sizeof(zone_dir), "%s.%04x", outbound, zone & 0xFFFFU);
        else
            zr = snprintf(zone_dir, sizeof(zone_dir), "%s.%03x", outbound, zone & 0xFFFFU);

        if (zr >= (int)sizeof(zone_dir))
            return -1;

        freq_str_tolower(zone_dir);
        base_dir = zone_dir;
    }
    else
    {
        base_dir = outbound;
    }

    if (freq_mkdir_recursive(base_dir) < 0 && !freq_path_exists(base_dir))
        return -1;

    if (point == 0)
    {
        if (snprintf(req_path, (size_t)pathsize, "%s%c%04x%04x.req", base_dir, FREQ_PATHSEP, net & 0xFFFFU, node & 0xFFFFU) >= pathsize)
            return -1;

        if (snprintf(clo_path, (size_t)pathsize, "%s%c%04x%04x.clo", base_dir, FREQ_PATHSEP, net & 0xFFFFU, node & 0xFFFFU) >= pathsize)
            return -1;
    }
    else
    {
        if (snprintf(node_dir, sizeof(node_dir), "%s%c%04x%04x.pnt", base_dir, FREQ_PATHSEP, net & 0xFFFFU, node & 0xFFFFU) >= (int)sizeof(node_dir))
            return -1;

        freq_str_tolower(node_dir);

        if (freq_mkdir_recursive(node_dir) < 0 && !freq_path_exists(node_dir))
            return -1;

        if (snprintf(req_path, (size_t)pathsize, "%s%c%08lx.req", node_dir, FREQ_PATHSEP, (unsigned long)point) >= pathsize)
            return -1;

        if (snprintf(clo_path, (size_t)pathsize, "%s%c%08lx.clo", node_dir, FREQ_PATHSEP, (unsigned long)point) >= pathsize)
            return -1;
    }

    return 0;
}

/* Public writer */
int freq_write(const char *outbound, int mode, unsigned int zone, unsigned int net, unsigned int node, unsigned int point, char *const *filenames, int nfiles, const char *password, long newer_than, int update, char *out_req_path, int out_req_path_sz)
{
    char req_path[FREQ_MAX_PATH];
    char clo_path[FREQ_MAX_PATH];
    FILE *f;
    int i;
    int written = 0;

    if (!outbound || !outbound[0] || !filenames || nfiles <= 0)
        return -1;

    if (!freq_is_directory(outbound))
    {
        /* Try to create it; binkd outbound may not exist yet */
        if (freq_mkdir_recursive(outbound) < 0)
            return -1;
    }

    switch (mode)
    {
    case FREQ_MODE_ASO:
        if (build_aso_paths(outbound, zone, net, node, point, req_path, clo_path, FREQ_MAX_PATH) != 0)
            return -1;

        break;
    case FREQ_MODE_BSO:
        if (build_bso_paths(outbound, zone, net, node, point, 0, req_path, clo_path, FREQ_MAX_PATH) != 0)
            return -1;

        break;
    case FREQ_MODE_BSO_EXT:
        if (build_bso_paths(outbound, zone, net, node, point, 1, req_path, clo_path, FREQ_MAX_PATH) != 0)
            return -1;

        break;
    default:
        return -1; /* unset / unknown */
    }

    /* Write all filenames to the .req */
    f = fopen(req_path, "w");

    if (!f)
        return -1;

    for (i = 0; i < nfiles; i++)
    {
        const char *fname = filenames[i];

        if (!freq_is_safe_filename(fname))
        {
            fclose(f);

            /* Remove empty .req if nothing was written yet */
            if (written == 0)
                remove(req_path);

            return -1;
        }

        fprintf(f, "%s", fname);

        if (password && password[0])
            fprintf(f, " !%s", password);

        if (newer_than > 0)
            fprintf(f, " +%ld", newer_than);

        if (update)
            fprintf(f, " U");

        fprintf(f, "\r\n");
        written++;
    }

    fclose(f);

    if (written == 0)
        return -1;

    /* Reference .req from .clo so the mailer picks it up */
    f = fopen(clo_path, "w");

    if (!f)
        return -1;

    fprintf(f, "%s\r\n", req_path);
    fclose(f);

    if (out_req_path && out_req_path_sz > 0)
        safe_copy(out_req_path, req_path, out_req_path_sz);

    return 0;
}
