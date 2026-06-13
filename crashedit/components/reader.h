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

/* reader.h -- Message reader with wchar_t internal */
#ifndef WRAPPER_READER_H
#define WRAPPER_READER_H

#include <wchar.h>

typedef struct Reader Reader;

Reader *rd_new(int viewkludge, int viewhidden);
void rd_free(Reader *rd);

/* Load a message body in plain (non-ANSI) mode */
void rd_load(Reader *rd, const char *utf8_body, int wrap_width);

/* Load a message body in ANSI mode */
void rd_load_ansi(Reader *rd, const char *raw_bytes, int raw_len, const char *charset, int max_cols);

/* Scroll */
void rd_set_page(Reader *rd, int visible_rows);
void rd_scroll_up(Reader *rd, int n);
void rd_scroll_down(Reader *rd, int n);
void rd_page_up(Reader *rd);
void rd_page_down(Reader *rd);
void rd_home(Reader *rd);
void rd_end(Reader *rd);

/* Kludge toggle */
void rd_toggle_kludges(Reader *rd);
int rd_kludges_visible(const Reader *rd);
void rd_toggle_vias(Reader *rd);
int rd_vias_visible(const Reader *rd);
void rd_toggle_hidden(Reader *rd);
int rd_hidden_visible(const Reader *rd);
void rd_toggle_hiddklud(Reader *rd);

/* ANSI toggle */
void rd_toggle_ansi(Reader *rd);
int rd_ansi_visible(const Reader *rd);

/* Query */
int rd_total(const Reader *rd); /* visible line count */
int rd_count(const Reader *rd); /* total line count (global) */
int rd_visible(const Reader *rd);
int rd_top(const Reader *rd);
int rd_percent(const Reader *rd);
int rd_global_to_visible(const Reader *rd, int global_idx); /* -1 if not visible */
const wchar_t *rd_get_line(const Reader *rd, int vis_idx);  /* wchar_t line */
int rd_get_len(const Reader *rd, int vis_idx);              /* character count */
int rd_get_type(const Reader *rd, int vis_idx);             /* FTN_LT_* */
int rd_get_line_idx(const Reader *rd, int vis_idx);         /* message line index from visible index */

/* Convenience: get line as UTF-8 into caller buffer */
int rd_line_utf8(const Reader *rd, int vis_idx, char *buf, int bufsz);

/* ANSI attributes for a line */
int rd_get_ansi_color(const Reader *rd, int vis_idx); /* ncurses color pair or -1 */
int rd_get_ansi_attrs(const Reader *rd, int vis_idx); /* ncurses attributes */

/* Forward-declare AnsiCell to avoid dragging in full canvas header */
struct AnsiCell;
const struct AnsiCell *rd_get_ansi_cells(const Reader *rd, int vis_idx);

/* Export the message currently loaded in 'src' to a text file */
int rd_export_to_file(const Reader *src, const char *body_utf8, const char *path, const char *charset_out);

/* Search: find all matches, returns count and malloc'd arrays (caller frees) */
int rd_search_all(const Reader *rd, const wchar_t *needle, int **out_rows, int **out_cols);

#endif
