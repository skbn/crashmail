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

#include "ui_hyph.h"
#include "ui_internal.h"
#include "../components/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_HYPHEN
#include "../core/hyph.h"
#include "../core/utf8.h"
#endif

int ui_hyph_load_from_config(UiApp *app)
{
#ifdef HAVE_HYPHEN
    char path[512];
    HyphDict *h = NULL;
    const CrashEditCfg *cfg;

    if (!app || !app->cfg)
        return 0;

    cfg = app->cfg;

    ui_hyph_unload(app);

    if (!cfg->hyph_enabled)
        return 0;

    if (!cfg->hyph_dict_path[0] || !cfg->hyph_dict_name[0])
        return 0;

    snprintf(path, sizeof(path), "%.220s/%.40s.dic", cfg->hyph_dict_path, cfg->hyph_dict_name);

    h = hyph_new(path);

    if (!h)
    {
        /* Fallback: prepend "hyph_" */
        snprintf(path, sizeof(path), "%.220s/hyph_%.40s.dic", cfg->hyph_dict_path, cfg->hyph_dict_name);
        h = hyph_new(path);
    }

    if (!h)
        return 0;

    app->hyph_handle = h;
    app->hyph_wrap_enabled = cfg->hyph_wrap_enabled;

    return 1;
#else

    return 0;
#endif
}

void ui_hyph_unload(UiApp *app)
{
#ifdef HAVE_HYPHEN
    if (!app)
        return;

    if (app->hyph_handle)
    {
        hyph_free((HyphDict *)app->hyph_handle);
        app->hyph_handle = NULL;
    }
#endif
}

int ui_hyph_split_word(UiApp *app, const char *word, int word_len, int *out_pos, int *out_count)
{
#ifdef HAVE_HYPHEN
    if (out_count)
        *out_count = 0;

    if (!app || !app->hyph_handle || !word || word_len <= 0)
        return 0;

    return hyph_breakpoints((HyphDict *)app->hyph_handle, word, word_len, out_pos, out_count);
#else

    if (out_count)
        *out_count = 0;

    return 0;
#endif
}

/* Character width helper */
static int char_width(wchar_t c)
{
    if (c == L'\t')
        return 1;

    /* Assume 1 for wide chars */
    return 1;
}

int ui_hyph_find_break(UiApp *app, const wchar_t *word, int word_len, int col_limit)
{
#ifdef HAVE_HYPHEN
    char *utf8_word = NULL;
    int utf8_len;
    int hyph_pos[16];
    int hyph_count = 0;
    int k;

    /* Validate */
    if (!app || !word || word_len <= 0)
        return -1;

    if (col_limit <= 0)
        return -1;

    if (word_len < 4 || word_len >= 512)
        return -1;

    if (!app->hyph_handle)
        return -1;

    /* Convert to UTF-8 */
    utf8_word = wcs_to_utf8(word, word_len);

    if (!utf8_word)
        return -1;

    utf8_len = (int)strlen(utf8_word);

    /* Get breakpoints */
    if (!ui_hyph_split_word(app, utf8_word, utf8_len, hyph_pos, &hyph_count))
    {
        free(utf8_word);
        return -1;
    }

    /* Limit to array size */
    if (hyph_count > 16)
        hyph_count = 16;

    /* Search from right to left */
    for (k = hyph_count - 1; k >= 0; k--)
    {
        int char_off = utf8_charcount(utf8_word, hyph_pos[k]);
        int break_col = 0;
        int m;

        /* Validate position */
        if (char_off <= 0 || char_off >= word_len)
            continue;

        /* Calculate columns */
        for (m = 0; m < char_off; m++)
            break_col += char_width(word[m]);

        /* Reserve space for '-' */
        if (break_col > col_limit - 1)
            continue;

        free(utf8_word);
        return char_off;
    }

    free(utf8_word);
#endif

    return -1;
}
