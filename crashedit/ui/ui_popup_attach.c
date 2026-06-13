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

/* ui_popup_attach.c -- File attachment popups */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "ui_internal.h"
#include "../core/msghdr.h"
#include "../core/keys.h"
#include "ui_files.h"

/* EF_SUBJECT lives in ui_editor.c, keep private const here (independent of editor field IDs) */
#define UI_ATTACH_EF_SUBJECT 3

/* After attaching, refresh edit_hdr SUBJECT field if editing (shows filename immediately) */
static void refresh_subject_if_editing(UiApp *app)
{
    char *subject_str;
    wchar_t *wsubj;
    size_t len;

    if (!app || !app->edit_hdr)
        return;

    /* Only refresh the subject preview while the user is on the subject field */
    if (app->edit_active_field != UI_ATTACH_EF_SUBJECT)
        return;

    subject_str = attach_build_subject(app->attach_list);

    if (!subject_str)
        return;

    len = strlen(subject_str);
    wsubj = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));

    if (wsubj)
    {
        size_t n = mbstowcs(wsubj, subject_str, len + 1);

        /* mbstowcs failed, salvage ASCII subset to avoid garbage */
        if (n == (size_t)-1)
        {
            size_t i;

            for (i = 0; i < len; i++)
            {
                unsigned char b = (unsigned char)subject_str[i];
                wsubj[i] = (b < 0x80) ? (wchar_t)b : L'?';
            }

            wsubj[len] = L'\0';
        }
        else
        {
            /* Ensure NUL termination, mbstowcs doesn't guarantee it on exact fit */
            wsubj[len] = L'\0';
        }

        msghdr_set(app->edit_hdr, HDR_SUBJECT, wsubj);

        free(wsubj);
    }

    free(subject_str);
}

int ui_popup_attach_add(UiApp *app)
{
    char path[1024];

    if (!app || !app->attach_list)
        return -1;

    path[0] = '\0';

    if (ui_files_pick("Add attachment", NULL, path, sizeof(path)) != 0)
        return 0;

    if (attach_add(app->attach_list, path) != 0)
    {
        ui_status(app, "Failed to attach: %s", path);
        return -1;
    }

    ui_status(app, "Attached: %s", path);
    refresh_subject_if_editing(app);

    return 1;
}

/* Simple modal list of current attachments, Enter removes selected */
int ui_popup_attach_remove(UiApp *app)
{
    int y, x, h, w;
    int sel = 0, top = 0;
    int key;
    int visible;
    const char *t;
    int tl;
    int tx;

    if (!app || !app->attach_list || app->attach_list->count == 0)
        return 0;

    ui_popup_center(20, 60, &y, &x, &h, &w);
    visible = h - 4;

    for (;;)
    {
        int i, j;

        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < h; i++)
        {
            move(y + i, x);

            for (j = 0; j < w; j++)
                addch(' ');
        }

        ui_box(y, x, h, w);

        t = " Remove attachment ";
        tl = (int)strlen(t);
        tx = x + (w - tl) / 2;

        mvaddnstr(y, tx, t, tl);

        for (i = 0; i < visible && top + i < app->attach_list->count; i++)
        {
            int idx = top + i;
            char line[128];

            if (idx == sel)
                attron(COLOR_PAIR(COL_POPUP_SEL));
            else
                attron(COLOR_PAIR(COL_POPUP));

            snprintf(line, sizeof(line), "%s (%u bytes)", app->attach_list->entries[idx].path, (unsigned)app->attach_list->entries[idx].size);
            mvaddnstr(y + 2 + i, x + 2, line, w - 4);
            attroff(COLOR_PAIR(COL_POPUP_SEL));
        }

        attron(COLOR_PAIR(COL_POPUP));
        mvaddnstr(y + h - 2, x + 2, "Enter=remove  ESC=cancel", w - 4);
        attroff(COLOR_PAIR(COL_POPUP));
        refresh();

        key = wrapper_getch();

        if (key == 27)
            return 0;

        if (key == '\n' || key == '\r' || key == KEY_ENTER)
        {
            if (attach_remove(app->attach_list, sel) == 0)
            {
                ui_status(app, "Attachment removed");

                /* Stay for multiple removals, re-clamp cursor, exit when empty */
                if (app->attach_list->count == 0)
                    return 1;

                if (sel >= app->attach_list->count)
                    sel = app->attach_list->count - 1;

                if (sel < top)
                    top = sel;

                continue;
            }

            return -1;
        }

        if ((key == KEY_UP || key == 'k') && sel > 0)
        {
            sel--;

            if (sel < top)
                top = sel;
        }

        if ((key == KEY_DOWN || key == 'j') && sel < app->attach_list->count - 1)
        {
            sel++;

            if (sel >= top + visible)
                top = sel - visible + 1;
        }
    }
}

int ui_popup_attach_clear(UiApp *app)
{
    if (!app || !app->attach_list)
        return -1;

    if (app->attach_list->count == 0)
        return 0;

    if (ui_popup_confirm("Clear attachments", "Remove all attachments?") == 1)
    {
        attach_clear(app->attach_list);
        ui_status(app, "All attachments removed");
        return 1;
    }

    return 0;
}

int ui_popup_attach_list(UiApp *app)
{
    int y, x, h, w;
    int top = 0;
    int key;
    int visible;

    if (!app || !app->attach_list)
        return 0;

    ui_popup_center(20, 60, &y, &x, &h, &w);
    visible = h - 5; /* Reserve one extra line for the mode footer */

    for (;;)
    {
        int i, j, tl, tx;
        char counter[80];
        const char *t;

        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < h; i++)
        {
            move(y + i, x);

            for (j = 0; j < w; j++)
                addch(' ');
        }

        ui_box(y, x, h, w);

        t = " Attachments ";
        tl = (int)strlen(t);
        tx = x + (w - tl) / 2;

        mvaddnstr(y, tx, t, tl);

        for (i = 0; i < visible && top + i < app->attach_list->count; i++)
        {
            int idx = top + i;
            char line[128];

            snprintf(line, sizeof(line), "%s (%u bytes)", app->attach_list->entries[idx].path, (unsigned)app->attach_list->entries[idx].size);
            mvaddnstr(y + 2 + i, x + 2, line, w - 4);
        }

        snprintf(counter, sizeof(counter), "Total: %d file(s)", app->attach_list->count);

        mvaddnstr(y + h - 2, x + 2, counter, w - 4);
        attroff(COLOR_PAIR(COL_POPUP));
        refresh();

        key = wrapper_getch();

        if (key == 27 || key == 'q' || key == 'Q')
            break;

        if (key == KEY_UP && top > 0)
            top--;

        if (key == KEY_DOWN && top + visible < app->attach_list->count)
            top++;
    }

    return 0;
}
