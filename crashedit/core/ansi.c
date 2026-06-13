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

/* ansi.c -- ANSI escape sequence renderer (mini terminal emulator)
 *
 * References:
 * https://gist.github.com/P-Y-R-O-B-O-T/17028c4837d45821f46e94917c7e8903
 * https://en.wikipedia.org/wiki/ANSI_escape_code
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "ansi.h"
#include "utf8.h"

#ifdef PLATFORM_AMIGA
#include "../ncursesw_amiga.h"
#elif defined(PLATFORM_WIN32)
#include "../ncursesw_win32.h"
#else
#include <ncurses.h>
#endif

#define ESC 0x1B

/* Initial geometry, canvas grows on demand */
#define INIT_ROWS 32
#define INIT_ROW_CAP 80

/* Per-CSI sequence parameter cap */
#define MAX_CSI_PARAMS 16

/* Defaults matching BBS terminal assumptions */
#define DEFAULT_FG 7
#define DEFAULT_BG 0

/* Mini state machine */
typedef struct
{
    int row; /* 0-indexed */
    int col; /* 0-indexed */
    int saved_row;
    int saved_col;
    int has_saved;
    int fg; /* 0..7 */
    int bg; /* 0..7 */
    int bold;
    int reverse;
    int max_cols;

    /* ANSI escape sequence tracking */
    int in_escape;
    char seq_buf[64];
    int seq_len;
    int seq_type;
} TermState;

/* Reset escape sequence buffer */
static void reset_escape(TermState *st)
{
    st->in_escape = 0;
    st->seq_len = 0;
    st->seq_type = 0;
}

/* AnsiCanvas helpers */
static AnsiCanvas *canvas_new()
{
    AnsiCanvas *cv = (AnsiCanvas *)calloc(1, sizeof(*cv));

    if (!cv)
        return NULL;

    cv->row_cap = INIT_ROWS;
    cv->rows = (AnsiRow *)calloc((size_t)cv->row_cap, sizeof(AnsiRow));

    if (!cv->rows)
    {
        free(cv);
        return NULL;
    }

    return cv;
}

void ansi_canvas_free(AnsiCanvas *cv)
{
    int i;

    if (!cv)
        return;

    for (i = 0; i < cv->row_count; i++)
    {
        free(cv->rows[i].wcs);
        free(cv->rows[i].cells);
    }

    free(cv->rows);
    free(cv);
}

/* Ensure cv->rows has space for row r. Grows row_count as needed */
static int canvas_ensure_row(AnsiCanvas *cv, int r)
{
    if (r < 0)
        return -1;

    if (r >= cv->row_cap)
    {
        int new_cap = cv->row_cap > 0 ? cv->row_cap : INIT_ROWS;
        AnsiRow *nr;

        while (new_cap <= r)
            new_cap *= 2;

        nr = (AnsiRow *)realloc(cv->rows, (size_t)new_cap * sizeof(AnsiRow));

        if (!nr)
            return -1;

        /* Zero newly-acquired slots for safe free() */
        memset(&nr[cv->row_cap], 0, (size_t)(new_cap - cv->row_cap) * sizeof(AnsiRow));

        cv->rows = nr;
        cv->row_cap = new_cap;
    }

    while (cv->row_count <= r)
    {
        cv->rows[cv->row_count].wcs = NULL;
        cv->rows[cv->row_count].cells = NULL;
        cv->rows[cv->row_count].len = 0;
        cv->rows[cv->row_count].cap = 0;
        cv->row_count++;
    }

    return 0;
}

