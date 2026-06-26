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

#define _XOPEN_SOURCE
#include <wchar.h>
#include "ui_editor_softwrap.h"
#include "ui.h"
#include "ui_internal.h"
#include "ui_editor_helper.h"
#include "../components/editor.h"
#include "../core/utf8.h"

/* Soft-wrap viewport state: no prefix-sum cache, viewport anchored by (s_soft_top_line, s_soft_top_sub), cursor/page operations walk O(distance) lines */
int s_soft_top_line = 0;
int s_soft_top_sub = 0;
int s_soft_desired_vcol = -1;
int s_soft_last_width = 80;

int s_soft_vtop = 0;
int s_tab_width = 4; /* visual tab stop width, copied from config */

void soft_reset_desired(void)
{
    s_soft_desired_vcol = -1;
}

/* Soft-wrap: returns end of current visual segment (exclusive), breaks at last space boundary, hard-cuts if word longer than width */
/*int wrap_next(const wchar_t *line, int len, int width, int start)
{
   int hard_end;
   int k;

   if (width < 1)
       width = 1;

   hard_end = start + width;

   if (hard_end >= len)
       return len;*/

/* Search backwards from hard_end for a space to break at */
/*for (k = hard_end; k > start; k--)
{
    if (line[k - 1] == L' ' || line[k - 1] == L'\t')
        return k;
}*/

/* No space found: hard cut */
/*return hard_end;
}*/

/* Added wide character support, hard-cuts at visual boundary if word longer than width */
int wrap_next(const wchar_t *line, int len, int width, int start)
{
    int vcol = 0;
    int col = 0;
    int k = start;
    int hard_end;
    int tab_width = s_tab_width > 0 ? s_tab_width : 4;

    if (width < 1)
        width = 1;

    /* col = absolute visual column; vcol = width within this segment */
    if (start > 0 && start <= len)
        col = wcs_vwidth_ex(line, start, 0, tab_width);

    /* Walk forward from start, accumulating visual width with wcswidth */
    while (k < len)
    {
        int w = 1;

        if (line[k] == L'\t')
            w = tab_width - (col % tab_width);
        else
        {
            w = wcswidth(&line[k], 1);
            if (w <= 0)
                w = 1; /* control/zero-width -> 1 */
        }

        if (vcol + w > width)
            break;

        vcol += w;
        col += w;
        k++;
    }

    hard_end = k;

    if (k >= len)
        return len;

    /* Search backwards from hard_end for a space to break at */
    for (k = hard_end; k > start; k--)
    {
        if (line[k - 1] == L' ' || line[k - 1] == L'\t')
            return k;
    }

    /* No space found: hard cut at visual boundary */
    return hard_end;
}

/* Number of visual sub-rows a logical line occupies (>= 1) */
int wrap_count(const wchar_t *line, int len, int width)
{
    int pos = 0;
    int rows = 1;

    if (len <= 0)
        return 1;

    for (;;)
    {
        int end = wrap_next(line, len, width, pos);

        if (end >= len)
            break;

        if (end <= pos)
            end = pos + (width < 1 ? 1 : width); /* never stall */

        pos = end;
        rows++;
    }

    return rows;
}

/* Sub-row geometry within a single logical line, return [seg_start, seg_end) wchar range for target_sub sub-row, if target_sub beyond line's sub-rows returns last sub-row, returns sub-row index actually scanned */
int line_subrow_range(const wchar_t *l, int len, int width, int target_sub, int *seg_start, int *seg_end)
{
    int pos = 0;
    int sub = 0;

    if (!l || len <= 0)
    {
        *seg_start = 0;
        *seg_end = 0;

        return 0;
    }

    for (;;)
    {
        int end = wrap_next(l, len, width, pos);

        if (sub == target_sub || end >= len)
        {
            *seg_start = pos;
            *seg_end = end;

            return sub;
        }

        if (end <= pos)
            end = pos + (width < 1 ? 1 : width);

        pos = end;
        sub++;
    }
}

