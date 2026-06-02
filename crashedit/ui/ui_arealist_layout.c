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

/* ui_arealist_layout.c -- Area list layout computation */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "ui_arealist_layout.h"

void ui_arealist_layout_compute(const AreaList *areas, const char *fmt, int maxcol, ArealistLayout *L)
{
    const char *p;
    int spaces = 0;
    int area_found = 0, marked_found = 0, desc_found = 0;
    int count_found = 0, pmark_found = 0, unread_found = 0;
    int changed_found = 0, echoid_found = 0, groupid_found = 0;
    int pos;
    int i;

    if (!L)
        return;

    /* Initial widths (GoldED+ defaults) */
    L->area_width = 4;
    L->marked_width = 1;
    L->desc_width = -1; /* sentinel: fills the remainder */
    L->count_width = 6;
    L->pmark_width = 1;
    L->unread_width = 6;
    L->changed_width = 1;
    L->echoid_width = 0;
    L->groupid_width = 0;

    L->area_pos = L->marked_pos = L->desc_pos = -1;
    L->count_pos = L->pmark_pos = L->unread_pos = -1;
    L->changed_pos = L->echoid_pos = L->groupid_pos = -1;

    /* echoid_width = longest area name, clamped to [1, 40] */
    if (areas)
    {
        for (i = 0; i < areas->count; i++)
        {
            const char *nm = areas->entries[i].name;
            int l = nm ? (int)strlen(nm) : 0;

            if (l > L->echoid_width)
                L->echoid_width = l;
        }
    }

    if (L->echoid_width < 1)
        L->echoid_width = 1;

    if (L->echoid_width > 40)
        L->echoid_width = 40;

    /* groupid_width: 1 char (A-Z) unless any group > 26 -> 3 chars */
    L->groupid_width = 1;

    if (areas)
    {
        for (i = 0; i < areas->count; i++)
        {
            if (areas->entries[i].groupid > 26)
            {
                L->groupid_width = 3;
                break;
            }
        }
    }

    if (!fmt)
        return;

    /* Which fields appear, plus explicit widths */
    p = fmt;

    while (*p)
    {
        char c = (char)toupper((unsigned char)*p);
        char d = *(++p);
        int w = atoi(p);

        while (isdigit((unsigned char)*p))
            p++;

        if (!isalpha((unsigned char)c))
        {
            spaces++;
            continue;
        }

        switch (c)
        {
        case 'A':
            area_found = 1;

            if (isdigit((unsigned char)d))
                L->area_width = w;

            break;
        case 'M':
            marked_found = 1;

            if (isdigit((unsigned char)d))
                L->marked_width = w;

            break;
        case 'D':
            desc_found = 1;

            if (isdigit((unsigned char)d))
                L->desc_width = w;

            break;
        case 'C':
            count_found = 1;

            if (isdigit((unsigned char)d))
                L->count_width = w;

            break;
        case 'P':
            pmark_found = 1;

            if (isdigit((unsigned char)d))
                L->pmark_width = w;

            break;
        case 'U':
            unread_found = 1;

            if (isdigit((unsigned char)d))
                L->unread_width = w;

            break;
        case 'N':
            changed_found = 1;

            if (isdigit((unsigned char)d))
                L->changed_width = w;

            break;
        case 'E':
            echoid_found = 1;

            if (isdigit((unsigned char)d))
                L->echoid_width = w;

            break;
        case 'G':
            groupid_found = 1;

            if (isdigit((unsigned char)d))
                L->groupid_width = w;

            break;
        default:
            break;
        }
    }

    /* Zero-out absent fields */
    if (!area_found)
        L->area_width = 0;

    if (!marked_found)
        L->marked_width = 0;

    if (!desc_found)
        L->desc_width = 0;

    if (!count_found)
        L->count_width = 0;

    if (!pmark_found)
        L->pmark_width = 0;

    if (!unread_found)
        L->unread_width = 0;

    if (!changed_found)
        L->changed_width = 0;

    if (!echoid_found)
        L->echoid_width = 0;

    if (!groupid_found)
        L->groupid_width = 0;

    /* Description default = leftover space */
    if (desc_found && L->desc_width == -1)
    {
        L->desc_width = maxcol - spaces - L->area_width - L->marked_width - L->count_width - L->pmark_width - L->unread_width - L->changed_width - L->echoid_width - L->groupid_width;

        if (L->desc_width < 0)
            L->desc_width = 0;
    }
    else if (!desc_found)
    {
        L->desc_width = 0;
    }

    /* Assign positions in format order */
    pos = 0;
    p = fmt;

    while (*p)
    {
        char c = (char)toupper((unsigned char)*p);
        p++;

        while (isdigit((unsigned char)*p))
            p++;

        switch (c)
        {
        case 'A':
            L->area_pos = pos;
            pos += L->area_width;
            break;
        case 'M':
            L->marked_pos = pos;
            pos += L->marked_width;
            break;
        case 'D':
            L->desc_pos = pos;
            pos += L->desc_width;
            break;
        case 'C':
            L->count_pos = pos;
            pos += L->count_width;
            break;
        case 'P':
            L->pmark_pos = pos;
            pos += L->pmark_width;
            break;
        case 'U':
            L->unread_pos = pos;
            pos += L->unread_width;
            break;
        case 'N':
            L->changed_pos = pos;
            pos += L->changed_width;
            break;
        case 'E':
            L->echoid_pos = pos;
            pos += L->echoid_width;
            break;
        case 'G':
            L->groupid_pos = pos;
            pos += L->groupid_width;
            break;
        default:
            pos++;
            break; /* any non-letter = single space */
        }
    }

    L->total_used = pos;
}

/* Cache keyed on inputs (saves N strlen() calls per frame) */
void ui_arealist_layout_get(const AreaList *areas, const char *fmt, int maxcol, ArealistLayout *out)
{
    static ArealistLayout cache;
    static const AreaList *cache_areas = NULL;
    static const char *cache_fmt = NULL;
    static int cache_cols = -1;
    static int cache_count = -1;

    if (cache_cols == maxcol && cache_areas == areas && cache_fmt == fmt && cache_count == (areas ? areas->count : 0))
    {
        *out = cache;
        return;
    }

    ui_arealist_layout_compute(areas, fmt, maxcol, out);

    cache = *out;
    cache_cols = maxcol;
    cache_areas = areas;
    cache_fmt = fmt;
    cache_count = areas ? areas->count : 0;
}