/* Ensure row r has at least need cells. Returns 0 on success, -1 on OOM */
static int row_ensure_cap(AnsiRow *r, int need)
{
    int new_cap;
    int old_cap;
    wchar_t *nw;
    AnsiCell *nc;
    int i;

    if (r->cap >= need)
        return 0;

    old_cap = r->cap;
    new_cap = r->cap > 0 ? r->cap : INIT_ROW_CAP;

    while (new_cap < need)
        new_cap *= 2;

    nw = (wchar_t *)realloc(r->wcs, (size_t)new_cap * sizeof(wchar_t));

    if (!nw)
        return -1;

    r->wcs = nw;

    nc = (AnsiCell *)realloc(r->cells, (size_t)new_cap * sizeof(AnsiCell));

    if (!nc)
    {
        /* cells realloc failed: shrink wcs to keep buffers consistent */
        wchar_t *shrink = (wchar_t *)realloc(r->wcs, (size_t)old_cap * sizeof(wchar_t));

        if (shrink || old_cap == 0)
            r->wcs = shrink;

        return -1;
    }

    r->cells = nc;

    /* Initialize newly allocated cells to avoid garbage */
    for (i = old_cap; i < new_cap; i++)
    {
        r->wcs[i] = L' ';
        r->cells[i].color_pair = ANSI_PAIR(DEFAULT_FG, DEFAULT_BG);
        r->cells[i].attrs = 0;
    }

    r->cap = new_cap;

    return 0;
}

/* Pad row with blank cells up to column for cursor gaps */
static int row_pad_to(AnsiRow *r, int col)
{
    AnsiCell blank;
    int i;

    if (r->len >= col)
        return 0;

    if (row_ensure_cap(r, col) != 0)
        return -1;

    blank.color_pair = ANSI_PAIR(DEFAULT_FG, DEFAULT_BG);
    blank.attrs = 0;

    for (i = r->len; i < col; i++)
    {
        r->wcs[i] = L' ';
        r->cells[i] = blank;
    }

    r->len = col;
    return 0;
}

/* Place glyph at cursor position, advance column clamped to max_cols */
static int term_putch(AnsiCanvas *cv, TermState *st, wchar_t ch)
{
    AnsiRow *r;
    AnsiCell c;

    if (st->row < 0)
        st->row = 0;

    if (st->col < 0)
        st->col = 0;

    /* Create new line if cannot advance further right */
    if (st->col >= st->max_cols)
    {
        st->col = 0;
        st->row++;
        canvas_ensure_row(cv, st->row);
    }

    if (canvas_ensure_row(cv, st->row) != 0)
        return -1;

    r = &cv->rows[st->row];

    if (row_pad_to(r, st->col) != 0)
        return -1;

    if (row_ensure_cap(r, st->col + 1) != 0)
        return -1;

    /* Swap fg/bg for color pair when reverse active */
    if (st->reverse)
        c.color_pair = ANSI_PAIR(st->bg, st->fg);
    else
        c.color_pair = ANSI_PAIR(st->fg, st->bg);

    c.attrs = 0;

    if (st->bold)
        c.attrs |= A_BOLD;

    r->wcs[st->col] = ch;
    r->cells[st->col] = c;

    r->len = (r->len > st->col + 1) ? r->len : st->col + 1;

    st->col++;
    return 0;
}

/* SGR (color/bold/reverse) handling */
int ansi_color_to_ncurses(int ansi_color)
{
    if (ansi_color >= 30 && ansi_color <= 37)
        return ansi_color - 30;

    if (ansi_color >= 40 && ansi_color <= 47)
        return ansi_color - 40;

    if (ansi_color >= 90 && ansi_color <= 97)
        return ansi_color - 90;

    if (ansi_color >= 100 && ansi_color <= 107)
        return ansi_color - 100;

    return 7;
}

