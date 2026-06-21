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

#include "ui_editor_paste.h"
#include "ui.h"
#include "ui_internal.h"
#include "ui_editor_internal.h"
#include "ui_editor_helper.h"
#include "ui_editor_search.h"
#ifdef HAVE_HYPHEN
#include "../core/hyph.h"
#endif
#include "../core/charset.h"
#include "../core/msghdr.h"
#include "../core/utf8.h"
#include "../components/editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* Word-wrap UTF-8 paste to col columns */
int paste_char_width(wchar_t c)
{
    /* Zero-width chars: combining marks, BOM, control */
    if (c == 0)
        return 0;

    if (c < 0x20)
        return 0; /* control chars */

    if (c >= 0x0300 && c <= 0x036F)
        return 0; /* combining marks */

    if (c == 0x200B || c == 0xFEFF)
        return 0; /* ZWSP / BOM */

    return 1;
}

/* Word-wrap UTF-8 paste (space-only variant) */
char *wrap_paste_text(const char *utf8, int col)
{
    return wrap_paste_text_ex(utf8, col, NULL, NULL);
}

/* Word-wrap UTF-8 paste with hyphenation support */
char *wrap_paste_text_ex(const char *utf8, int col, PasteHyphFn hyph, void *hyph_data)
{
    wchar_t *w = NULL;
    int wlen = 0;
    int i;
    int line_start = 0;
    int last_space = -1;
    int col_pos = 0;
    int out_cap, out_len = 0;
    wchar_t *out = NULL;
    char *result = NULL;

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
            else
            {
                /* Buffer full: expand */
                int new_cap = out_cap * 2;
                wchar_t *new_out = (wchar_t *)realloc(out, (size_t)new_cap * sizeof(wchar_t));

                if (!new_out)
                    break;

                out = new_out;
                out_cap = new_cap;
                out[out_len++] = L'\n';
            }

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
        else
        {
            int new_cap = out_cap * 2;
            wchar_t *new_out = (wchar_t *)realloc(out, (size_t)new_cap * sizeof(wchar_t));

            if (!new_out)
                break;

            out = new_out;
            out_cap = new_cap;
            out[out_len++] = ch;
        }

        if (ch == L' ' || ch == L'\t')
            last_space = out_len - 1;

        col_pos += cw;

        if (col_pos > col)
        {
            int new_start;
            int j;
            int width_after = 0;
            int break_found = 0;

            /* Try hyphenation break first */
            if (hyph)
            {
                int word_start = (last_space >= 0) ? last_space + 1 : line_start;
                int word_end = out_len;
                int word_wlen = word_end - word_start;

                if (word_wlen > 3 && word_wlen < 512)
                {
                    char *utf8_word = wcs_to_utf8(&out[word_start], word_wlen);

                    if (utf8_word)
                    {
                        int utf8_len = (int)strlen(utf8_word);
#ifdef HAVE_HYPHEN
                        int hp[HYPH_MAX_BREAKS];
                        int hn = HYPH_MAX_BREAKS;
#else
                        int hp[64];
                        int hn = 64;
#endif
                        int k;

                        if (hyph(hyph_data, utf8_word, utf8_len, hp, &hn) && hn > 0)
                        {
                            /* Find rightmost hyphenation point that fits */
                            for (k = hn - 1; k >= 0; k--)
                            {
                                int char_off = utf8_charcount(utf8_word, hp[k]);
                                int break_at = word_start + char_off;
                                int break_col = 0;
                                int m;

                                if (break_at <= word_start || break_at >= word_end)
                                    continue;

                                if (break_at <= line_start)
                                    continue;

                                for (m = line_start; m < break_at; m++)
                                    break_col += (out[m] == L'\t') ? 1 : paste_char_width(out[m]);

                                /* Reserve column for '-' */
                                if (break_col > col - 1)
                                    continue;

                                /* Need room for '-' and '\n' */
                                if (out_len + 2 > out_cap)
                                {
                                    int new_cap = (out_cap + 64) * 2;
                                    wchar_t *new_out = (wchar_t *)realloc(out, (size_t)new_cap * sizeof(wchar_t));

                                    if (!new_out)
                                        continue; /* try next hyph point */

                                    out = new_out;
                                    out_cap = new_cap;
                                }

                                if (out_len + 2 > out_cap)
                                    break; /* no room */

                                /* Shift [break_at..out_len) right by 2 */
                                for (m = out_len - 1; m >= break_at; m--)
                                    out[m + 2] = out[m];

                                out[break_at] = L'-';
                                out[break_at + 1] = L'\n';

                                out_len += 2;
                                new_start = break_at + 2;
                                break_found = 1;

                                width_after = 0;

                                for (m = new_start; m < out_len; m++)
                                    width_after += (out[m] == L'\t') ? 1 : paste_char_width(out[m]);

                                line_start = new_start;
                                last_space = -1;
                                col_pos = width_after;
                                break;
                            }
                        }

                        free(utf8_word);
                    }
                }
            }

            if (!break_found && last_space > line_start)
            {
                /* Space-based break */
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
            else if (!break_found)
            {
                /* No space, no hyphenation: hard cut */
                if (out_len + 1 >= out_cap)
                {
                    int new_cap = (out_cap + 64) * 2;
                    wchar_t *new_out = (wchar_t *)realloc(out, (size_t)new_cap * sizeof(wchar_t));

                    if (new_out)
                    {
                        out = new_out;
                        out_cap = new_cap;
                    }
                }

                if (out_len + 1 < out_cap)
                {
                    out[out_len++] = L'\n';

                    line_start = out_len;
                    last_space = -1;
                    col_pos = 0;
                }
            }
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

/* Adapter: HyphDict callback for PasteHyphFn */
#ifdef HAVE_HYPHEN
#include "../core/hyph.h"

static int paste_hyph_thunk(void *user_data, const char *word, int word_len, int *out_pos, int *out_count)
{
    HyphDict *h = (HyphDict *)user_data;
    int n;

    if (!out_count)
        return 0;

    n = 0;

    if (!hyph_breakpoints(h, word, word_len, out_pos, &n) || n == 0)
    {
        *out_count = 0;
        return 0;
    }

    *out_count = n;
    return 1;
}
#endif

/* Paste UTF-8 buffer at cursor */
void deliver_paste(UiApp *app, const char *utf8)
{
    if (!utf8 || !utf8[0])
        return;

    if (app->edit_active_field == EF_BODY)
    {
        char *wrapped = NULL;
        const char *to_insert = utf8;
        int reported_len;

        /* HARD-WRAP: reflow pasted text */
        if (app->cfg && app->cfg->hard_wrap)
        {
            int pw = editor_eff_wrap(app);

            if (pw > 0)
            {
#ifdef HAVE_HYPHEN
                /* Use hyphenation if enabled */
                if (app->hyph_wrap_enabled && app->hyph_handle)
                    wrapped = wrap_paste_text_ex(utf8, pw, paste_hyph_thunk, app->hyph_handle);
                else
                    wrapped = wrap_paste_text(utf8, pw);
#else
                wrapped = wrap_paste_text(utf8, pw);
#endif

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

/* Read characters until KEY_PASTE_END */
char *collect_bracketed_paste()
{
    wchar_t *wbuf = NULL;
    int wlen = 0, wcap = 0;
    char *out = NULL;

    for (;;)
    {
        wint_t wch;
        int wrc = wrapper_read_key(&wch);

        if (wrc == ERR)
            break;

        if (wrc == KEY_CODE_YES && (int)wch == KEY_PASTE_END)
            break;

        if (wrc == KEY_CODE_YES)
            continue; /* ignore special keys */

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

/* Detect rapid paste (fallback for no bracketed paste) */
char *collect_rapid_paste(wint_t first_wch)
{
    wchar_t *wbuf = NULL;
    int wlen = 0, wcap = 0;
    char *out = NULL;
    const int MAX_CHARS = 10; /* 10+ chars = paste */
    wint_t next_wch;
    wint_t third_wch;
    int next_wrc;
    int third_wrc;

    /* Check if more chars available */
    nodelay(stdscr, TRUE);

    next_wrc = get_wch(&next_wch);

    /* No more chars: not a paste */
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

    /* Check for 2 more to confirm paste */
    third_wrc = get_wch(&third_wch);

    if (third_wrc == ERR || third_wrc == KEY_CODE_YES)
    {
        /* Only 2 chars: manual typing */
        nodelay(stdscr, FALSE);
        ungetch((int)next_wch);
        return NULL;
    }

    /* 3+ chars: this is a paste */
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

    /* Continue collecting */
    while (wlen < MAX_CHARS)
    {
        wint_t more_wch;
        int more_wrc = get_wch(&more_wch);

        /* No more chars: end of paste */
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
