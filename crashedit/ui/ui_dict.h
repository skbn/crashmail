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

#ifndef UI_DICT_H
#define UI_DICT_H

#include "ui.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DICT_PANEL_HEIGHT 6
#define DICT_PANEL_ROWS (DICT_PANEL_HEIGHT - 1)

    /* Draw dict panel when app->show_dict is on */
    void ui_dict_draw_panel(UiApp *app);

    /* Show/hide panel. Returns 1 (force redraw) */
    int ui_dict_toggle_panel(UiApp *app);

    /* Set panel content. word_or_phrase in header, text is definition */
    void ui_dict_set_result(UiApp *app, const char *word_or_phrase, const char *text);

    /* Free buffered content */
    void ui_dict_free(UiApp *app);

    /* Scroll panel by one line. Return 1 if moved, 0 otherwise */
    int ui_dict_scroll_up(UiApp *app);
    int ui_dict_scroll_down(UiApp *app);

#ifdef __cplusplus
}
#endif

#endif /* UI_DICT_H */
