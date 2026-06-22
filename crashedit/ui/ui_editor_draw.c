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
        AreaEntry *ae;
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
            ln_offset = lineno_width(info.line_count);

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
            cy = start_row + (info.row - info.top);
            cx = ln_offset + info.col;
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
    AreaEntry *ae;
    int rows, start_row, i;
    int b_r1 = -1, b_c1 = 0, b_r2 = -1, b_c2 = 0;
    int soft;
    int width;
    int li, sr;
    int ln_width = 0;  /* line number width */
    int ln_offset = 0; /* offset for editor content */
    int show_lnum = app->cfg && app->cfg->show_line_numbers;
    int sub_skip;

    ae = &app->areas->entries[app->sess.area_idx];
    start_row = (ae->type == AREATYPE_NETMAIL) ? 8 : 7;
    rows = LINES - start_row - 1;

    /* Reserve space for spell panel at bottom when active */
    if (app->show_spell)
        rows -= SPELL_PANEL_H;

    if (rows < 1)
        rows = 1;

    soft = !(app->cfg && app->cfg->hard_wrap);
    width = COLS; /* visual wrap fits the terminal */

    if (width < 1)
        width = 1;

    ed_set_page(app->editor, rows);
    ed_get_info(app->editor, &info);

    /* Calculate line number width if enabled */
    if (show_lnum)
    {
        ln_width = lineno_width(info.line_count);
        ln_offset = ln_width;
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

            move(start_row + i, 0);
            clrtoeol();

            if (line_idx >= info.line_count)
                continue;

            /* Draw line number if enabled */
            if (show_lnum)
            {
                attron(COLOR_PAIR(COL_BORDER));
                mvprintw(start_row + i, 0, "%*d", ln_width - 1, line_idx + 1);
                attroff(COLOR_PAIR(COL_BORDER));
            }

            /* mvaddnwstr: n is in wide chars, no UTF-8 conversion needed */
            wl = ed_line_wcs(app->editor, line_idx);
            line_len = ed_line_len(app->editor, line_idx);

            if (wl && line_len > 0)
                mvaddnwstr(start_row + i, ln_offset, wl, line_len);

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
                            mvaddnwstr(start_row + i, ln_offset + wcs_vwidth(wl, match_col), &wl[match_col], match_len);
                            attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                        }
                    }
                }
            }

#ifdef HAVE_HUNSPELL
            /* Highlight misspelled words */
            if (app->spell_active && app->spell_handle)
            {
                int word_start = 0;
                int word_end;

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

                        /* Ignore single-character words like word processors do */
                        if (word_len > 1 && ui_spell_check_word_simple(app, &wl[word_start], word_len))
                        {
                            attron(COLOR_PAIR(COL_SPELL_CURRENT));
                            mvaddnwstr(start_row + i, ln_offset + wcs_vwidth(wl, word_start), &wl[word_start], word_len);
                            attroff(COLOR_PAIR(COL_SPELL_CURRENT));
                        }
                    }

                    word_start = word_end;
                }
            }
#endif

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
                    mvaddnwstr(start_row + i, ln_offset + wcs_vwidth(wcs, hs), &wcs[hs], he - hs);
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
        }

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
                    attron(COLOR_PAIR(COL_BORDER));
                    mvprintw(start_row + sr, 0, "%*d", ln_width - 1, li + 1);
                    attroff(COLOR_PAIR(COL_BORDER));
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

                if (seg_len < 0)
                    seg_len = 0;

                /* Line number on the first painted sub-row of this line */
                if (show_lnum && first_seg)
                {
                    attron(COLOR_PAIR(COL_BORDER));
                    mvprintw(start_row + sr, 0, "%*d", ln_width - 1, li + 1);
                    attroff(COLOR_PAIR(COL_BORDER));
                    first_seg = 0;
                }

                if (seg_len > 0)
                    mvaddnwstr(start_row + sr, ln_offset, &l[seg_start], seg_len);

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
                                mvaddnwstr(start_row + sr, ln_offset + wcs_vwidth(&l[seg_start], match_col - seg_start), &l[match_col], match_len);
                                attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                            }
                            else if (match_col >= seg_start && match_col < seg_end)
                            {
                                int partial_len = seg_end - match_col;
                                attron(COLOR_PAIR(COL_SEARCH_MATCH));
                                mvaddnwstr(start_row + sr, ln_offset + wcs_vwidth(&l[seg_start], match_col - seg_start), &l[match_col], partial_len);
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
                /* Highlight misspelled words */
                if (app->spell_active && app->spell_handle)
                {
                    int word_start = seg_start;
                    int word_end;

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

                            /* Ignore single-character words like word processors do */
                            if (word_len > 1 && ui_spell_check_word_simple(app, &l[word_start], word_len))
                            {
                                attron(COLOR_PAIR(COL_SPELL_CURRENT));
                                mvaddnwstr(start_row + sr, ln_offset + wcs_vwidth(&l[seg_start], word_start - seg_start), &l[word_start], word_len);
                                attroff(COLOR_PAIR(COL_SPELL_CURRENT));
                            }
                        }

                        word_start = word_end;
                    }
                }
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
                        mvaddnwstr(start_row + sr, ln_offset + wcs_vwidth(&l[seg_start], hs - seg_start), &l[hs], he - hs);
                        attroff(A_REVERSE);
                    }
                    else if (hs == seg_start && he == seg_start)
                    {
                        attron(A_REVERSE);
                        mvaddch(start_row + sr, ln_offset, ' ');
                        attroff(A_REVERSE);
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
}
