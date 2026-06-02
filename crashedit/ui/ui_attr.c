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

/* ui_attr.c -- Message attribute utilities */
#include <string.h>
#include <stdio.h>
#include "ui_attr.h"
#include "../../src/jamlib/jam.h"
#include "../core/jam_wrap.h"

/* Append tag to out[] if room, advancing terminator (truncates if overflow) */
static void append_tag(char *out, int outsz, const char *tag)
{
    int cur = (int)strlen(out);
    int need = (int)strlen(tag) + 1; /* trailing space */

    if (cur + need + 1 > outsz)
        return;

    memcpy(out + cur, tag, (size_t)(need - 1));

    out[cur + need - 1] = ' ';
    out[cur + need] = '\0';
}

void ui_attr_build(uint32_t attr, char *out, int outsz)
{
    int l;

    if (!out || outsz < 2)
        return;

    out[0] = '\0';

    if (attr & MSG_LOCAL)
        append_tag(out, outsz, "Loc");

    if (attr & MSG_PRIVATE)
        append_tag(out, outsz, "Pvt");

    if (attr & MSG_CRASH)
        append_tag(out, outsz, "Crash");

    if (attr & MSG_HOLD)
        append_tag(out, outsz, "Hold");

    if (attr & MSG_KILLSENT)
        append_tag(out, outsz, "K/S");

    if (attr & MSG_SENT)
        append_tag(out, outsz, "Snt");

    /* JAM-specific attributes */
    if (attr & JAMATTR_FILEREQUEST)
        append_tag(out, outsz, "Freq");

    if (attr & JAMATTR_FILEATTACH)
        append_tag(out, outsz, "Att");

    /* Trim trailing space */
    l = (int)strlen(out);

    while (l > 0 && out[l - 1] == ' ')
        out[--l] = '\0';

    if (!out[0])
    {
        out[0] = '-';
        out[1] = '\0';
    }
}
