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

/* config.h -- crashedit configuration */
#ifndef WRAPPER_CONFIG_H
#define WRAPPER_CONFIG_H

#include "../core/charset.h"

#define CFG_STR_MAX 256
#define CFG_AKA_MAX 32
#define CFG_AKA_COUNT 256 /* Maximum number of AKAs */

/* GoldED+ extended config field sizes */
#define CFG_SORT_MAX 32
#define CFG_FORMAT_MAX 64
#define CFG_COLOR_MAX 24 /* COL_* are 1..23; slot 0 unused */

typedef struct
{
    char sysop[80];                       /* sysop full name */
    char aka[CFG_AKA_COUNT][CFG_AKA_MAX]; /* AKAs "z:n/f.p" */
    int aka_count;                        /* Number of configured AKAs */
    int aka_selected;                     /* Currently selected AKA index*/
    char areafile[CFG_STR_MAX];           /* path to areas.golded */
    char charset[CHARSET_NAME_MAX];       /* default outbound encoding */
    int timezone_offset;                  /* local time offset from UTC in minutes */
    int timezone_is_manual;               /* 1 if TIMEZONE was set in config, 0 if auto-detected */

    /* Display preferences */
    int viewhidden;                          /* show SEEN-BY/Via lines */
    int viewkludge;                          /* show ^A kludges */
    int viewansi;                            /* 0=raw, 1=render ANSI */
    int msglistfirst;                        /* skip area list */
    int msglistfast;                         /* fast message list */
    int msglistheader;                       /* colored header bar in msglist */
    int showdeleted;                         /* show deleted messages */
    int msglistmax;                          /* max msgs to load (0=all) */
    int search_max;                          /* max search hits to keep; 0=default, limited only by RAM */
    char arealistsort[CFG_SORT_MAX];         /* sort spec, e.g. "FYT-U+GE" */
    char arealistsort_default[CFG_SORT_MAX]; /* default from config file */
    char arealistformat[CFG_FORMAT_MAX];     /* columns, e.g. "A E PNU C D G" */

    /* Message signature elements */
    char origin[80];
    char tearline[80];
    char tagline_file[CFG_STR_MAX];
    char template_file[CFG_STR_MAX];
    int forceintl;    /* 0=auto, 1=always INTL, 2=never INTL */
    int autowrap_col; /* auto-wrap column; 0 disables */
    int undo_levels;  /* undo stack depth, default 50 */
    int quotemargin;  /* quote wrap column; 0 disables */
    int hard_wrap;    /* 0=soft-wrap (no CR), 1=hard-wrap (CR at column) */

    /* Message body framing with printf-style templates (%t=to, %f=from, %o=orig recipient, %d=orig date) */
    int greeting;                     /* 0=off, 1=emit greeting line */
    char greeting_text[CFG_STR_MAX];  /* e.g. "Hello %t!" */
    int attribution;                  /* 0=off, 1=emit attribution (reply) */
    char attrib_self[CFG_STR_MAX];    /* original was addressed to you */
    char attrib_other[CFG_STR_MAX];   /* original was addressed to someone else */
    int signature;                    /* 0=off, 1=emit signature line */
    char signature_text[CFG_STR_MAX]; /* e.g. "%f" */

    /* Color pairs. Indexed by COL_* (1..22); slot 0 unused. Filled with
       built-in defaults by cfg_defaults(); COLOR keyword overrides */
    int color_fg[CFG_COLOR_MAX];
    int color_bg[CFG_COLOR_MAX];

    /* Track which COL_* slots were set explicitly via COLOR directive */
    int color_explicit[CFG_COLOR_MAX];

    /* Cursor color: Amiga pen index (0..255) or OSC 12 spec for terminals */
    int cursor_color;          /* -1 = unset; 0..7 = ncurses color */
    char cursor_color_rgb[16]; /* alternative: "#RRGGBB" hex spec */

    /* Default background color for COLOR_PAIR(0). On Amiga: COLOR_RED for black pen */
    int default_bg_color;

    /* UI font name (e.g. "Consolas" on Windows, "topaz.font" on Amiga) */
    char font[CFG_STR_MAX];
    char ansifont[CFG_STR_MAX];

    /* TrueType font support (Amiga via ttengine.library v6+) */
    int ttf_enabled;            /* 0=disabled, 1=enabled */
    char ttf_font[CFG_STR_MAX]; /* e.g. "FONTS:_ttf/DejaVuSansMono.ttf" */
    int ttf_size;               /* point size, default 14 */
    int ttf_antialias;          /* 0=auto, 1=off, 2=on */
    int ttf_use_utf8;           /* 0=UTF-16 BE (BMP only), 1=UTF-8 (full Unicode/emojis) */

    /* Amiga color palette (0-15) */
    int color_map[16];
    int color_map_initialized;

    /* Freq output: outbound dir and mode (ASO/BSO/BSO+ext). See core/freq.c */
    char freq_outbound[CFG_STR_MAX];
    int freq_mode;

    /* INCLUDE: paths to FTS-5000 nodelists/pointlists to load into RAM on startup */
    char nodelist_includes[16][CFG_STR_MAX];
    int nodelist_includes_count;
} CrashEditCfg;

/* Load config from path (0=ok, -1=error) */
int cfg_load(CrashEditCfg *cfg, const char *path);

/* Save the setup-managed keywords back to the config file */
int cfg_save(const CrashEditCfg *cfg, const char *path);

/* Fill cfg with safe defaults */
void cfg_defaults(CrashEditCfg *cfg);

#endif /* WRAPPER_CONFIG_H */
