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
 */

#include "ui_editor_draw.h"
#include "ui.h"
#include "ui_internal.h"
#include "ui_editor_internal.h"
#include "ui_editor_softwrap.h"
#include "ui_editor_helper.h"
#include "ui_attr.h"
#include "ui_spell.h"
#include "ui_grammar.h"
#include "ui_dict.h"
#include "ui_assist.h"
#include "../core/msghdr.h"
#include "../components/editor.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

/* Calculate width needed for line numbers (digits + 1 space) */
int lineno_width(int line_count)
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

    return width + 1; /* +1 for space after number */
}

#ifdef HAVE_HUNSPELL
/* Check whether a word is misspelled, ignoring words split across two lines by a trailing hyphen (e.g. "adqui-" / "rido") */
static int spell_word_incorrect(UiApp *app, int line_idx, const wchar_t *line, int line_len, int word_start, int word_end)
{
    int word_len = word_end - word_start;
    int incorrect = 0;
    int joined_len = 0;
    int i, j;
    int next_end = 0;
    int prev_word_end = 0;
    int prev_word_start = 0;
    int next_len = 0;
    int prev_len = 0;
    wchar_t joined[512];
    const wchar_t *next_line = NULL;
    const wchar_t *prev_line = NULL;
    Ed *ed = NULL;

    if (word_len <= 1 || !app || !app->spell_handle || !app->spell_active)
        return 0;

    incorrect = ui_spell_check_word_simple(app, &line[word_start], word_len);

    if (!incorrect)
        return 0;

    ed = app->editor;

    if (!ed || line_idx < 0 || line_idx >= ed->count)
        return incorrect;

    /* Word followed by '-' at EOL and continuation on next line */
    if (word_end < line_len && line[word_end] == L'-' && word_end + 1 == line_len && line_idx + 1 < ed->count)
    {
        next_line = ed_line_wcs(ed, line_idx + 1);
        next_len = next_line ? ed_line_len(ed, line_idx + 1) : -1;

        if (next_len > 0)
        {
            while (next_end < next_len && iswalnum((wint_t)next_line[next_end]))
                next_end++;

            if (next_end > 0 && iswlower((wint_t)next_line[0]))
            {
                for (i = 0; i < word_len && joined_len < 510; i++)
                    joined[joined_len++] = line[word_start + i];

                for (j = 0; j < next_end && joined_len < 510; j++)
                    joined[joined_len++] = next_line[j];

                joined[joined_len] = L'\0';

                if (!ui_spell_check_word_simple(app, joined, joined_len))
                    incorrect = 0;
            }
        }
    }

    /* Word at column 0 preceded by "word-" on previous line */
    if (incorrect && word_start == 0 && line_idx > 0)
    {
        prev_line = ed_line_wcs(ed, line_idx - 1);
        prev_len = prev_line ? ed_line_len(ed, line_idx - 1) : -1;

        if (prev_len >= 2 && prev_line[prev_len - 1] == L'-')
        {
            prev_word_end = prev_len - 1;
            prev_word_start = prev_word_end;

            while (prev_word_start > 0 && iswalnum((wint_t)prev_line[prev_word_start - 1]))
                prev_word_start--;

            if (prev_word_end > prev_word_start)
            {
                joined_len = 0;

                for (j = prev_word_start; j < prev_word_end && joined_len < 510; j++)
                    joined[joined_len++] = prev_line[j];

                for (i = 0; i < word_len && joined_len < 510; i++)
                    joined[joined_len++] = line[word_start + i];

                joined[joined_len] = L'\0';

                if (!ui_spell_check_word_simple(app, joined, joined_len))
                    incorrect = 0;
            }
        }
    }

    return incorrect;
}
#endif

