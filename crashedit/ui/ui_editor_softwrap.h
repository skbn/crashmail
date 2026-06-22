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

#ifndef UI_EDITOR_SOFTWRAP_H
#define UI_EDITOR_SOFTWRAP_H

#include "../components/editor.h"
#include "ui.h"

/* Soft-wrap viewport state anchored by (top_line, top_sub): logical line at top of screen and sub-row first visible */
extern int s_soft_top_line;
extern int s_soft_top_sub;
extern int s_soft_desired_vcol;
extern int s_soft_last_width;

/* Legacy alias kept zero for backward compatibility */
extern int s_soft_vtop;

/* Reset desired column on horizontal moves */
void soft_reset_desired(void);

/* Soft-wrap: returns end of current visual segment (exclusive) */
int wrap_next(const wchar_t *line, int len, int width, int start);

/* Number of visual sub-rows a logical line occupies (>= 1) */
int wrap_count(const wchar_t *line, int len, int width);

/* Returns sub-row index inside line where column col lives */
int line_subrow_of_col(const wchar_t *l, int len, int width, int col);

/* Visual row/col of cursor, vrow is relative to viewport top (0 = first visible row) */
void soft_cursor_vpos(UiApp *app, int width, int *out_vrow, int *out_vcol);

/* Cursor screen row (0-based from viewport top), negative if above viewport, >= body_rows if below */
int soft_cursor_screen_row(UiApp *app, int width);

/* Visual column of cursor within its sub-row */
int soft_cursor_vcol(Ed *ed, int width);

/* Adjust viewport so cursor is inside [0, body_rows), called by draw.c before painting */
void soft_ensure_visible_for_draw(UiApp *app, int width, int body_rows);

/* Legacy stub -- returns approximation, no longer scanned exactly */
int soft_count_rows_before(Ed *ed, int upto, int width);

/* Within one logical line, find logical column corresponding to (vrow_in_line, target_vcol) */
int soft_seg_at(const wchar_t *l, int len, int width, int vrow_in_line, int target_vcol, int *out_col);

/* Sub-row geometry within a single logical line, return [seg_start, seg_end) wchar range for target_sub sub-row */
int line_subrow_range(const wchar_t *l, int len, int width, int target_sub, int *seg_start, int *seg_end);

/* Convert screen position to logical buffer position (line, col) for mouse support */
int ui_editor_screen_to_logical(UiApp *app, int width, int screen_y, int screen_x, int *out_line, int *out_col, int body_rows);

/* Legacy: convert absolute visual position to (row, col), slow path kept for compatibility */
void soft_visual_to_logical(Ed *ed, int width, int target_vrow, int target_vcol, int *out_row, int *out_col);

/* Move cursor one visual row up */
void soft_move_up_visual(UiApp *app, int width);

/* Move cursor one visual row down */
void soft_move_down_visual(UiApp *app, int width);

/* HOME in soft-wrap: go to column 0 of current visual row */
void soft_move_home_visual(UiApp *app, int width);

/* END in soft-wrap: go to last column of current visual row */
void soft_move_end_visual(UiApp *app, int width);

/* Move pg visual rows up */
void soft_move_pgup_visual(UiApp *app, int width, int pg);

/* Move pg visual rows down */
void soft_move_pgdn_visual(UiApp *app, int width, int pg);

#endif /* UI_EDITOR_SOFTWRAP_H */