/* Returns the sub-row index inside line where the column col lives */
int line_subrow_of_col(const wchar_t *l, int len, int width, int col)
{
    int pos = 0;
    int sub = 0;

    if (!l || len <= 0)
        return 0;

    for (;;)
    {
        int end = wrap_next(l, len, width, pos);

        if (col < end || end >= len)
            return sub;

        if (end <= pos)
            end = pos + (width < 1 ? 1 : width);

        pos = end;
        sub++;
    }
}

int soft_seg_at(const wchar_t *l, int len, int width, int vrow_in_line, int target_vcol, int *out_col)
{
    int seg_start = 0, seg_end = 0;
    int last_sub;
    int v;
    int seg_len;

    if (!l || len <= 0)
    {
        if (vrow_in_line == 0)
        {
            *out_col = 0;
            return 0;
        }

        return -1;
    }

    last_sub = line_subrow_range(l, len, width, vrow_in_line, &seg_start, &seg_end);

    if (last_sub < vrow_in_line)
        return -1; /* requested vrow past last sub-row */

    seg_len = seg_end - seg_start;

    if (seg_len < 0)
        seg_len = 0;

    v = target_vcol;

    if (v < 0)
        v = 0;

    /* End on last segment: insertion point (seg_len), interior segment: stay on last visible char so cursor doesn't jump to next sub-row */
    if (v >= seg_len)
    {
        if (seg_end >= len)
            v = seg_len;
        else
            v = (seg_len > 0) ? seg_len - 1 : 0;
    }

    *out_col = seg_start + v;

    return 0;
}

/* Walk N visual rows down/up from (from_line, from_sub), O(|delta|) */
static void walk_vrows_forward(Ed *ed, int width, int from_line, int from_sub, int delta, int *out_line, int *out_sub)
{
    EdInfo info;
    int line = from_line;
    int sub = from_sub;
    int remaining = delta;

    ed_get_info(ed, &info);

    while (remaining > 0 && line < info.line_count)
    {
        const wchar_t *l = ed_line_wcs(ed, line);
        int len = ed_line_len(ed, line);
        int n = wrap_count(l ? l : L"", l ? len : 0, width);
        int avail = n - sub - 1;

        if (remaining <= avail)
        {
            sub += remaining;
            remaining = 0;
            break;
        }

        remaining -= (avail + 1);
        line++;
        sub = 0;
    }

    if (line >= info.line_count)
    {
        if (info.line_count > 0)
        {
            const wchar_t *l;
            int len, n;

            line = info.line_count - 1;
            l = ed_line_wcs(ed, line);
            len = ed_line_len(ed, line);
            n = wrap_count(l ? l : L"", l ? len : 0, width);
            sub = n - 1;

            if (sub < 0)
                sub = 0;
        }
        else
        {
            line = 0;
            sub = 0;
        }
    }

    *out_line = line;
    *out_sub = sub;
}

static void walk_vrows_backward(Ed *ed, int width, int from_line, int from_sub, int delta, int *out_line, int *out_sub)
{
    int line = from_line;
    int sub = from_sub;
    int remaining = delta;

    while (remaining > 0)
    {
        if (sub > 0)
        {
            int take = sub;

            if (take > remaining)
                take = remaining;

            sub -= take;
            remaining -= take;

            if (remaining == 0)
                break;
        }

        if (line == 0)
        {
            sub = 0;
            break;
        }

        line--;
        {
            const wchar_t *l = ed_line_wcs(ed, line);
            int len = ed_line_len(ed, line);
            int n = wrap_count(l ? l : L"", l ? len : 0, width);

            sub = n - 1;
        }

        remaining--;

        if (remaining == 0)
            break;
    }

    if (line < 0)
    {
        line = 0;
        sub = 0;
    }

    *out_line = line;
    *out_sub = sub;
}

