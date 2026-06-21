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

/* areafile.c -- Area file parser */
#include "areafile.h"
#include "../core/msgbase.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AREA_INITIAL_CAPACITY 256
#define AREA_GROWTH_FACTOR 2

/* Ensure capacity for one more entry */
static int ensure_capacity(AreaList *list)
{
    int new_capacity;
    AreaEntry *new_entries = NULL;

    if (list->count < list->capacity)
        return 0;

    new_capacity = (list->capacity == 0) ? AREA_INITIAL_CAPACITY : list->capacity * AREA_GROWTH_FACTOR;

    new_entries = realloc(list->entries, sizeof(AreaEntry) * new_capacity);

    if (!new_entries)
        return -1;

    list->entries = new_entries;
    list->capacity = new_capacity;

    return 0;
}

/* Parse quoted string, returns pointer after closing quote */
static const char *parse_quoted_string_dyn(const char *p, char **out)
{
    size_t len = 0;
    const char *start;

    if (*p != '"')
        return NULL;

    p++;
    start = p;

    while (*p && *p != '"')
    {
        len++;
        p++;
    }

    *out = malloc(len + 1);

    if (*out)
    {
        memcpy(*out, start, len);
        (*out)[len] = '\0';
    }

    if (*p == '"')
        p++;

    return p;
}

/* Parse unquoted token, returns pointer after token */
static const char *parse_unquoted_token_dyn(const char *p, char **out)
{
    size_t len = 0;
    const char *start = p;

    while (*p && !isspace((unsigned char)*p))
    {
        len++;
        p++;
    }

    *out = malloc(len + 1);

    if (*out)
    {
        memcpy(*out, start, len);
        (*out)[len] = '\0';
    }

    return p;
}

/* Parse token (quoted or unquoted), returns pointer after token */
static const char *parse_token_dyn(const char *p, char **out)
{
    /* Skip leading whitespace */
    while (p && *p && isspace((unsigned char)*p))
        p++;

    /* Check if we're at end of string */
    if (!p || !*p)
    {
        *out = NULL;
        return NULL;
    }

    if (*p == '"')
        return parse_quoted_string_dyn(p, out);
    else
        return parse_unquoted_token_dyn(p, out);
}

/* Converts format string tokens to MB_FORMAT enum constants */
static int format_from_token(const char *tok)
{
    if (!tok || !tok[0])
        return MB_FORMAT_JAM;

    if (strcasecmp(tok, "JAM") == 0)
        return MB_FORMAT_JAM;

    if (strcasecmp(tok, "MSG") == 0 ||
        strcasecmp(tok, "FTS1") == 0 ||
        strcasecmp(tok, "OPUS") == 0 ||
        strcasecmp(tok, "SDM") == 0)
        return MB_FORMAT_MSG;

    return MB_FORMAT_JAM;
}