/* Header field drawing */
void draw_edit_header(UiApp *app)
{
    AreaEntry *ae;
    const wchar_t *from = msghdr_get(app->edit_hdr, HDR_FROM);
    const wchar_t *to = msghdr_get(app->edit_hdr, HDR_TO);
    static const char SPACES[257] =
        "                                                                "
        "                                                                "
        "                                                                "
        "                                                                ";
    const wchar_t *subj = msghdr_get(app->edit_hdr, HDR_SUBJECT);
    const char *oaddr = ui_wcs2u8(msghdr_get(app->edit_hdr, HDR_OADDR));
    const char *daddr = ui_wcs2u8(msghdr_get(app->edit_hdr, HDR_DADDR));
    const char *area = ui_wcs2u8(msghdr_get(app->edit_hdr, HDR_AREA));
    char attr_str[40];
    int i;
    const char *cs;
    int cl;
    const char *attach_mode = "";

    standend();

    ae = &app->areas->entries[app->sess.area_idx];

    /* Build attribute string */
    ui_attr_build(app->edit_attr, attr_str, sizeof(attr_str));

    attron(COLOR_PAIR(COL_HEADER));

    for (i = 1; i < 7; i++)
    {
        int written = 0;

        move(i, 0);
        clrtoeol();

        while (written < COLS)
        {
            int chunk = COLS - written;

            if (chunk > (int)sizeof(SPACES))
                chunk = (int)sizeof(SPACES);

            mvaddnstr(i, written, SPACES, chunk);
            written += chunk;
        }
    }

    /* Area on left, edit-mode label in menu bar */
    /*mvprintw(1, 1, "Area: %-30.30s  %s", area, app->edit_is_new ? (app->edit_is_reply ? "Reply" : "New Message") : "Edit");*/

    if (app->attach_list && app->attach_list->count > 0)
        attach_mode = "[ATT]";

    mvprintw(1, 1, "Area: %-30.30s  %s  %s", area, app->edit_is_new ? (app->edit_is_reply ? "Reply" : "New Message") : "Edit", attach_mode);

    /* From + origin AKA */
    if (app->edit_active_field == EF_FROM)
        attron(A_REVERSE);

    mvprintw(2, 1, "From: %-25.25s  %-20.20s %s", ui_wcs2u8(from), oaddr, "[locked]");

    if (app->edit_active_field == EF_FROM)
    {
        attroff(A_REVERSE);
        move(msghdr_field_row(app->edit_hdr, HDR_FROM), msghdr_field_col(app->edit_hdr, HDR_FROM) + msghdr_edit_col(app->edit_hdr));
        curs_set(1);
    }

    /* To + destination AKA (netmail only) */
    if (app->edit_active_field == EF_TO)
        attron(A_REVERSE);

    mvprintw(3, 1, "  To: %-25.25s", ui_wcs2u8(to));

    if (app->edit_active_field == EF_TO)
    {
        attroff(A_REVERSE);
        move(msghdr_field_row(app->edit_hdr, HDR_TO), msghdr_field_col(app->edit_hdr, HDR_TO) + msghdr_edit_col(app->edit_hdr));
        curs_set(1);
    }

    /* Subject (or Dest for netmail) */
    if (ae->type != AREATYPE_NETMAIL)
    {
        if (app->edit_active_field == EF_SUBJECT)
            attron(A_REVERSE);

        mvprintw(4, 1, "Subj: %s", ui_wcs2u8(subj));

        if (app->edit_active_field == EF_SUBJECT)
        {
            attroff(A_REVERSE);
            move(msghdr_field_row(app->edit_hdr, HDR_SUBJECT), msghdr_field_col(app->edit_hdr, HDR_SUBJECT) + msghdr_edit_col(app->edit_hdr));
            curs_set(1);
        }
    }
    else
    {
        /* Destination AKA (netmail only, editable) */
        if (app->edit_active_field == EF_DADDR)
            attron(A_REVERSE);

        mvprintw(4, 1, "Dest: %-25.25s", daddr[0] ? daddr : "");

        if (app->edit_active_field == EF_DADDR)
        {
            attroff(A_REVERSE);
            move(msghdr_field_row(app->edit_hdr, HDR_DADDR), msghdr_field_col(app->edit_hdr, HDR_DADDR) + msghdr_edit_col(app->edit_hdr));
            curs_set(1);
        }

        /* Subject (netmail only) */
        if (app->edit_active_field == EF_SUBJECT)
            attron(A_REVERSE);

        mvprintw(5, 1, "Subj: %s", ui_wcs2u8(subj));

        if (app->edit_active_field == EF_SUBJECT)
        {
            attroff(A_REVERSE);
            move(msghdr_field_row(app->edit_hdr, HDR_SUBJECT), msghdr_field_col(app->edit_hdr, HDR_SUBJECT) + msghdr_edit_col(app->edit_hdr));
            curs_set(1);
        }
    }

    /* Attr on left, Charset on right */
    mvprintw(ae->type == AREATYPE_NETMAIL ? 6 : 5, 1, "Attr: %s", attr_str[0] ? attr_str : "-");

    /* Show effective output charset (resolve Auto to cfg->charset) */
    cs = app->edit_charset[0] ? app->edit_charset : (app->cfg && app->cfg->charset[0] ? app->cfg->charset : "UTF-8");
    cl = (int)strlen(cs);

    mvprintw(ae->type == AREATYPE_NETMAIL ? 6 : 5, COLS - cl - 10, "Charset: %s", cs);

    /* Hide cursor if not in header edit mode */
    if (app->edit_active_field != EF_FROM && app->edit_active_field != EF_TO && app->edit_active_field != EF_SUBJECT && app->edit_active_field != EF_DADDR)
        curs_set(0);

    attroff(COLOR_PAIR(COL_HEADER));

    attron(COLOR_PAIR(COL_BORDER));
    ui_hline(ae->type == AREATYPE_NETMAIL ? 7 : 6, 0, COLS);
    attroff(COLOR_PAIR(COL_BORDER));
}

