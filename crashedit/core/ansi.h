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

/* ANSI escape sequence renderer with terminal emulator for BBS ANSI art */
#ifndef CORE_ANSI_H
#define CORE_ANSI_H

#include <wchar.h>

/* ANSI colour pairs: index = ANSI_PAIR_BASE + bg * 16 + fg, caller must init_pair() */
#define ANSI_PAIR_BASE 32
/*#define ANSI_PAIR_COUNT 64*/
#define ANSI_PAIR_COUNT 256

/* Build pair index from foreground and background colour indices */
/*#define ANSI_PAIR(fg, bg) (ANSI_PAIR_BASE + ((bg) & 7) * 8 + ((fg) & 7))*/
#define ANSI_PAIR(fg, bg) (ANSI_PAIR_BASE + ((bg) & 15) * 16 + ((fg) & 15))

/* Screen cell with rendering attributes */
typedef struct AnsiCell
{
    int color_pair; /* ncurses COLOR_PAIR number, e.g. ANSI_PAIR(fg,bg) */
    int attrs;      /* ncurses A_BOLD / A_REVERSE bits, OR'd together */
} AnsiCell;

/* Canvas row with parallel wcs/cells arrays */
typedef struct
{
    wchar_t *wcs;
    AnsiCell *cells;
    int len;
    int cap;
} AnsiRow;

/* Canvas: dense rows (0..row_count-1), empty rows allocated as zero-length */
typedef struct
{
    AnsiRow *rows;
    int row_count;
    int row_cap;
} AnsiCanvas;

/* Render UTF-8 text containing ANSI control sequences into canvas */
AnsiCanvas *ansi_render_utf8(const char *utf8, int utf8_len, int max_cols);

/* Render raw bytes (charset) into canvas, NULL charset = LATIN-1, preserves ANSI control bytes */
AnsiCanvas *ansi_render_bytes(const char *bytes, int len, const char *charset, int max_cols);

/* Free canvas and all rows, NULL is no-op */
void ansi_canvas_free(AnsiCanvas *cv);

/* Map ANSI numeric colour (30..37 / 40..47) to 0..7 index, returns 7 (white) for others */
int ansi_color_to_ncurses(int ansi_color);

#endif /* CORE_ANSI_H */