static void apply_sgr(TermState *st, int n)
{
    if (n == 0)
    {
        /* Reset to defaults */
        st->fg = DEFAULT_FG;
        st->bg = DEFAULT_BG;
        st->bold = 0;
        st->reverse = 0;
        return;
    }

    if (n == 1)
    {
        st->bold = 1;
        return;
    }

    if (n == 7)
    {
        st->reverse = 1;
        return;
    }

    if (n == 22)
    {
        st->bold = 0;
        return;
    }

    if (n == 27)
    {
        st->reverse = 0;
        return;
    }

    if (n >= 30 && n <= 37)
    {
        st->fg = n - 30;
        return;
    }

    if (n == 39)
    {
        st->fg = DEFAULT_FG;
        return;
    }

    if (n >= 40 && n <= 47)
    {
        st->bg = n - 40;
        return;
    }

    if (n == 49)
    {
        st->bg = DEFAULT_BG;
        return;
    }

    /* Use bold to simulate brighter color */
    if (n >= 90 && n <= 97)
    {
        st->fg = n - 90;
        st->bold = 1;
        return;
    }

    if (n >= 100 && n <= 107)
    {
        st->bg = n - 100;
        return;
    }
}

/* CSI parser */
/* Parse CSI sequence at p (after ESC), returns bytes consumed or 0 on error */
static int parse_csi(const char *p, int max_len, int params[MAX_CSI_PARAMS], int *n_params, int *priv, int *final)
{
    int i = 1; /* skip '[' */
    int num = 0;
    int has_digit = 0;

    *n_params = 0;
    *priv = 0;
    *final = 0;

    if (max_len < 2)
        return 0;

    /* Detect private mode prefixes */
    if (i < max_len && (p[i] == '<' || p[i] == '=' || p[i] == '>' || p[i] == '?'))
    {
        *priv = 1;
        i++;
    }

    while (i < max_len)
    {
        unsigned char c = (unsigned char)p[i];

        if (c >= '0' && c <= '9')
        {
            num = num * 10 + (c - '0');
            has_digit = 1;
            i++;
            continue;
        }

        if (c == ';')
        {
            if (*n_params < MAX_CSI_PARAMS)
                params[(*n_params)++] = has_digit ? num : 0;

            num = 0;
            has_digit = 0;
            i++;

            continue;
        }

        /* CSI parameter range */
        /* Valid CSI body characters not handled above */
        if (c >= 0x30 && c <= 0x3F)
        {
            i++;
            continue;
        }

        /* Final byte range */
        if (c >= 0x40 && c <= 0x7E)
        {
            if (has_digit && *n_params < MAX_CSI_PARAMS)
                params[(*n_params)++] = num;
            else if (!has_digit && *n_params > 0 && *n_params < MAX_CSI_PARAMS)
            {
                /* Trailing semicolon with empty final param */
                params[(*n_params)++] = 0;
            }

            *final = (int)c;

            return i + 1;
        }

        /* Stray byte inside CSI: treat as end-of-sequence */
        return 0;
    }

    /* Ran off end without seeing final byte */
    return 0;
}

/* Default param value when absent */
static int csi_param(const int *params, int n_params, int idx, int dflt)
{
    if (idx >= n_params)
        return dflt;

    if (params[idx] == 0)
        return dflt;

    return params[idx];
}

