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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "ui_spell.h"
#include "ui_internal.h"
#include "../components/editor.h"
#include "../components/config.h"
#include "../core/utf8.h"

#ifdef HAVE_HUNSPELL
#include "../core/spell.h"
#endif

#ifndef A_BOLD
#include <ncurses.h>
#endif

static void spell_panel_geometry(int *x, int *y, int *w, int *h)
{
    int W, H;

    W = COLS;
    H = LINES;

    *w = W;
    *h = SPELL_PANEL_H;

    /* Full width, just above status bar */
    *x = 0;
    *y = H - *h - 1; /* -1 for status bar */

    if (*y < 1)
        *y = 1;
}

void ui_spell_draw_panel(UiApp *app)
{
    int x, y, w, h;
    int j;

    if (!app || !app->show_spell)
        return;

    spell_panel_geometry(&x, &y, &w, &h);

    standend();

    /* Background */
    attron(COLOR_PAIR(COL_POPUP));

    for (j = 0; j < w; j++)
        mvaddch(y, x + j, ' ');

#ifdef HAVE_HUNSPELL

    if (!app->spell_handle)
    {
        mvprintw(y, x + 2, "[ Spell ] Dictionary not loaded. Configure in setup.");
    }
    else if (app->spell_current_word[0])
    {
        char *u8;
        const char *status_txt;
        int status_col;

        u8 = wcs_to_utf8(app->spell_current_word, (int)wcslen(app->spell_current_word));

        if (u8)
        {
            mvprintw(y, x + 2, "[ Spell ] Word: %s", u8);
            free(u8);
        }

        if (app->spell_word_status == 1)
        {
            status_txt = "Correct";
            status_col = COL_NORMAL;
        }
        else if (app->spell_word_status == 2)
        {
            status_txt = "Incorrect";
            status_col = COL_UNREAD;
        }
        else
        {
            status_txt = "Press Alt+W to check";
            status_col = COL_NORMAL;
        }

        attroff(COLOR_PAIR(COL_POPUP));
        attron(COLOR_PAIR(status_col));

        mvprintw(y + 1, x + 2, "Status: %s", status_txt);

        attroff(COLOR_PAIR(status_col));
        attron(COLOR_PAIR(COL_POPUP));

        if (app->spell_word_status == 2)
        {
            mvprintw(y + 2, x + 2, "Suggestions: %d (Alt+W to choose)", app->spell_suggestion_count);

            if (app->spell_suggestion_count == 0)
                mvprintw(y + 3, x + 2, "Not in database?");
        }
    }
    else
    {
        mvprintw(y, x + 2, "[ Spell ] Move cursor to a word and press Alt+W to check it.");
    }

#else
    mvprintw(y, x + 2, "[ Spell ] Built without Hunspell support.");
#endif

    attroff(COLOR_PAIR(COL_POPUP));
}

#ifdef HAVE_HUNSPELL

void ui_spell_unload(UiApp *app)
{
    if (!app)
        return;

    if (app->spell_suggestions)
    {
        spell_free_suggestions(app->spell_handle, app->spell_suggestions, app->spell_suggestion_count);

        app->spell_suggestions = NULL;
        app->spell_suggestion_count = 0;
    }

    if (app->spell_handle)
    {
        spell_free(app->spell_handle);
        app->spell_handle = NULL;
    }

    app->spell_current_word[0] = L'\0';
    app->spell_word_status = 0;
}

int ui_spell_load_from_config(UiApp *app)
{
    char aff_path[512];
    char dic_path[512];
    const CrashEditCfg *cfg;

    if (!app || !app->cfg)
        return 0;

    cfg = app->cfg;

    /* Drop any previously-loaded dictionary */
    ui_spell_unload(app);

    if (!cfg->spell_enabled)
        return 0;

    if (cfg->spell_dict_path[0] == '\0' || cfg->spell_dict_name[0] == '\0')
        return 0;

    snprintf(aff_path, sizeof(aff_path), "%.240s/%.240s.aff", cfg->spell_dict_path, cfg->spell_dict_name);
    snprintf(dic_path, sizeof(dic_path), "%.240s/%.240s.dic", cfg->spell_dict_path, cfg->spell_dict_name);

    app->spell_handle = spell_new(aff_path, dic_path);

    if (!app->spell_handle)
    {
        app->spell_enabled = 0;
        return 0;
    }

    app->spell_enabled = cfg->spell_enabled;
    app->spell_active = 0;

    return 1;
}

int ui_spell_toggle_panel(UiApp *app)
{
    if (!app)
        return 1;

    app->show_spell = !app->show_spell;
    ui_status(app, "Spell panel %s", app->show_spell ? "shown" : "hidden");

    return 1;
}

/* word_chars_only: include letters and apostrophes (typical word boundary) */
int spell_is_word_char(wchar_t c)
{
    if (iswalnum((wint_t)c))
        return 1;

    if (c == L'\'' || c == L'-')
        return 1;

    return 0;
}