/* Cursor position vs. viewport, visual column of cursor within its sub-row (in display cells) */
int soft_cursor_vcol(Ed *ed, int width)
{
    EdInfo info;
    const wchar_t *l;
    int len;
    int sub;
    int seg_start = 0, seg_end = 0;
    int n;

    ed_get_info(ed, &info);

    l = ed_line_wcs(ed, info.row);
    len = ed_line_len(ed, info.row);

    if (!l || len <= 0)
        return 0;

    sub = line_subrow_of_col(l, len, width, info.col);
    line_subrow_range(l, len, width, sub, &seg_start, &seg_end);

    n = info.col - seg_start;

    if (n < 0)
        n = 0;

    if (n > len - seg_start)
        n = len - seg_start;

    /* start_col must be 0 for soft-wrap sub-row consistency */
    return wcs_vwidth_ex(&l[seg_start], n, 0, s_tab_width);
}

/* Number of visual rows between (a_line, a_sub) and (b_line, b_sub), returns positive if b is after a, negative if before */
static int soft_vrows_between(Ed *ed, int width, int a_line, int a_sub, int b_line, int b_sub)
{
    int i;
    int delta = 0;
    const wchar_t *l;
    int len;
    int n;

    if (a_line == b_line)
        return b_sub - a_sub;

    if (a_line < b_line)
    {
        const wchar_t *l = ed_line_wcs(ed, a_line);
        int len = ed_line_len(ed, a_line);
        int n = wrap_count(l ? l : L"", l ? len : 0, width);

        delta = (n - a_sub);

        for (i = a_line + 1; i < b_line; i++)
        {
            l = ed_line_wcs(ed, i);
            len = ed_line_len(ed, i);
            delta += wrap_count(l ? l : L"", l ? len : 0, width);
        }

        delta += b_sub;

        return delta;
    }

    l = ed_line_wcs(ed, b_line);
    len = ed_line_len(ed, b_line);
    n = wrap_count(l ? l : L"", l ? len : 0, width);

    delta = (n - b_sub);

    for (i = b_line + 1; i < a_line; i++)
    {
        l = ed_line_wcs(ed, i);
        len = ed_line_len(ed, i);
        delta += wrap_count(l ? l : L"", l ? len : 0, width);
    }

    delta += a_sub;

    return -delta;
}

/* Compute cursor screen row given current viewport, returns negative if cursor is above viewport, >= body_rows if below */
int soft_cursor_screen_row(UiApp *app, int width)
{
    EdInfo info;
    Ed *ed = app->editor;
    int sub_cursor;
    const wchar_t *l;
    int len;

    ed_get_info(ed, &info);

    l = ed_line_wcs(ed, info.row);
    len = ed_line_len(ed, info.row);
    sub_cursor = line_subrow_of_col(l ? l : L"", l ? len : 0, width, info.col);

    return soft_vrows_between(ed, width, s_soft_top_line, s_soft_top_sub, info.row, sub_cursor);
}

/* Visual row/col of cursor, kept for backward compatibility with old draw code, vrow returned is RELATIVE to viewport top (0 means cursor is on first visible row) */
void soft_cursor_vpos(UiApp *app, int width, int *out_vrow, int *out_vcol)
{
    if (out_vrow)
        *out_vrow = soft_cursor_screen_row(app, width);

    if (out_vcol)
        *out_vcol = soft_cursor_vcol(app->editor, width);
}

/* Stub: pre-rewrite callers wanted total document vrows, not needed in new design, return coarse upper bound for defensive code */
int soft_count_rows_before(Ed *ed, int upto, int width)
{
    EdInfo info;

    ed_get_info(ed, &info);

    if (upto <= 0)
        return 0;

    if (upto > info.line_count)
        upto = info.line_count;

    /* Approximate: assume 1 vrow per line, callers in rewritten code don't rely on exact value */
    return upto;
}