/* Apply fully-parsed CSI sequence to terminal state and canvas */
static void apply_csi(AnsiCanvas *cv, TermState *st, int *params, int n_params, int priv, int final)
{
    if (priv)
    {
        /* Private mode sequences: consume but don't model */
        return;
    }

    switch (final)
    {
    case 'A': /* cursor up */
        st->row -= csi_param(params, n_params, 0, 1);

        if (st->row < 0)
            st->row = 0;

        canvas_ensure_row(cv, st->row);
        break;

    case 'B': /* cursor down */
        st->row += csi_param(params, n_params, 0, 1);
        canvas_ensure_row(cv, st->row);
        break;

    case 'C': /* cursor right */
        st->col += csi_param(params, n_params, 0, 1);

        if (st->col >= st->max_cols)
            st->col = st->max_cols - 1;

        break;
    case 'D': /* cursor left */
        st->col -= csi_param(params, n_params, 0, 1);

        if (st->col < 0)
            st->col = 0;
        break;

    case 'H': /* CUP: cursor position (1-indexed) */
    case 'f': /* HVP: same */
    {
        int r = csi_param(params, n_params, 0, 1);
        int c = csi_param(params, n_params, 1, 1);

        st->row = r - 1;
        st->col = c - 1;

        if (st->row < 0)
            st->row = 0;

        if (st->col < 0)
            st->col = 0;

        if (st->col > st->max_cols)
            st->col = st->max_cols;

        break;
    }
    case 'J': /* erase display */
    {
        int mode = (n_params > 0) ? params[0] : 0;

        if (mode == 2 || mode == 3)
        {
            /* Clear screen (VT100): drop all rows, cursor unchanged */
            int i;

            for (i = 0; i < cv->row_count; i++)
            {
                free(cv->rows[i].wcs);
                free(cv->rows[i].cells);

                cv->rows[i].wcs = NULL;
                cv->rows[i].cells = NULL;
                cv->rows[i].len = 0;
                cv->rows[i].cap = 0;
            }

            cv->row_count = 0;
        }

        /* Erase modes 0/1: not fully supported */
        break;
    }
    case 'K': /* erase in line */
    {
        int mode = (n_params > 0) ? params[0] : 0;

        if (st->row >= 0 && st->row < cv->row_count)
        {
            AnsiRow *r = &cv->rows[st->row];

            if (mode == 0)
            {
                /* Clear cursor -> end: truncate */
                if (st->col < r->len)
                {
                    r->len = st->col;

                    /* Clear cells beyond new len to avoid stale attributes */
                    if (r->len < r->cap)
                    {
                        int j;

                        for (j = r->len; j < r->cap; j++)
                        {
                            r->wcs[j] = L' ';
                            r->cells[j].color_pair = ANSI_PAIR(DEFAULT_FG, DEFAULT_BG);
                            r->cells[j].attrs = 0;
                        }
                    }
                }
            }
            else if (mode == 1 || mode == 2)
            {
                /* Clear start -> cursor (or whole line): blank out */
                int upto = (mode == 2) ? r->len : st->col + 1;
                int j;

                if (upto > r->len)
                    upto = r->len;

                for (j = 0; j < upto; j++)
                {
                    r->wcs[j] = L' ';
                    r->cells[j].color_pair = ANSI_PAIR(DEFAULT_FG, DEFAULT_BG);
                    r->cells[j].attrs = 0;
                }
            }
        }
        break;
    }
    case 's': /* save cursor */
        st->saved_row = st->row;
        st->saved_col = st->col;
        st->has_saved = 1;
        break;
    case 'u': /* restore cursor */
        if (st->has_saved)
        {
            st->row = st->saved_row;
            st->col = st->saved_col;
        }
        break;
    case 'm': /* SGR */
    {
        int i;

        if (n_params == 0)
        {
            apply_sgr(st, 0); /* ESC[m == ESC[0m */
            break;
        }

        for (i = 0; i < n_params; i++)
            apply_sgr(st, params[i]);

        break;
    }
    default:
        /* Unknown final byte: consume and ignore */
        break;
    }
}

