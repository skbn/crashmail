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

#ifndef UI_ASSIST_H
#define UI_ASSIST_H

#include "ui_internal.h"
#include <wchar.h>

/* Called after inserting char; may replace via ed_delete/ed_insert_char */
int ui_assist_on_char(UiApp *app, wchar_t just_typed);

/* Returns 1 if word at (line, col_start) repeats previous word on same line (whitespace-separated). For rendering highlight */
int ui_assist_check_repeat(UiApp *app, int line, int col_start, int word_len);

#endif
