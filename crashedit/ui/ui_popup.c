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

/* ui_popup.c -- Modal popup dialogs (blocking input) */

#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif

#include "../core/charset.h"
#include "../core/keys.h"
#include "../core/utf8.h"
#include "ui_aka.h"
#include "ui_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

/* Collect AKA pointers (no strdup; owned by AreaList/cfg) */
typedef struct
{
    const char **items;
    int count;
    int cap;
} AkaCollect;

/* Sort spec picker (presets + custom) */
static const char *sort_preset_specs[] =
    {
        "_DEFAULT_",
        "FYT-U+GE", /* default */
        "E",        /* by echoid */
        "+U+E",     /* unread desc, then echoid */
        "-T+E",     /* type desc, echoid */
        "O",        /* original order */
        "Y+U+E",    /* new first, unread desc, echoid */
        "M+E",      /* marked first, echoid */
        "_CUSTOM_"};

static const char *sort_preset_labels[] =
    {
        "Default (config file)",
        "By (fuzzy/new/type/unread/group/echo)",
        "By echoid",
        "Unread first, then echoid",
        "Type first, then echoid",
        "Original (file) order",
        "New mail first, unread desc, echoid",
        "Marked first, echoid",
        "Custom..."};

#define SORT_PRESETS_N ((int)(sizeof(sort_preset_specs) / sizeof(sort_preset_specs[0])))

/* Common: centered window helper */
static void draw_popup_frame(int y, int x, int h, int w, const char *title)
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

    if (title && title[0])
    {
        int tl = (int)strlen(title);
        int tx = x + (w - tl - 2) / 2;

        if (tx < x + 1)
            tx = x + 1;

        mvaddch(y, tx - 1, ' ');
        mvaddnstr(y, tx, title, tl);
        mvaddch(y, tx + tl, ' ');
    }

    attroff(COLOR_PAIR(COL_POPUP));
}

void ui_popup_center(int want_h, int want_w, int *y, int *x, int *h, int *w)
{
    if (want_w > COLS - 2)
        want_w = COLS - 2;

    if (want_h > LINES - 2)
        want_h = LINES - 2;

    if (want_w < 10)
        want_w = 10;

    if (want_h < 3)
        want_h = 3;

    *h = want_h;
    *w = want_w;
    *y = (LINES - want_h) / 2;
    *x = (COLS - want_w) / 2;
}

/* Confirm Yes/No/Cancel */
int ui_popup_confirm(const char *title, const char *msg)
{
    int y, x, h, w;
    int msglen = msg ? (int)strlen(msg) : 0;
    int want_w = msglen + 8;
    int ch;

    if (want_w < 40)
        want_w = 40;

    ui_popup_center(7, want_w, &y, &x, &h, &w);

    draw_popup_frame(y, x, h, w, title ? title : "Confirm");
    attron(COLOR_PAIR(COL_POPUP));

    mvaddnstr(y + 2, x + 2, msg ? msg : "", w - 4);
    mvaddnstr(y + 4, x + 2, "Y=Yes  N=No  ESC=Cancel", w - 4);

    attroff(COLOR_PAIR(COL_POPUP));
    refresh();

    for (;;)
    {
        ch = wrapper_getch();

        if (ch == 'y' || ch == 'Y' || ch == '\n' || ch == '\r' || ch == KEY_RIGHT)
            return 1;

        if (ch == 'n' || ch == 'N' || ch == KEY_LEFT)
            return 0;

        if (ch == 27 || ch == 'q' || ch == 'Q')
            return -1;
    }
}

/* Simple message popup (waits for any key to dismiss) */
void ui_popup_message(const char *title, const char *msg)
{
    int y, x, h, w;
    int msglen = msg ? (int)strlen(msg) : 0;
    int want_w = msglen + 8;
    int ch;

    if (want_w < 40)
        want_w = 40;

    ui_popup_center(7, want_w, &y, &x, &h, &w);

    draw_popup_frame(y, x, h, w, title ? title : "Message");
    attron(COLOR_PAIR(COL_POPUP));

    mvaddnstr(y + 2, x + 2, msg ? msg : "", w - 4);
    mvaddnstr(y + 4, x + 2, "Press any key to continue", w - 4);

    attroff(COLOR_PAIR(COL_POPUP));
    refresh();

    wrapper_getch();
}

