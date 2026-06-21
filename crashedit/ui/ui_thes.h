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

#ifndef UI_THES_H
#define UI_THES_H

#include "ui_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef HAVE_MYTHES

    /* Load thesaurus from config */
    int ui_thes_load_from_config(UiApp *app);

    /* Free thesaurus handle */
    void ui_thes_unload(UiApp *app);

    /* Look up word under cursor, show synonyms popup */
    int ui_thes_lookup_word(UiApp *app);

#else /* !HAVE_MYTHES */

/* Inert stubs for callers */
int ui_thes_load_from_config(UiApp *app);
void ui_thes_unload(UiApp *app);
int ui_thes_lookup_word(UiApp *app);

#endif

#ifdef __cplusplus
}
#endif

#endif /* UI_THES_H */
