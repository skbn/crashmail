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

/* reader.c -- Message reader with wchar_t internal representation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "reader.h"
#include "../core/ftn.h"
#include "../core/utf8.h"
#include "../core/ansi.h"
#include "../core/charset.h"
#include "../ui/ui_editor_softwrap.h"

/* One display line: single color for non-ANSI, per-cell attrs for ANSI */
typedef struct
{
    wchar_t *wcs;    /* malloc'd, NUL-terminated */
    int len;         /* character count */
    int type;        /* FTN_LT_* */
    int ansi_color;  /* ncurses color pair if ANSI, -1 otherwise */
    int ansi_attrs;  /* ncurses attributes (A_BOLD, etc.) */
    AnsiCell *cells; /* per-character attrs, len entries, or NULL */
} RdLine;

struct Reader
{
    RdLine *lines;
    int count, alloc;
    int top, page;
    int show_klg;
    int show_via;
    int show_hidden;
    int show_ansi; /* 0 = show ANSI codes as text, 1 = render ANSI colors */
    int *vis;
    int vis_count;
};

#define INIT_ALLOC 256

static int rd_grow(Reader *rd)
{
    int na;
    RdLine *t = NULL;

    if (rd->count < rd->alloc)
        return 0;

    na = rd->alloc > 0 ? rd->alloc * 2 : INIT_ALLOC;
    t = (RdLine *)realloc(rd->lines, (size_t)na * sizeof(RdLine));

    if (!t)
        return -1;

    rd->lines = t;
    rd->alloc = na;

    return 0;
}

static int rd_add(Reader *rd, const wchar_t *wcs, int len, int type, int ansi_color, int ansi_attrs, const AnsiCell *cells)
{
    RdLine *ln = NULL;

    if (rd_grow(rd) != 0)
        return -1;

    ln = &rd->lines[rd->count];
    ln->wcs = (wchar_t *)malloc((size_t)(len + 1) * sizeof(wchar_t));

    if (!ln->wcs)
        return -1;

    if (len > 0)
        wmemcpy(ln->wcs, wcs, (size_t)len);

    ln->wcs[len] = L'\0';
    ln->len = len;
    ln->type = type;
    ln->ansi_color = ansi_color;
    ln->ansi_attrs = ansi_attrs;
    ln->cells = NULL;

    if (cells && len > 0)
    {
        ln->cells = (AnsiCell *)malloc((size_t)len * sizeof(AnsiCell));

        if (!ln->cells)
        {
            free(ln->wcs);
            return -1;
        }

        memcpy(ln->cells, cells, (size_t)len * sizeof(AnsiCell));
    }

    rd->count++;

    return 0;
}

static void rd_clear(Reader *rd)
{
    int i;

    for (i = 0; i < rd->count; i++)
    {
        free(rd->lines[i].wcs);
        free(rd->lines[i].cells);
    }

    rd->count = 0;
}

static void rebuild_vis(Reader *rd)
{
    int i, n = 0;
    int in_kludges = 0;
    int in_normal = 0;
    int cap;

    free(rd->vis);

    /* Worst case: separator between every line */
    cap = rd->count * 2 + 4;
    rd->vis = (int *)malloc((size_t)cap * sizeof(int));

    if (!rd->vis)
    {
        rd->vis_count = 0;
        return;
    }

    for (i = 0; i < rd->count; i++)
    {
        int type = rd->lines[i].type;
        int is_kludge = (type & FTN_LT_KLUDGE);
        int is_seenby = (type & (FTN_LT_SEENBY | FTN_LT_VIA | FTN_LT_PATH));
        int is_normal = !is_kludge && !is_seenby;

        if (!rd->show_klg && is_kludge)
            continue;

        if (!rd->show_via && is_seenby)
            continue;

        /* Add separator after kludges when transitioning to normal */
        if (in_kludges && is_normal && rd->show_klg)
        {
            rd->vis[n++] = -1; /* -1 = blank separator line */
            in_kludges = 0;
        }

        /* Add separator before seenby/via/path when transitioning from normal */
        if (in_normal && is_seenby && rd->show_via)
        {
            rd->vis[n++] = -1; /* -1 = blank separator line */
            in_normal = 0;
        }

        rd->vis[n++] = i;

        if (is_kludge && rd->show_klg)
            in_kludges = 1;

        if (is_normal)
            in_normal = 1;
    }

    rd->vis_count = n;

    if (rd->top >= rd->vis_count)
    {
        rd->top = rd->vis_count - rd->page;

        if (rd->top < 0)
            rd->top = 0;
    }
}

