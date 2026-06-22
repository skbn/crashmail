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

#include "ui_thes.h"
#include "ui_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "../components/config.h"
#include "../components/editor.h"
#include "../core/utf8.h"
#include "../core/charset.h"

#ifdef HAVE_MYTHES
#if defined(PLATFORM_AMIGA)
#include "../spellchecker/thes.h"
#else
#include "../core/thes.h"
#endif
#endif

#ifdef HAVE_HUNSPELL
#if defined(PLATFORM_AMIGA)
#include "../spellchecker/spell.h"
#else
#include "../core/spell.h"
#endif
#endif

#ifdef HAVE_MYTHES

static int thes_is_word_char(wchar_t c)
{
    if (iswalnum((wint_t)c))
        return 1;

    if (c == L'\'' || c == L'-')
        return 1;

    return 0;
}

int ui_thes_load_from_config(UiApp *app)
{
    char idx_path[512];
    char dat_path[512];
    const CrashEditCfg *cfg;
    const char *base_path;
    const char *base_name;
    ThesHandle *th = NULL;

    if (!app || !app->cfg)
        return 0;

    cfg = app->cfg;

    ui_thes_unload(app);

    if (!cfg->thes_enabled)
        return 0;

    base_path = cfg->thes_dict_path;
    base_name = cfg->thes_dict_name;

    if (!base_path[0] || !base_name[0])
        return 0;

    /* Use dictionary name as-is */
    snprintf(idx_path, sizeof(idx_path), "%.220s/%.40s.idx", base_path, base_name);
    snprintf(dat_path, sizeof(dat_path), "%.220s/%.40s.dat", base_path, base_name);

    th = thes_new(idx_path, dat_path);

    if (!th)
    {
        /* Fallback: prepend "th_" */
        snprintf(idx_path, sizeof(idx_path), "%.220s/th_%.40s.idx", base_path, base_name);
        snprintf(dat_path, sizeof(dat_path), "%.220s/th_%.40s.dat", base_path, base_name);

        th = thes_new(idx_path, dat_path);
    }

    app->thes_handle = th;

    /* Attach speller for morphological fallback */
#ifdef HAVE_HUNSPELL
    if (app->thes_handle)
        thes_set_speller((ThesHandle *)app->thes_handle, (SpellChecker *)app->spell_handle);
#endif

    return app->thes_handle ? 1 : 0;
}

void ui_thes_unload(UiApp *app)
{
    if (!app)
        return;

    if (app->thes_handle)
    {
        thes_free((ThesHandle *)app->thes_handle);
        app->thes_handle = NULL;
    }
}

