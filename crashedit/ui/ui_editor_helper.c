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

#define _XOPEN_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include "ui_editor_helper.h"
#include "ui.h"
#include "ui_internal.h"
#include "ui_hyph.h"
#include "ui_spell.h"
#include "../core/utf8.h"
#include "../components/editor.h"

#ifdef PLATFORM_WIN32
/* Windows wcswidth() does not know East Asian Wide / Fullwidth / emoji /
 * box-drawing / block glyphs. Return 2 for those ranges, fall back otherwise */
static int ed_char_vwidth(wchar_t ch)
{
    unsigned int cp = (unsigned int)ch;
    int w;

    if (cp < 0x1100)
        return 1;

    if (cp >= 0x2500 && cp <= 0x257F)
        return 1; /* Box Drawing - narrow, same as ncurses Linux */

    if ((cp >= 0x1100 && cp <= 0x115F) ||
        (cp >= 0x2190 && cp <= 0x21FF) ||
        (cp >= 0x2329 && cp <= 0x232A) ||
        (cp >= 0x2580 && cp <= 0x259F) ||
        (cp >= 0x25A0 && cp <= 0x25FF) ||
        (cp >= 0x2600 && cp <= 0x26FF) ||
        (cp >= 0x2700 && cp <= 0x27BF) ||
        (cp >= 0x2B00 && cp <= 0x2BFF) ||
        (cp >= 0x2E80 && cp <= 0x303E) ||
        (cp >= 0x3041 && cp <= 0x33FF) ||
        (cp >= 0x3400 && cp <= 0x4DBF) ||
        (cp >= 0x4E00 && cp <= 0x9FFF) ||
        (cp >= 0xA000 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) ||
        (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE30 && cp <= 0xFE4F) ||
        (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) ||
        cp >= 0x1F000)
        return 2;

    w = wcswidth(&ch, 1);
    return (w > 0) ? w : 1;
}

#define CHAR_VWIDTH(c) ed_char_vwidth(c)
#else
#define CHAR_VWIDTH(c) ((wcswidth(&(c), 1) > 0) ? wcswidth(&(c), 1) : 1)
#endif

int wcs_vwidth(const wchar_t *s, int n)
{
    int v = 0;
    int i;

    if (!s || n <= 0)
        return 0;

    for (i = 0; i < n; i++)
        v += CHAR_VWIDTH(s[i]);

    return v;
}

/* Draw wide string with tab expansion */
void ui_draw_wcs_line_with_tabs(int y, int x, const wchar_t *s, int n, int tab_width)
{
    int i;
    int col = 0;
    int out_len = 0;
    wchar_t buf[4096];
    int max_len = (int)(sizeof(buf) / sizeof(buf[0]));

    if (!s || n <= 0 || tab_width < 1)
        return;

    if (n > max_len / 2)
        n = max_len / 2;

    for (i = 0; i < n && out_len < max_len - 1; i++)
    {
        if (s[i] == L'\t')
        {
            int w = tab_width - (col % tab_width);
            int j;

            for (j = 0; j < w && out_len < max_len - 1; j++)
            {
                buf[out_len] = L' ';
                out_len++;
            }

            col += w;
        }
        else
        {
            int w = CHAR_VWIDTH(s[i]);

            buf[out_len] = s[i];

            out_len++;
            col += w;
        }
    }

    mvaddnwstr(y, x, buf, out_len);
}

/* Visual width with tab-stop support */
int wcs_vwidth_ex(const wchar_t *s, int n, int start_col, int tab_width)
{
    int v = 0;
    int col = start_col;
    int i;

    if (!s || n <= 0 || tab_width < 1)
        return 0;

    for (i = 0; i < n; i++)
    {
        if (s[i] == L'\t')
        {
            int w = tab_width - (col % tab_width);

            v += w;
            col += w;
        }
        else
        {
            int w = CHAR_VWIDTH(s[i]);

            v += w;
            col += w;
        }
    }

    return v;
}

/* Effective wrap column, clamp AUTOWRAP to COLS-1, 0=disabled */
int editor_eff_wrap(const UiApp *app)
{
    int cfgw = (app && app->cfg) ? app->cfg->autowrap_col : 0;
    int limit = COLS - 1; /* leave one column of margin */

    if (cfgw <= 0)
        return 0; /* AUTOWRAP disabled in config: never wrap */

    if (COLS <= 10)
        return 0; /* Unusably narrow: scroll instead of wrapping */

    if (cfgw > limit)
        return limit; /* Clamp to screen width */

    return cfgw;
}

/* Left margin for editor body with line numbers */
int editor_body_offset(const UiApp *app, int line_count)
{
    int margin = 1;
    int tab_width;

    if (!app || !app->cfg || !app->cfg->show_line_numbers)
        return 0;

    if (line_count <= 0)
        line_count = 1;

    while (line_count >= 10)
    {
        line_count /= 10;
        margin++;
    }

    margin += 1; /* space after the number */

    tab_width = app->cfg->tab_width > 0 ? app->cfg->tab_width : 4;

    /* Round up to the next multiple of tab_width */
    margin = ((margin + tab_width - 1) / tab_width) * tab_width;

    return margin;
}