/* Word wrap in wchar_t. Non-ANSI only. Uses editor's wrap_next for consistency */
static void wrap_line(Reader *rd, const wchar_t *wcs, int len, int ww, int type, int ansi_color, int ansi_attrs)
{
    int pos = 0;

    if (ww < 1)
        ww = 1;

    if (len == 0)
    {
        rd_add(rd, L"", 0, type, ansi_color, ansi_attrs, NULL);
        return;
    }

    while (pos < len)
    {
        int end = wrap_next(wcs, len, ww, pos);
        int seg_len = end - pos;

        if (seg_len < 0)
            seg_len = 0;

        rd_add(rd, &wcs[pos], seg_len, type, ansi_color, ansi_attrs, NULL);

        pos = end;

        /* Skip leading space on continuation */
        if (pos < len && wcs[pos] == L' ')
            pos++;
    }
}

Reader *rd_new(int viewkludge, int viewhidden)
{
    Reader *rd = (Reader *)calloc(1, sizeof(Reader));

    if (!rd)
        return NULL;

    rd->page = 25;
    rd->show_klg = viewkludge;
    rd->show_via = viewhidden;
    rd->show_hidden = 0;
    rd->show_ansi = 0; /* Default: show ANSI codes as text */

    return rd;
}

void rd_free(Reader *rd)
{
    if (!rd)
        return;

    rd_clear(rd);

    free(rd->lines);
    free(rd->vis);
    free(rd);
}

/* Convert AnsiCanvas to Reader lines */
static void rd_apply_canvas(Reader *rd, AnsiCanvas *cv)
{
    int i;

    for (i = 0; i < cv->row_count; i++)
    {
        AnsiRow *r = &cv->rows[i];
        int color = -1;
        int attrs = 0;

        if (r->len > 0 && r->cells)
        {
            color = r->cells[0].color_pair;
            attrs = r->cells[0].attrs;
        }

        if (r->len > 0)
            rd_add(rd, r->wcs, r->len, FTN_LT_NORMAL,
                   color, attrs, r->cells);
        else
            rd_add(rd, L"", 0, FTN_LT_NORMAL, -1, 0, NULL);
    }
}

void rd_load_ansi(Reader *rd, const char *raw_bytes, int raw_len, const char *charset, int max_cols)
{
    AnsiCanvas *cv = NULL;

    if (!rd)
        return;

    rd_clear(rd);
    rd->top = 0;

    /* Force ANSI mode ON for display since caller asserts ANSI body */
    rd->show_ansi = 1;

    if (!raw_bytes || raw_len <= 0)
    {
        rd_add(rd, L"", 0, FTN_LT_NORMAL, -1, 0, NULL);
        rebuild_vis(rd);
        return;
    }

    cv = ansi_render_bytes(raw_bytes, raw_len, charset, max_cols);

    if (cv)
    {
        rd_apply_canvas(rd, cv);
        ansi_canvas_free(cv);
    }

    if (rd->count == 0)
        rd_add(rd, L"", 0, FTN_LT_NORMAL, -1, 0, NULL);

    rebuild_vis(rd);
}

void rd_load(Reader *rd, const char *utf8_body, int wrap_width)
{
    const char *p;

    if (!rd)
        return;

    rd_clear(rd);
    rd->top = 0;

    if (!utf8_body || !utf8_body[0])
    {
        rd_add(rd, L"", 0, FTN_LT_NORMAL, -1, 0, NULL);
        rebuild_vis(rd);
        return;
    }

    /* Plain mode: split body, convert to wchar_t, classify, wrap. ANSI as literal text */
    p = utf8_body;

    while (*p)
    {
        const char *start = p;
        int blen, wlen, type;
        char *line_utf8 = NULL;
        wchar_t *wcs = NULL;

        while (*p && *p != '\r' && *p != '\n')
            p++;

        blen = (int)(p - start);

        line_utf8 = (char *)malloc((size_t)(blen + 1));

        if (line_utf8)
        {
            memcpy(line_utf8, start, (size_t)blen);
            line_utf8[blen] = '\0';

            wcs = utf8_to_wcs(line_utf8, &wlen);

            if (wcs)
            {
                type = ftn_classify_wcs(wcs, wlen);
                wrap_line(rd, wcs, wlen, wrap_width, type, -1, 0);
                free(wcs);
            }

            free(line_utf8);
        }

        if (*p == '\r')
            p++;

        if (*p == '\n')
            p++;
    }

    if (rd->count == 0)
        rd_add(rd, L"", 0, FTN_LT_NORMAL, -1, 0, NULL);

    rebuild_vis(rd);
}

