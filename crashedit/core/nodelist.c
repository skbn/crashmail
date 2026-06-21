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

/* nodelist.c -- Parser for FTS-5000 nodelists and pointlists */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nodelist.h"

void nodelist_init(Nodelist *nl)
{
    if (!nl)
        return;

    nl->entries = NULL;
    nl->count = 0;
    nl->capacity = 0;
}

void nodelist_free(Nodelist *nl)
{
    if (!nl)
        return;

    free(nl->entries);

    nl->entries = NULL;
    nl->count = 0;
    nl->capacity = 0;
}

static int ensure_capacity(Nodelist *nl)
{
    int new_cap;
    NodelistEntry *ne;

    if (nl->count < nl->capacity)
        return 0;

    new_cap = nl->capacity ? nl->capacity * 2 : 256;
    ne = (NodelistEntry *)realloc(nl->entries, (size_t)new_cap * sizeof(NodelistEntry));

    if (!ne)
        return -1;

    nl->entries = ne;
    nl->capacity = new_cap;

    return 0;
}

/* Convert "Underscore_Name" to "Underscore Name" in place */
static void unescape_name(char *s)
{
    if (!s)
        return;

    for (; *s; s++)
    {
        if (*s == '_')
            *s = ' ';
    }
}

/* Split next CSV field starting at *p, returns pointer to field, advances *p past comma */
static char *next_field(char **p)
{
    char *start, *q;

    if (!p || !*p)
        return NULL;

    start = *p;
    q = strchr(start, ',');

    if (q)
    {
        *q = '\0';
        *p = q + 1;
    }
    else
    {
        /* last field: leave *p at terminating NUL so next call returns empty field */
        *p = start + strlen(start);
    }

    return start;
}

/* Build canonical FTN address into out[], append "@network" only if input lacks one */
static void build_addr(char *out, int outsz, int zone, int net, int node, int point, const char *default_network)
{
    char tmp[24]; /* "Z:N/F.P" with 5-digit numbers fits easily */

    if (point > 0)
        snprintf(tmp, sizeof(tmp), "%d:%d/%d.%d", zone, net, node, point);
    else
        snprintf(tmp, sizeof(tmp), "%d:%d/%d", zone, net, node);

    if (default_network && default_network[0])
        snprintf(out, (size_t)outsz, "%.23s@%.15s", tmp, default_network);
    else
        snprintf(out, (size_t)outsz, "%.23s", tmp);
}

/* Append a single entry, 0 on success, -1 on OOM */
static int append_entry(Nodelist *nl, const char *name, const char *addr)
{
    NodelistEntry *e;

    if (ensure_capacity(nl) != 0)
        return -1;

    e = &nl->entries[nl->count++];

    if (name)
    {
        strncpy(e->name, name, NODELIST_NAME_MAX - 1);
        e->name[NODELIST_NAME_MAX - 1] = '\0';
        unescape_name(e->name);
    }
    else
        e->name[0] = '\0';

    if (addr)
    {
        strncpy(e->addr, addr, NODELIST_ADDR_MAX - 1);
        e->addr[NODELIST_ADDR_MAX - 1] = '\0';
    }
    else
        e->addr[0] = '\0';

    return 0;
}