/* Position cursor in active field after status bar redraw */
void position_edit_cursor(UiApp *app)
{
    int ec;

    if (!app)
        return;

    /* Clear residual attributes before positioning cursor */
    standend();

    ec = msghdr_edit_col(app->edit_hdr);

    switch (app->edit_active_field)
    {
    case EF_FROM:
        move(msghdr_field_row(app->edit_hdr, HDR_FROM), msghdr_field_col(app->edit_hdr, HDR_FROM) + ec);
        curs_set(1);
        return;
    case EF_TO:
        move(msghdr_field_row(app->edit_hdr, HDR_TO), msghdr_field_col(app->edit_hdr, HDR_TO) + ec);
        curs_set(1);
        return;
    case EF_DADDR:
        move(msghdr_field_row(app->edit_hdr, HDR_DADDR), msghdr_field_col(app->edit_hdr, HDR_DADDR) + ec);
        curs_set(1);
        return;
    case EF_SUBJECT:
        move(msghdr_field_row(app->edit_hdr, HDR_SUBJECT), msghdr_field_col(app->edit_hdr, HDR_SUBJECT) + ec);
        curs_set(1);
        return;
    case EF_BODY:
    {
        EdInfo info;
        AreaEntry *ae = NULL;
        int start_row, rows;
        int cy, cx;
        int max_y;
        int soft;
        int ln_offset = 0;
        int show_lnum = app->cfg && app->cfg->show_line_numbers;

        ae = &app->areas->entries[app->sess.area_idx];
        start_row = (ae->type == AREATYPE_NETMAIL) ? 8 : 7;
        rows = LINES - start_row - 1;

        /* Reserve space for spell panel at bottom when active */
        if (app->show_spell)
            rows -= SPELL_PANEL_H;

        if (rows < 1)
            rows = 1;

        soft = !(app->cfg && app->cfg->hard_wrap);
        ed_get_info(app->editor, &info);

        /* Calculate line number offset if enabled */
        if (show_lnum)
            ln_offset = editor_body_offset(app, info.line_count);

        if (soft)
        {
            int width = COLS < 1 ? 1 : COLS;
            int screen_row;

            if (show_lnum)
                width = COLS - ln_offset;

            screen_row = soft_cursor_screen_row(app, width);
            cy = start_row + screen_row;
            cx = ln_offset + soft_cursor_vcol(app->editor, width);
        }
        else
        {
            const wchar_t *wl = ed_line_wcs(app->editor, info.row);
            int line_len = ed_line_len(app->editor, info.row);
            int wchar_col = info.col;
            extern int s_tab_width;

            if (wchar_col > line_len)
                wchar_col = line_len;
            if (wchar_col < 0)
                wchar_col = 0;

            cy = start_row + (info.row - info.top);
            cx = ln_offset + (wl ? wcs_vwidth_ex(wl, wchar_col, 0, s_tab_width) : wchar_col);
        }

        /* Always move; clamp to body region */
        max_y = start_row + rows - 1;

        if (max_y < start_row)
            max_y = start_row;

        if (cy < start_row)
            cy = start_row;

        if (cy > max_y)
            cy = max_y;

        if (show_lnum && cx < ln_offset)
            cx = ln_offset;
        else if (cx < 0)
            cx = 0;

        if (cx > COLS - 1)
            cx = COLS - 1;

        move(cy, cx);

        /* Use normal cursor visibility */
        curs_set(1);

        return;
    }
    default:
        curs_set(0);
        return;
    }
}