/* Adjust viewport so the cursor lies inside [0, body_rows) */
static void soft_ensure_visible(UiApp *app, int width, int body_rows)
{
    EdInfo info;
    Ed *ed = app->editor;
    int sub_cursor;
    int screen_row;
    const wchar_t *l;
    int len;
    const wchar_t *tl;
    int tlen;
    int tn;

    ed_get_info(ed, &info);

    if (info.line_count <= 0)
    {
        s_soft_top_line = 0;
        s_soft_top_sub = 0;

        return;
    }

    /* Clamp top to valid range */
    if (s_soft_top_line < 0)
        s_soft_top_line = 0;

    if (s_soft_top_line >= info.line_count)
        s_soft_top_line = info.line_count - 1;

    tl = ed_line_wcs(ed, s_soft_top_line);
    tlen = ed_line_len(ed, s_soft_top_line);
    tn = wrap_count(tl ? tl : L"", tl ? tlen : 0, width);

    if (s_soft_top_sub < 0)
        s_soft_top_sub = 0;

    if (s_soft_top_sub >= tn)
        s_soft_top_sub = tn - 1;

    l = ed_line_wcs(ed, info.row);
    len = ed_line_len(ed, info.row);
    sub_cursor = line_subrow_of_col(l ? l : L"", l ? len : 0, width, info.col);

    screen_row = soft_vrows_between(ed, width, s_soft_top_line, s_soft_top_sub, info.row, sub_cursor);

    if (screen_row < 0)
    {
        s_soft_top_line = info.row;
        s_soft_top_sub = sub_cursor;
    }
    else if (screen_row >= body_rows)
    {
        int over = screen_row - (body_rows - 1);
        int new_line, new_sub;

        walk_vrows_forward(ed, width, s_soft_top_line, s_soft_top_sub, over, &new_line, &new_sub);

        s_soft_top_line = new_line;
        s_soft_top_sub = new_sub;
    }
}

/* Public entry so draw.c can call ensure_visible */
void soft_ensure_visible_for_draw(UiApp *app, int width, int body_rows)
{
    soft_ensure_visible(app, width, body_rows);
}

/* Cursor movement */
static void soft_set_cursor_at(Ed *ed, int width, int line, int sub, int desired_vcol)
{
    const wchar_t *l;
    int len;
    int seg_start = 0, seg_end = 0;
    int v, i, col;

    l = ed_line_wcs(ed, line);
    len = ed_line_len(ed, line);

    line_subrow_range(l ? l : L"", l ? len : 0, width, sub, &seg_start, &seg_end);

    if (desired_vcol < 0)
        desired_vcol = 0;

    v = 0;
    col = seg_start;

    if (l && len > 0)
    {
        for (i = seg_start; i < seg_end; i++)
        {
            int w = wcs_vwidth_ex(&l[i], 1, v, s_tab_width);

            if (v + w > desired_vcol)
                break;

            v += w;
            col = i + 1;
        }
    }

    if (seg_end < len && col >= seg_end)
        col = seg_end > seg_start ? seg_end - 1 : seg_start;

    if (col < 0)
        col = 0;

    if (col > len)
        col = len;

    ed_set_pos(ed, line, col);
}

static void soft_move_vrows(UiApp *app, int width, int delta)
{
    EdInfo info;
    Ed *ed = app->editor;
    const wchar_t *l;
    int len;
    int sub_cursor;
    int new_line, new_sub;
    int vcol;

    if (delta == 0)
        return;

    ed_get_info(ed, &info);

    if (info.line_count <= 0)
        return;

    l = ed_line_wcs(ed, info.row);
    len = ed_line_len(ed, info.row);
    sub_cursor = line_subrow_of_col(l ? l : L"", l ? len : 0, width, info.col);

    if (s_soft_desired_vcol < 0)
        s_soft_desired_vcol = soft_cursor_vcol(ed, width);

    if (delta > 0)
        walk_vrows_forward(ed, width, info.row, sub_cursor, delta, &new_line, &new_sub);
    else
        walk_vrows_backward(ed, width, info.row, sub_cursor, -delta, &new_line, &new_sub);

    vcol = s_soft_desired_vcol;
    soft_set_cursor_at(ed, width, new_line, new_sub, vcol);
}