/* Scroll */
void rd_set_page(Reader *rd, int v)
{
    if (!rd || v <= 0)
        return;

    rd->page = v;

    /* Clamp top after terminal resize */
    if (rd->top > rd->vis_count - rd->page)
        rd->top = rd->vis_count - rd->page;

    if (rd->top < 0)
        rd->top = 0;
}

void rd_scroll_down(Reader *rd, int n)
{
    if (!rd || n <= 0)
        return;

    rd->top += n;

    if (rd->top > rd->vis_count - rd->page)
        rd->top = rd->vis_count - rd->page;

    if (rd->top < 0)
        rd->top = 0;
}

void rd_scroll_up(Reader *rd, int n)
{
    if (!rd || n <= 0)
        return;

    rd->top -= n;

    if (rd->top < 0)
        rd->top = 0;
}

void rd_page_down(Reader *rd)
{
    if (rd)
        rd_scroll_down(rd, rd->page);
}

void rd_page_up(Reader *rd)
{
    if (rd)
        rd_scroll_up(rd, rd->page);
}

void rd_home(Reader *rd)
{
    if (rd)
        rd->top = 0;
}

void rd_end(Reader *rd)
{
    if (!rd)
        return;

    rd->top = rd->vis_count - rd->page;

    if (rd->top < 0)
        rd->top = 0;
}

/* Kludge toggle */
void rd_toggle_kludges(Reader *rd)
{
    if (!rd)
        return;

    rd->show_klg = !rd->show_klg;

    rebuild_vis(rd);
}

void rd_toggle_vias(Reader *rd)
{
    if (!rd)
        return;

    rd->show_via = !rd->show_via;

    rebuild_vis(rd);
}

void rd_toggle_hidden(Reader *rd)
{
    if (!rd)
        return;

    rd->show_hidden = !rd->show_hidden;

    rebuild_vis(rd);
}

int rd_hidden_visible(const Reader *rd)
{
    return rd ? rd->show_hidden : 0;
}

void rd_toggle_hiddklud(Reader *rd)
{
    if (!rd)
        return;

    rd->show_via = !rd->show_via;

    rebuild_vis(rd);
}

int rd_kludges_visible(const Reader *rd)
{
    return rd ? rd->show_klg : 0;
}

int rd_vias_visible(const Reader *rd)
{
    return rd ? rd->show_via : 0;
}

/* ANSI toggle */
void rd_toggle_ansi(Reader *rd)
{
    if (!rd)
        return;

    rd->show_ansi = !rd->show_ansi;
}

int rd_ansi_visible(const Reader *rd)
{
    return rd ? rd->show_ansi : 0;
}

/* Query */
int rd_total(const Reader *rd)
{
    return rd ? rd->vis_count : 0;
}

int rd_count(const Reader *rd)
{
    return rd ? rd->count : 0;
}

int rd_top(const Reader *rd)
{
    return rd ? rd->top : 0;
}

/* Convert global line index to visible index, -1 if not visible */
int rd_global_to_visible(const Reader *rd, int global_idx)
{
    int i;

    if (!rd || !rd->vis || global_idx < 0)
        return -1;

    for (i = 0; i < rd->vis_count; i++)
    {
        if (rd->vis[i] == global_idx)
            return i;
    }

    return -1; /* not visible */
}

int rd_visible(const Reader *rd)
{
    int a;

    if (!rd)
        return 0;

    a = rd->vis_count - rd->top;

    return a < rd->page ? a : rd->page;
}

int rd_percent(const Reader *rd)
{
    int last_visible;

    if (!rd || rd->vis_count == 0)
        return 100;

    /* Calculate percentage based on last visible line */
    last_visible = rd->top + rd->page;

    /* Clamp to avoid percentage > 100 */
    if (last_visible > rd->vis_count)
        last_visible = rd->vis_count;

    return last_visible * 100 / rd->vis_count;
}

const wchar_t *rd_get_line(const Reader *rd, int vi)
{
    int r;

    if (!rd || !rd->vis || vi < 0 || vi >= rd->vis_count)
        return NULL;

    r = rd->vis[vi];

    if (r < 0 || r >= rd->count)
        return NULL;

    return rd->lines[r].wcs;
}