/* Confirm Yes/No/Cancel/All (catch-up style, returns: 1=Yes, 0=No, -1=Cancel, 2=All) */
int ui_popup_confirm_all(const char *title, const char *msg)
{
    int y, x, h, w;
    int msglen = msg ? (int)strlen(msg) : 0;
    int want_w = msglen + 8;
    int ch;

    if (want_w < 48)
        want_w = 48;

    ui_popup_center(7, want_w, &y, &x, &h, &w);
    draw_popup_frame(y, x, h, w, title ? title : "Confirm");

    attron(COLOR_PAIR(COL_POPUP));
    mvaddnstr(y + 2, x + 2, msg ? msg : "", w - 4);
    mvaddnstr(y + 4, x + 2, "Y=Yes  N=No  A=All  ESC=Cancel", w - 4);

    attroff(COLOR_PAIR(COL_POPUP));
    refresh();

    for (;;)
    {
        ch = wrapper_getch();

        if (ch == 'y' || ch == 'Y' || ch == '\n' || ch == '\r')
            return 1;

        if (ch == 'n' || ch == 'N')
            return 0;

        if (ch == 'a' || ch == 'A')
            return 2;

        if (ch == 27 || ch == 'q' || ch == 'Q')
            return -1;
    }
}

/* Generic list picker */
int ui_popup_list(const char *title, const char **items, int count, int initial)
{
    int y, x, h, w;
    int want_h, want_w = 30;
    int sel = initial, top = 0;
    int i, ch;
    int saved_cursor;

    if (count <= 0)
        return -1;

    if (sel < 0 || sel >= count)
        sel = 0;

    /* Hide cursor for list selection */
    saved_cursor = curs_set(0);

    /* Compute width from longest item */
    for (i = 0; i < count; i++)
    {
        int l = items[i] ? (int)strlen(items[i]) : 0;

        if (l + 6 > want_w)
            want_w = l + 6;
    }

    want_h = count + 4;

    if (want_h > LINES - 4)
        want_h = LINES - 4;

    ui_popup_center(want_h, want_w, &y, &x, &h, &w);

    for (;;)
    {
        int rows = h - 4;

        if (rows < 1)
            rows = 1;

        if (sel < top)
            top = sel;

        if (sel >= top + rows)
            top = sel - rows + 1;

        if (top < 0)
            top = 0;

        standend(); /* Clear any residual attributes from previous iteration */
        draw_popup_frame(y, x, h, w, title ? title : "Select");
        attron(COLOR_PAIR(COL_POPUP));

        /* Show scroll indicators if needed */
        if (top > 0)
        {
            move(y + 1, x + w - 2);
            addch('^');
        }

        if (top + rows < count)
        {
            move(y + h - 2, x + w - 2);
            addch('v');
        }

        for (i = 0; i < rows && top + i < count; i++)
        {
            int idx = top + i;

            move(y + 2 + i, x + 2);

            if (idx == sel)
            {
                int used;

                attron(COLOR_PAIR(COL_POPUP_SEL));
                addch('>');
                addch(' ');
                addnstr(items[idx] ? items[idx] : "", w - 6);

                used = 2 + (items[idx] ? (int)strlen(items[idx]) : 0);

                while (used < w - 4)
                {
                    addch(' ');
                    used++;
                }

                attroff(COLOR_PAIR(COL_POPUP_SEL));
                attron(COLOR_PAIR(COL_POPUP));
            }
            else
            {
                addch(' ');
                addch(' ');
                addnstr(items[idx] ? items[idx] : "", w - 6);
            }
        }

        mvaddnstr(y + h - 2, x + 2, "Enter=OK  ESC=Cancel", w - 4);

        attroff(COLOR_PAIR(COL_POPUP));
        refresh();

        ch = getch();

        if (ch == 27 || ch == 'q' || ch == 'Q')
        {
            curs_set(saved_cursor);
            return -1;
        }

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
        {
            curs_set(saved_cursor);
            return sel;
        }

        if (ch == KEY_UP || ch == 'k')
        {
            if (sel > 0)
                sel--;
        }
        else if (ch == KEY_DOWN || ch == 'j')
        {
            if (sel < count - 1)
                sel++;
        }
        else if (ch == KEY_PPAGE || ch == CTRL('U')) /* Ctrl+U: Page Up (Amiga) */
        {
            sel -= rows;

            if (sel < 0)
                sel = 0;
        }
        else if (ch == KEY_NPAGE || ch == CTRL('D')) /* Ctrl+D: Page Down (Amiga) */
        {
            sel += rows;

            if (sel >= count)
                sel = count - 1;
        }
        else if (ch == KEY_HOME || ch == CTRL('B')) /* Ctrl+B: Home (Amiga) */
            sel = 0;
        else if (ch == KEY_END || ch == CTRL('E')) /* Ctrl+E: End (Amiga) */
            sel = count - 1;
    }
}