/* Public movement handlers */
void soft_move_up_visual(UiApp *app, int width)
{
    soft_move_vrows(app, width, -1);
}

void soft_move_down_visual(UiApp *app, int width)
{
    soft_move_vrows(app, width, +1);
}

void soft_move_home_visual(UiApp *app, int width)
{
    EdInfo info;
    Ed *ed = app->editor;
    const wchar_t *l;
    int len;
    int sub_cursor;
    int seg_start = 0, seg_end = 0;

    ed_get_info(ed, &info);

    l = ed_line_wcs(ed, info.row);
    len = ed_line_len(ed, info.row);
    sub_cursor = line_subrow_of_col(l ? l : L"", l ? len : 0, width, info.col);

    line_subrow_range(l ? l : L"", l ? len : 0, width, sub_cursor, &seg_start, &seg_end);

    ed_set_pos(ed, info.row, seg_start);
    soft_reset_desired();
}

void soft_move_end_visual(UiApp *app, int width)
{
    EdInfo info;
    Ed *ed = app->editor;
    const wchar_t *l;
    int len;
    int sub_cursor;
    int seg_start = 0, seg_end = 0;
    int target_col;

    ed_get_info(ed, &info);

    l = ed_line_wcs(ed, info.row);
    len = ed_line_len(ed, info.row);
    sub_cursor = line_subrow_of_col(l ? l : L"", l ? len : 0, width, info.col);

    line_subrow_range(l ? l : L"", l ? len : 0, width, sub_cursor, &seg_start, &seg_end);

    if (seg_end >= len)
        target_col = len;
    else
        target_col = seg_end > seg_start ? seg_end - 1 : seg_start;

    ed_set_pos(ed, info.row, target_col);
    soft_reset_desired();
}

void soft_move_pgup_visual(UiApp *app, int width, int pg)
{
    int new_line, new_sub;
    Ed *ed = app->editor;

    if (pg <= 0)
        pg = 1;

    soft_move_vrows(app, width, -pg);

    walk_vrows_backward(ed, width, s_soft_top_line, s_soft_top_sub, pg, &new_line, &new_sub);

    s_soft_top_line = new_line;
    s_soft_top_sub = new_sub;
}

void soft_move_pgdn_visual(UiApp *app, int width, int pg)
{
    int new_line, new_sub;
    Ed *ed = app->editor;

    if (pg <= 0)
        pg = 1;

    soft_move_vrows(app, width, +pg);

    walk_vrows_forward(ed, width, s_soft_top_line, s_soft_top_sub, pg, &new_line, &new_sub);

    s_soft_top_line = new_line;
    s_soft_top_sub = new_sub;
}

/* Legacy: kept for compatibility, pre-rewrite this was the convert function */
void soft_visual_to_logical(Ed *ed, int width, int target_vrow, int target_vcol, int *out_row, int *out_col)
{
    EdInfo info;
    int new_line, new_sub;
    int seg_start = 0, seg_end = 0;
    const wchar_t *l;
    int len;
    int v;
    int seg_len;

    ed_get_info(ed, &info);

    if (info.line_count <= 0)
    {
        *out_row = 0;
        *out_col = 0;
        return;
    }

    /* target_vrow is interpreted as ABSOLUTE distance from line 0 in legacy contract, walk forward from line 0, this is O(target_vrow) but only used by external code paths that don't matter in new design */
    if (target_vrow > 0)
        walk_vrows_forward(ed, width, 0, 0, target_vrow, &new_line, &new_sub);
    else
    {
        new_line = 0;
        new_sub = 0;
    }

    /* Compute column from desired vcol */
    l = ed_line_wcs(ed, new_line);
    len = ed_line_len(ed, new_line);

    line_subrow_range(l ? l : L"", l ? len : 0, width, new_sub, &seg_start, &seg_end);

    v = target_vcol < 0 ? 0 : target_vcol;
    seg_len = seg_end - seg_start;

    if (seg_len < 0)
        seg_len = 0;

    if (v > seg_len)
        v = seg_len;

    *out_row = new_line;
    *out_col = seg_start + v;
}

