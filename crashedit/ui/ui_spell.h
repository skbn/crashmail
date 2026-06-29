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

#ifndef UI_SPELL_H
#define UI_SPELL_H

#include "ui_internal.h"
#include "../spellchecker/spell.h"

/* Spell panel height in lines */
#define SPELL_PANEL_H 4

/* Draws the floating spell panel over the editor when app->show_spell is on */
void ui_spell_draw_panel(UiApp *app);

#ifdef HAVE_HUNSPELL

/* Load Hunspell dictionary from config (cfg->spell_dict_path + spell_dict_name) */
int ui_spell_load_from_config(UiApp *app);

/* Free dictionary handle and current suggestions, if any */
void ui_spell_unload(UiApp *app);

/* Spell-check the word under the cursor of the editor */
int ui_spell_check_word_at_cursor(UiApp *app);

/* Simple word check for highlighting - returns 1 if incorrect, 0 if correct */
int ui_spell_check_word_simple(UiApp *app, const wchar_t *word, int word_len);

/* Word character check for spell highlighting */
int spell_is_word_char(wchar_t c);

/* Toggle the spell-panel visibility (Alt+S in the editor) */
int ui_spell_toggle_panel(UiApp *app);

/* UI-level spell-check cache */
void ui_spell_cache_init(UiSpellCache *cache);
void ui_spell_cache_clear(UiSpellCache *cache);
int ui_spell_cache_lookup(UiSpellCache *cache, const wchar_t *word, int word_len, int *out_incorrect);
void ui_spell_cache_put(UiSpellCache *cache, const wchar_t *word, int word_len, int incorrect);

#endif /* HAVE_HUNSPELL */

#endif /* UI_SPELL_H */
