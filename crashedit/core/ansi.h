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

/* ansi.h -- ANSI escape sequence renderer
 *
 * The old API parsed one source line at a time and threw away every
 * positioning sequence. That is wrong for real BBS ANSI art, which is
 * laid out with ESC[<r>;<c>H jumps, ESC[s/u save/restore, ESC[<n>C
 * relative moves, and so on. Sources that look like one line per
 * source-text line are unusual; the common case is many source lines
 * cooperating to build one visual frame
 *
 * The new API renders the input through a tiny terminal emulator into
 * a 2D canvas of (wchar_t, attrs) cells. Each canvas row is what the
 * reader will display as one visible line. Per-cell attrs let a single
 * row mix colours, which is the rule rather than the exception
 */
#ifndef CORE_ANSI_H
#define CORE_ANSI_H

#include <wchar.h>

/* ANSI colour pairs: index = ANSI_PAIR_BASE + bg * 16 + fg. Caller must init_pair() */
#define ANSI_PAIR_BASE 32
/*#define ANSI_PAIR_COUNT 64*/
#define ANSI_PAIR_COUNT 256

/* Build a pair index from foreground and background colour indices */
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

/* Render UTF-8 text containing ANSI control sequences into a canvas */
AnsiCanvas *ansi_render_utf8(const char *utf8, int utf8_len, int max_cols);

/* Render raw bytes (charset) into canvas. NULL charset = LATIN-1
 * Direct byte path preserves ANSI control bytes; UTF-8 routes through utf8_to_wcs */
AnsiCanvas *ansi_render_bytes(const char *bytes, int len, const char *charset, int max_cols);

/* Free a canvas and all its rows. NULL is a no-op */
void ansi_canvas_free(AnsiCanvas *cv);

/* Map an ANSI numeric colour (30..37 / 40..47) to a 0..7 index
 * Anything else returns 7 (white) to match the historical behaviour */
int ansi_color_to_ncurses(int ansi_color);

#endif /* CORE_ANSI_H */