/* Main render loop: wchar_t stream through terminal emulator */
static AnsiCanvas *render_wcs_stream(const wchar_t *all, int all_len, int max_cols)
{
    AnsiCanvas *cv;
    TermState st;
    int i;
    int ok = 1;

    if (!all || all_len <= 0)
        return NULL;

    if (max_cols < 1)
        max_cols = 80;

    cv = canvas_new();
    if (!cv)
        return NULL;

    /* Initialize to fresh terminal */
    memset(&st, 0, sizeof(st));

    st.fg = DEFAULT_FG;
    st.bg = DEFAULT_BG;
    st.max_cols = 80; /* ANSI BBS standard width */

    for (i = 0; ok && i < all_len; i++)
    {
        wchar_t ch = all[i];

        /* Handle escape sequences */
        if (ch == (wchar_t)ESC)
        {
            st.in_escape = 1;
            st.seq_len = 0;
            st.seq_type = 0;
            continue;
        }

        if (st.in_escape)
        {
            /* Non-ASCII characters terminate escape sequence */
            if (ch > 0x7F)
            {
                reset_escape(&st);
                /* Don't continue - render as text */
            }
            else
            {
                /* Determine sequence type from first byte after ESC */
                if (st.seq_len == 0)
                {
                    if (ch == L'[')
                    {
                        st.seq_type = 0; /* CSI */

                        /* Add '[' to buffer */
                        if (st.seq_len < (int)sizeof(st.seq_buf) - 1)
                            st.seq_buf[st.seq_len++] = (char)ch;

                        continue; /* Skip to next character */
                    }
                    else if (ch == L']' || ch == L'P' || ch == L'^' || ch == L'_')
                    {
                        st.seq_type = 1; /* OSC/DCS/PM/APS string */

                        /* Add to buffer */
                        if (st.seq_len < (int)sizeof(st.seq_buf) - 1)
                            st.seq_buf[st.seq_len++] = (char)ch;

                        continue; /* Skip to next character */
                    }
                    else if (ch == L'X')
                    {
                        st.seq_type = 2; /* SOS */
                        /* Add to buffer */

                        if (st.seq_len < (int)sizeof(st.seq_buf) - 1)
                            st.seq_buf[st.seq_len++] = (char)ch;

                        continue; /* Skip to next character */
                    }
                    else if (ch >= 0x40 && ch <= 0x7E)
                    {
                        /* Simple 2-character escape sequence */
                        reset_escape(&st);
                        continue;
                    }
                    else
                    {
                        /* Unknown or malformed, drop it */
                        reset_escape(&st);
                        continue;
                    }
                }

                /* Check for sequence termination BEFORE adding to buffer */
                if (st.seq_type == 0) /* CSI */
                {
                    /* CSI ends with final byte 0x40-0x7E (not at position 0) */
                    if (st.seq_len > 0 && ch >= 0x40 && ch <= 0x7E)
                    {
                        /* Add final byte to buffer */
                        if (st.seq_len < (int)sizeof(st.seq_buf) - 1)
                        {
                            st.seq_buf[st.seq_len++] = (char)ch;
                            st.seq_buf[st.seq_len] = '\0';
                        }

                        /* Parse and apply CSI */
                        int params[MAX_CSI_PARAMS];
                        int n_params, priv, final;
                        int consumed = parse_csi(st.seq_buf, st.seq_len, params, &n_params, &priv, &final);

                        if (consumed > 0)
                            apply_csi(cv, &st, params, n_params, priv, final);

                        reset_escape(&st);
                        continue; /* Sequence complete, skip to next char */
                    }
                    /* Accumulate all other CSI body characters */
                    else if (st.seq_len < (int)sizeof(st.seq_buf) - 1)
                    {
                        st.seq_buf[st.seq_len++] = (char)ch;
                    }
                    else
                    {
                        /* Buffer overflow: abort sequence */
                        reset_escape(&st);
                    }
                }
                else if (st.seq_type == 1) /* String sequences (OSC/DCS/PM/APS) */
                {
                    /* These end with ST (ESC \) */
                    if (ch == (wchar_t)ESC)
                    {
                        /* Mark that we saw ESC, wait for backslash */
                        st.seq_type = 3; /* Temporary state: waiting for ST backslash */
                    }
                    else if (ch < 0x20 || ch > 0x7E)
                    {
                        /* Invalid char in string, abort */
                        reset_escape(&st);
                    }
                    else if (st.seq_len < (int)sizeof(st.seq_buf) - 1)
                    {
                        st.seq_buf[st.seq_len++] = (char)ch;
                    }
                    else
                    {
                        /* Buffer overflow: abort sequence and treat as text */
                        reset_escape(&st);
                    }

                    continue;
                }
                else if (st.seq_type == 2) /* SOS */
                {
                    /* SOS ends with ST (ESC \) */
                    if (ch == (wchar_t)ESC)
                    {
                        st.seq_type = 3; /* Temporary state: waiting for ST backslash */
                    }
                    else if (st.seq_len < (int)sizeof(st.seq_buf) - 1)
                    {
                        st.seq_buf[st.seq_len++] = (char)ch;
                    }
                    else
                    {
                        /* Buffer overflow: abort sequence and treat as text */
                        reset_escape(&st);
                    }

                    continue;
                }
                else if (st.seq_type == 3) /* Waiting for ST backslash */
                {
                    if (ch == L'\\')
                    {
                        /* Complete ST sequence */
                        reset_escape(&st);
                    }
                    else
                    {
                        /* ESC not followed by backslash, malformed */
                        reset_escape(&st);
                    }

                    continue;
                }

                continue;
            }
        }

        /* CR: col=0, row++ (JAM uses bare CR as line separator). LF: row++ */
        if (ch == L'\r')
        {
            st.col = 0;
            st.row++;
            canvas_ensure_row(cv, st.row);
            continue;
        }

        if (ch == L'\n')
        {
            st.row++;
            canvas_ensure_row(cv, st.row);
            continue;
        }

        if (ch == L'\b')
        {
            if (st.col > 0)
                st.col--;

            continue;
        }

        if (ch == L'\t')
        {
            st.col = ((st.col / 8) + 1) * 8;

            if (st.col > st.max_cols)
                st.col = st.max_cols;

            continue;
        }

        if (ch < 0x20)
            continue; /* Skip remaining controls (BEL, SO, etc) */

        if (term_putch(cv, &st, ch) != 0)
        {
            ok = 0;
            break;
        }
    }

    if (!ok)
    {
        ansi_canvas_free(cv);
        return NULL;
    }

    /* Check for incomplete ANSI sequence at end of input */
    if (st.in_escape)
    {
        /* Cut-off sequence: reset and continue (safe to ignore) */
        reset_escape(&st);
    }

    return cv;
}