/* Charset picker */
int ui_popup_charset(const char *title, const char *cur, char *out, int outsz)
{
    const char **charset_names;
    int charset_count, initial = 0, i, choice;

    charset_names = charset_get_list(&charset_count);

    if (!charset_names || charset_count == 0)
        return -1;

    if (cur)
    {
        for (i = 0; i < charset_count; i++)
        {
            if (strcasecmp(charset_names[i], cur) == 0)
            {
                initial = i;
                break;
            }
        }
    }

    choice = ui_popup_list(title ? title : "Charset", charset_names, charset_count, initial);

    if (choice < 0)
        return -1;

    if (out && outsz > 0)
    {
        strncpy(out, charset_names[choice], (size_t)(outsz - 1));
        out[outsz - 1] = '\0';
    }

    return 0;
}

/* Two-field charset picker: VIEW (display) and OUTPUT (save) */
int ui_popup_charset_pair(const char *view_in, const char *output_in, const char *view_def, const char *output_def, char *view_out, int view_outsz, char *output_out, int output_outsz)
{
    const char **charset_names;
    int charset_count;

    int row = 0; /* 0 = view, 1 = output */
    int sel[2];  /* selection per row */
    int n_opts;  /* +1 for Auto */
    int y, x, h, w, want_h, want_w;
    int i, ch;
    char auto_label_view[48], auto_label_out[48];

    charset_names = charset_get_list(&charset_count);

    if (!charset_names || charset_count == 0)
        return -1;

    n_opts = charset_count + 1; /* +1 for Auto */

    /* Build "Auto" labels and preselect (empty = Auto, else find match) */
    snprintf(auto_label_view, sizeof(auto_label_view), "Auto");
    snprintf(auto_label_out, sizeof(auto_label_out), "Auto");

    for (i = 0; i < 2; i++)
    {
        const char *cur = (i == 0) ? view_in : output_in;
        int j, found = 0;

        sel[i] = 0; /* Auto */

        if (cur && cur[0])
        {
            for (j = 0; j < charset_count; j++)
            {
                if (strcasecmp(charset_names[j], cur) == 0)
                {
                    sel[i] = j + 1;
                    found = 1;
                    break;
                }
            }

            if (!found)
                sel[i] = 0;
        }
    }

    /* Size the box to fit the longest auto label + a margin */
    want_w = (int)strlen(auto_label_view);

    if ((int)strlen(auto_label_out) > want_w)
        want_w = (int)strlen(auto_label_out);

    want_w += 22; /* label column + brackets + padding */

    if (want_w < 40)
        want_w = 40;

    want_h = 8; /* frame + 2 rows + hint + spacing */
    ui_popup_center(want_h, want_w, &y, &x, &h, &w);

    for (;;)
    {
        const char *labels[2] = {"View charset:  ", "Save charset:  "};
        const char *cur_label;

        standend(); /* Clear any residual attributes from previous iteration */
        draw_popup_frame(y, x, h, w, "Charsets");
        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < 2; i++)
        {
            int line_y = y + 2 + i;
            int s = sel[i];
            const char *opt_name;

            if (s == 0)
                opt_name = (i == 0) ? auto_label_view : auto_label_out;
            else
                opt_name = charset_names[s - 1];

            move(line_y, x + 2);

            addstr(labels[i]);

            /* Highlight active row brackets */
            if (i == row)
            {
                int used;

                attron(COLOR_PAIR(COL_POPUP_SEL));
                addstr("[ ");
                addstr(opt_name);
                addstr(" ]");

                used = (int)strlen(labels[i]) + 4 + (int)strlen(opt_name);

                while (used < w - 4)
                {
                    addch(' ');
                    used++;
                }

                attroff(COLOR_PAIR(COL_POPUP_SEL));
                attron(COLOR_PAIR(COL_POPUP));
            }
            else
            {
                addstr("  ");
                addstr(opt_name);
                addstr("  ");
            }
        }

        /* Hint line */
        cur_label = "Up/Down: row  Left/Right: change";
        move(y + h - 2, x + 2);

        if ((int)strlen(cur_label) <= w - 4)
            addstr(cur_label);
        else
            addnstr(cur_label, w - 4);

        attroff(COLOR_PAIR(COL_POPUP));
        refresh();

        ch = getch();

        if (ch == 27)
            return -1;

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
            break;

        if (ch == KEY_UP || ch == 'k')
            row = (row == 0) ? 1 : 0;
        else if (ch == KEY_DOWN || ch == 'j' || ch == '\t')
            row = (row + 1) & 1;
        else if (ch == KEY_LEFT || ch == 'h')
            sel[row] = (sel[row] - 1 + n_opts) % n_opts;
        else if (ch == KEY_RIGHT || ch == 'l' || ch == ' ')
            sel[row] = (sel[row] + 1) % n_opts;
    }

    /* Resolve selections into output buffers */
    if (view_out && view_outsz > 0)
    {
        if (sel[0] == 0)
            view_out[0] = '\0';
        else
        {
            strncpy(view_out, charset_names[sel[0] - 1], (size_t)(view_outsz - 1));
            view_out[view_outsz - 1] = '\0';
        }
    }

    if (output_out && output_outsz > 0)
    {
        if (sel[1] == 0)
            output_out[0] = '\0';
        else
        {
            strncpy(output_out, charset_names[sel[1] - 1], (size_t)(output_outsz - 1));
            output_out[output_outsz - 1] = '\0';
        }
    }

    return 0;
}

