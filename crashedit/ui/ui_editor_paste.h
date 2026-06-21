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

#ifndef UI_EDITOR_PASTE_H
#define UI_EDITOR_PASTE_H

#include "ui.h"

/* Word-wrap UTF-8 paste to col columns */
int paste_char_width(wchar_t c);

/* Hyphenation callback for wrap_paste_text_ex */
typedef int (*PasteHyphFn)(void *user_data, const char *word_utf8, int word_byte_len, int *out_byte_pos, int *out_count);

/* Word-wrap UTF-8 paste to col columns (space-only) */
char *wrap_paste_text(const char *utf8, int col);
char *wrap_paste_text_ex(const char *utf8, int col, PasteHyphFn hyph, void *hyph_data);

/* Read characters until KEY_PASTE_END */
char *collect_bracketed_paste(void);

/* Detect rapid paste (fallback for no bracketed paste) */
char *collect_rapid_paste(wint_t first_wch);

/* Paste UTF-8 buffer at cursor */
void deliver_paste(UiApp *app, const char *utf8);

#endif /* UI_EDITOR_PASTE_H */
