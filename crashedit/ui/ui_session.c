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

/* ui_session.c -- Per-area session management */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ui_internal.h"
#include <wchar.h>
#include <wctype.h>

/* Open / close */
void ui_session_close(UiApp *app)
{
    UiSession *s;
    if (!app)
        return;
    s = &app->sess;

    /* Refresh cached counts before closing */
    if (s->area_idx >= 0 && s->area_idx < app->areas->count)
        areafile_refresh_one(&app->areas->entries[s->area_idx], app->cfg->sysop);

    if (s->jam_open)
    {
        jam_close(&s->jam);
        s->jam_open = 0;
    }

    free(s->msgs);

    s->msgs = NULL;

    free(s->order);

    s->order = NULL;
    s->msg_count = 0;
    s->order_count = 0;
    s->msg_sel = 1;
    s->msg_top = 1;
    s->area_idx = -1;
    s->search[0] = L'\0';
}

int ui_session_open(UiApp *app, int area_idx)
{
    UiSession *s;
    AreaEntry *ae;
    int count = 0;
    uint32_t mask;
    uint32_t ls;
    uint32_t lr;

    if (!app)
        return -1;

    if (area_idx < 0 || area_idx >= app->areas->count)
        return -1;

    /* Close any previous session */
    ui_session_close(app);

    s = &app->sess;
    ae = &app->areas->entries[area_idx];

    if (jam_open(&s->jam, ae->path) != 0)
    {
        ui_status(app, "Cannot open area: %s", ae->path);
        return -1;
    }

    s->jam_open = 1;
    s->area_idx = area_idx;
    s->user_crc = jam_username_crc(app->cfg->sysop);

    /* Load LastReadMsg/HighReadMsg; enforce lastseen >= lastread invariant for ancient bases */
    lr = 0, ls = 0;
    jam_read_lastread_pair(&s->jam, s->user_crc, &lr, &ls);

    if (ls < lr)
        ls = lr;

    s->lastread = lr;
    s->lastseen = ls;

    /* Load messages (honour SHOWDELETED, filter_mask = bits to skip) */
    mask = 0;

    s->msgs = jam_load_headers(&s->jam, &count, mask, (uint32_t)app->cfg->msglistmax);

    if (!s->msgs && count != 0)
    {
        s->msg_count = 0;
        ui_status(app, "Failed to load message headers");
        return -1;
    }

    s->msg_count = count; /* may be 0 -- empty area is fine */

    ui_session_rebuild_order(app);

    /* Park cursor on first unread message (msgnum > lastread) */
    s->msg_sel = 1;
    s->msg_top = 1;

    if (s->lastread > 0 && s->order_count > 0)
    {
        int i, first_unread = -1, last_read_row = -1;

        for (i = 0; i < s->order_count; i++)
        {
            uint32_t mn = s->msgs[s->order[i]].msgnum;

            if (mn > s->lastread)
            {
                if (first_unread < 0)
                    first_unread = i;
            }
            else
            {
                /* Fallback: park on most recent already-read message */
                if (last_read_row < 0 || s->msgs[s->order[i]].msgnum > s->msgs[s->order[last_read_row]].msgnum)
                    last_read_row = i;
            }
        }

        if (first_unread >= 0)
            s->msg_sel = first_unread + 1;
        else if (last_read_row >= 0)
            s->msg_sel = last_read_row + 1;
    }

    return 0;
}

/* Message list filtering and sorting */
static int field_matches_wcs(const char *hay_utf8, const wchar_t *needle)
{
    wchar_t *w;
    int found;

    if (!needle || !needle[0])
        return 1;

    if (!hay_utf8)
        return 0;

    w = utf8_to_wcs(hay_utf8, NULL);

    if (!w)
        return 0;

    found = wcs_casestr(w, needle) != NULL;

    free(w);

    return found;
}

void ui_session_rebuild_order(UiApp *app)
{
    UiSession *s;
    int i, n;
    unsigned char *match_tbl = NULL;

    if (!app)
        return;

    s = &app->sess;
    free(s->order);
    s->order = NULL;
    s->order_count = 0;

    if (s->msg_count <= 0)
        return;

    s->order = (int *)malloc((size_t)s->msg_count * sizeof(int));

    if (!s->order)
        return;

    /* Precompute filter matches to avoid repeated mallocs */
    if (s->search[0])
    {
        match_tbl = (unsigned char *)malloc((size_t)s->msg_count);

        if (match_tbl)
        {
            for (i = 0; i < s->msg_count; i++)
            {
                const JamMsgInfo *m = &s->msgs[i];
                match_tbl[i] = (field_matches_wcs(m->from, s->search) || field_matches_wcs(m->to, s->search) || field_matches_wcs(m->subject, s->search)) ? 1 : 0;
            }
        }
    }

    n = 0;

    for (i = 0; i < s->msg_count; i++)
    {
        if (match_tbl && !match_tbl[i])
            continue;

        s->order[n++] = i;
    }

    s->order_count = n;

    if (match_tbl)
        free(match_tbl);

    /* Clamp selection */
    if (s->msg_sel > s->order_count)
        s->msg_sel = s->order_count;

    if (s->msg_sel < 1)
        s->msg_sel = 1;
}