int rd_get_len(const Reader *rd, int vi)
{
    int r;

    if (!rd || !rd->vis || vi < 0 || vi >= rd->vis_count)
        return -1;

    r = rd->vis[vi];

    if (r < 0 || r >= rd->count)
        return -1;

    return rd->lines[r].len;
}

int rd_get_type(const Reader *rd, int vi)
{
    int r;

    if (!rd || !rd->vis || vi < 0 || vi >= rd->vis_count)
        return FTN_LT_NORMAL;

    r = rd->vis[vi];

    if (r < 0 || r >= rd->count)
        return FTN_LT_NORMAL;

    return rd->lines[r].type;
}

int rd_get_line_idx(const Reader *rd, int vi)
{
    if (!rd || !rd->vis || vi < 0 || vi >= rd->vis_count)
        return -1;

    return rd->vis[vi];
}

int rd_get_ansi_color(const Reader *rd, int vi)
{
    int r;

    if (!rd || !rd->vis || vi < 0 || vi >= rd->vis_count)
        return -1;

    r = rd->vis[vi];

    if (r < 0 || r >= rd->count)
        return -1;

    return rd->lines[r].ansi_color;
}

int rd_get_ansi_attrs(const Reader *rd, int vi)
{
    int r;

    if (!rd || !rd->vis || vi < 0 || vi >= rd->vis_count)
        return 0;

    r = rd->vis[vi];

    if (r < 0 || r >= rd->count)
        return 0;

    return rd->lines[r].ansi_attrs;
}

const struct AnsiCell *rd_get_ansi_cells(const Reader *rd, int vi)
{
    int r;

    if (!rd || !rd->vis || vi < 0 || vi >= rd->vis_count)
        return NULL;

    r = rd->vis[vi];

    if (r < 0 || r >= rd->count)
        return NULL;

    return (const struct AnsiCell *)rd->lines[r].cells;
}