int ui_thes_lookup_word(UiApp *app)
{
    Ed *ed = NULL;
    EdInfo info;
    const wchar_t *line;
    int line_len, ws, we, i;
    wchar_t wbuf[128];
    char *u8 = NULL;
    ThesMeaning *meanings = NULL;
    int nmeanings, total_items, idx, sel;
    char **items = NULL;
    int *item_to_syn = NULL;
    int m, k;
    const char *thes_encoding;
    char utf8_buf[1024];
    char defn_utf8_buf[1024];

    if (!app)
        return 0;

    if (!app->thes_handle)
    {
        ui_status(app, "No thesaurus loaded");
        return 0;
    }

    if (!app->editor)
        return 0;

    ed = app->editor;

    ed_get_info(ed, &info);
    line = ed_line_wcs(ed, info.row);

    if (!line)
        return 0;

    line_len = ed_line_len(ed, info.row);

    ws = info.col;

    while (ws > 0 && thes_is_word_char(line[ws - 1]))
        ws--;

    we = info.col;

    while (we < line_len && thes_is_word_char(line[we]))
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

    u8 = wcs_to_utf8(wbuf, we - ws);

    if (!u8)
        return 0;

    meanings = NULL;
    nmeanings = thes_lookup((ThesHandle *)app->thes_handle, u8, &meanings);

    if (nmeanings <= 0 || !meanings)
    {
        ui_status(app, "No synonyms for '%s'", u8);

        free(u8);
        return 0;
    }

    /* Count total non-NULL synonyms */
    total_items = 0;

    for (i = 0; i < nmeanings; i++)
    {
        int kk;

        for (kk = 0; kk < meanings[i].nsyns; kk++)
        {
            if (meanings[i].syns[kk])
                total_items++;
        }
    }

    if (total_items == 0)
    {
        thes_free_meanings((ThesHandle *)app->thes_handle, meanings, nmeanings);
        ui_status(app, "No synonyms for '%s'", u8);

        free(u8);
        return 0;
    }

    items = (char **)calloc((size_t)total_items, sizeof(char *));
    item_to_syn = (int *)calloc((size_t)total_items * 2, sizeof(int));

    if (!items || !item_to_syn)
    {
        free(items);
        free(item_to_syn);

        thes_free_meanings((ThesHandle *)app->thes_handle, meanings, nmeanings);

        free(u8);
        return 0;
    }

    idx = 0;

    thes_encoding = thes_get_encoding((ThesHandle *)app->thes_handle);

    for (m = 0; m < nmeanings; m++)
    {
        for (k = 0; k < meanings[m].nsyns; k++)
        {
            size_t need;
            const char *defn = NULL;
            const char *syn_utf8 = NULL;
            const char *defn_utf8 = NULL;

            if (!meanings[m].syns[k])
                continue;

            defn = meanings[m].def ? meanings[m].def : "";

            /* Convert synonym to UTF-8 if needed */
            if (thes_encoding && thes_encoding[0] && strcmp(thes_encoding, "UTF-8") != 0)
            {
                utf8_buf[0] = '\0';

                charset_to_utf8(thes_encoding, meanings[m].syns[k], (int)strlen(meanings[m].syns[k]), utf8_buf, sizeof(utf8_buf));
                syn_utf8 = utf8_buf;
            }
            else
            {
                syn_utf8 = meanings[m].syns[k];
            }

            /* Convert definition to UTF-8 if needed */
            if (defn[0] && thes_encoding && thes_encoding[0] && strcmp(thes_encoding, "UTF-8") != 0)
            {
                defn_utf8_buf[0] = '\0';

                charset_to_utf8(thes_encoding, defn, (int)strlen(defn), defn_utf8_buf, sizeof(defn_utf8_buf));
                defn_utf8 = defn_utf8_buf;
            }
            else
            {
                defn_utf8 = defn;
            }

            need = strlen(syn_utf8) + strlen(defn_utf8) + 16;
            items[idx] = (char *)malloc(need);

            if (items[idx])
            {
                if (defn_utf8[0])
                    snprintf(items[idx], need, "%s  (%s)", syn_utf8, defn_utf8);
                else
                    snprintf(items[idx], need, "%s", syn_utf8);
            }
            else
            {
                items[idx] = (char *)malloc(2);

                if (items[idx])
                    strcpy(items[idx], "?");
            }

            item_to_syn[idx * 2 + 0] = m;
            item_to_syn[idx * 2 + 1] = k;

            idx++;
        }
    }

    sel = ui_popup_list("Synonyms", (const char **)items, idx, 0);

    if (sel >= 0 && sel < idx)
    {
        int sm = item_to_syn[sel * 2 + 0];
        int sk = item_to_syn[sel * 2 + 1];
        const char *chosen_orig = meanings[sm].syns[sk];
        char chosen_utf8[1024];
        const char *chosen;
        wchar_t *wsyn = NULL;
        int wlen;

        if (chosen_orig)
        {
            if (thes_encoding && thes_encoding[0] && strcmp(thes_encoding, "UTF-8") != 0)
            {
                chosen_utf8[0] = '\0';

                charset_to_utf8(thes_encoding, chosen_orig, (int)strlen(chosen_orig), chosen_utf8, sizeof(chosen_utf8));
                chosen = chosen_utf8;
            }
            else
            {
                chosen = chosen_orig;
            }

            wsyn = utf8_to_wcs(chosen, &wlen);

            if (wsyn)
            {
                ed_save_undo(ed);
                ed_set_pos(ed, info.row, ws);

                for (i = 0; i < (we - ws); i++)
                    ed_delete(ed);

                for (i = 0; i < wlen; i++)
                    ed_insert_char(ed, wsyn[i]);

                free(wsyn);

                ui_status(app, "Replaced with '%s'", chosen);
            }
        }
    }

    /* Free allocated items */
    for (i = 0; i < idx; i++)
        free(items[i]);

    free(items);
    free(item_to_syn);

    thes_free_meanings((ThesHandle *)app->thes_handle, meanings, nmeanings);

    free(u8);

    return 1;
}

#else /* !HAVE_MYTHES -- inert stubs */

int ui_thes_load_from_config(UiApp *app)
{
    return 0;
}

void ui_thes_unload(UiApp *app)
{
}

int ui_thes_lookup_word(UiApp *app)
{
    if (app)
        ui_status(app, "Thesaurus support not built in");

    return 0;
}

#endif /* HAVE_MYTHES */