int areafile_load(AreaList *list, const char *path)
{
    FILE *fp = NULL;
    char line[1024];

    if (!list || !path)
        return 0;

    list->count = 0;
    list->capacity = 0;
    list->entries = NULL;

    fp = fopen(path, "r");

    if (!fp)
        return 0;

    while (fgets(line, sizeof(line), fp))
    {
        char *p = line;
        char keyword[32];
        int i = 0;

        /* Skip leading whitespace */
        while (isspace((unsigned char)*p))
            p++;

        /* Skip comments and empty lines */
        if (*p == '#' || *p == ';' || *p == '\0')
            continue;

        /* Extract keyword */
        while (*p && !isspace((unsigned char)*p) && i < (int)sizeof(keyword) - 1)
        {
            keyword[i++] = *p++;
        }

        keyword[i] = '\0';

        /* Skip whitespace after keyword */
        while (isspace((unsigned char)*p))
            p++;

        if (strcasecmp(keyword, "ECHO") == 0 || strcasecmp(keyword, "ECHOTAG") == 0 || strcasecmp(keyword, "AREA") == 0 || strcasecmp(keyword, "AREADEF") == 0)
        {
            AreaEntry *ae = NULL;
            const char *rest = NULL;
            char *skip_buf = NULL;
            char *type_kw = NULL;
            char *group_str = NULL;

            /* Parse GoldED+ AREADEF format */
            if (ensure_capacity(list) != 0)
                break;

            ae = &list->entries[list->count];

            memset(ae, 0, sizeof(*ae));
            ae->type = AREATYPE_ECHO; /* Echo by default */

            /* Parse area tag */
            rest = parse_token_dyn(p, &ae->name);

            if (!rest || !ae->name || !ae->name[0])
            {
                free(ae->name);
                ae->name = NULL;
                continue;
            }

            while (rest && *rest && isspace((unsigned char)*rest))
                rest++;

            /* Parse description (quoted or unquoted). Falls back to tag */
            rest = parse_token_dyn(rest, &ae->description);

            if (!ae->description || !ae->description[0])
            {
                free(ae->description);
                ae->description = strdup(ae->name ? ae->name : "");

                if (!ae->description)
                {
                    free(ae->name);
                    ae->name = NULL;

                    free(ae->path);
                    ae->path = NULL;

                    free(ae->aka);
                    ae->aka = NULL;

                    continue;
                }
            }

            while (rest && *rest && isspace((unsigned char)*rest))
                rest++;

            /* Parse group letter (single character A..Z) */
            ae->groupid = 0;
            rest = parse_token_dyn(rest, &group_str);

            if (group_str && group_str[0])
            {
                unsigned char c = (unsigned char)group_str[0];

                if (c >= 'A' && c <= 'Z')
                    ae->groupid = c - 'A' + 1;
                else if (c >= 'a' && c <= 'z')
                    ae->groupid = c - 'a' + 1;
                else if (isdigit(c))
                    ae->groupid = atoi(group_str);
            }

            free(group_str);

            while (rest && *rest && isspace((unsigned char)*rest))
                rest++;

            /* Parse base-type keyword (ECHO / NET / LOCAL) */
            rest = parse_token_dyn(rest, &type_kw);

            if (type_kw)
            {
                if (strcasecmp(type_kw, "NET") == 0 || strcasecmp(type_kw, "NETMAIL") == 0)
                    ae->type = AREATYPE_NETMAIL;
                else if (strcasecmp(type_kw, "LOCAL") == 0)
                    ae->type = AREATYPE_LOCAL;
                else
                    ae->type = AREATYPE_ECHO;

                free(type_kw);
            }

            while (rest && *rest && isspace((unsigned char)*rest))
                rest++;

            /* Storage format token (JAM / MSG / FTS1 / OPUS / SDM) */
            rest = parse_token_dyn(rest, &skip_buf);

            ae->format = format_from_token(skip_buf);

            free(skip_buf);

            skip_buf = NULL;

            while (rest && *rest && isspace((unsigned char)*rest))
                rest++;

            /* Parse path */
            rest = parse_token_dyn(rest, &ae->path);

            while (rest && *rest && isspace((unsigned char)*rest))
                rest++;

            /* Parse optional FTN address (z:n/f.p format, requires colon) */
            if (rest && *rest && *rest != '(' && strchr(rest, ':') != NULL)
            {
                const char *tok_end = rest;
                const char *colon = NULL;

                while (*tok_end && !isspace((unsigned char)*tok_end))
                    tok_end++;

                /* Only consume if this token has a ':' before any whitespace */
                colon = rest;

                while (colon < tok_end && *colon != ':')
                    colon++;

                if (colon < tok_end)
                    rest = parse_token_dyn(rest, &ae->aka);
            }

            /* Remaining "(flags)" parenthesis block is ignored for now */
            list->count++;
        }
        else if (strcasecmp(keyword, "NETMAIL") == 0 || strcasecmp(keyword, "NETAREA") == 0)
        {
            AreaEntry *ae;
            const char *rest;
            char *skip_buf = NULL;

            if (ensure_capacity(list) != 0)
                break;

            ae = &list->entries[list->count];

            memset(ae, 0, sizeof(*ae));
            ae->type = AREATYPE_NETMAIL;

            /* Crashmail format: skip AKA field if present */
            rest = p;

            if (rest && *rest && *rest != '"' && strchr(rest, ':') != NULL)
            {
                rest = parse_token_dyn(rest, &skip_buf);
                free(skip_buf);

                skip_buf = NULL;

                while (rest && *rest && isspace((unsigned char)*rest))
                    rest++;
            }

            /* Parse description (quoted or unquoted) */
            rest = parse_token_dyn(rest, &ae->description);

            if (!rest || !ae->description || !ae->description[0])
            {
                free(ae->description);
                ae->description = strdup("Netmail");
            }

            /* Skip whitespace */
            while (rest && *rest && isspace((unsigned char)*rest))
                rest++;

            /* Crashmail format: messagebase type if present */
            if (rest && *rest && *rest != '"')
            {
                rest = parse_token_dyn(rest, &skip_buf);

                ae->format = format_from_token(skip_buf);

                free(skip_buf);

                skip_buf = NULL;

                while (rest && *rest && isspace((unsigned char)*rest))
                    rest++;
            }
            else
            {
                ae->format = MB_FORMAT_JAM;
            }

            /* Parse path (quoted or unquoted) */
            rest = parse_token_dyn(rest, &ae->path);

            /* Use description as name if available, else default */
            ae->name = strdup(ae->description ? ae->description : "Netmail");

            if (!ae->name)
            {
                free(ae->description);
                ae->description = NULL;

                free(ae->path);
                ae->path = NULL;

                free(ae->aka);
                ae->aka = NULL;

                continue;
            }

            list->count++;
        }
        else if (strcasecmp(keyword, "LOCAL") == 0 || strcasecmp(keyword, "LOCALAREA") == 0)
        {
            AreaEntry *ae;
            const char *rest;
            char *skip_buf = NULL;

            if (ensure_capacity(list) != 0)
                break;

            ae = &list->entries[list->count];

            memset(ae, 0, sizeof(*ae));
            ae->type = AREATYPE_LOCAL;

            /* Crashmail format: skip AKA field if present */
            rest = p;

            if (rest && *rest && *rest != '"' && strchr(rest, ':') != NULL)
            {
                rest = parse_token_dyn(rest, &skip_buf);

                free(skip_buf);
                skip_buf = NULL;

                while (rest && *rest && isspace((unsigned char)*rest))
                    rest++;
            }

            /* Parse description (quoted or unquoted) */
            rest = parse_token_dyn(rest, &ae->description);

            if (!rest || !ae->description || !ae->description[0])
            {
                free(ae->description);
                ae->description = strdup("Local");
            }

            /* Skip whitespace */
            while (rest && *rest && isspace((unsigned char)*rest))
                rest++;

            /* Crashmail format: messagebase type if present */
            if (rest && *rest && *rest != '"')
            {
                rest = parse_token_dyn(rest, &skip_buf);

                ae->format = format_from_token(skip_buf);

                free(skip_buf);

                skip_buf = NULL;

                while (rest && *rest && isspace((unsigned char)*rest))
                    rest++;
            }
            else
            {
                ae->format = MB_FORMAT_JAM;
            }

            /* Parse path (quoted or unquoted) */
            rest = parse_token_dyn(rest, &ae->path);

            ae->name = strdup(ae->description ? ae->description : "Local");
            if (!ae->name)
            {
                free(ae->description);
                ae->description = NULL;

                free(ae->path);
                ae->path = NULL;

                free(ae->aka);
                ae->aka = NULL;

                continue;
            }

            list->count++;
        }
    }

    fclose(fp);

    return list->count;
}

