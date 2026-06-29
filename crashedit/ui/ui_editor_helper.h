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

#ifndef UI_EDITOR_HELPER_H
#define UI_EDITOR_HELPER_H

#include "ui.h"

/* Effective wrap column. Clamp AUTOWRAP to COLS-1; 0=disabled */
int editor_eff_wrap(const UiApp *app);

/* Trigger auto hard-wrap after an editing action */
void ed_auto_rewrap_after_edit(UiApp *app);

/* Manual hard-wrap rewrap for the current paragraph (Ctrl+W fallback) */
int ed_manual_rewrap_paragraph(UiApp *app, int width);

/* Left margin for editor body with line numbers */
int editor_body_offset(const UiApp *app, int line_count);

/* Detect loaded wrap-hyphens using the active spell checker */
void ui_editor_detect_wrap_hyphens(UiApp *app);

/* Visual width in display columns of n wchars starting at s, wide chars count 2, narrow 1, used by ui_editor.c for positioning */
int wcs_vwidth(const wchar_t *s, int n);

/* Visual width with tab-stop support */
int wcs_vwidth_ex(const wchar_t *s, int n, int start_col, int tab_width);

/* Draw wide string with tab expansion */
void ui_draw_wcs_line_with_tabs(int y, int x, const wchar_t *s, int n, int tab_width);

#endif /* UI_EDITOR_HELPER_H */