int rd_line_utf8(const Reader *rd, int vi, char *buf, int bufsz)
{
    const wchar_t *wcs;
    int r;
    int i, n;
    unsigned long cp;
    int wlen;

    if (!rd || !buf || bufsz <= 0)
    {
        if (buf && bufsz > 0)
            buf[0] = '\0';

        return -1;
    }

    if (!rd->vis || vi < 0 || vi >= rd->vis_count)
    {
        buf[0] = '\0';
        return -1;
    }

    r = rd->vis[vi];

    if (r < 0 || r >= rd->count)
    {
        buf[0] = '\0';
        return -1;
    }

    wcs = rd->lines[r].wcs;

    wlen = rd->lines[r].len;

    /* Hide leading SOH (^A) byte from kludge lines */
    if (wlen > 0 && wcs[0] == 0x01)
    {
        wcs++;
        wlen--;
    }

    /* Fast path: convert directly to buf for redraws */
    n = 0;

    for (i = 0; i < wlen && n < bufsz - 4; i++)
    {
        cp = (unsigned long)wcs[i];

        if (cp < 0x80)
            buf[n++] = (char)cp;

        else if (cp < 0x800)
        {
            buf[n++] = (char)(0xC0 | (cp >> 6));
            buf[n++] = (char)(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            buf[n++] = (char)(0xE0 | (cp >> 12));
            buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            buf[n++] = (char)(0x80 | (cp & 0x3F));
        }
        else
        {
            buf[n++] = (char)(0xF0 | (cp >> 18));
            buf[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            buf[n++] = (char)(0x80 | (cp & 0x3F));
        }
    }

    buf[n] = '\0';

    return n;
}

/* Write message to file with wide reader to avoid wrapped lines, Unix line endings */
int rd_export_to_file(const Reader *src, const char *body_utf8, const char *path, const char *charset_out)
{
    Reader *ex = NULL;
    FILE *f = NULL;
    int total;
    int i;
    int written = 0;
    int needs_conv;
    char line_utf8[2048];
    char line_conv[4096];

    if (!path || !path[0] || !body_utf8)
        return -1;

    /* Convert charset if not AUTO/UTF-8 */
    needs_conv = charset_out && charset_out[0] && strcasecmp(charset_out, "AUTO") != 0 && strcasecmp(charset_out, "UTF-8") != 0 && strcasecmp(charset_out, "UTF8") != 0;

    f = fopen(path, "wb");

    if (!f)
        return -1;

    /* Private reader with wide wrap to pass logical lines through whole */
    ex = rd_new(src ? rd_kludges_visible(src) : 0, src ? rd_hidden_visible(src) : 0);

    if (!ex)
    {
        fclose(f);
        return -1;
    }

    rd_load(ex, body_utf8, 1000000);

    /* Mirror visibility flags from live reader to match user view */
    if (src)
    {
        if (rd_kludges_visible(ex) != rd_kludges_visible(src))
            rd_toggle_kludges(ex);

        if (rd_vias_visible(ex) != rd_vias_visible(src))
            rd_toggle_vias(ex);

        if (rd_hidden_visible(ex) != rd_hidden_visible(src))
            rd_toggle_hidden(ex);
    }

    total = rd_total(ex);

    for (i = 0; i < total; i++)
    {
        int n = rd_line_utf8(ex, i, line_utf8, sizeof(line_utf8));

        if (n < 0)
            continue;

        if (needs_conv)
        {
            int cn = charset_body_from_utf8(charset_out, line_utf8, (int)strlen(line_utf8), line_conv, sizeof(line_conv));

            if (cn > 0)
                fwrite(line_conv, 1, (size_t)cn, f);
        }
        else
        {
            /* UTF-8 verbatim */
            fwrite(line_utf8, 1, strlen(line_utf8), f);
        }

        fputc('\n', f);
        written++;
    }

    rd_free(ex);

    if (fclose(f) != 0)
        return -1;

    return written;
}

/* Search: find all matches of needle in all lines, case-insensitive */
int rd_search_all(const Reader *rd, const wchar_t *needle, int **out_rows, int **out_cols)
{
    int count = 0;
    int i, j;
    int nlen;
    const wchar_t *line;
    int line_len;
    int *rows = NULL;
    int *cols = NULL;

    if (!rd || !needle || !needle[0] || !out_rows || !out_cols)
        return 0;

    nlen = (int)wcslen(needle);

    /* First pass: count matches */
    for (i = 0; i < rd->count; i++)
    {
        line = rd->lines[i].wcs;
        line_len = rd->lines[i].len;

        /* Skip kludge lines (start with ^A) */
        if (line_len > 0 && line[0] == 0x01)
            continue;

        /* Search for needle in this line */
        for (j = 0; j + nlen <= line_len; j++)
        {
            int k;
            int match = 1;

            for (k = 0; k < nlen; k++)
            {
                wchar_t a = line[j + k];
                wchar_t b = needle[k];

                /* Case-insensitive comparison */
                if (a >= L'A' && a <= L'Z')
                    a = a - L'A' + L'a';

                if (b >= L'A' && b <= L'Z')
                    b = b - L'A' + L'a';

                if (a != b)
                {
                    match = 0;
                    break;
                }
            }

            if (match)
            {
                count++;
            }
        }
    }

    if (count == 0)
    {
        *out_rows = NULL;
        *out_cols = NULL;
        return 0;
    }

    /* Allocate arrays for matches */
    rows = (int *)malloc((size_t)count * sizeof(int));
    cols = (int *)malloc((size_t)count * sizeof(int));

    if (!rows || !cols)
    {
        if (rows)
            free(rows);

        if (cols)
            free(cols);

        *out_rows = NULL;
        *out_cols = NULL;

        return 0;
    }

    /* Second pass: fill arrays */
    count = 0;
    for (i = 0; i < rd->count; i++)
    {
        line = rd->lines[i].wcs;
        line_len = rd->lines[i].len;

        /* Skip kludge lines (start with ^A) */
        if (line_len > 0 && line[0] == 0x01)
            continue;

        /* Search for needle in this line */
        for (j = 0; j + nlen <= line_len; j++)
        {
            int k;
            int match = 1;

            for (k = 0; k < nlen; k++)
            {
                wchar_t a = line[j + k];
                wchar_t b = needle[k];

                /* Case-insensitive comparison */
                if (a >= L'A' && a <= L'Z')
                    a = a - L'A' + L'a';

                if (b >= L'A' && b <= L'Z')
                    b = b - L'A' + L'a';

                if (a != b)
                {
                    match = 0;
                    break;
                }
            }

            if (match)
            {
                rows[count] = i;
                cols[count] = j;
                count++;
            }
        }
    }

    *out_rows = rows;
    *out_cols = cols;

    return count;
}