void areafile_free(AreaList *list)
{
    int i;

    if (list)
    {
        for (i = 0; i < list->count; i++)
        {
            AreaEntry *ae = &list->entries[i];

            free(ae->name);
            free(ae->description);
            free(ae->path);
            free(ae->aka);
        }

        free(list->entries);

        list->entries = NULL;
        list->count = 0;
        list->capacity = 0;
    }
}

/* Refresh counts for a single area against user's lastread and lastseen */
void areafile_refresh_one(AreaEntry *ae, const char *sysop)
{
    MsgBase mb;
    int total = 0;
    int unread = 0;
    int new_count = 0;
    uint32_t lr = 0;
    uint32_t ls = 0;
    uint32_t ucrc = 0;

    if (!ae || !ae->path || !ae->path[0])
        return;

    if (mb_open(&mb, ae->path, ae->format) != 0)
    {
        ae->total_msgs = 0;
        ae->unread = 0;
        ae->new_count = 0;
        ae->lastread = 0;
        ae->lastseen = 0;
        return;
    }

    ucrc = mb_username_crc(sysop ? sysop : "");
    mb_read_lastread_pair(&mb, ucrc, &lr, &ls);

    /* lastseen never trails lastread. New bases start at 0, all messages are new */
    if (ls < lr)
        ls = lr;

    /* One pass through the headers counts total + true-unread + new */
    mb_count_msgs(&mb, lr, ls, &total, &unread, &new_count);

    ae->total_msgs = total;
    ae->unread = unread;
    ae->new_count = new_count;
    ae->lastread = lr;
    ae->lastseen = ls;

    mb_close(&mb);
}

/* Bulk recompute for every area (startup and rescan) */
void areafile_calculate_counts(AreaList *list, const char *sysop)
{
    int i;

    if (!list || !list->entries)
        return;

    for (i = 0; i < list->count; i++)
        areafile_refresh_one(&list->entries[i], sysop);
}
