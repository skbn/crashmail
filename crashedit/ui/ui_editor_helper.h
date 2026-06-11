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

#ifndef UI_EDITOR_HELPER_H
#define UI_EDITOR_HELPER_H

#include "ui.h"

/* Effective wrap column. Clamp AUTOWRAP to COLS-1; 0=disabled */
int editor_eff_wrap(const UiApp *app);

/* Visual width in display columns of n wchars starting at s. Wide chars
 * (CJK ideographs, wide emoji) count 2, narrow 1, control/non-printable 1
 * Used by ui_editor.c to convert wchar offsets into visual column offsets
 * when positioning mvaddnwstr() within a line that contains wide glyphs */
int wcs_vwidth(const wchar_t *s, int n);

#endif /* UI_EDITOR_HELPER_H */
