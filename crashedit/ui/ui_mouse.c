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
#include "ui_mouse.h"
#include "../components/editor.h"
#include "ui_spell.h"

#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include <time.h>

#ifdef PLATFORM_WIN32
#include <windows.h>
#elif defined(PLATFORM_AMIGA)
#include <devices/timer.h>
#include <proto/timer.h>
extern struct Device *TimerBase;
#else
#include <sys/time.h>
#endif

/* Click counter for double/triple click detection */
static int s_last_y = -1;
static int s_last_x = -1;
static int s_click_count = 0;
static int s_dragging = 0;
static int s_did_drag = 0;
static int s_keep_block = 0; /* set by double/triple click */
static unsigned long s_last_click_time = 0;
static unsigned long s_event_time_ms = 0;
static int s_event_time_valid = 0; /* 1 = s_event_time_ms was set by platform */

#define DBLCLICK_MS 400UL /* ms between clicks for double-click */

/* Set on first use, makes timestamps relative */
static unsigned long s_time_base = 0;

static unsigned long get_time_ms(void)
{
    unsigned long t;

#ifdef PLATFORM_WIN32
    t = (unsigned long)GetTickCount();
#elif defined(PLATFORM_AMIGA)
    struct timeval tv;

    if (TimerBase)
    {
        GetSysTime(&tv);
        t = (unsigned long)tv.tv_secs * 1000UL + (unsigned long)tv.tv_micro / 1000UL;
    }
    else
    {
        t = (s_time_base != 0) ? s_time_base : 0UL;
    }
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);
    t = (unsigned long)(tv.tv_sec * 1000UL + tv.tv_usec / 1000UL);
#endif

    if (s_time_base == 0)
        s_time_base = t;

    return t - s_time_base;
}

void ui_mouse_reset(void)
{
    s_last_y = -1;
    s_last_x = -1;
    s_click_count = 0;
    s_dragging = 0;
    s_last_click_time = 0;
    s_event_time_ms = 0;
    s_event_time_valid = 0;
    s_did_drag = 0;
    s_keep_block = 0;
}

void ui_mouse_set_event_time_ms(unsigned long ms)
{
    if (s_time_base == 0)
        s_time_base = ms;

    s_event_time_ms = ms - s_time_base;
    s_event_time_valid = 1;
}

/* Compute editor body width with line-number offset */
static int lineno_width(int line_count)
{
    int width = 1;
    int n = line_count;

    if (n <= 0)
        n = 1;

    while (n >= 10)
    {
        n /= 10;
        width++;
    }

    return width + 1; /* digits + 1 space */
}

static int compute_body_width(UiApp *app)
{
    EdInfo info;
    int width;

    ed_get_info(app->editor, &info);

    /* Use COLS as default width */
    width = COLS;

    if (app->cfg && app->cfg->show_line_numbers)
        width -= lineno_width(info.line_count);

    if (width < 1)
        width = 1;

    return width;
}

/* Snap cursor to (line, col), clearing block if not dragging */
static void place_cursor(UiApp *app, int line, int col, int keep_block)
{
    Ed *ed = app->editor;

    if (!keep_block)
        ed_block_clear(ed);

    ed_set_pos(ed, line, col);
}

static int find_word_at(UiApp *app, int line, int col, int *word_start, int *word_end)
{
    Ed *ed = NULL;
    const wchar_t *str = NULL;
    int str_len;
    int wall_left;
    int wall_right;
    int i;

    ed = app->editor;
    str = ed_line_wcs(ed, line);
    str_len = ed_line_len(ed, line);

    if (str == NULL || str_len == 0 || col < 0 || col > str_len)
        return -1;

    if (col == str_len)
        col = str_len - 1;

    if (te_is_word_char(str[col]) == 0)
        return -1;

    wall_left = -1;

    for (i = col - 1; i >= 0; i--)
    {
        if (te_is_word_char(str[i]) == 0)
        {
            wall_left = i;
            break;
        }
    }

    wall_right = str_len;

    for (i = col + 1; i < str_len; i++)
    {
        if (te_is_word_char(str[i]) == 0)
        {
            wall_right = i;
            break;
        }
    }

    *word_start = wall_left + 1;
    *word_end = wall_right;
    return 0;
}