/* AKA picker (only useful in netmail) */
static int aka_collect_cb(int idx, const char *aka, void *user)
{
    AkaCollect *c = (AkaCollect *)user;

    if (c->count >= c->cap)
        return 1; /* stop */

    c->items[c->count++] = aka;

    return 0;
}

int ui_popup_aka(const UiApp *app, int cur_idx)
{
    const char **items;
    AkaCollect col;
    int max_count;
    int result;

    if (!app)
        return -1;

    max_count = app->areas->count + app->cfg->aka_count;

    if (max_count <= 0)
        return -1;

    items = (const char **)malloc((size_t)max_count * sizeof(const char *));

    if (!items)
        return -1;

    col.items = items;
    col.count = 0;
    col.cap = max_count;

    ui_aka_walk(app->areas, app->cfg, aka_collect_cb, &col);

    if (col.count <= 0)
    {
        free(items);
        return -1;
    }

    result = ui_popup_list("Select AKA", items, col.count, cur_idx);

    free(items);

    return result;
}

/* Draw input field with optional active highlighting */
static void input_draw(InputState *state, int y, int x, int width, int is_active)
{
    int avail = width - 2;
    int show_off = 0, used, i;

    if (!state || !state->buf)
        return;

    /* Set background color for the input field */
    if (is_active)
        attron(COLOR_PAIR(COL_POPUP_SEL));
    else
        attron(COLOR_PAIR(COL_POPUP));

    mvaddch(y, x, '[');

    /* Calculate scroll offset - show cursor if it's beyond visible area */
    if (state->len <= avail)
    {
        show_off = 0;
    }
    else if (state->cursor >= avail)
    {
        show_off = state->cursor - avail + 1;
    }
    else
    {
        show_off = (state->len > avail) ? (state->len - avail) : 0;

        if (state->cursor < show_off)
            show_off = state->cursor;
    }

    mvaddnwstr(y, x + 1, state->buf + show_off, avail);
    used = state->len - show_off;

    for (i = used; i < avail; i++)
        mvaddch(y, x + 1 + i, ' ');

    mvaddch(y, x + 1 + avail, ']');

    if (is_active)
        attroff(COLOR_PAIR(COL_POPUP_SEL));
    else
        attroff(COLOR_PAIR(COL_POPUP));
}

/* Move cursor to correct position based on scroll offset */
static void input_move_cursor(InputState *state, int y, int x, int width)
{
    int avail = width - 2;
    int show_off = 0;

    if (!state || !state->buf)
        return;

    /* Calculate scroll offset - same logic as input_draw */
    if (state->len <= avail)
    {
        show_off = 0;
    }
    else if (state->cursor >= avail)
    {
        show_off = state->cursor - avail + 1;
    }
    else
    {
        show_off = (state->len > avail) ? (state->len - avail) : 0;

        if (state->cursor < show_off)
            show_off = state->cursor;
    }

    move(y, x + 1 + (state->cursor - show_off));
}

/* Modular input handling: returns 1 if key handled, 0 if not */
static int input_handle_key(InputState *state, int ch)
{
    if (!state || !state->buf)
        return -1;

    if (ch == KEY_LEFT)
    {
        if (state->cursor > 0)
            state->cursor--;
    }
    else if (ch == KEY_RIGHT)
    {
        if (state->cursor < state->len)
            state->cursor++;
    }
    else if (ch == KEY_HOME || ch == CTRL('B'))
    {
        state->cursor = 0;
    }
    else if (ch == KEY_END || ch == CTRL('E'))
    {
        state->cursor = state->len;
    }
    else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
    {
        if (state->cursor > 0)
        {
            wmemmove(&state->buf[state->cursor - 1], &state->buf[state->cursor], (size_t)(state->len - state->cursor + 1));
            state->cursor--;
            state->len--;
            state->buf[state->len] = L'\0';
        }
    }
    else if (ch == KEY_DC)
    {
        if (state->cursor < state->len)
        {
            wmemmove(&state->buf[state->cursor], &state->buf[state->cursor + 1], (size_t)(state->len - state->cursor));
            state->len--;
            state->buf[state->len] = L'\0';
        }
    }
    else if (ch >= 0x20 && ch < 127 && state->len + 1 < state->bufsz)
    {
        wmemmove(&state->buf[state->cursor + 1], &state->buf[state->cursor], (size_t)(state->len - state->cursor + 1));
        state->buf[state->cursor++] = (wchar_t)ch;
        state->len++;
        state->buf[state->len] = L'\0';
    }
    else
    {
        return 0;
    }

    return 1;
}

