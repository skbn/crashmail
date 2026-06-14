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

#ifndef UI_EDITOR_SEARCH_H
#define UI_EDITOR_SEARCH_H

#include "ui.h"

/* Reset search state in editor */
void reset_search(UiApp *app);

/* Navigate to previous match in editor */
int search_prev_editor(UiApp *app);

/* Navigate to next match in editor */
int search_next_editor(UiApp *app);

/* Helper: replace all occurrences of needle with repl (for replace_all) */
int do_replace(UiApp *app, const wchar_t *needle, const wchar_t *repl);

/* Interactive replace - uses ui_popup_replace with case/whole-word options */
int replace(UiApp *app);

/* Replace current match - uses last replacement text without popup */
int replace_current(UiApp *app);

/* Replace all matches - uses last replacement text with Yes/No confirmation */
int replace_all(UiApp *app);

/* Handle search with popup results list */
int handle_search_with_popup(UiApp *app);

#endif /* UI_EDITOR_SEARCH_H */
