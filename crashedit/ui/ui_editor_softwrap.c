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

#include "ui_editor_softwrap.h"
#include "ui.h"
#include "ui_internal.h"
#include "../components/editor.h"

/* Soft-wrap viewport state (reset on editor entry, only for soft-wrap mode) */
int s_soft_vtop = 0;
int s_soft_desired_vcol = -1;
int s_soft_last_width = 80;

/* Reset desired column on horizontal moves */
void soft_reset_desired(void)
{
    s_soft_desired_vcol = -1;
}

/* Soft-wrap: returns the end of the current visual segment (exclusive)
 * Breaks at the last space boundary that fits within width columns
 * If no space fits (word longer than width), hard-cuts at start+width
 * The next segment starts exactly at the returned position - no chars skipped */
int wrap_next(const wchar_t *line, int len, int width, int start)
{
    int hard_end;
    int k;

    if (width < 1)
        width = 1;

    hard_end = start + width;

    if (hard_end >= len)
        return len;

    /* Search backwards from hard_end for a space to break at */
    for (k = hard_end; k > start; k--)
    {
        if (line[k - 1] == L' ' || line[k - 1] == L'\t')
            return k;
    }

    /* No space found: hard cut */
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

/* Visual row/col of cursor within soft-wrapped text */
void soft_cursor_vpos(UiApp *app, int width, int *out_vrow, int *out_vcol)
{
    EdInfo info;
    int li, vrow = 0;
    const wchar_t *l;
    int len;
    int pos;
    int col;

    ed_get_info(app->editor, &info);

    for (li = 0; li < info.row && li < info.line_count; li++)
    {
        const wchar_t *l = ed_line_wcs(app->editor, li);
        int len = ed_line_len(app->editor, li);

        vrow += wrap_count(l ? l : L"", l ? len : 0, width);
    }

    l = ed_line_wcs(app->editor, info.row);
    len = ed_line_len(app->editor, info.row);
    pos = 0;
    col = info.col;

    if (l && len > 0)
    {
        for (;;)
        {
            int end = wrap_next(l, len, width, pos);

            /* Cursor is on this sub-row if it sits before the next
             * sub-row's start, or this is the last sub-row */
            if (end >= len || col < end)
            {
                if (out_vcol)
                {
                    int vc = 0;
                    int i;

                    /* Calculate visual column width using wcswidth for multi-byte UTF-8 */
                    for (i = pos; i < col && i < len; i++)
                    {
                        int w = wcswidth(&l[i], 1);

                        if (w < 0)
                            w = 1; /* Fallback for unprintable chars */

                        vc += w;
                    }

                    if (vc < 0)
                        vc = 0;

                    *out_vcol = vc;
                }

                break;
            }

            if (end <= pos)
                end = pos + (width < 1 ? 1 : width);

            pos = end;
            vrow++;
        }
    }
    else if (out_vcol)
    {
        *out_vcol = 0;
    }

    if (out_vrow)
        *out_vrow = vrow;
}

/* Total visual rows occupied by logical lines [0..upto) at the given width */
int soft_count_rows_before(Ed *ed, int upto, int width)
{
    int total = 0;
    int li;

    for (li = 0; li < upto; li++)
    {
        const wchar_t *l = ed_line_wcs(ed, li);
        int len = ed_line_len(ed, li);

        total += wrap_count(l ? l : L"", l ? len : 0, width);
    }

    return total;
}

/* Within one logical line, find the logical column that corresponds to
 * (vrow_in_line, target_vcol). vrow_in_line is the sub-row offset within
 * this line (0 = first sub-row)
 *
 * Returns 0 on success, -1 if vrow_in_line is beyond this line's sub-rows
 * If target_vcol overruns the sub-row's content, the result is clamped to
 * the end of that sub-row's segment - matching what the user sees */
int soft_seg_at(const wchar_t *l, int len, int width, int vrow_in_line, int target_vcol, int *out_col)
{
    int pos = 0;
    int sub = 0;

    if (!l || len <= 0)
    {
        if (vrow_in_line == 0)
        {
            *out_col = 0;
            return 0;
        }

        return -1;
    }

    for (;;)
    {
        int seg_end = wrap_next(l, len, width, pos);
        int seg_len = seg_end - pos;
        int is_last = (seg_end >= len);
        int v;

        if (sub == vrow_in_line)
        {
            v = target_vcol;
            if (v < 0)
                v = 0;

            /* End on last segment: go to insertion point (seg_len)
             * End on interior segment: stay on last visible char (seg_len-1)
             * so cursor doesn't jump to next row */
            if (v >= seg_len)
            {
                if (is_last)
                    v = seg_len;
                else
                    v = (seg_len > 0) ? seg_len - 1 : 0;
            }

            *out_col = pos + v;

            return 0;
        }

        sub++;

        if (seg_end >= len)
            return -1; /* requested vrow past last sub-row */

        if (seg_end <= pos)
            seg_end = pos + (width < 1 ? 1 : width);

        pos = seg_end;
    }
}

/* Convert an absolute visual position (target_vrow, target_vcol) - across
 * all logical lines - to (row, col) for ed_set_pos
 *
 * Out-of-range vrow clamps to the last visual row of the document
 * Out-of-range vcol clamps to the end of the destination sub-row */
void soft_visual_to_logical(Ed *ed, int width, int target_vrow, int target_vcol, int *out_row, int *out_col)
{
    EdInfo info;
    int li;
    int vacc = 0;

    ed_get_info(ed, &info);

    if (target_vrow < 0)
        target_vrow = 0;

    for (li = 0; li < info.line_count; li++)
    {
        const wchar_t *l = ed_line_wcs(ed, li);
        int len = ed_line_len(ed, li);
        int wc = wrap_count(l ? l : L"", l ? len : 0, width);

        if (target_vrow < vacc + wc)
        {
            int vrow_in_line = target_vrow - vacc;
            int col = 0;

            if (soft_seg_at(l, len, width, vrow_in_line, target_vcol, &col) == 0)
            {
                *out_row = li;
                *out_col = col;

                return;
            }
        }

        vacc += wc;
    }

    /* Past the end: clamp to last line's last sub-row */
    if (info.line_count > 0)
    {
        int last = info.line_count - 1;
        const wchar_t *l = ed_line_wcs(ed, last);
        int len = ed_line_len(ed, last);
        int wc = wrap_count(l ? l : L"", l ? len : 0, width);
        int col = 0;

        if (wc > 0 && soft_seg_at(l, len, width, wc - 1, target_vcol, &col) == 0)
        {
            *out_row = last;
            *out_col = col;

            return;
        }

        *out_row = last;
        *out_col = len;

        return;
    }

    *out_row = 0;
    *out_col = 0;
}

/* Move cursor one visual row up. Preserves desired_vcol so consecutive
 * UPs over shorter rows still land back on the same column on long rows */
void soft_move_up_visual(UiApp *app, int width)
{
    int vr, vc, nr, nc;

    soft_cursor_vpos(app, width, &vr, &vc);

    /* Already on the first visual row: nothing to do (matches ed_move_up) */
    if (vr <= 0)
        return;

    if (s_soft_desired_vcol < 0)
        s_soft_desired_vcol = vc;

    soft_visual_to_logical(app->editor, width, vr - 1, s_soft_desired_vcol, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);

    ed_ensure_visible(app->editor);
}

/* Move cursor one visual row down. Mirror of soft_move_up_visual */
void soft_move_down_visual(UiApp *app, int width)
{
    EdInfo info;
    int total_vrows;
    int vr, vc, nr, nc;

    ed_get_info(app->editor, &info);

    if (info.line_count <= 0)
        return;

    total_vrows = soft_count_rows_before(app->editor, info.line_count, width);
    soft_cursor_vpos(app, width, &vr, &vc);

    /* Already on the last visual row: nothing to do (matches ed_move_down) */
    if (vr >= total_vrows - 1)
        return;

    if (s_soft_desired_vcol < 0)
        s_soft_desired_vcol = vc;

    soft_visual_to_logical(app->editor, width, vr + 1, s_soft_desired_vcol, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);

    ed_ensure_visible(app->editor);
}

/* HOME in soft-wrap: go to column 0 of current visual row */
void soft_move_home_visual(UiApp *app, int width)
{
    int vr, vc, nr, nc;

    soft_cursor_vpos(app, width, &vr, &vc);
    soft_visual_to_logical(app->editor, width, vr, 0, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);

    ed_ensure_visible(app->editor);
}

/* END in soft-wrap: go to last column of current visual row */
void soft_move_end_visual(UiApp *app, int width)
{
    int vr, vc, nr, nc;

    soft_cursor_vpos(app, width, &vr, &vc);

    /* Use large value (not INT_MAX) to clamp to segment end */
    soft_visual_to_logical(app->editor, width, vr, width + 1000000, &nr, &nc);

    ed_set_pos(app->editor, nr, nc);
    ed_ensure_visible(app->editor);
}

/* Move pg visual rows up. Preserves desired_vcol */
void soft_move_pgup_visual(UiApp *app, int width, int pg)
{
    int vr, vc, nr, nc;
    int target;

    if (pg <= 0)
        pg = 1;

    soft_cursor_vpos(app, width, &vr, &vc);

    if (s_soft_desired_vcol < 0)
        s_soft_desired_vcol = vc;

    target = vr - pg;

    if (target < 0)
        target = 0;

    soft_visual_to_logical(app->editor, width, target, s_soft_desired_vcol, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);

    /* Scroll viewport so the cursor stays at roughly the same screen row */
    s_soft_vtop -= pg;

    if (s_soft_vtop < 0)
        s_soft_vtop = 0;

    ed_ensure_visible(app->editor);
}

/* Move pg visual rows down. Mirror of soft_move_pgup_visual */
void soft_move_pgdn_visual(UiApp *app, int width, int pg)
{
    EdInfo info;
    int total_vrows;
    int vr, vc, nr, nc;
    int target;

    if (pg <= 0)
        pg = 1;

    ed_get_info(app->editor, &info);

    if (info.line_count <= 0)
        return;

    total_vrows = soft_count_rows_before(app->editor, info.line_count, width);
    soft_cursor_vpos(app, width, &vr, &vc);

    if (s_soft_desired_vcol < 0)
        s_soft_desired_vcol = vc;

    target = vr + pg;

    if (target > total_vrows - 1)
        target = total_vrows - 1;

    if (target < 0)
        target = 0;

    soft_visual_to_logical(app->editor, width, target, s_soft_desired_vcol, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);

    s_soft_vtop += pg;

    if (s_soft_vtop < 0)
        s_soft_vtop = 0;

    ed_ensure_visible(app->editor);
}