/* Wide-char text input popup (shared by ui_popup_input and ui_popup_input) */
static int popup_input_core(const char *title, const char *prompt, wchar_t *wbuf, int wcap)
{
    int y, x, h, w;
    int want_w = wcap + 4;
    wint_t wch;
    int rc;
    InputState state;
    WINDOW *saved;

    if (!wbuf || wcap < 2)
        return -1;

    if (want_w < 50)
        want_w = 50;

    ui_popup_center(7, want_w, &y, &x, &h, &w);

    saved = newwin(h, w, y, x);

    if (saved)
        copywin(stdscr, saved, y, x, 0, 0, h - 1, w - 1, 0);

    /* Init InputState */
    state.buf = wbuf;
    state.bufsz = wcap;
    state.cursor = (int)wcslen(wbuf);
    state.len = state.cursor;

    if (state.len > wcap - 1)
    {
        wbuf[wcap - 1] = L'\0';
        state.len = wcap - 1;
        state.cursor = state.len;
    }

    curs_set(1);

    for (;;)
    {
        int i;

        draw_popup_frame(y, x, h, w, title ? title : "Input");
        attron(COLOR_PAIR(COL_POPUP));

        if (prompt)
            mvaddnstr(y + 2, x + 2, prompt, w - 4);

        /* Use modular input_draw */
        input_draw(&state, y + 3, x + 2, w - 4, 1);
        standend(); /* Clear attributes after input_draw */

        attroff(COLOR_PAIR(COL_POPUP));

        /* Use modular input_move_cursor */
        input_move_cursor(&state, y + 3, x + 2, w - 4);
        refresh();

        rc = wrapper_read_key(&wch);

        if (rc == ERR)
            continue;

        if (wch == 27)
        {
            curs_set(0);
            if (saved)
            {
                copywin(saved, stdscr, 0, 0, y, x, h - 1, w - 1, 0);
                delwin(saved);
                refresh();
            }
            return -1;
        }

        if (wch == L'\n' || wch == L'\r' || wch == KEY_ENTER)
        {
            wbuf[state.len] = L'\0';
            curs_set(0);
            if (saved)
            {
                copywin(saved, stdscr, 0, 0, y, x, h - 1, w - 1, 0);
                delwin(saved);
                refresh();
            }
            return 0;
        }

        /* Use modular input_handle_key */
        if (input_handle_key(&state, (int)wch) == 1)
            continue;
    }

    /* Cleanup saved window (should not reach here, but for safety) */
    if (saved)
        delwin(saved);
}

/* Public wide-char input wrapper */
int ui_popup_input(const char *title, const char *prompt, wchar_t *wbuf, int wcap)
{
    return popup_input_core(title, prompt, wbuf, wcap);
}

int ui_popup_sort(char *spec, int specsz, const char *cfg_default)
{
    int choice, initial;
    char title[80];
    int i;

    if (!spec || specsz < 2)
        return -1;

    /* Calculate initial selection based on current spec */
    initial = 0; /* Default to _DEFAULT_ */

    if (spec[0] == '\0')
    {
        /* Empty config: select the actual default preset (index 1) */
        initial = 1;
    }
    else
    {
        /* If spec matches config default, select _DEFAULT_ */
        if (cfg_default && cfg_default[0] && strcmp(spec, cfg_default) == 0)
        {
            initial = 0;
        }
        else
        {
            /* Try to match current spec with a preset */
            for (i = 1; i < SORT_PRESETS_N - 1; i++)
            {
                if (strcmp(spec, sort_preset_specs[i]) == 0)
                {
                    initial = i;
                    break;
                }
            }
        }

        /* If no match, keep _DEFAULT_ selected */
    }

    snprintf(title, sizeof(title), "Sort areas by");

    choice = ui_popup_list(title, sort_preset_labels, SORT_PRESETS_N, initial);

    if (choice < 0)
        return -1;

    if (strcmp(sort_preset_specs[choice], "_CUSTOM_") == 0)
    {
        wchar_t wtmp[CFG_SORT_MAX];
        wchar_t *w_initial;

        w_initial = utf8_to_wcs(spec, NULL);
        wtmp[0] = L'\0';

        if (w_initial)
        {
            wcsncpy(wtmp, w_initial, (size_t)(CFG_SORT_MAX - 1));
            wtmp[CFG_SORT_MAX - 1] = L'\0';
            free(w_initial);
        }

        if (ui_popup_input("Custom sort spec", "Letters: A B D E F G M O P T U X Y Z (prefix - = desc)", wtmp, CFG_SORT_MAX) != 0)
            return -1;

        char *u = wcs_to_utf8(wtmp, (int)wcslen(wtmp));
        if (u)
        {
            strncpy(spec, u, (size_t)(specsz - 1));
            spec[specsz - 1] = '\0';
            free(u);
        }
    }
    else if (strcmp(sort_preset_specs[choice], "_DEFAULT_") == 0)
    {
        /* _DEFAULT_: use config file default */
        if (cfg_default && cfg_default[0])
        {
            strncpy(spec, cfg_default, (size_t)(specsz - 1));
            spec[specsz - 1] = '\0';
        }
        else
        {
            spec[0] = '\0';
        }
    }
    else
    {
        strncpy(spec, sort_preset_specs[choice], (size_t)(specsz - 1));
        spec[specsz - 1] = '\0';
    }

    return 0;
}

