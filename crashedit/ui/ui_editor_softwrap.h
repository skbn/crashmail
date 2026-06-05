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

#ifndef UI_EDITOR_SOFTWRAP_H
#define UI_EDITOR_SOFTWRAP_H

#include "../components/editor.h"
#include "ui.h"

/* Soft-wrap viewport state (reset on editor entry, only for soft-wrap mode) */
extern int s_soft_vtop;
extern int s_soft_desired_vcol;
extern int s_soft_last_width;

/* Reset desired column on horizontal moves */
void soft_reset_desired(void);

/* Soft-wrap: returns the end of the current visual segment (exclusive) */
int wrap_next(const wchar_t *line, int len, int width, int start);

/* Number of visual sub-rows a logical line occupies (>= 1) */
int wrap_count(const wchar_t *line, int len, int width);

/* Visual row/col of cursor within soft-wrapped text */
void soft_cursor_vpos(UiApp *app, int width, int *out_vrow, int *out_vcol);

/* Total visual rows occupied by logical lines [0..upto) at the given width */
int soft_count_rows_before(Ed *ed, int upto, int width);

/* Within one logical line, find the logical column that corresponds to
 * (vrow_in_line, target_vcol) */
int soft_seg_at(const wchar_t *l, int len, int width, int vrow_in_line, int target_vcol, int *out_col);

/* Convert an absolute visual position to (row, col) for ed_set_pos */
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
