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
 */

#include "ui_editor_paste.h"
#include "ui.h"
#include "ui_internal.h"
#include "ui_editor_internal.h"
#include "../core/charset.h"
#include "../core/msghdr.h"
#include "../components/editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* Forward declaration for reset_search (defined in ui_editor_search.c) */
extern void reset_search(UiApp *app);

/* Forward declaration for editor_eff_wrap (defined in ui_editor.c) */
extern int editor_eff_wrap(const UiApp *app);

/* Word-wrap UTF-8 paste to col columns, preserving newlines. No hard-breaks for URLs/code */
int paste_char_width(wchar_t c)
{
    /* For FTN editing: does this char take 1 column or 0? Zero-width cases: combining marks, BOM. CJK treated as 1 */
    if (c == 0)
        return 0;

    if (c < 0x20)
        return 0; /* control chars, including \r */

    if (c >= 0x0300 && c <= 0x036F)
        return 0; /* combining diacritical marks */

    if (c == 0x200B || c == 0xFEFF)
        return 0; /* ZWSP / BOM */

    return 1;
}

/* Word-wrap UTF-8 paste to col columns, preserving newlines */
char *wrap_paste_text(const char *utf8, int col)
{
    wchar_t *w;
    int wlen = 0;
    int i;
    int line_start = 0;
    int last_space = -1;
    int col_pos = 0;
    int out_cap, out_len = 0;
    wchar_t *out;
    char *result;

    if (col <= 0)
        return NULL;

    w = utf8_to_wcs(utf8, &wlen);

    if (!w)
        return NULL;

    out_cap = wlen + (wlen / col) + 16;
    out = (wchar_t *)malloc((size_t)out_cap * sizeof(wchar_t));

    if (!out)
    {
        free(w);
        return NULL;
    }

    for (i = 0; i < wlen; i++)
    {
        wchar_t ch = w[i];
        int cw;

        if (ch == L'\n')
        {
            if (out_len < out_cap)
                out[out_len++] = L'\n';

            line_start = out_len;
            last_space = -1;
            col_pos = 0;

            continue;
        }
        if (ch == L'\r')
            continue;

        cw = paste_char_width(ch);

        if (ch == L'\t')
            cw = 1;

        if (out_len < out_cap)
            out[out_len++] = ch;

        if (ch == L' ' || ch == L'\t')
            last_space = out_len - 1;

        col_pos += cw;

        if (col_pos > col && last_space > line_start)
        {
            int new_start;
            int j;
            int width_after = 0;

            out[last_space] = L'\n';
            new_start = last_space + 1;

            for (j = new_start; j < out_len; j++)
            {
                int w2 = (out[j] == L'\t') ? 1 : paste_char_width(out[j]);
                width_after += w2;
            }

            line_start = new_start;
            last_space = -1;
            col_pos = width_after;
        }
    }

    free(w);

    if (out_len < out_cap)
        out[out_len] = L'\0';
    else
        out[out_cap - 1] = L'\0';

    result = wcs_to_utf8(out, out_len);
    free(out);

    return result;
}

/* Paste UTF-8 buffer at cursor: body preserves newlines, header strips them */
void deliver_paste(UiApp *app, const char *utf8)
{
    if (!utf8 || !utf8[0])
        return;

    if (app->edit_active_field == EF_BODY)
    {
        char *wrapped = NULL;
        const char *to_insert = utf8;
        int reported_len;

        /* HARD-WRAP only: reflow pasted text; soft-wrap inserts verbatim */
        if (app->cfg && app->cfg->hard_wrap)
        {
            int pw = editor_eff_wrap(app);

            if (pw > 0)
            {
                wrapped = wrap_paste_text(utf8, pw);

                if (wrapped)
                    to_insert = wrapped;
            }
        }

        ed_paste_text_with_undo(app->editor, to_insert);
        reset_search(app);
        reported_len = (int)strlen(to_insert);

        if (wrapped)
            free(wrapped);

        ui_status(app, "Pasted %d bytes into body", reported_len);
    }
    else
    {
        wchar_t *w = utf8_to_wcs(utf8, NULL);

        if (w)
        {
            int i, n = 0;

            for (i = 0; w[i] && w[i] != L'\n' && w[i] != L'\r'; i++)
            {
                if (w[i] >= 0x20)
                {
                    msghdr_edit_key(app->edit_hdr, (int)w[i]);
                    n++;
                }
            }

            free(w);

            ui_status(app, "Pasted %d chars into header", n);
        }
    }
}