void draw_edit_body(UiApp *app)
{
    EdInfo info;
    AreaEntry *ae = NULL;
    int rows, start_row, i;
    int b_r1 = -1, b_c1 = 0, b_r2 = -1, b_c2 = 0;
    int soft;
    int width;
    int li, sr;
    int ln_width = 0;  /* line number width */
    int ln_offset = 0; /* offset for editor content */
    int show_lnum = app->cfg && app->cfg->show_line_numbers;
    int sub_skip;
    int tab_width = (app->cfg && app->cfg->tab_width > 0) ? app->cfg->tab_width : 4;

    standend();

    extern int s_tab_width;

    s_tab_width = tab_width;
    ed_set_tab_width(tab_width);

    ae = &app->areas->entries[app->sess.area_idx];
    start_row = (ae->type == AREATYPE_NETMAIL) ? 8 : 7;
    rows = LINES - start_row - 1;

    /* Reserve space for spell/dict panel at bottom when active */
    if (app->show_spell)
        rows -= SPELL_PANEL_H;
    else if (app->show_dict)
        rows -= DICT_PANEL_HEIGHT;

    if (rows < 1)
        rows = 1;

    soft = !(app->cfg && app->cfg->hard_wrap);
    width = COLS; /* visual wrap fits the terminal */

    if (width < 1)
        width = 1;

    ed_set_page(app->editor, rows);
    ed_get_info(app->editor, &info);

    /* Bracket matching: find partner bracket across buffer */
    {
        int match_row = -1;
        int match_col = -1;
        app->bracket_match_row = -1;
        app->bracket_match_col = -1;

        if (app->cfg && app->cfg->show_brackets)
        {
            const wchar_t *line = ed_line_wcs(app->editor, info.row);
            int line_len = ed_line_len(app->editor, info.row);

            if (line && info.col < line_len)
            {
                wchar_t ch = line[info.col];
                wchar_t partner = 0;
                int dir = 0;

                switch (ch)
                {
                case L'(':
                    partner = L')';
                    dir = +1;
                    break;
                case L'[':
                    partner = L']';
                    dir = +1;
                    break;
                case L'{':
                    partner = L'}';
                    dir = +1;
                    break;
                case L')':
                    partner = L'(';
                    dir = -1;
                    break;
                case L']':
                    partner = L'[';
                    dir = -1;
                    break;
                case L'}':
                    partner = L'{';
                    dir = -1;
                    break;
                default:
                    break;
                }

                if (partner)
                {
                    int depth = 1;
                    int row;

                    if (dir > 0)
                    {
                        for (row = info.row; row < info.line_count && match_row < 0; row++)
                        {
                            const wchar_t *rl = ed_line_wcs(app->editor, row);
                            int rl_len = ed_line_len(app->editor, row);
                            int col;
                            int start_col = (row == info.row) ? info.col + 1 : 0;

                            if (!rl)
                                continue;

                            for (col = start_col; col < rl_len; col++)
                            {
                                if (rl[col] == ch)
                                    depth++;
                                else if (rl[col] == partner)
                                {
                                    depth--;

                                    if (depth == 0)
                                    {
                                        match_row = row;
                                        match_col = col;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        for (row = info.row; row >= 0 && match_row < 0; row--)
                        {
                            const wchar_t *rl = ed_line_wcs(app->editor, row);
                            int rl_len = ed_line_len(app->editor, row);
                            int col;
                            int start_col = (row == info.row) ? info.col - 1 : rl_len - 1;

                            if (!rl)
                                continue;

                            for (col = start_col; col >= 0; col--)
                            {
                                if (rl[col] == ch)
                                    depth++;
                                else if (rl[col] == partner)
                                {
                                    depth--;

                                    if (depth == 0)
                                    {
                                        match_row = row;
                                        match_col = col;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if (match_row >= 0)
                {
                    app->bracket_match_row = match_row;
                    app->bracket_match_col = match_col;
                }
            }
        }
    }

    /* Calculate line number width if enabled */
    if (show_lnum)
    {
        ln_width = lineno_width(info.line_count);
        ln_offset = editor_body_offset(app, info.line_count);
        width = COLS - ln_offset; /* Reduce available width for text */
    }

    /* Normalize block range (anchor vs cursor) for highlight */
    if (info.block.active)
    {
        if (info.block.anchor_row < info.row || (info.block.anchor_row == info.row && info.block.anchor_col <= info.col))
        {
            b_r1 = info.block.anchor_row;
            b_c1 = info.block.anchor_col;
            b_r2 = info.row;
            b_c2 = info.col;
        }
        else
        {
            b_r1 = info.row;
            b_c1 = info.col;
            b_r2 = info.block.anchor_row;
            b_c2 = info.block.anchor_col;
        }
    }

    if (!soft)
    {
        /* HARD-WRAP: classic 1 logical line == 1 screen row */
        for (i = 0; i < rows; i++)
        {
            int line_idx = info.top + i;
            int line_len;
            const wchar_t *wl;
            int line_vw;
            int x_text_end;
            int x_screen_end;

            move(start_row + i, 0);
            clrtoeol();

            if (line_idx >= info.line_count)
                continue;

            /* Draw line number if enabled */
            if (show_lnum)
            {
                attrset(COLOR_PAIR(COL_BORDER));
                mvprintw(start_row + i, 0, "%*d", ln_width - 1, line_idx + 1);
                standend();

                attron(COLOR_PAIR(COL_NORMAL));
            }

            /* mvaddnwstr: n is in wide chars, no UTF-8 conversion needed */
            wl = ed_line_wcs(app->editor, line_idx);
            line_len = ed_line_len(app->editor, line_idx);

            if (wl && line_len > 0)
                ui_draw_wcs_line_with_tabs(start_row + i, ln_offset, wl, line_len, tab_width);

            /* Highlight matched bracket partner only */
            if (app->cfg && app->cfg->show_brackets && app->bracket_match_row == line_idx && app->bracket_match_col >= 0 && app->bracket_match_col < line_len && wl)
            {
                int tc = app->bracket_match_col;
                int col_x = ln_offset + wcs_vwidth_ex(wl, tc, 0, tab_width);

                attron(COLOR_PAIR(COL_BRACKET_MATCH) | A_BOLD);
                mvaddnwstr(start_row + i, col_x, &wl[tc], 1);
                attroff(COLOR_PAIR(COL_BRACKET_MATCH) | A_BOLD);
            }

            /* Highlight search matches */
            if (app->edit_search.rows && app->edit_search.match_count > 0)
            {
                int j;

                for (j = 0; j < app->edit_search.match_count; j++)
                {
                    if (app->edit_search.rows[j] == line_idx)
                    {
                        int match_col = app->edit_search.cols[j];
                        int match_len = (int)wcslen(app->edit_search.query);

                        if (match_col >= 0 && match_col + match_len <= line_len)
                        {
                            attron(COLOR_PAIR(COL_SEARCH_MATCH));
                            mvaddnwstr(start_row + i, ln_offset + wcs_vwidth_ex(wl, match_col, 0, tab_width), &wl[match_col], match_len);
                            attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                        }
                    }
                }
            }

#ifdef HAVE_HUNSPELL
            /* Highlight misspelled words AND/OR repeated words */
            if ((app->spell_active && app->spell_handle) || app->cfg->assist_repeat_check)
            {
                int word_start = 0;
                int word_end;
                int spell_on = (app->spell_active && app->spell_handle);

                standend();

                while (word_start < line_len)
                {
                    while (word_start < line_len && !iswalnum((wint_t)wl[word_start]))
                        word_start++;

                    word_end = word_start;

                    while (word_end < line_len && iswalnum((wint_t)wl[word_end]))
                        word_end++;

                    if (word_end > word_start)
                    {
                        int word_len = word_end - word_start;
                        int marked = 0;

                        /* Ignore single-character words like word processors do */
                        if (spell_on && word_len > 1 && spell_word_incorrect(app, line_idx, wl, line_len, word_start, word_end))
                        {
                            attron(COLOR_PAIR(COL_SPELL_CURRENT));
                            mvaddnwstr(start_row + i, ln_offset + wcs_vwidth_ex(wl, word_start, 0, tab_width), &wl[word_start], word_len);
                            attroff(COLOR_PAIR(COL_SPELL_CURRENT));

                            marked = 1;
                        }

                        /* Repeated-word check (independent of spell) Highlight if previous word on same line is the same */
                        if (!marked && app->cfg->assist_repeat_check && ui_assist_check_repeat(app, line_idx, word_start, word_len))
                        {
                            attron(A_REVERSE);
                            mvaddnwstr(start_row + i, ln_offset + wcs_vwidth_ex(wl, word_start, 0, tab_width), &wl[word_start], word_len);
                            attroff(A_REVERSE);
                        }
                    }

                    word_start = word_end;
                }
            }
#endif

#ifdef HAVE_GRAMMAR
            /* Grammar/punctuation overlay — viewport-scoped, LRU-cached */
            if (app->grammar_active && app->grammar_handle && wl && line_len > 0)
                ui_grammar_draw_row(app, start_row + i, ln_offset, tab_width, wl, line_len, line_idx);
#endif

            /* Paint tabs and trailing spaces as visible glyphs */
            if (app->cfg->show_whitespace && line_len > 0)
            {
                int k;
                int trail_start = line_len;
                int max_chars = width;
                int display_len = line_len;

                if (display_len > max_chars)
                    display_len = max_chars;

                while (trail_start > 0 && (wl[trail_start - 1] == L' ' || wl[trail_start - 1] == L'\t'))
                    trail_start--;

                attron(COLOR_PAIR(COL_BORDER));

                for (k = 0; k < display_len; k++)
                {
                    wchar_t ch = wl[k];
                    int col_x = ln_offset + wcs_vwidth_ex(wl, k, 0, tab_width);

                    if (ch == L'\t')
                    {
#ifdef PLATFORM_AMIGA
                        if (app->cfg->ttf_enabled)
                            mvaddnwstr(start_row + i, col_x, L"\u2192", 1);
                        else
                            mvaddch(start_row + i, col_x, '>');
#else
                        mvaddnwstr(start_row + i, col_x, L"\u2192", 1);
#endif
                    }
                    else if (ch == L' ' && k >= trail_start)
                        mvaddnwstr(start_row + i, col_x, L"\u00b7", 1);
                }

                attroff(COLOR_PAIR(COL_BORDER));
            }

            if (b_r1 >= 0 && line_idx >= b_r1 && line_idx <= b_r2)
            {
                const wchar_t *wcs;
                int hs, he;

                hs = (line_idx == b_r1) ? b_c1 : 0;
                he = (line_idx == b_r2) ? b_c2 : line_len;

                if (hs < 0)
                    hs = 0;

                if (he > line_len)
                    he = line_len;

                wcs = wl;

                if (wcs && hs < he)
                {
                    attron(A_REVERSE);
                    mvaddnwstr(start_row + i, ln_offset + wcs_vwidth_ex(wcs, hs, 0, tab_width), &wcs[hs], he - hs);
                    attroff(A_REVERSE);
                }
                else if (hs == 0 && he == 0)
                {
                    /* Cursor at col 0 or empty line: show one reversed space */
                    attron(A_REVERSE);
                    mvaddch(start_row + i, ln_offset, ' ');
                    attroff(A_REVERSE);
                }
            }

            /* Visual overlays (hard-wrap path) */
            line_vw = wl ? wcs_vwidth_ex(wl, line_len, 0, tab_width) : 0;
            x_text_end = ln_offset + line_vw;
            x_screen_end = width;

            /* Highlight current line: paint empty area only */
            if (app->cfg && app->cfg->highlight_line && line_idx == info.row && x_text_end < x_screen_end)
            {
                int x;

                attron(COLOR_PAIR(COL_CURRENT_LINE));

                for (x = x_text_end; x < x_screen_end; x++)
                    mvaddch(start_row + i, x, ' ');

                attroff(COLOR_PAIR(COL_CURRENT_LINE));
            }

            /* Column ruler */
            if (app->cfg && app->cfg->ruler_col > 0)
            {
                int rx = ln_offset + app->cfg->ruler_col;

                if (rx > x_text_end && rx < x_screen_end)
                {
                    attron(COLOR_PAIR(COL_GUIDE));
                    mvaddch(start_row + i, rx, '|');
                    attroff(COLOR_PAIR(COL_GUIDE));
                }
            }

            /* Indent guides */
            if (app->cfg && app->cfg->indent_guides && wl && line_len > 0 && tab_width > 1)
            {
                int k;
                int leading_count = 0;

                for (k = 0; k < line_len; k++)
                {
                    if (wl[k] != L' ')
                        break;

                    leading_count++;
                }

                if (leading_count > 0)
                {
                    int g;

                    attron(COLOR_PAIR(COL_GUIDE));

                    for (g = tab_width; g <= leading_count; g += tab_width)
                    {
                        int gx = ln_offset + g - 1;

                        if (gx >= ln_offset && gx < width)
                            mvaddch(start_row + i, gx, '|');
                    }

                    attroff(COLOR_PAIR(COL_GUIDE));
                }
            }
        }

#ifdef HAVE_GRAMMAR
        /* Prewarm grammar cache for a small margin above/below viewport */
        if (app->grammar_active && app->grammar_handle)
            ui_grammar_prewarm(app, info.top, rows, info.line_count);
#endif

        return;
    }

    /* SOFT-WRAP: one logical line spans several screen rows */
    /* Adjust viewport so cursor is visible, O(distance to cursor) bounded by rows */
    soft_ensure_visible_for_draw(app, width, rows);

    /* Clear body */
    for (sr = 0; sr < rows; sr++)
    {
        move(start_row + sr, 0);
        clrtoeol();
    }

    /* Start drawing from s_soft_top_line, skipping the first s_soft_top_sub sub-rows of that line */
    li = s_soft_top_line;
    sub_skip = s_soft_top_sub;
    sr = 0;

    while (li < info.line_count && sr < rows)
    {
        const wchar_t *l = ed_line_wcs(app->editor, li);
        int len = ed_line_len(app->editor, li);
        int pos = 0;
        int s = 0;         /* sub-row index within this line */
        int first_seg = 1; /* tracks first painted sub-row for line number */

        if (!l || len <= 0)
        {
            /* Empty line: one blank sub-row, paint if not skipped */
            if (sub_skip == 0)
            {
                if (show_lnum)
                {
                    attrset(COLOR_PAIR(COL_BORDER));
                    mvprintw(start_row + sr, 0, "%*d", ln_width - 1, li + 1);
                    standend();

                    attron(COLOR_PAIR(COL_NORMAL));
                }

                if (b_r1 >= 0 && li >= b_r1 && li <= b_r2)
                {
                    attron(A_REVERSE);
                    mvaddch(start_row + sr, ln_offset, ' ');
                    attroff(A_REVERSE);
                }

                sr++;
            }

            li++;
            sub_skip = 0;

            continue;
        }

        while (sr < rows)
        {
            int seg_start = pos;
            int seg_end = wrap_next(l, len, width, pos);
            int np = seg_end;

            if (s >= sub_skip)
            {
                int seg_len = seg_end - seg_start;
                int text_vw;
                int x_text_end;
                int x_screen_end;
                int has_more_subrows;

                /* Tab offset is 0 inside each sub-row */
                int seg_start_vcol = 0;

                if (seg_len < 0)
                    seg_len = 0;

                /* Line number on the first painted sub-row of this line */
                if (show_lnum && first_seg)
                {
                    attrset(COLOR_PAIR(COL_BORDER));
                    mvprintw(start_row + sr, 0, "%*d", ln_width - 1, li + 1);
                    standend();

                    attron(COLOR_PAIR(COL_NORMAL));
                    first_seg = 0;
                }

                if (seg_len > 0)
                    ui_draw_wcs_line_with_tabs(start_row + sr, ln_offset, &l[seg_start], seg_len, tab_width);

                /* Paint tabs and trailing spaces as visible glyphs */
                if (app->cfg->show_whitespace && seg_len > 0)
                {
                    int k;
                    int trail_start = seg_end;

                    if (seg_end == len)
                    {
                        while (trail_start > seg_start && (l[trail_start - 1] == L' ' || l[trail_start - 1] == L'\t'))
                            trail_start--;
                    }

                    attron(COLOR_PAIR(COL_BORDER));

                    for (k = 0; k < seg_len; k++)
                    {
                        wchar_t ch = l[seg_start + k];
                        int col_x = ln_offset + wcs_vwidth_ex(&l[seg_start], k, seg_start_vcol, tab_width);

                        if (ch == L'\t')
                        {
#ifdef PLATFORM_AMIGA
                            if (app->cfg->ttf_enabled)
                                mvaddnwstr(start_row + sr, col_x, L"\u2192", 1);
                            else
                                mvaddch(start_row + sr, col_x, '>');
#else
                            mvaddnwstr(start_row + sr, col_x, L"\u2192", 1);
#endif
                        }
                        else if (ch == L' ' &&
                                 (seg_start + k) >= trail_start)
                            mvaddnwstr(start_row + sr, col_x, L"\u00b7", 1);
                    }

                    attroff(COLOR_PAIR(COL_BORDER));
                }

                /* Highlight matched bracket partner only */
                if (app->cfg && app->cfg->show_brackets && seg_len > 0 && app->bracket_match_row == li && app->bracket_match_col >= seg_start && app->bracket_match_col < seg_end)
                {
                    int tc = app->bracket_match_col;
                    int col_x = ln_offset + wcs_vwidth_ex(&l[seg_start], tc - seg_start, seg_start_vcol, tab_width);

                    attron(COLOR_PAIR(COL_BRACKET_MATCH) | A_BOLD);
                    mvaddnwstr(start_row + sr, col_x, &l[tc], 1);
                    attroff(COLOR_PAIR(COL_BRACKET_MATCH) | A_BOLD);
                }

                /* Highlight search matches */
                if (app->edit_search.rows && app->edit_search.match_count > 0)
                {
                    int j;

                    for (j = 0; j < app->edit_search.match_count; j++)
                    {
                        if (app->edit_search.rows[j] == li)
                        {
                            int match_col = app->edit_search.cols[j];
                            int match_len = (int)wcslen(app->edit_search.query);
                            int match_end = match_col + match_len;

                            if (match_col >= seg_start && match_end <= seg_end)
                            {
                                attron(COLOR_PAIR(COL_SEARCH_MATCH));
                                mvaddnwstr(start_row + sr, ln_offset + wcs_vwidth_ex(&l[seg_start], match_col - seg_start, seg_start_vcol, tab_width), &l[match_col], match_len);
                                attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                            }
                            else if (match_col >= seg_start && match_col < seg_end)
                            {
                                int partial_len = seg_end - match_col;
                                attron(COLOR_PAIR(COL_SEARCH_MATCH));
                                mvaddnwstr(start_row + sr, ln_offset + wcs_vwidth_ex(&l[seg_start], match_col - seg_start, seg_start_vcol, tab_width), &l[match_col], partial_len);
                                attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                            }
                            else if (match_end > seg_start && match_end <= seg_end)
                            {
                                int partial_len = match_end - seg_start;
                                attron(COLOR_PAIR(COL_SEARCH_MATCH));
                                mvaddnwstr(start_row + sr, ln_offset, &l[seg_start], partial_len);
                                attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                            }
                        }
                    }
                }

#ifdef HAVE_HUNSPELL
                /* Highlight misspelled words AND/OR repeated words. Enter loop if either feature is active */
                if ((app->spell_active && app->spell_handle) || app->cfg->assist_repeat_check)
                {
                    int word_start = seg_start;
                    int word_end;
                    int spell_on = (app->spell_active && app->spell_handle);

                    standend();

                    while (word_start < seg_end)
                    {
                        while (word_start < seg_end && !iswalnum((wint_t)l[word_start]))
                            word_start++;

                        word_end = word_start;

                        while (word_end < seg_end && iswalnum((wint_t)l[word_end]))
                            word_end++;

                        if (word_end > word_start)
                        {
                            int word_len = word_end - word_start;
                            int marked = 0;

                            /* Ignore single-character words */
                            if (spell_on && word_len > 1 && spell_word_incorrect(app, li, l, len, word_start, word_end))
                            {
                                attron(COLOR_PAIR(COL_SPELL_CURRENT));
                                mvaddnwstr(start_row + sr, ln_offset + wcs_vwidth_ex(&l[seg_start], word_start - seg_start, seg_start_vcol, tab_width), &l[word_start], word_len);
                                attroff(COLOR_PAIR(COL_SPELL_CURRENT));

                                marked = 1;
                            }

                            /* Repeated-word check (independent of spell) Highlight if previous word on same line is the same */
                            if (!marked && app->cfg->assist_repeat_check && ui_assist_check_repeat(app, li, word_start, word_len))
                            {
                                attron(A_REVERSE);
                                mvaddnwstr(start_row + sr, ln_offset + wcs_vwidth_ex(&l[seg_start], word_start - seg_start, seg_start_vcol, tab_width), &l[word_start], word_len);
                                attroff(A_REVERSE);
                            }
                        }

                        word_start = word_end;
                    }
                }
#endif

#ifdef HAVE_GRAMMAR
                /* Grammar overlay for soft-wrap segment, cache-friendly check runs on full logical line once */
                if (app->grammar_active && app->grammar_handle && l && len > 0 && seg_start < seg_end)
                    ui_grammar_draw_row_segment(app, start_row + sr, ln_offset, tab_width, l, len, seg_start, seg_end, seg_start_vcol, li);
#endif

                /* Block-selection overlay */
                if (b_r1 >= 0 && li >= b_r1 && li <= b_r2 && l)
                {
                    int hs = (li == b_r1) ? b_c1 : 0;
                    int he = (li == b_r2) ? b_c2 : len;

                    if (hs < seg_start)
                        hs = seg_start;

                    if (he > seg_end)
                        he = seg_end;

                    if (hs < he)
                    {
                        attron(A_REVERSE);
                        mvaddnwstr(start_row + sr, ln_offset + wcs_vwidth_ex(&l[seg_start], hs - seg_start, seg_start_vcol, tab_width), &l[hs], he - hs);
                        attroff(A_REVERSE);
                    }
                    else if (hs == seg_start && he == seg_start)
                    {
                        attron(A_REVERSE);
                        mvaddch(start_row + sr, ln_offset, ' ');
                        attroff(A_REVERSE);
                    }
                }

                /* Visual overlays (soft-wrap path) */
                text_vw = wcs_vwidth_ex(&l[seg_start], seg_len, 0, tab_width);
                x_text_end = ln_offset + text_vw;
                x_screen_end = ln_offset + width;
                has_more_subrows = (seg_end < len);

                /* Highlight current line: paint empty area only */
                if (app->cfg && app->cfg->highlight_line && li == info.row && x_text_end < x_screen_end)
                {
                    int x;

                    attron(COLOR_PAIR(COL_CURRENT_LINE));

                    for (x = x_text_end; x < x_screen_end; x++)
                        mvaddch(start_row + sr, x, ' ');

                    attroff(COLOR_PAIR(COL_CURRENT_LINE));
                }

                /* Column ruler */
                if (app->cfg && app->cfg->ruler_col > 0)
                {
                    int rx = ln_offset + app->cfg->ruler_col;

                    if (rx > x_text_end && rx < x_screen_end)
                    {
                        attron(COLOR_PAIR(COL_GUIDE));
                        mvaddch(start_row + sr, rx, '|');
                        attroff(COLOR_PAIR(COL_GUIDE));
                    }
                }

                /* Draw wrap indicator */
                if (app->cfg && app->cfg->wrap_indicator && has_more_subrows && x_screen_end > 0)
                {
                    int free_cols = x_screen_end - x_text_end;

                    if (free_cols >= 1)
                    {
                        attron(COLOR_PAIR(COL_GUIDE));

                        if (free_cols >= 2)
                        {
                            int wx = x_screen_end - 2;
                            wchar_t wrap_mark[2];
#ifdef PLATFORM_AMIGA
                            wrap_mark[0] = app->cfg->ttf_enabled ? L'\x21B5' : L'<';
#else
                            wrap_mark[0] = L'\x21B5';
#endif
                            wrap_mark[1] = L'\0';

                            mvaddnwstr(start_row + sr, wx, wrap_mark, 1);
                            mvaddch(start_row + sr, x_screen_end - 1, ' ');
                        }
                        else
                        {
                            mvaddch(start_row + sr, x_screen_end - 1, '<');
                        }

                        attroff(COLOR_PAIR(COL_GUIDE));
                    }
                }

                /* Indent guides (start of logical line only) */
                if (app->cfg && app->cfg->indent_guides && s == sub_skip && seg_len > 0 && tab_width > 1)
                {
                    int k;
                    int leading_count = 0;

                    for (k = seg_start; k < seg_end && k < len; k++)
                    {
                        if (l[k] != L' ')
                            break;
                        leading_count++;
                    }

                    if (leading_count > 0)
                    {
                        int g;

                        attron(COLOR_PAIR(COL_GUIDE));

                        for (g = tab_width; g <= leading_count; g += tab_width)
                        {
                            int gx = ln_offset + g - 1;

                            if (gx >= ln_offset && gx < width)
                                mvaddch(start_row + sr, gx, '|');
                        }

                        attroff(COLOR_PAIR(COL_GUIDE));
                    }
                }

                sr++;
            }

            s++;

            if (np >= len)
                break;

            if (np <= pos)
                np = pos + (width < 1 ? 1 : width);

            pos = np;
        }

        li++;
        sub_skip = 0; /* subsequent lines start at sub-row 0 */
    }

#ifdef HAVE_GRAMMAR
    /* Prewarm grammar cache above/below viewport using info.top/rows computed for this function */
    if (app->grammar_active && app->grammar_handle)
        ui_grammar_prewarm(app, info.top, rows, info.line_count);
#endif
}