/* Find paragraph boundaries (inclusive) */
static void find_paragraph_at(UiApp *app, int line, int *line_first, int *line_last)
{
    Ed *ed = app->editor;
    EdInfo info;
    int i;

    ed_get_info(ed, &info);

    *line_first = line;
    *line_last = line;

    for (i = line - 1; i >= 0; i--)
    {
        if (ed_line_len(ed, i) == 0)
            break;

        *line_first = i;
    }

    for (i = line + 1; i < info.line_count; i++)
    {
        if (ed_line_len(ed, i) == 0)
            break;

        *line_last = i;
    }
}

int ui_mouse_dispatch(UiApp *app, UiMouseEventType type, int y, int x, int body_rows)
{
    int line = 0;
    int col = 0;
    int width;
    int lnw = 0;

    if (!app)
        return 0;

    width = compute_body_width(app);

    if (app->cfg && app->cfg->show_line_numbers)
    {
        EdInfo info;

        ed_get_info(app->editor, &info);

        lnw = lineno_width(info.line_count);
        x -= lnw;

        if (x < 0)
            x = 0;
    }

    switch (type)
    {
    case UI_MOUSE_PRESS_LEFT:
    {
        unsigned long now;
        int dx;
        int dy;

        /* Resolve target position */
        if (ui_editor_screen_to_logical(app, width, y, x, &line, &col, body_rows) < 0)
            return 0;

        /* Detect multi-click with timeout and movement tolerance (±2 cells) */
        now = s_event_time_valid ? s_event_time_ms : get_time_ms();
        s_event_time_ms = 0;
        s_event_time_valid = 0;
        dx = abs(s_last_x - x);
        dy = abs(s_last_y - y);

        /* Reset if timeout exceeded or movement too large */
        if (s_last_click_time == 0 || (now - s_last_click_time) > DBLCLICK_MS || dx > 2 || dy > 2)
            s_click_count = 1;
        else
            s_click_count++;

        s_last_y = y;
        s_last_x = x;
        s_last_click_time = now;
        s_dragging = 1;
        s_did_drag = 0;
        s_keep_block = 0;

        if (s_click_count == 2)
        {
            /* Double click: select word under pointer */
            int ws, we;

            if (find_word_at(app, line, col, &ws, &we) == 0)
            {
                ed_block_clear(app->editor);

                ed_set_pos(app->editor, line, ws);
                ed_block_anchor(app->editor);
                ed_set_pos(app->editor, line, we);

                s_keep_block = 1;
                return 1;
            }

            /* Fall through: no word, just place cursor */
            place_cursor(app, line, col, 0);

            return 1;
        }

        if (s_click_count >= 3)
        {
            /* Triple click: select paragraph */
            int pf, pl;
            int last_len;

            find_paragraph_at(app, line, &pf, &pl);
            last_len = ed_line_len(app->editor, pl);

            ed_block_clear(app->editor);

            ed_set_pos(app->editor, pf, 0);
            ed_block_anchor(app->editor);
            ed_set_pos(app->editor, pl, last_len);

            s_click_count = 0; /* reset chain */
            s_keep_block = 1;
            return 1;
        }

        /* Single click: place cursor and anchor block start here */
        place_cursor(app, line, col, 0);
        ed_block_anchor(app->editor);

        return 1;
    }

    case UI_MOUSE_DRAG_LEFT:
    {
        if (!s_dragging)
            return 0;

        if (ui_editor_screen_to_logical(app, width, y, x, &line, &col, body_rows) < 0)
            return 0;

        s_did_drag = 1;

        /* Extend selection to current pointer position */
        ed_set_pos(app->editor, line, col);
        return 1;
    }

    case UI_MOUSE_RELEASE_LEFT:
    {
        /* If no drag and no word/paragraph selection, clear the block */
        if (!s_did_drag && !s_keep_block)
            ed_block_clear(app->editor);

        s_dragging = 0;
        s_did_drag = 0;
        return 1;
    }

    case UI_MOUSE_WHEEL_UP:
    {
        /* Scroll one line up */
        extern void soft_reset_desired(void);

        Ed *ed = app->editor;
        EdInfo info;

        ed_get_info(ed, &info);

        if (info.row > 0)
        {
            ed_set_pos(ed, info.row - 1, info.col);
            return 1;
        }

        return 0;
    }

    case UI_MOUSE_WHEEL_DOWN:
    {
        Ed *ed = app->editor;
        EdInfo info;

        ed_get_info(ed, &info);

        if (info.row + 1 < info.line_count)
        {
            ed_set_pos(ed, info.row + 1, info.col);
            return 1;
        }

        return 0;
    }

    case UI_MOUSE_MOVE:
    default:
        return 0;
    }
}