#ifdef HAVE_HYPHEN
static int crashedit_hyph_cb(void *data, const wchar_t *word, int word_len, int col_limit)
{
    UiApp *app = (UiApp *)data;

    if (!app || !app->hyph_wrap_enabled || !app->hyph_handle)
        return 0;

    return ui_hyph_find_break(app, word, word_len, col_limit);
}
#endif

/* Trigger auto hard-wrap after an editing action */
void ed_auto_rewrap_after_edit(UiApp *app)
{
    int width;
    int old_mode;

    if (!app || !app->editor || !app->cfg || !app->cfg->hard_wrap)
        return;

    width = editor_eff_wrap(app);

    if (width < 4)
        return;

    old_mode = app->editor->undo_snapshot_mode;
    app->editor->undo_snapshot_mode = 1;

#ifdef HAVE_HYPHEN
    ed_rewrap_paragraph_ex(app->editor, width, crashedit_hyph_cb, app);
#else
    ed_rewrap_paragraph_ex(app->editor, width, NULL, NULL);
#endif

    app->editor->undo_snapshot_mode = old_mode;
}

/* Manual hard-wrap rewrap for the current paragraph (Ctrl+W fallback) */
int ed_manual_rewrap_paragraph(UiApp *app, int width)
{
    int old_mode;
    int rc;

    if (!app || !app->editor || width < 4)
        return -1;

    old_mode = app->editor->undo_snapshot_mode;
    app->editor->undo_snapshot_mode = 1;

#ifdef HAVE_HYPHEN
    rc = ed_rewrap_paragraph_ex(app->editor, width, crashedit_hyph_cb, app);
#else
    rc = ed_rewrap_paragraph_ex(app->editor, width, NULL, NULL);
#endif

    app->editor->undo_snapshot_mode = old_mode;

    return rc;
}

/* Detect loaded wrap-hyphens using the active spell checker */
void ui_editor_detect_wrap_hyphens(UiApp *app)
{
#ifdef HAVE_HUNSPELL
    Ed *ed = NULL;
    EdLine *ln = NULL;
    EdLine *next = NULL;
    int i, j, k;
    int first_start, first_end, next_start, next_end;
    int combined_len;
    int spell_ok;
    char *word_utf8 = NULL;
    wchar_t combined[512];

    if (!app)
        return;

    ed = app->editor;
    if (!ed)
        return;

    if (!app->spell_handle)
        return;

    if (!app->cfg || !app->cfg->hyph_detect_on_load)
        return;

    for (i = 0; i < ed->count - 1; i++)
    {
        ln = ed->lines[i];
        next = ed->lines[i + 1];

        if (ln->len <= 0 || ln->wcs[ln->len - 1] != L'-')
            continue;

        if (!next || next->len <= 0)
            continue;

        /* Last word before the trailing hyphen */
        first_end = ln->len - 1;
        first_start = first_end - 1;

        while (first_start >= 0 && !iswspace(ln->wcs[first_start]))
            first_start--;

        first_start++;

        /* Strip leading punctuation from the first word */
        while (first_start < first_end && iswpunct(ln->wcs[first_start]))
            first_start++;

        if (first_start >= first_end)
            continue;

        /* First word on the next line */
        next_start = 0;

        while (next_start < next->len && iswspace(next->wcs[next_start]))
            next_start++;

        if (next_start >= next->len)
            continue;

        next_end = next_start + 1;

        while (next_end < next->len && !iswspace(next->wcs[next_end]))
            next_end++;

        /* Strip leading and trailing punctuation from the next word */
        while (next_start < next_end && iswpunct(next->wcs[next_start]))
            next_start++;

        while (next_end > next_start && iswpunct(next->wcs[next_end - 1]))
            next_end--;

        if (next_start >= next_end)
            continue;

        /* Build combined word from both halves */
        combined_len = 0;

        for (j = first_start; j < first_end && combined_len < 511; j++)
            combined[combined_len++] = ln->wcs[j];

        for (k = next_start; k < next_end && combined_len < 511; k++)
            combined[combined_len++] = next->wcs[k];

        combined[combined_len] = L'\0';

        if (combined_len < 4 || combined_len >= 256)
            continue;

        word_utf8 = wcs_to_utf8(combined, combined_len);
        spell_ok = (word_utf8 && spell_check(app->spell_handle, word_utf8));

        free(word_utf8);

        word_utf8 = NULL;

        /* If the joined word is valid, the hyphen was probably a wrap-hyphen */
        if (spell_ok)
            ln->has_wrap_hyphen = 1;
    }
#endif
}