/* Search results popup: shows list of matches with line numbers and context */
int ui_popup_search_results(const char *title, const int *line_nums, const char **contexts, int count, int initial)
{
    int y, x, h, w;
    int want_h, want_w = 60;
    int sel = initial, top = 0;
    int i, ch;
    int saved_cursor;
    int visible;
    int maxtop;

    if (!line_nums || !contexts || count <= 0)
        return -1;

    if (sel < 0 || sel >= count)
        sel = 0;

    /* Hide cursor for list selection */
    saved_cursor = curs_set(0);

    /* Compute width from longest context + line number prefix */
    for (i = 0; i < count; i++)
    {
        int l = contexts[i] ? (int)strlen(contexts[i]) : 0;
        int prefix_len = 10; /* "Line XXXX: " */

        if (l + prefix_len + 4 > want_w)
            want_w = l + prefix_len + 4;
    }

    want_h = count + 4;

    if (want_h > LINES - 4)
        want_h = LINES - 4;

    if (want_w > COLS - 4)
        want_w = COLS - 4;

    ui_popup_center(want_h, want_w, &y, &x, &h, &w);

    visible = h - 4;

    if (visible < 1)
        visible = 1;

    maxtop = count - visible;

    if (maxtop < 0)
        maxtop = 0;

    for (;;)
    {
        standend(); /* Clear any residual attributes from previous iteration */
        draw_popup_frame(y, x, h, w, title ? title : "Search Results");
        attron(COLOR_PAIR(COL_POPUP));

        /* Show scroll indicators if needed */
        if (top > 0)
        {
            move(y + 1, x + w - 2);
            addch('^');
        }
        if (top + visible < count)
        {
            move(y + h - 2, x + w - 2);
            addch('v');
        }

        for (i = 0; i < visible && top + i < count; i++)
        {
            int idx = top + i;
            char line_buf[256];
            int line_y = y + 2 + i;

            move(line_y, x + 2);

            if (idx == sel)
            {
                int used;
                attron(COLOR_PAIR(COL_POPUP_SEL));

                /* Format: "Line XXXX: context..." */
                snprintf(line_buf, sizeof(line_buf), "Line %4d: %s", line_nums[idx], contexts[idx] ? contexts[idx] : "");

                addch('>');
                addch(' ');
                addnstr(line_buf, w - 6);

                used = 2 + (int)strlen(line_buf);

                while (used < w - 4)
                {
                    addch(' ');
                    used++;
                }

                attroff(COLOR_PAIR(COL_POPUP_SEL));
                attron(COLOR_PAIR(COL_POPUP));
            }
            else
            {
                snprintf(line_buf, sizeof(line_buf), "Line %4d: %s", line_nums[idx], contexts[idx] ? contexts[idx] : "");

                addch(' ');
                addch(' ');
                addnstr(line_buf, w - 6);
            }
        }

        /* Footer with position info */
        if (maxtop > 0)
        {
            char foot[80];
            snprintf(foot, sizeof(foot), "%d/%d  Enter=Jump  ESC=Cancel", sel + 1, count);
            mvaddnstr(y + h - 2, x + 2, foot, w - 4);
        }
        else
        {
            mvaddnstr(y + h - 2, x + 2, "Enter=Jump  ESC=Cancel", w - 4);
        }

        attroff(COLOR_PAIR(COL_POPUP));
        refresh();

        ch = getch();

        if (ch == 27 || ch == 'q' || ch == 'Q')
        {
            curs_set(saved_cursor);
            return -1;
        }

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
        {
            curs_set(saved_cursor);
            return sel;
        }

        if (ch == KEY_UP || ch == 'k')
        {
            if (sel > 0)
                sel--;
        }
        else if (ch == KEY_DOWN || ch == 'j')
        {
            if (sel < count - 1)
                sel++;
        }
        else if (ch == KEY_PPAGE || ch == CTRL('U') || ch == 'b')
        {
            sel -= visible;

            if (sel < 0)
                sel = 0;
        }
        else if (ch == KEY_NPAGE || ch == CTRL('D') || ch == ' ')
        {
            sel += visible;

            if (sel >= count)
                sel = count - 1;
        }
        else if (ch == KEY_HOME || ch == CTRL('B'))
        {
            sel = 0;
        }
        else if (ch == KEY_END || ch == CTRL('E'))
        {
            sel = count - 1;
        }

        /* Update scroll position */
        if (sel < top)
            top = sel;

        if (sel >= top + visible)
            top = sel - visible + 1;

        if (top < 0)
            top = 0;

        if (top > maxtop)
            top = maxtop;
    }
}

