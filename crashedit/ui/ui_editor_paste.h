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

#ifndef UI_EDITOR_PASTE_H
#define UI_EDITOR_PASTE_H

#include "ui.h"

/* Word-wrap UTF-8 paste to col columns, preserving newlines. No hard-breaks for URLs/code */
int paste_char_width(wchar_t c);

/* Word-wrap UTF-8 paste to col columns, preserving newlines */
char *wrap_paste_text(const char *utf8, int col);

/* Read characters until KEY_PASTE_END. Returns malloc'd UTF-8 buffer or NULL */
char *collect_bracketed_paste(void);

/* Detect rapid paste (fallback for terminals without bracketed paste support) */
char *collect_rapid_paste(void);

/* Paste UTF-8 buffer at cursor: body preserves newlines, header strips them */
void deliver_paste(UiApp *app, const char *utf8);

#endif /* UI_EDITOR_PASTE_H */