/* Simple word check for highlighting - returns 1 if incorrect, 0 if correct */
int ui_spell_check_word_simple(UiApp *app, const wchar_t *word, int word_len)
{
    char *word_utf8;
    int result;

    if (!app || !app->spell_handle || !app->spell_active)
        return 0;

    if (word_len <= 0 || word_len >= 256)
        return 0;

    word_utf8 = wcs_to_utf8(word, word_len);

    if (!word_utf8)
        return 0;

    result = spell_check(app->spell_handle, word_utf8);

    free(word_utf8);

    /* spell_check returns 1 for correct, 0 for incorrect */
    return result == 0;
}

int ui_spell_check_word_at_cursor(UiApp *app)
{
    Ed *ed;
    EdInfo info;
    const wchar_t *line;
    int line_len, ws, we, i;
    wchar_t wbuf[256];
    char *u8;
    int correct;
    char **sugs;
    int n_sugs, sel, sug_len;
    wchar_t *sug_wcs;
    const char *u8_status;

    if (!app || !app->editor || !app->spell_handle)
        return 0;

    ed = app->editor;
    ed_get_info(ed, &info);

    line = ed_line_wcs(ed, info.row);

    if (!line)
        return 0;

    line_len = ed_line_len(ed, info.row);

    /* Locate word boundaries around the cursor column */
    ws = info.col;

    while (ws > 0 && spell_is_word_char(line[ws - 1]))
        ws--;

    we = info.col;

    while (we < line_len && spell_is_word_char(line[we]))
        we++;

    if (ws == we)
    {
        ui_status(app, "No word under cursor");
        return 0;
    }

    if ((we - ws) >= (int)(sizeof(wbuf) / sizeof(wbuf[0])))
        return 0;

    for (i = 0; i < (we - ws); i++)
        wbuf[i] = line[ws + i];

    wbuf[we - ws] = L'\0';

    /* Stash for the panel */
    if ((we - ws) < (int)(sizeof(app->spell_current_word) / sizeof(app->spell_current_word[0])))
    {
        wcsncpy(app->spell_current_word, wbuf, sizeof(app->spell_current_word) / sizeof(app->spell_current_word[0]) - 1);
        app->spell_current_word[sizeof(app->spell_current_word) / sizeof(app->spell_current_word[0]) - 1] = L'\0';
    }

    app->show_spell = 1;

    u8 = wcs_to_utf8(wbuf, we - ws);

    if (!u8)
        return 0;

    correct = spell_check(app->spell_handle, u8);

    if (correct)
    {
        app->spell_word_status = 1;

        /* Free any previous suggestions before marking as correct */
        if (app->spell_suggestions)
        {
            spell_free_suggestions(app->spell_handle, app->spell_suggestions, app->spell_suggestion_count);

            app->spell_suggestions = NULL;
            app->spell_suggestion_count = 0;
        }

        u8_status = u8;
        ui_status(app, "'%s' is correct", u8_status);

        free(u8);
        return 1;
    }

    app->spell_word_status = 2;

    /* Free any previous suggestions before fetching new ones */
    if (app->spell_suggestions)
    {
        spell_free_suggestions(app->spell_handle, app->spell_suggestions, app->spell_suggestion_count);

        app->spell_suggestions = NULL;
        app->spell_suggestion_count = 0;
    }

    sugs = spell_suggest(app->spell_handle, u8, &n_sugs);

    if (!sugs || n_sugs == 0)
    {
        if (sugs)
            spell_free_suggestions(app->spell_handle, sugs, n_sugs);

        ui_status(app, "No suggestions for '%s'", u8);

        free(u8);
        return 1;
    }

    app->spell_suggestions = sugs;
    app->spell_suggestion_count = n_sugs;

    sel = ui_popup_list("Spelling Suggestions", (const char **)sugs, n_sugs, 0);

    if (sel >= 0 && sel < n_sugs)
    {
        sug_wcs = utf8_to_wcs(sugs[sel], &sug_len);

        if (sug_wcs)
        {
            ed_save_undo(ed);
            ed_set_pos(ed, info.row, ws);

            for (i = 0; i < (we - ws); i++)
                ed_delete(ed);

            for (i = 0; i < sug_len; i++)
                ed_insert_char(ed, sug_wcs[i]);

            ui_status(app, "Replaced with '%s'", sugs[sel]);
            free(sug_wcs);

            /* Word fixed -- mark it correct in the panel */
            app->spell_word_status = 1;
        }
    }

    free(u8);

    /* Release suggestions now that the user has chosen (or cancelled) */
    spell_free_suggestions(app->spell_handle, app->spell_suggestions, app->spell_suggestion_count);

    app->spell_suggestions = NULL;
    app->spell_suggestion_count = 0;

    return 1;
}

#endif /* HAVE_HUNSPELL */