/* Help screen */
void ui_popup_help(const char *title, const char *const *lines, int n)
{
    int y, x, h, w;
    int want_h = n + 4;
    int want_w = 0;
    int i;
    int top = 0;
    int visible;
    int maxtop;
    int saved_cursor;

    for (i = 0; i < n; i++)
    {
        int l = lines[i] ? (int)strlen(lines[i]) : 0;

        if (l + 4 > want_w)
            want_w = l + 4;
    }

    if (want_w < 50)
        want_w = 50;

    ui_popup_center(want_h, want_w, &y, &x, &h, &w);

    /* Rows available for help text: frame (2) + footer line (1) + spacing (1)
     * The list scrolls when it does not all fit */
    visible = h - 4;

    if (visible < 1)
        visible = 1;

    maxtop = n - visible;

    if (maxtop < 0)
        maxtop = 0;

    /* Hide cursor for help display */
    saved_cursor = curs_set(0);

    for (;;)
    {
        int ch;

        standend(); /* Clear any residual attributes from previous iteration */
        draw_popup_frame(y, x, h, w, title ? title : "Help");
        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < visible && top + i < n; i++)
            mvaddnstr(y + 2 + i, x + 2, lines[top + i] ? lines[top + i] : "", w - 4);

        /* Footer - clear area first to apply popup background */
        for (i = 0; i < w - 4; i++)
            mvaddch(y + h - 2, x + 2 + i, ' ');

        if (maxtop > 0)
        {
            char foot[80];

            snprintf(foot, sizeof(foot), "%d-%d/%d  arrows/PgUp/PgDn  ESC=close", top + 1, (top + visible < n) ? top + visible : n, n);
            mvaddnstr(y + h - 2, x + 2, foot, w - 4);
        }
        else
        {
            mvaddnstr(y + h - 2, x + 2, "Press any key to close", w - 4);
        }

        attroff(COLOR_PAIR(COL_POPUP));
        refresh();

        ch = getch();

        if (maxtop == 0)
        {
            curs_set(saved_cursor);
            break; /* everything fits: any key closes (old behaviour) */
        }

        if (ch == 27 || ch == 'q' || ch == 'Q' || ch == '\n' || ch == '\r' ||
            ch == KEY_ENTER)
        {
            curs_set(saved_cursor);
            break;
        }
        else if (ch == KEY_UP || ch == 'k')
        {
            if (top > 0)
                top--;
        }
        else if (ch == KEY_DOWN || ch == 'j')
        {
            if (top < maxtop)
                top++;
        }
        else if (ch == KEY_PPAGE || ch == CTRL('U') || ch == 'b') /* Page Up */
        {
            top -= visible;

            if (top < 0)
                top = 0;
        }
        else if (ch == KEY_NPAGE || ch == CTRL('D') || ch == ' ') /* Page Down */
        {
            top += visible;

            if (top > maxtop)
                top = maxtop;
        }
        else if (ch == KEY_HOME || ch == CTRL('B'))
        {
            top = 0;
        }
        else if (ch == KEY_END || ch == CTRL('E'))
        {
            top = maxtop;
        }
    }
}

