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

/* ui_editor_popups.c -- Editor sub-popups (kludge list, attr toggle, special chars) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_editor_internal.h"
#include "../../src/jamlib/jam.h"

/* Kludge view/edit popup (F8): shows saved_kludges, Enter=delete, ^A displayed as '@' */
void editor_kludge_popup(UiApp *app)
{
    char *src;    /* working copy with '\0' line terminators */
    char **lines; /* pointers into src, one per original kludge line */
    char **disp;  /* display copies with ^A->'@', live list */
    int *raw_idx; /* disp[i] -> lines[raw_idx[i]] */
    int count = 0, cap = 0;
    int total_lines = 0; /* original number of lines parsed */
    int i;
    char *p;
    int sel;
    int deleted_any = 0;

    if (!app->saved_kludges || !app->saved_kludges[0])
    {
        ui_status(app, "(No kludges)");
        return;
    }

    src = (char *)malloc(strlen(app->saved_kludges) + 1);

    if (!src)
        return;

    strcpy(src, app->saved_kludges);

    lines = NULL;
    p = src;

    while (*p)
    {
        char *start = p;

        while (*p && *p != '\r' && *p != '\n')
            p++;

        if (*p)
        {
            *p = '\0';
            p++;
        }

        if (*p == '\n')
            p++;

        if (start[0] == '\0')
            continue;

        if (count >= cap)
        {
            int nc = cap ? cap * 2 : 16;
            char **nl = (char **)realloc(lines, (size_t)nc * sizeof(char *));

            if (!nl)
                break;

            lines = nl;
            cap = nc;
        }

        lines[count++] = start;
    }

    total_lines = count;

    if (count == 0)
    {
        free(src);
        free(lines);
        return;
    }

    disp = (char **)malloc((size_t)count * sizeof(char *));
    raw_idx = (int *)malloc((size_t)count * sizeof(int));

    if (!disp || !raw_idx)
    {
        free(disp);
        free(raw_idx);
        free(src);
        free(lines);
        return;
    }

    for (i = 0; i < count; i++)
    {
        int l = (int)strlen(lines[i]);
        char *d = (char *)malloc((size_t)l + 1);

        if (!d)
        {
            disp[i] = (char *)"";
            raw_idx[i] = i;
            continue;
        }

        memcpy(d, lines[i], (size_t)l + 1);

        if (d[0] == 1)
            d[0] = '@';

        disp[i] = d;
        raw_idx[i] = i;
    }

    /* Re-open popup after each deletion to refresh list */
    sel = 0;

    while (count > 0)
    {
        int picked = ui_popup_list("Kludges (Enter=delete, ESC=close)", (const char **)disp, count, sel);

        if (picked < 0)
            break;

        sel = picked;

        if (ui_popup_confirm("Delete kludge", "Remove this kludge line?") == 1)
        {
            /* Mark source line as deleted (clear its content) */
            int ri = raw_idx[picked];

            lines[ri][0] = '\0';
            deleted_any = 1;

            /* Remove from display list (shift down) */
            free(disp[picked]);

            for (i = picked; i < count - 1; i++)
            {
                disp[i] = disp[i + 1];
                raw_idx[i] = raw_idx[i + 1];
            }

            count--;

            if (sel >= count)
                sel = count - 1;
        }
    }

    /* Rebuild saved_kludges from remaining lines (deleted ones now empty) */
    if (deleted_any)
    {
        size_t cap2 = 256, used = 0;
        char *new_buf = (char *)malloc(cap2);

        if (new_buf)
        {
            for (i = 0; i < total_lines; i++)
            {
                size_t ll = strlen(lines[i]);

                if (ll == 0)
                    continue;

                if (used + ll + 2 >= cap2)
                {
                    cap2 = (used + ll + 64) * 2;
                    char *tmp_buf = (char *)realloc(new_buf, cap2);

                    if (!tmp_buf)
                    {
                        free(new_buf);
                        new_buf = NULL; /* prevent UAF after realloc failure */
                        break;
                    }

                    new_buf = tmp_buf;
                }

                memcpy(new_buf + used, lines[i], ll);

                used += ll;
                new_buf[used++] = '\n';
            }

            if (new_buf)
            {
                new_buf[used] = '\0';

                free(app->saved_kludges);

                app->saved_kludges = new_buf;

                ui_status(app, "Kludges updated");
            }
        }
    }

    for (i = 0; i < count; i++)
    {
        if (disp[i] && disp[i][0])
            free(disp[i]);
    }

    free(disp);
    free(raw_idx);
    free(lines);
    free(src);
}

/* Popup to toggle attribute flags (0=OK, -1=cancel) */
int editor_attr_popup(UiApp *app)
{
    const char *labels[4];
    char tmp[4][24];
    int sel;

    snprintf(tmp[0], sizeof(tmp[0]), "[%c] Private", (app->edit_attr & MSG_PRIVATE) ? 'X' : ' ');
    snprintf(tmp[1], sizeof(tmp[1]), "[%c] Crash", (app->edit_attr & MSG_CRASH) ? 'X' : ' ');
    snprintf(tmp[2], sizeof(tmp[2]), "[%c] Hold", (app->edit_attr & MSG_HOLD) ? 'X' : ' ');
    snprintf(tmp[3], sizeof(tmp[3]), "[%c] K/S", (app->edit_attr & MSG_KILLSENT) ? 'X' : ' ');

    labels[0] = tmp[0];
    labels[1] = tmp[1];
    labels[2] = tmp[2];
    labels[3] = tmp[3];

    sel = ui_popup_list("Message attributes (toggle)", labels, 4, 0);

    if (sel < 0)
        return -1;

    switch (sel)
    {
    case 0:
        app->edit_attr ^= MSG_PRIVATE;
        break;
    case 1:
        app->edit_attr ^= MSG_CRASH;
        break;
    case 2:
        app->edit_attr ^= MSG_HOLD;
        break;
    case 3:
        app->edit_attr ^= MSG_KILLSENT;
        break;
    }

    return 0;
}