/* Trim trailing \r, \n, and spaces in place */
static void trim_trailing(char *s)
{
    int n;

    if (!s)
        return;

    n = (int)strlen(s);

    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

/* Parse one line, mutates zone/net/boss_* to track context, boss_set != 0 means in pointlist */
static void parse_line(Nodelist *nl, char *line, int *zone, int *net, int *boss_set, int *boss_z, int *boss_n, int *boss_f, const char *default_network)
{
    char *p, *type, *num, *sysop;
    int node_num;

    if (!line || !line[0])
        return;

    /* Strip CR/LF/spaces from end before parsing */
    trim_trailing(line);

    /* Skip comments and section markers */
    if (line[0] == ';' || line[0] == '#')
        return;

    p = line;
    type = next_field(&p);
    num = next_field(&p);
    next_field(&p);
    next_field(&p);
    sysop = next_field(&p);

    if (!type)
        return;

    /* "Boss,Z:N/F" — sets current point boss */
    if (strcasecmp(type, "Boss") == 0)
    {
        if (num && num[0])
        {
            int z = 0, n = 0, f = 0;

            if (sscanf(num, "%d:%d/%d", &z, &n, &f) == 3 || sscanf(num, "%d/%d", &n, &f) == 2)
            {
                if (z == 0)
                    z = (zone && *zone) ? *zone : 2;

                *boss_set = 1;
                *boss_z = z;
                *boss_n = n;
                *boss_f = f;
            }
        }

        return;
    }

    /* Numeric "number" field required for everything below */
    if (!num || !num[0])
        return;

    node_num = atoi(num);

    if (strcasecmp(type, "Zone") == 0)
    {
        *zone = node_num;
        *net = node_num; /* zone X has a default net = X */
        *boss_set = 0;

        /* Zone line itself usually has its own coordinator entry */
        if (sysop && sysop[0] && *zone > 0)
        {
            char addr[NODELIST_ADDR_MAX];

            build_addr(addr, sizeof(addr), *zone, node_num, 0, 0, default_network);
            append_entry(nl, sysop, addr);
        }

        return;
    }

    if (strcasecmp(type, "Region") == 0 || strcasecmp(type, "Host") == 0)
    {
        *net = node_num;
        *boss_set = 0;

        if (sysop && sysop[0] && *zone > 0)
        {
            char addr[NODELIST_ADDR_MAX];

            build_addr(addr, sizeof(addr), *zone, node_num, 0, 0, default_network);
            append_entry(nl, sysop, addr);
        }

        return;
    }

    if (strcasecmp(type, "Hub") == 0 || strcasecmp(type, "Pvt") == 0 || strcasecmp(type, "Hold") == 0 || strcasecmp(type, "Down") == 0)
    {
        if (sysop && sysop[0] && *zone > 0 && *net > 0)
        {
            char addr[NODELIST_ADDR_MAX];

            build_addr(addr, sizeof(addr), *zone, *net, node_num, 0, default_network);
            append_entry(nl, sysop, addr);
        }

        *boss_set = 0; /* a node line ends any open pointlist segment */

        return;
    }

    /* Empty type field = regular node OR point */
    if (type[0] == '\0')
    {
        if (*boss_set)
        {
            /* Point of the current boss */
            if (sysop && sysop[0])
            {
                char addr[NODELIST_ADDR_MAX];

                build_addr(addr, sizeof(addr), *boss_z, *boss_n, *boss_f, node_num, default_network);
                append_entry(nl, sysop, addr);
            }
        }
        else
        {
            /* Plain node under the current zone/net */
            if (sysop && sysop[0] && *zone > 0 && *net > 0)
            {
                char addr[NODELIST_ADDR_MAX];

                build_addr(addr, sizeof(addr), *zone, *net, node_num, 0, default_network);
                append_entry(nl, sysop, addr);
            }
        }

        return;
    }
}

int nodelist_load_file(Nodelist *nl, const char *path, const char *default_network)
{
    FILE *fp = NULL;
    char line[1024];
    int added = 0;
    int before;
    int zone = 0, net = 0;
    int boss_set = 0, boss_z = 0, boss_n = 0, boss_f = 0;

    if (!nl || !path || !path[0])
        return -1;

    fp = fopen(path, "rb");

    if (!fp)
        return -1;

    before = nl->count;

    while (fgets(line, sizeof(line), fp))
        parse_line(nl, line, &zone, &net, &boss_set, &boss_z, &boss_n, &boss_f, default_network);

    added = nl->count - before;

    fclose(fp);

    return added;
}

int nodelist_entry_matches(const NodelistEntry *e, const char *needle)
{
    int nl;
    int i;
    int hl_name, hl_addr;

    if (!e)
        return 0;

    if (!needle || !needle[0])
        return 1;

    nl = (int)strlen(needle);
    hl_name = (int)strlen(e->name);
    hl_addr = (int)strlen(e->addr);

    /* Case-insensitive substring on name */
    if (hl_name >= nl)
    {
        for (i = 0; i <= hl_name - nl; i++)
        {
            int j, ok = 1;

            for (j = 0; j < nl; j++)
            {
                if (tolower((unsigned char)e->name[i + j]) != tolower((unsigned char)needle[j]))
                {
                    ok = 0;
                    break;
                }
            }

            if (ok)
                return 1;
        }
    }

    /* Same on address */
    if (hl_addr >= nl)
    {
        for (i = 0; i <= hl_addr - nl; i++)
        {
            int j, ok = 1;

            for (j = 0; j < nl; j++)
            {
                if (tolower((unsigned char)e->addr[i + j]) != tolower((unsigned char)needle[j]))
                {
                    ok = 0;
                    break;
                }
            }

            if (ok)
                return 1;
        }
    }

    return 0;
}