AnsiCanvas *ansi_render_bytes(const char *bytes, int len, const char *charset, int max_cols)
{
    wchar_t *stream;
    int stream_len = 0;
    int is_utf8;
    AnsiCanvas *cv;

    if (!bytes || len <= 0)
        return NULL;

    is_utf8 = (charset && (strcasecmp(charset, "UTF-8") == 0 || strcasecmp(charset, "UTF8") == 0));

    if (is_utf8)
    {
        /* UTF-8 needs decode (bytes != chars), make NUL-terminated copy */
        char *zterm = (char *)malloc((size_t)len + 1);

        if (!zterm)
            return NULL;

        memcpy(zterm, bytes, (size_t)len);
        zterm[len] = '\0';

        stream = utf8_to_wcs(zterm, &stream_len);
        free(zterm);

        if (!stream)
            return NULL;
    }
    else
    {
        /* Single-byte charset: byte == wchar_t, preserves ANSI cursor arithmetic */
        int i;

        stream = (wchar_t *)malloc((size_t)len * sizeof(wchar_t));
        if (!stream)
            return NULL;

        for (i = 0; i < len; i++)
        {
            unsigned char b = (unsigned char)bytes[i];
            stream[i] = (wchar_t)charset_byte_to_unicode(charset, b);
        }

        stream_len = len;
    }

    cv = render_wcs_stream(stream, stream_len, 80); /* Force 80 cols, ignore param */
    free(stream);

    return cv;
}

AnsiCanvas *ansi_render_utf8(const char *utf8, int utf8_len, int max_cols)
{
    return ansi_render_bytes(utf8, utf8_len, "UTF-8", max_cols);
}
