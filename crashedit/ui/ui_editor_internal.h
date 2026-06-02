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

/* ui_editor_internal.h -- Shared between ui_editor.c and ui_editor_prep.c */
#ifndef UI_EDITOR_INTERNAL_H
#define UI_EDITOR_INTERNAL_H

#include "ui_internal.h"
#include "../core/keys.h"

/* Field indices for TAB cycle (GoldED+ order: FROM->TO->DADDR->SUBJECT->BODY) */
#define EF_FROM 0
#define EF_TO 1
#define EF_DADDR 2
#define EF_SUBJECT 3
#define EF_BODY 4
#define EF_NONE -1

/* KEY_TAB may not be defined in all ncurses implementations */
#ifndef KEY_TAB
#define KEY_TAB 9
#endif

/* Editor internal helpers (defined in ui_editor_prep.c / ui_editor_popups.c) */

char *editor_build_kludge_block(const CrashEditCfg *cfg, const char *oaddr, const char *daddr, const char *raw_daddr, const char *reply_msgid_value, int is_netmail);
const char *editor_daddr_for_intl(UiApp *app, const char *daddr);
void editor_reset_state(UiApp *app);

/* Editor prep API */
void ui_editor_prep_new(UiApp *app);
void ui_editor_prep_reply(UiApp *app, uint32_t orig_msgnum);
void ui_editor_prep_edit(UiApp *app, uint32_t msgnum);
int ui_editor_save(UiApp *app);

/* Sub-popups (F8=kludges, F9=attributes) */
void editor_kludge_popup(UiApp *app);
int editor_attr_popup(UiApp *app);

#endif /* UI_EDITOR_INTERNAL_H */
