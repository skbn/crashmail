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

#ifndef UI_HYPH_H
#define UI_HYPH_H

#include "ui_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Load hyphenation dictionary from config */
    int ui_hyph_load_from_config(UiApp *app);

    /* Free hyphenation handle */
    void ui_hyph_unload(UiApp *app);

    /* Get hyphenation break points for UTF-8 word */
    int ui_hyph_split_word(UiApp *app, const char *word, int word_len, int *out_pos, int *out_count);

    /* Find hyphenation break point for word at column limit */
    int ui_hyph_find_break(UiApp *app, const wchar_t *word, int word_len, int col_limit);

#ifdef __cplusplus
}
#endif

#endif /* UI_HYPH_H */