/* Find & Replace popup with case-sensitive and whole-word options */
int ui_popup_replace(const wchar_t *search_in, const wchar_t *replace_in, wchar_t *search_out, int search_outsz, wchar_t *replace_out, int replace_outsz, int *case_sensitive, int *whole_word)
{
    int y, x, h, w;
    int field = 0; /* 0=search, 1=replace, 2=case, 3=whole */
    int ch;
    wchar_t search_buf[64], replace_buf[64];
    InputState search_state, replace_state;
    int case_flag = case_sensitive ? *case_sensitive : 0;
    int whole_flag = whole_word ? *whole_word : 0;

    /* Initialize buffers */
    wcsncpy(search_buf, search_in, 63);
    search_buf[63] = L'\0';

    wcsncpy(replace_buf, replace_in, 63);
    replace_buf[63] = L'\0';

    /* Init search state */
    search_state.buf = search_buf;
    search_state.bufsz = 64;
    search_state.cursor = (int)wcslen(search_buf);
    search_state.len = search_state.cursor;

    /* Init replace state */
    replace_state.buf = replace_buf;
    replace_state.bufsz = 64;
    replace_state.cursor = (int)wcslen(replace_buf);
    replace_state.len = replace_state.cursor;

    ui_popup_center(8, 70, &y, &x, &h, &w);

    curs_set(1);

    for (;;)
    {
        const char *labels[4] = {"Search:      ", "Replace with: ", "Case sens:    ", "Whole word:   "};
        int field_start = 15;
        int i;

        standend(); /* Clear any residual attributes from previous iteration */
        draw_popup_frame(y, x, h, w, "Find & Replace");
        attron(COLOR_PAIR(COL_POPUP));

        /* Draw search field */
        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < w - 4; i++)
            mvaddch(y + 2, x + 2 + i, ' ');

        if (field == 0)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        mvaddnstr(y + 2, x + 2, labels[0], 14);

        if (field == 0)
            attroff(COLOR_PAIR(COL_POPUP_SEL));

        input_draw(&search_state, y + 2, x + field_start, w - field_start - 3, field == 0);
        standend(); /* Clear attributes after input_draw */

        /* Draw replace field */
        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < w - 4; i++)
            mvaddch(y + 3, x + 2 + i, ' ');

        if (field == 1)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        mvaddnstr(y + 3, x + 2, labels[1], 14);

        if (field == 1)
            attroff(COLOR_PAIR(COL_POPUP_SEL));

        input_draw(&replace_state, y + 3, x + field_start, w - field_start - 3, field == 1);
        standend(); /* Clear attributes after input_draw */

        /* Draw checkboxes */
        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < w - 4; i++)
            mvaddch(y + 4, x + 2 + i, ' ');

        mvaddnstr(y + 4, x + 2, labels[2], 14);

        if (field == 2)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        mvaddnstr(y + 4, x + field_start, case_flag ? "<Yes>" : "<No>", -1);

        if (field == 2)
            attroff(COLOR_PAIR(COL_POPUP_SEL));

        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < w - 4; i++)
            mvaddch(y + 5, x + 2 + i, ' ');

        mvaddnstr(y + 5, x + 2, labels[3], 14);

        if (field == 3)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        mvaddnstr(y + 5, x + field_start, whole_flag ? "<Yes>" : "<No>", -1);

        if (field == 3)
            attroff(COLOR_PAIR(COL_POPUP_SEL));

        standend(); /* Clear attributes after checkbox */

        /* Status bar */
        attron(COLOR_PAIR(COL_STATUS));

        for (i = 0; i < w - 4; i++)
            mvaddch(y + h - 2, x + 2 + i, ' ');

        mvaddnstr(y + h - 2, x + 2, "TAB: field  Enter=OK  ESC=Cancel", w - 4);
        attroff(COLOR_PAIR(COL_STATUS));

        /* Position cursor */
        if (field == 0)
            input_move_cursor(&search_state, y + 2, x + field_start, w - field_start - 3);
        else if (field == 1)
            input_move_cursor(&replace_state, y + 3, x + field_start, w - field_start - 3);
        else if (field == 2)
            move(y + 4, x + field_start);
        else if (field == 3)
            move(y + 5, x + field_start);

        refresh();

        ch = getch();

        if (ch == 27)
        {
            standend();
            curs_set(0);
            refresh();
            return -1;
        }

        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
        {
            standend();
            curs_set(0);
            refresh();

            /* Copy results */
            wcsncpy(search_out, search_buf, search_outsz - 1);
            search_out[search_outsz - 1] = L'\0';

            wcsncpy(replace_out, replace_buf, replace_outsz - 1);
            replace_out[replace_outsz - 1] = L'\0';

            /* Copy checkbox states */
            if (case_sensitive)
                *case_sensitive = case_flag;

            if (whole_word)
                *whole_word = whole_flag;

            return 0;
        }

        if (ch == '\t' || ch == KEY_DOWN)
        {
            field = (field + 1) % 4;
            continue;
        }

        if (ch == KEY_UP)
        {
            field = (field - 1 + 4) % 4;
            continue;
        }

        /* Handle space and arrow keys for checkboxes */
        if ((ch == ' ' || ch == KEY_LEFT || ch == KEY_RIGHT) && (field == 2 || field == 3))
        {
            if (field == 2)
                case_flag = !case_flag;
            else if (field == 3)
                whole_flag = !whole_flag;

            continue;
        }

        /* Handle text input for search/replace fields */
        if (field == 0)
        {
            if (input_handle_key(&search_state, ch) == 1)
                continue;
        }

        if (field == 1)
        {
            if (input_handle_key(&replace_state, ch) == 1)
                continue;
        }
    }
}
