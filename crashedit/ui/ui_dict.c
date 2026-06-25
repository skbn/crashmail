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

#include "ui_internal.h"
#include "ui_dict.h"
#include "ui_spell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DICT_PANEL_H
#define DICT_PANEL_H SPELL_PANEL_H
#endif

static int count_lines(const char *text)
{
    int n;
    const char *p = NULL;
    int saw_char = 0;

    if (!text || !*text)
        return 0;

    n = 1;

    for (p = text; *p; p++)
    {
        if (*p == '\n')
        {
            n++;
            saw_char = 0;
        }
        else
        {
            saw_char = 1;
        }
    }

    if (!saw_char && n > 0)
        n--;

    return n;
}

static const char *line_at(const char *text, int n)
{
    int cur = 0;
    const char *p = text;

    if (!text)
        return NULL;

    if (n <= 0)
        return text;

    while (*p && cur < n)
    {
        if (*p == '\n')
            cur++;

        p++;
    }

    if (!*p && cur < n)
        return NULL;
    return p;
}

static int copy_line(const char *src, char *out, int max_bytes)
{
    int n = 0;

    while (n < max_bytes - 1 && src[n] && src[n] != '\n')
    {
        out[n] = src[n];
        n++;
    }

    out[n] = '\0';
    return n;
}

static void dict_panel_geometry(int *x, int *y, int *w, int *h)
{
    int W = COLS;
    int H = LINES;

    *w = W;
    *h = DICT_PANEL_H;
    *x = 0;
    *y = H - *h - 1; /* same area as spell panel: above status bar */

    if (*y < 1)
        *y = 1;
}

void ui_dict_draw_panel(UiApp *app)
{
    int x;
    int y;
    int w;
    int h;
    int j;
    int content_rows;
    int line_idx;
    const char *p = NULL;
    char line_buf[512];
    int row;
    char title[160];
    int total;
    int x_pos;

    if (!app || !app->show_dict)
        return;

    dict_panel_geometry(&x, &y, &w, &h);

    standend();
    attron(COLOR_PAIR(COL_POPUP));

    /* Clear panel */
    for (j = 0; j < w; j++)
        mvaddch(y, x + j, ' ');

    for (row = 1; row < h; row++)
    {
        for (j = 0; j < w; j++)
            mvaddch(y + row, x + j, ' ');
    }

    /* Title */
    if (app->dict_word[0])
        snprintf(title, sizeof(title), "[ Dictionary ] %s", app->dict_word);
    else
        snprintf(title, sizeof(title), "[ Dictionary ]");

    mvprintw(y, x + 2, "%s", title);

    content_rows = h - 1;

    if (content_rows <= 0)
        return;

    if (!app->dict_result || !app->dict_result[0])
    {
        mvprintw(y + 1, x + 2, "Select text and press Alt+R (translator backend must be STARDICT)");
        return;
    }

    p = line_at(app->dict_result, app->dict_scroll);

    if (!p)
        p = app->dict_result;

    for (line_idx = 0; line_idx < content_rows && *p; line_idx++)
    {
        int limit = w - 4;

        if (limit < 1)
            limit = 1;

        if (limit > (int)sizeof(line_buf) - 1)
            limit = (int)sizeof(line_buf) - 1;

        copy_line(p, line_buf, limit + 1);
        mvprintw(y + 1 + line_idx, x + 2, "%s", line_buf);

        while (*p && *p != '\n')
            p++;

        if (*p == '\n')
            p++;
    }

    /* Scroll indicator on the title bar */
    total = count_lines(app->dict_result);

    if (total > content_rows)
    {
        char ind[40];
        int below = (app->dict_scroll + content_rows < total);
        int above = (app->dict_scroll > 0);

        if (above && below)
            snprintf(ind, sizeof(ind), "[%d/%d] PgUp/PgDn", app->dict_scroll + 1, total);
        else if (above)
            snprintf(ind, sizeof(ind), "[%d/%d] PgUp", app->dict_scroll + 1, total);
        else
            snprintf(ind, sizeof(ind), "[%d/%d] PgDn", app->dict_scroll + 1, total);

        x_pos = x + w - (int)strlen(ind) - 2;

        if (x_pos > x + 2)
            mvprintw(y, x_pos, "%s", ind);
    }
}

int ui_dict_toggle_panel(UiApp *app)
{
    if (!app)
        return 1;

    app->show_dict = !app->show_dict;
    ui_status(app, "Dictionary panel %s", app->show_dict ? "shown" : "hidden");

    return 1;
}

void ui_dict_set_result(UiApp *app, const char *word_or_phrase, const char *text)
{
    size_t tl;

    if (!app)
        return;

    if (app->dict_result)
    {
        free(app->dict_result);
        app->dict_result = NULL;
    }

    if (!text)
    {
        app->dict_word[0] = '\0';
        app->dict_scroll = 0;
        return;
    }

    if (word_or_phrase)
    {
        strncpy(app->dict_word, word_or_phrase, sizeof(app->dict_word) - 1);
        app->dict_word[sizeof(app->dict_word) - 1] = '\0';
    }
    else
    {
        app->dict_word[0] = '\0';
    }

    tl = strlen(text);
    app->dict_result = (char *)malloc(tl + 1);

    if (app->dict_result)
        memcpy(app->dict_result, text, tl + 1);

    app->dict_scroll = 0;
}

void ui_dict_free(UiApp *app)
{
    if (!app)
        return;

    if (app->dict_result)
    {
        free(app->dict_result);
        app->dict_result = NULL;
    }

    app->dict_word[0] = '\0';
    app->dict_scroll = 0;
}

int ui_dict_scroll_up(UiApp *app)
{
    if (!app || !app->show_dict)
        return 0;

    if (!app->dict_result || !app->dict_result[0])
        return 0;

    if (app->dict_scroll <= 0)
        return 0;

    app->dict_scroll--;
    return 1;
}

int ui_dict_scroll_down(UiApp *app)
{
    int total;
    int content_rows;

    if (!app || !app->show_dict)
        return 0;

    if (!app->dict_result || !app->dict_result[0])
        return 0;

    content_rows = DICT_PANEL_H - 1;

    if (content_rows <= 0)
        return 0;

    total = count_lines(app->dict_result);

    if (app->dict_scroll >= total - content_rows)
        return 0;

    app->dict_scroll++;
    return 1;
}