/* Convert screen position to logical buffer position (line, col) for mouse support */
int ui_editor_screen_to_logical(UiApp *app, int width, int screen_y, int screen_x, int *out_line, int *out_col, int body_rows)
{
    Ed *ed = NULL;
    EdInfo info;
    int line;
    int sub;
    int remaining;
    int last;
    int ll;
    int soft;
    const wchar_t *l = NULL;
    int len;
    int j;
    int acc_w;
    int n_sub;
    int sub_left_in_line;
    int target_sub;
    int seg_start;
    int seg_end;
    int cw;

    if (!app || !out_line || !out_col)
        return -1;

    ed = app->editor;

    if (!ed)
        return -1;

    ed_get_info(ed, &info);

    if (info.line_count <= 0)
    {
        *out_line = 0;
        *out_col = 0;
        return 0;
    }

    if (width < 1)
        width = 1;

    if (screen_y < 0)
        screen_y = 0;

    if (screen_x < 0)
        screen_x = 0;

    /* In soft-wrap mode, ensure viewport is correct for current body size */
    soft = !(app->cfg && app->cfg->hard_wrap);

    if (soft && body_rows > 0)
        soft_ensure_visible_for_draw(app, width, body_rows);

    /* In hard-wrap mode, use info.top directly (1 logical line = 1 screen row) */
    soft = !(app->cfg && app->cfg->hard_wrap);

    if (!soft)
    {
        line = info.top + screen_y;
        sub = 0;
        remaining = 0;

        if (line >= info.line_count)
        {
            line = info.line_count - 1;
            ll = ed_line_len(ed, line);
            *out_line = line;
            *out_col = ll;
            return 0;
        }

        if (line < 0)
        {
            line = 0;
        }

        /* Calculate column from screen_x (walk char by char, summing visual width) */
        l = ed_line_wcs(ed, line);
        len = ed_line_len(ed, line);
        acc_w = 0;

        for (j = 0; j < len; j++)
        {
            cw = wcs_vwidth_ex(&l[j], 1, acc_w, s_tab_width);

            if (acc_w + cw > screen_x)
                break;

            acc_w += cw;
        }

        *out_line = line;
        *out_col = j;
        return 0;
    }

    /* Soft-wrap: walk forward through logical lines, consuming sub-rows */
    line = s_soft_top_line;
    sub = s_soft_top_sub;
    remaining = screen_y;

    while (line < info.line_count)
    {
        l = ed_line_wcs(ed, line);
        ll = ed_line_len(ed, line);
        n_sub = wrap_count(l ? l : L"", l ? ll : 0, width);
        sub_left_in_line = n_sub - sub;

        if (remaining < sub_left_in_line)
        {
            /* Target sub-row is in this logical line at sub + remaining */
            target_sub = sub + remaining;
            seg_start = 0;
            seg_end = 0;
            j = 0;
            acc_w = 0;

            line_subrow_range(l ? l : L"", l ? ll : 0, width, target_sub, &seg_start, &seg_end);

            /* Walk char by char, summing visual width until we exceed screen_x */
            for (j = seg_start; j < seg_end; j++)
            {
                cw = wcs_vwidth_ex(&l[j], 1, acc_w, s_tab_width);

                if (acc_w + cw > screen_x)
                    break;

                acc_w += cw;
            }

            *out_line = line;
            *out_col = j; /* may equal seg_end (click past end of segment) */
            return 0;
        }

        remaining -= sub_left_in_line;
        line++;
        sub = 0;
    }

    /* y is past last line -> clamp to end of document */
    last = info.line_count - 1;

    ll = ed_line_len(ed, last);

    *out_line = last;
    *out_col = ll;

    return 0;
}