/* Read characters until KEY_PASTE_END. Returns malloc'd UTF-8 buffer or NULL. Keeps tabs, line feeds, printable; drops other control codes */
char *collect_bracketed_paste()
{
    wchar_t *wbuf = NULL;
    int wlen = 0, wcap = 0;
    char *out;

    for (;;)
    {
        wint_t wch;
        int wrc = wrapper_read_key(&wch);

        if (wrc == ERR)
            break;

        if (wrc == KEY_CODE_YES && (int)wch == KEY_PASTE_END)
            break;

        if (wrc == KEY_CODE_YES)
            continue; /* ignore stray special keys during paste */

        if (wch != L'\n' && wch != L'\t' && wch < 0x20)
            continue;

        if (wlen + 1 >= wcap)
        {
            int ncap = wcap ? wcap * 2 : 256;
            wchar_t *nb = (wchar_t *)realloc(wbuf, (size_t)ncap * sizeof(wchar_t));

            if (!nb)
            {
                free(wbuf);
                return NULL;
            }

            wbuf = nb;
            wcap = ncap;
        }

        wbuf[wlen++] = (wchar_t)wch;
    }

    if (!wbuf || wlen == 0)
    {
        free(wbuf);
        return NULL;
    }

    out = wcs_to_utf8(wbuf, wlen);
    free(wbuf);

    if (!out)
        return NULL;

    return out;
}

/* Detect rapid paste (fallback for terminals without bracketed paste support) */
char *collect_rapid_paste(wint_t first_wch)
{
    wchar_t *wbuf = NULL;
    int wlen = 0, wcap = 0;
    char *out;
    const int MAX_CHARS = 10; /* If 10+ chars arrive instantly, it's a paste not manual typing */

    /* Check if more characters are available (rapid paste detection) */
    nodelay(stdscr, TRUE);

    wint_t next_wch;
    int next_wrc = get_wch(&next_wch);

    /* No more characters: not a paste, return NULL so caller handles single char */
    if (next_wrc == ERR)
    {
        nodelay(stdscr, FALSE);
        return NULL;
    }

    /* Special key: not a paste */
    if (next_wrc == KEY_CODE_YES)
    {
        nodelay(stdscr, FALSE);
        return NULL;
    }

    /* At least one more char available - check for 2 more to confirm paste */
    wint_t third_wch;
    int third_wrc = get_wch(&third_wch);

    if (third_wrc == ERR || third_wrc == KEY_CODE_YES)
    {
        /* Only 2 chars total, probably manual typing - push back the second char */
        nodelay(stdscr, FALSE);
        ungetch((int)next_wch);
        return NULL;
    }

    /* We have at least 3 chars - this is a paste, collect them all */
    wbuf = (wchar_t *)malloc(256 * sizeof(wchar_t));

    if (!wbuf)
    {
        nodelay(stdscr, FALSE);
        return NULL;
    }

    wcap = 256;
    wbuf[wlen++] = (wchar_t)first_wch;
    wbuf[wlen++] = (wchar_t)next_wch;
    wbuf[wlen++] = (wchar_t)third_wch;

    /* Continue collecting remaining characters */
    while (wlen < MAX_CHARS)
    {
        wint_t more_wch;
        int more_wrc = get_wch(&more_wch);

        /* No more characters: end of rapid paste */
        if (more_wrc == ERR)
            break;

        /* Special key: not a paste */
        if (more_wrc == KEY_CODE_YES)
            break;

        /* Add to buffer */
        if (wlen + 1 >= wcap)
        {
            int ncap = wcap * 2;
            wchar_t *nb = (wchar_t *)realloc(wbuf, (size_t)ncap * sizeof(wchar_t));

            if (!nb)
            {
                free(wbuf);
                nodelay(stdscr, FALSE);
                return NULL;
            }

            wbuf = nb;
            wcap = ncap;
        }

        wbuf[wlen++] = (wchar_t)more_wch;
    }

    nodelay(stdscr, FALSE);

    /* Convert to UTF-8 */
    out = wcs_to_utf8(wbuf, wlen);
    free(wbuf);

    return out;
}
