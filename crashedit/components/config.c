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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "../core/utf8.h"
#include "../core/portable.h"
#include "../core/charset.h"
#include "../core/ftn.h"
#include "../core/freq.h"
#include "../core/search.h"

typedef struct
{
    const char *key;
    char val[CFG_STR_MAX];
    int done; /* set once written, so the append pass skips it */
} CfgKV;

/* Append one key/value pair to a dynamically-growing CfgKV array */
static int kv_append(CfgKV **kv, int *nkv, int *cap, const char *key, const char *val)
{
    size_t _n;

    if (*nkv >= *cap)
    {
        int new_cap = *cap ? *cap * 2 : 64;
        CfgKV *new_kv = (CfgKV *)realloc(*kv, (size_t)new_cap * sizeof(CfgKV));

        if (!new_kv)
            return -1;

        *kv = new_kv;
        *cap = new_cap;
    }

    _n = strlen(val);

    if (_n >= CFG_STR_MAX)
        _n = CFG_STR_MAX - 1;

    (*kv)[*nkv].key = key;

    memcpy((*kv)[*nkv].val, val, _n);

    (*kv)[*nkv].val[_n] = '\0';
    (*kv)[*nkv].done = 0;

    (*nkv)++;

    return 0;
}

static void strip_trailing(char *s)
{
    int len = (int)strlen(s);

    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n'))
        s[--len] = '\0';
}

/* Strip surrounding quotes from a string */
static void strip_quotes(char *s)
{
    int len = (int)strlen(s);

    if (len >= 2 && s[0] == '"' && s[len - 1] == '"')
    {
        memmove(s, s + 1, len - 1);
        s[len - 2] = '\0';
    }
}

/* Copy first token (no quotes) into dst, max dstlen-1 chars */
static void get_token(const char *src, char *dst, int dstlen)
{
    int i = 0;

    while (*src && (*src == ' ' || *src == '\t'))
        src++;

    while (*src && *src != ' ' && *src != '\t' && *src != '\r' && *src != '\n' && i < dstlen - 1)
        dst[i++] = *src++;

    dst[i] = '\0';
}

/* Skip first token, return pointer to rest of line */
static const char *skip_token(const char *p)
{
    while (*p && *p != ' ' && *p != '\t')
        p++;

    while (*p == ' ' || *p == '\t')
        p++;

    return p;
}

/* Parse YES/NO/ON/OFF/TRUE/FALSE/1/0 -> 1 or 0 */
static int yn_token_end(char c)
{
    return c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int parse_yesno(const char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;

    if (strncasecmp(s, "YES", 3) == 0 && yn_token_end(s[3]))
        return 1;

    if (strncasecmp(s, "ON", 2) == 0 && yn_token_end(s[2]))
        return 1;

    if (strncasecmp(s, "TRUE", 4) == 0 && yn_token_end(s[4]))
        return 1;

    if (*s == '1' && yn_token_end(s[1]))
        return 1;

    return 0;
}

/* Copy rest-of-line, optionally stripping surrounding quotes */
static void copy_rest(const char *src, char *dst, int dstlen)
{
    int i = 0;
    int in_quote = 0;

    while (*src == ' ' || *src == '\t')
        src++;

    if (*src == '"')
    {
        in_quote = 1;
        src++;
    }

    while (*src && *src != '\r' && *src != '\n' && i < dstlen - 1)
    {
        if (in_quote && *src == '"')
        {
            src++;
            break;
        }

        dst[i++] = *src++;
    }

    while (i > 0 && (dst[i - 1] == ' ' || dst[i - 1] == '\t'))
        i--;

    dst[i] = '\0';
}

/* Canonicalise user-supplied charset name */
static void normalize_charset(char *cs)
{
    const char *canonical = charset_resolve(cs);

    if (canonical && canonical != cs)
    {
        strncpy(cs, canonical, CHARSET_NAME_MAX - 1);
        cs[CHARSET_NAME_MAX - 1] = '\0';
    }
}

/* Map color name to base ncurses number (0..15), or -1 (no COLORMAP) */
static int color_by_name_base(const char *s)
{
    if (strcasecmp(s, "black") == 0)
        return 0;

    if (strcasecmp(s, "red") == 0)
        return 1;

    if (strcasecmp(s, "green") == 0)
        return 2;

    if (strcasecmp(s, "yellow") == 0)
        return 3;

    if (strcasecmp(s, "blue") == 0)
        return 4;

    if (strcasecmp(s, "magenta") == 0)
        return 5;

    if (strcasecmp(s, "cyan") == 0)
        return 6;

    if (strcasecmp(s, "white") == 0)
        return 7;

    if (strcasecmp(s, "brightblack") == 0)
        return 8;

    if (strcasecmp(s, "brightred") == 0)
        return 9;

    if (strcasecmp(s, "brightgreen") == 0)
        return 10;

    if (strcasecmp(s, "brightyellow") == 0)
        return 11;

    if (strcasecmp(s, "brightblue") == 0)
        return 12;

    if (strcasecmp(s, "brightmagenta") == 0)
        return 13;

    if (strcasecmp(s, "brightcyan") == 0)
        return 14;

    if (strcasecmp(s, "brightwhite") == 0)
        return 15;

    return -1;
}

/* Map color name to ncurses number (0..15), or -1 (with COLORMAP) */
static int color_by_name(const char *s, CrashEditCfg *cfg)
{
    /* If custom mapping is enabled, use it */
    if (cfg && cfg->color_map_initialized)
    {
        if (strcasecmp(s, "black") == 0)
            return cfg->color_map[0];

        if (strcasecmp(s, "red") == 0)
            return cfg->color_map[1];

        if (strcasecmp(s, "green") == 0)
            return cfg->color_map[2];

        if (strcasecmp(s, "yellow") == 0)
            return cfg->color_map[3];

        if (strcasecmp(s, "blue") == 0)
            return cfg->color_map[4];

        if (strcasecmp(s, "magenta") == 0)
            return cfg->color_map[5];

        if (strcasecmp(s, "cyan") == 0)
            return cfg->color_map[6];

        if (strcasecmp(s, "white") == 0)
            return cfg->color_map[7];

        if (strcasecmp(s, "brightblack") == 0)
            return cfg->color_map[8];

        if (strcasecmp(s, "brightred") == 0)
            return cfg->color_map[9];

        if (strcasecmp(s, "brightgreen") == 0)
            return cfg->color_map[10];

        if (strcasecmp(s, "brightyellow") == 0)
            return cfg->color_map[11];

        if (strcasecmp(s, "brightblue") == 0)
            return cfg->color_map[12];

        if (strcasecmp(s, "brightmagenta") == 0)
            return cfg->color_map[13];

        if (strcasecmp(s, "brightcyan") == 0)
            return cfg->color_map[14];

        if (strcasecmp(s, "brightwhite") == 0)
            return cfg->color_map[15];

        return -1;
    }

    /* Otherwise, use default mapping */
    if (strcasecmp(s, "black") == 0)
        return 0;

    if (strcasecmp(s, "red") == 0)
        return 1;

    if (strcasecmp(s, "green") == 0)
        return 2;

    if (strcasecmp(s, "yellow") == 0)
        return 3;

    if (strcasecmp(s, "blue") == 0)
        return 4;

    if (strcasecmp(s, "magenta") == 0)
        return 5;

    if (strcasecmp(s, "cyan") == 0)
        return 6;

    if (strcasecmp(s, "white") == 0)
        return 7;

    if (strcasecmp(s, "brightblack") == 0)
        return 8;

    if (strcasecmp(s, "brightred") == 0)
        return 9;

    if (strcasecmp(s, "brightgreen") == 0)
        return 10;

    if (strcasecmp(s, "brightyellow") == 0)
        return 11;

    if (strcasecmp(s, "brightblue") == 0)
        return 12;

    if (strcasecmp(s, "brightmagenta") == 0)
        return 13;

    if (strcasecmp(s, "brightcyan") == 0)
        return 14;

    if (strcasecmp(s, "brightwhite") == 0)
        return 15;

    return -1;
}

/* Map pair name to COL_* index, or -1 */
static int pair_by_name(const char *s)
{
    if (strcasecmp(s, "NORMAL") == 0)
        return 1;

    if (strcasecmp(s, "SELECTED") == 0)
        return 2;

    if (strcasecmp(s, "HEADER") == 0)
        return 3;

    if (strcasecmp(s, "STATUS") == 0)
        return 4;

    if (strcasecmp(s, "MENU") == 0)
        return 5;

    if (strcasecmp(s, "MENUHOT") == 0)
        return 6;

    if (strcasecmp(s, "BORDER") == 0)
        return 7;

    if (strcasecmp(s, "QUOTE1") == 0)
        return 8;

    if (strcasecmp(s, "QUOTE2") == 0)
        return 9;

    if (strcasecmp(s, "QUOTE3") == 0)
        return 10;

    if (strcasecmp(s, "QUOTE4") == 0)
        return 11;

    if (strcasecmp(s, "KLUDGE") == 0)
        return 12;

    if (strcasecmp(s, "TEAR") == 0)
        return 13;

    if (strcasecmp(s, "ORIGIN") == 0)
        return 14;

    if (strcasecmp(s, "SEENBY") == 0)
        return 15;

    if (strcasecmp(s, "VIA") == 0)
        return 16;

    if (strcasecmp(s, "UNREAD") == 0)
        return 17;

    if (strcasecmp(s, "DELETED") == 0)
        return 18;

    if (strcasecmp(s, "POPUP") == 0)
        return 19;

    if (strcasecmp(s, "POPUPSEL") == 0)
        return 20;

    if (strcasecmp(s, "ERROR") == 0)
        return 21;

    if (strcasecmp(s, "TAGLINE") == 0)
        return 22;

    if (strcasecmp(s, "SPELLCURRENT") == 0)
        return 24;

    if (strcasecmp(s, "BRACKETMATCH") == 0)
        return 25;

    if (strcasecmp(s, "CURRENTLINE") == 0)
        return 26;

    if (strcasecmp(s, "GUIDES") == 0)
        return 27;

    return -1;
}

void cfg_defaults(CrashEditCfg *cfg)
{
    int i;

    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->sysop, "Sysop", sizeof(cfg->sysop) - 1);

    /* AKAs start empty */
    cfg->aka_count = 0;
    cfg->aka_selected = 0;

    strncpy(cfg->areafile, "areas.golded", sizeof(cfg->areafile) - 1);
    strncpy(cfg->tearline, "", sizeof(cfg->tearline) - 1);
    strncpy(cfg->template_file, "", sizeof(cfg->template_file) - 1);

    cfg->forceintl = 0; /* 0 = auto (only when zones differ) */

    strncpy(cfg->charset, CHARSET_WRITE_DEFAULT, sizeof(cfg->charset) - 1);

    /* Detect timezone automatically only if not manually set */
    cfg->timezone_offset = 0;
    cfg->timezone_is_manual = 0;

    cfg->hard_wrap = 0; /* soft-wrap by default */

    /* GoldED+ defaults */
    cfg->viewhidden = 0;
    cfg->viewkludge = 0;
    cfg->viewansi = 0;
    cfg->msglistfirst = 0;
    cfg->msglistfast = 0;
    cfg->msglistheader = 1;
    cfg->showdeleted = 0;
    cfg->msglistmax = 0;
    cfg->search_max = SEARCH_DEFAULT_MAX; /* 0 == use SEARCH_DEFAULT_MAX; no hard upper limit */

    strncpy(cfg->arealistsort, "FYTUE", sizeof(cfg->arealistsort) - 1);
    cfg->arealistsort[sizeof(cfg->arealistsort) - 1] = '\0';
    cfg->arealistsort_default[0] = '\0';

    strncpy(cfg->arealistformat, "AM D CPUN E G", sizeof(cfg->arealistformat) - 1);
    cfg->arealistformat[sizeof(cfg->arealistformat) - 1] = '\0';

    /* Origin, tearline and tagline empty by default */
    cfg->origin[0] = '\0';
    cfg->tearline[0] = '\0';
    cfg->tagline_file[0] = '\0';
    cfg->template_file[0] = '\0';
    cfg->autowrap_col = 75;
    cfg->undo_levels = 50;
    cfg->quotemargin = 75;
    cfg->hard_wrap = 0;
    cfg->show_line_numbers = 0;
    cfg->show_whitespace = 0;
    cfg->show_brackets = 0;
    cfg->tab_width = 4;
    cfg->highlight_line = 0;
    cfg->word_count = 0;
    cfg->autoclose = 0;
    cfg->smart_indent = 0;
    cfg->ruler_col = 0;
    cfg->indent_guides = 0;
    cfg->wrap_indicator = 0;
    cfg->mouse_enabled = 1;

    /* Editor assists default OFF -- user opts in via setup */
    cfg->assist_smart_quotes = 0;
    cfg->assist_auto_cap = 0;
    cfg->assist_repeat_check = 0;

    /* Word movement: 0=standard, 1=vim-like (non-space blocks) */
    cfg->word_move_mode = 0;

    /* OFF by default */
    cfg->greeting = 0;
    strncpy(cfg->greeting_text, "Hello %t!", sizeof(cfg->greeting_text) - 1);
    cfg->greeting_text[sizeof(cfg->greeting_text) - 1] = '\0';

    cfg->attribution = 0;
    strncpy(cfg->attrib_self, "%d, you wrote to me:", sizeof(cfg->attrib_self) - 1);

    cfg->attrib_self[sizeof(cfg->attrib_self) - 1] = '\0';
    strncpy(cfg->attrib_other, "%d, %n wrote to %o:", sizeof(cfg->attrib_other) - 1);
    cfg->attrib_other[sizeof(cfg->attrib_other) - 1] = '\0';

    cfg->signature = 0;
    strncpy(cfg->signature_text, "%f", sizeof(cfg->signature_text) - 1);
    cfg->signature_text[sizeof(cfg->signature_text) - 1] = '\0';

    for (i = 0; i < CFG_COLOR_MAX; i++)
    {
        cfg->color_fg[i] = 7; /* white on black fallback */
        cfg->color_bg[i] = 0;
        cfg->color_explicit[i] = 0;
    }

    /* 1 COL_NORMAL */
    cfg->color_fg[1] = 7;
    cfg->color_bg[1] = 0;

    /* 2 COL_SELECTED */
    cfg->color_fg[2] = 0;
    cfg->color_bg[2] = 7;

    /* 3 COL_HEADER */
    cfg->color_fg[3] = 3;
    cfg->color_bg[3] = 4;

    /* 4 COL_STATUS */
    cfg->color_fg[4] = 0;
    cfg->color_bg[4] = 7;

    /* 5 COL_MENU */
    cfg->color_fg[5] = 0;
    cfg->color_bg[5] = 7;

    /* 6 COL_MENU_HOT */
    cfg->color_fg[6] = 1;
    cfg->color_bg[6] = 7;

    /* 7 COL_BORDER */
    cfg->color_fg[7] = 6;
    cfg->color_bg[7] = 0;

    /* 8 COL_QUOTE1 */
    cfg->color_fg[8] = 2;
    cfg->color_bg[8] = 0;

    /* 9 COL_QUOTE2 */
    cfg->color_fg[9] = 6;
    cfg->color_bg[9] = 0;

    /*10 COL_QUOTE3 */
    cfg->color_fg[10] = 5;
    cfg->color_bg[10] = 0;

    /*11 COL_QUOTE4 */
    cfg->color_fg[11] = 4;
    cfg->color_bg[11] = 0;

    /*12 COL_KLUDGE */
    cfg->color_fg[12] = 6;
    cfg->color_bg[12] = 0;

    /*13 COL_TEAR */
    cfg->color_fg[13] = 3;
    cfg->color_bg[13] = 0;

    /*14 COL_ORIGIN */
    cfg->color_fg[14] = 3;
    cfg->color_bg[14] = 0;

    /*15 COL_SEENBY */
    cfg->color_fg[15] = 5;
    cfg->color_bg[15] = 0;

    /*16 COL_VIA */
    cfg->color_fg[16] = 4;
    cfg->color_bg[16] = 0;

    /*17 COL_UNREAD */
    cfg->color_fg[17] = 3;
    cfg->color_bg[17] = 0;

    /*18 COL_DELETED */
    cfg->color_fg[18] = 1;
    cfg->color_bg[18] = 0;

    /*19 COL_POPUP */
    cfg->color_fg[19] = 7;
    cfg->color_bg[19] = 4;

    /*20 COL_POPUP_SEL */
    cfg->color_fg[20] = 0;
    cfg->color_bg[20] = 6;

    /*21 COL_ERROR */
    cfg->color_fg[21] = 7;
    cfg->color_bg[21] = 1;

    /*22 COL_TAGLINE */
    cfg->color_fg[22] = 6;
    cfg->color_bg[22] = 0;

    /*23 COL_SEARCH_MATCH */
    cfg->color_fg[23] = 3;
    cfg->color_bg[23] = 0;

    /*24 COL_SPELL_CURRENT */
    cfg->color_fg[24] = 7;
    cfg->color_bg[24] = 5;

    /*25 COL_BRACKET_MATCH */
    cfg->color_fg[25] = 0;
    cfg->color_bg[25] = 3;

    /*26 COL_CURRENT_LINE */
    cfg->color_fg[26] = 7;
    cfg->color_bg[26] = 4;

    /*27 COL_GUIDE */
    cfg->color_fg[27] = 6;
    cfg->color_bg[27] = 0;

    /* Cursor color: -1 = don't override (use terminal default on Linux, pen 1 on Amiga). User can set CURSORCOLOR in config */
    cfg->cursor_color = -1;
    cfg->cursor_color_rgb[0] = '\0';

    /* Default background color for COLOR_PAIR(0). On Amiga, use  (black per user mapping) */
    cfg->default_bg_color = 0;

    /* UI font: default to topaz.font on Amiga, Consolas on Windows */
    strncpy(cfg->font, "topaz.font", sizeof(cfg->font) - 1);
    cfg->font[sizeof(cfg->font) - 1] = '\0';

    /* ANSI font: default to topaz.font (same as regular font) */
    strncpy(cfg->ansifont, "topaz.font", sizeof(cfg->ansifont) - 1);
    cfg->ansifont[sizeof(cfg->ansifont) - 1] = '\0';

    /* Initialize color mapping (identity: 0-7 standard, 8-15 bright) */
    cfg->color_map[0] = 0;          /* black */
    cfg->color_map[1] = 1;          /* red */
    cfg->color_map[2] = 2;          /* green */
    cfg->color_map[3] = 3;          /* yellow */
    cfg->color_map[4] = 4;          /* blue */
    cfg->color_map[5] = 5;          /* magenta */
    cfg->color_map[6] = 6;          /* cyan */
    cfg->color_map[7] = 7;          /* white */
    cfg->color_map[8] = 8;          /* bright black */
    cfg->color_map[9] = 9;          /* bright red */
    cfg->color_map[10] = 10;        /* bright green */
    cfg->color_map[11] = 11;        /* bright yellow */
    cfg->color_map[12] = 12;        /* bright blue */
    cfg->color_map[13] = 13;        /* bright magenta */
    cfg->color_map[14] = 14;        /* bright cyan */
    cfg->color_map[15] = 15;        /* bright white */
    cfg->color_map_initialized = 0; /* Use defaults initially */

    /* TTF defaults: disabled (empty path) — bitmap font is used */
    cfg->ttf_enabled = 0;
    cfg->ttf_font[0] = '\0';
    cfg->ttf_size = 14;
    cfg->ttf_antialias = 0; /* auto */
    cfg->ttf_use_utf8 = 1;  /* UTF-8 for full Unicode/emoji support */

    /* Fallback slots empty by default -- user opts in by setting any of
     * TTF_FALLBACK1..TTF_FALLBACK8 in the config file */
    for (i = 0; i < CFG_TTF_FALLBACKS; i++)
    {
        cfg->ttf_fallback[i][0] = '\0';
        cfg->ttf_fallback_size[i] = 0;
    }

#ifdef HAVE_HUNSPELL
    cfg->spell_enabled = 0;

#if defined(PLATFORM_BSD)
    strncpy(cfg->spell_dict_path, "/usr/local/share/hunspell", sizeof(cfg->spell_dict_path) - 1);
#elif defined(PLATFORM_UNIX)
    strncpy(cfg->spell_dict_path, "/usr/share/hunspell", sizeof(cfg->spell_dict_path) - 1);
#elif defined(PLATFORM_WIN32)
    strncpy(cfg->spell_dict_path, "C:\\Program Files\\LibreOffice\\share\\extensions\\dict-en", sizeof(cfg->spell_dict_path) - 1);
#elif defined(PLATFORM_AMIGA)
    strncpy(cfg->spell_dict_path, "ENVARC:dictionaries", sizeof(cfg->spell_dict_path) - 1);
#else
    cfg->spell_dict_path[0] = '\0';
#endif
    cfg->spell_dict_path[sizeof(cfg->spell_dict_path) - 1] = '\0';
    cfg->spell_dict_name[0] = '\0';
    cfg->spell_custom_dict[0] = '\0';
#endif

#ifdef HAVE_HYPHEN
    cfg->hyph_enabled = 0;

#if defined(PLATFORM_BSD)
    strncpy(cfg->hyph_dict_path, "/usr/local/share/hyphen", sizeof(cfg->hyph_dict_path) - 1);
#elif defined(PLATFORM_UNIX)
    strncpy(cfg->hyph_dict_path, "/usr/share/hyphen", sizeof(cfg->hyph_dict_path) - 1);
#elif defined(PLATFORM_WIN32)
    strncpy(cfg->hyph_dict_path, "C:\\Program Files\\LibreOffice\\share\\extensions\\dict-en", sizeof(cfg->hyph_dict_path) - 1);
#elif defined(PLATFORM_AMIGA)
    strncpy(cfg->hyph_dict_path, "ENVARC:dictionaries", sizeof(cfg->hyph_dict_path) - 1);
#else
    cfg->hyph_dict_path[0] = '\0';
#endif
    cfg->hyph_dict_path[sizeof(cfg->hyph_dict_path) - 1] = '\0';
    cfg->hyph_dict_name[0] = '\0';
    cfg->hyph_wrap_enabled = 0;
#endif /* HAVE_HYPHEN */

#ifdef HAVE_MYTHES
    cfg->thes_enabled = 0;

#if defined(PLATFORM_BSD)
    strncpy(cfg->thes_dict_path, "/usr/local/share/mythes", sizeof(cfg->thes_dict_path) - 1);
#elif defined(PLATFORM_UNIX)
    strncpy(cfg->thes_dict_path, "/usr/share/mythes", sizeof(cfg->thes_dict_path) - 1);
#elif defined(PLATFORM_WIN32)
    strncpy(cfg->thes_dict_path, "C:\\Program Files\\LibreOffice\\share\\extensions\\dict-en", sizeof(cfg->thes_dict_path) - 1);
#elif defined(PLATFORM_AMIGA)
    strncpy(cfg->thes_dict_path, "ENVARC:dictionaries", sizeof(cfg->thes_dict_path) - 1);
#else
    cfg->thes_dict_path[0] = '\0';
#endif
    cfg->thes_dict_path[sizeof(cfg->thes_dict_path) - 1] = '\0';
    cfg->thes_dict_name[0] = '\0';
#endif /* HAVE_MYTHES */

#ifdef HAVE_TRANSLATE
    cfg->translate_enabled = 0;
    cfg->translate_backend = 0;        /* MyMemory */
    cfg->translate_endpoint[0] = '\0'; /* Use default endpoint */
    cfg->translate_api_key[0] = '\0';
    cfg->translate_email[0] = '\0';

    strncpy(cfg->translate_from_lang, "en", sizeof(cfg->translate_from_lang) - 1);
    strncpy(cfg->translate_to_lang, "es", sizeof(cfg->translate_to_lang) - 1);

    cfg->translate_from_lang[sizeof(cfg->translate_from_lang) - 1] = '\0';
    cfg->translate_to_lang[sizeof(cfg->translate_to_lang) - 1] = '\0';
    cfg->translate_timeout = 10;  /* 10 seconds */
    cfg->stardict_path[0] = '\0'; /* Empty by default */
#endif                            /* HAVE_TRANSLATE */
}

int cfg_load(CrashEditCfg *cfg, const char *path)
{
    FILE *f = NULL;
    char line[512];
    char word[64];
    const char *rest;
    char aka_buf[CFG_AKA_MAX];
    int pi;
    int fi;
    int bi;

    cfg_defaults(cfg);

    f = fopen(path, "r");

    if (!f)
    {
        /* Write defaults if no config exists, fail if parent dir missing */
        if (cfg_save(cfg, path) != 0)
            return -1; /* couldn't even create it (bad path / perms) */

        fprintf(stderr, "Note: config not found, created default: %s\n", path);

        return 0; /* run with in-memory defaults */
    }

    while (fgets(line, sizeof(line), f))
    {
        char *p = line;
        int in_quote = 0;
        int j;

        while (*p == ' ' || *p == '\t')
            p++;

        /* Skip comments and empty lines */
        if (*p == '#' || *p == ';' || *p == '\0' || *p == '\r' || *p == '\n')
            continue;

        /* Quote-aware inline comment stripper */
        for (j = 0; p[j]; j++)
        {
            if (p[j] == '"')
            {
                in_quote = !in_quote;
                continue;
            }

            if (!in_quote && (p[j] == ';' || p[j] == '#'))
            {
                p[j] = '\0';
                break;
            }
        }

        strip_trailing(p);

        get_token(p, word, sizeof(word));
        rest = skip_token(p);

        /* Base keywords */
        if (strcasecmp(word, "SYSOP") == 0)
        {
            copy_rest(rest, cfg->sysop, sizeof(cfg->sysop));
        }
        else if (strcasecmp(word, "AKA") == 0)
        {
            get_token(rest, aka_buf, sizeof(aka_buf));
            strip_quotes(aka_buf);

            if (cfg->aka_count < CFG_AKA_COUNT)
            {
                size_t l = strlen(aka_buf);

                if (l >= CFG_AKA_MAX)
                    l = CFG_AKA_MAX - 1;

                memcpy(cfg->aka[cfg->aka_count], aka_buf, l);

                cfg->aka[cfg->aka_count][l] = '\0';
                cfg->aka_count++;
            }
        }
        else if (strcasecmp(word, "AREAFILE") == 0)
        {
            copy_rest(rest, cfg->areafile, sizeof(cfg->areafile));
        }
        else if (strcasecmp(word, "OUTBOUND") == 0)
        {
            /* Outbound dir for freq .req/.clo files picked up by mailer */
            copy_rest(rest, cfg->freq_outbound, sizeof(cfg->freq_outbound));
        }
        else if (strcasecmp(word, "OUTBOUNDMODE") == 0)
        {
            char m[32];
            get_token(rest, m, sizeof(m));

            /* aso | bso | bso-ext. Other values leave freq_mode unset */
            if (strcasecmp(m, "aso") == 0)
                cfg->freq_mode = FREQ_MODE_ASO;
            else if (strcasecmp(m, "bso") == 0)
                cfg->freq_mode = FREQ_MODE_BSO;
            else if (strcasecmp(m, "bso-ext") == 0)
                cfg->freq_mode = FREQ_MODE_BSO_EXT;
            else
                cfg->freq_mode = FREQ_MODE_UNSET;
        }
        else if (strcasecmp(word, "CHARSET") == 0)
        {
            get_token(rest, cfg->charset, sizeof(cfg->charset));

            /* AUTO = empty string (auto-detect) */
            if (strcasecmp(cfg->charset, "AUTO") == 0)
                cfg->charset[0] = '\0';
            else
                normalize_charset(cfg->charset);
        }
        else if (strcasecmp(word, "TIMEZONE") == 0)
        {
            cfg->timezone_offset = atoi(rest);
            cfg->timezone_is_manual = 1;
        }
        else if (strcasecmp(word, "HARDWRAP") == 0)
        {
            /* YES=hard CR at wrap col, NO=soft-wrap visual only */
            cfg->hard_wrap = parse_yesno(rest);
        }
        else if (strcasecmp(word, "LINENUMBERS") == 0)
        {
            /* YES=show line numbers, NO=hide */
            cfg->show_line_numbers = parse_yesno(rest);
        }
        else if (strcasecmp(word, "SHOWWHITESPACE") == 0)
        {
            cfg->show_whitespace = parse_yesno(rest);
        }
        else if (strcasecmp(word, "SHOWBRACKETS") == 0)
        {
            cfg->show_brackets = parse_yesno(rest);
        }
        else if (strcasecmp(word, "HIGHLIGHTLINE") == 0)
        {
            cfg->highlight_line = parse_yesno(rest);
        }
        else if (strcasecmp(word, "WORDCOUNT") == 0)
        {
            cfg->word_count = parse_yesno(rest);
        }
        else if (strcasecmp(word, "AUTOCLOSE") == 0)
        {
            cfg->autoclose = parse_yesno(rest);
        }
        else if (strcasecmp(word, "SMARTINDENT") == 0)
        {
            cfg->smart_indent = parse_yesno(rest);
        }
        else if (strcasecmp(word, "RULERCOL") == 0)
        {
            cfg->ruler_col = atoi(rest);

            if (cfg->ruler_col < 0)
                cfg->ruler_col = 0;

            if (cfg->ruler_col > 500)
                cfg->ruler_col = 500;
        }
        else if (strcasecmp(word, "INDENTGUIDES") == 0)
        {
            cfg->indent_guides = parse_yesno(rest);
        }
        else if (strcasecmp(word, "WRAPINDICATOR") == 0)
        {
            cfg->wrap_indicator = parse_yesno(rest);
        }
        else if (strcasecmp(word, "TABWIDTH") == 0)
        {
            cfg->tab_width = atoi(rest);

            if (cfg->tab_width < 1)
                cfg->tab_width = 1;

            if (cfg->tab_width > 16)
                cfg->tab_width = 16;
        }
        else if (strcasecmp(word, "ASSIST_SMART_QUOTES") == 0)
        {
            cfg->assist_smart_quotes = parse_yesno(rest);
        }
        else if (strcasecmp(word, "ASSIST_AUTO_CAP") == 0)
        {
            cfg->assist_auto_cap = parse_yesno(rest);
        }
        else if (strcasecmp(word, "ASSIST_REPEAT_CHECK") == 0)
        {
            cfg->assist_repeat_check = parse_yesno(rest);
        }
        else if (strcasecmp(word, "WORD_MOVE_MODE") == 0)
        {
            cfg->word_move_mode = atoi(rest);
        }
        /* GoldED+ extended keywords */
        else if (strcasecmp(word, "VIEWHIDDEN") == 0)
            cfg->viewhidden = parse_yesno(rest);
        else if (strcasecmp(word, "VIEWKLUDGE") == 0)
            cfg->viewkludge = parse_yesno(rest);
        else if (strcasecmp(word, "VIEWANSI") == 0)
            cfg->viewansi = parse_yesno(rest);
        else if (strcasecmp(word, "MSGLISTFIRST") == 0)
            cfg->msglistfirst = parse_yesno(rest);
        else if (strcasecmp(word, "MSGLISTFAST") == 0)
            cfg->msglistfast = parse_yesno(rest);
        else if (strcasecmp(word, "MSGLISTHEADER") == 0)
            cfg->msglistheader = parse_yesno(rest);
        else if (strcasecmp(word, "SHOWDELETED") == 0)
            cfg->showdeleted = parse_yesno(rest);
        else if (strcasecmp(word, "MSGLISTMAX") == 0)
        {
            cfg->msglistmax = atoi(rest);

            if (cfg->msglistmax < 0)
                cfg->msglistmax = 0;
        }
        else if (strcasecmp(word, "SEARCHMAX") == 0)
        {
            cfg->search_max = atoi(rest);

            /* <=0 = use default. No upper clamp; real limit is RAM */
            if (cfg->search_max < 0)
                cfg->search_max = 0;
        }
        else if (strcasecmp(word, "AREALISTSORT") == 0)
        {
            get_token(rest, cfg->arealistsort, sizeof(cfg->arealistsort));
            strip_quotes(cfg->arealistsort);

            /* Save config value as default */
            strncpy(cfg->arealistsort_default, cfg->arealistsort, sizeof(cfg->arealistsort_default) - 1);
            cfg->arealistsort_default[sizeof(cfg->arealistsort_default) - 1] = '\0';
        }
        else if (strcasecmp(word, "AREALISTFORMAT") == 0)
        {
            copy_rest(rest, cfg->arealistformat, sizeof(cfg->arealistformat));
            strip_quotes(cfg->arealistformat);
        }
        else if (strcasecmp(word, "ORIGIN") == 0)
        {
            copy_rest(rest, cfg->origin, sizeof(cfg->origin));
        }
        else if (strcasecmp(word, "TEARLINE") == 0)
        {
            copy_rest(rest, cfg->tearline, sizeof(cfg->tearline));
        }
        else if (strcasecmp(word, "TAGLINEFILE") == 0)
        {
            copy_rest(rest, cfg->tagline_file, sizeof(cfg->tagline_file));
        }
        else if (strcasecmp(word, "INCLUDE") == 0)
        {
            if (cfg->nodelist_includes_count < (int)(sizeof(cfg->nodelist_includes) / sizeof(cfg->nodelist_includes[0])))
            {
                int slot = cfg->nodelist_includes_count;
                copy_rest(rest, cfg->nodelist_includes[slot], sizeof(cfg->nodelist_includes[slot]));

                if (cfg->nodelist_includes[slot][0])
                    cfg->nodelist_includes_count++;
            }
        }
#ifdef HAVE_HUNSPELL
        else if (strcasecmp(word, "SPELL_ENABLED") == 0)
        {
            cfg->spell_enabled = parse_yesno(rest);
        }
        else if (strcasecmp(word, "SPELL_DICT_PATH") == 0)
        {
            char tmp[CFG_STR_MAX];
            copy_rest(rest, tmp, sizeof(tmp));

            if (tmp[0] != '\0')
            {
                strncpy(cfg->spell_dict_path, tmp, sizeof(cfg->spell_dict_path) - 1);

                cfg->spell_dict_path[sizeof(cfg->spell_dict_path) - 1] = '\0';
            }
        }
        else if (strcasecmp(word, "SPELL_DICT_NAME") == 0)
        {
            copy_rest(rest, cfg->spell_dict_name, sizeof(cfg->spell_dict_name));
        }
        else if (strcasecmp(word, "SPELL_CUSTOM_DICT") == 0)
        {
            char tmp[CFG_STR_MAX];

            copy_rest(rest, tmp, sizeof(tmp));
            strip_quotes(tmp);

            strncpy(cfg->spell_custom_dict, tmp, sizeof(cfg->spell_custom_dict) - 1);

            cfg->spell_custom_dict[sizeof(cfg->spell_custom_dict) - 1] = '\0';
        }

#ifdef HAVE_HYPHEN
        else if (strcasecmp(word, "HYPH_ENABLED") == 0)
        {
            cfg->hyph_enabled = parse_yesno(rest);
        }
        else if (strcasecmp(word, "HYPH_DICT_PATH") == 0)
        {
            char tmp[CFG_STR_MAX];

            copy_rest(rest, tmp, sizeof(tmp));
            strip_quotes(tmp);

            if (tmp[0] != '\0')
            {
                strncpy(cfg->hyph_dict_path, tmp, sizeof(cfg->hyph_dict_path) - 1);
                cfg->hyph_dict_path[sizeof(cfg->hyph_dict_path) - 1] = '\0';
            }
        }
        else if (strcasecmp(word, "HYPH_DICT_NAME") == 0)
        {
            copy_rest(rest, cfg->hyph_dict_name, sizeof(cfg->hyph_dict_name));
        }
        else if (strcasecmp(word, "HYPH_WRAP_ENABLED") == 0)
        {
            cfg->hyph_wrap_enabled = parse_yesno(rest);
        }
#endif /* HAVE_HYPHEN */

#ifdef HAVE_MYTHES
        else if (strcasecmp(word, "THES_ENABLED") == 0)
        {
            cfg->thes_enabled = parse_yesno(rest);
        }
        else if (strcasecmp(word, "THES_DICT_PATH") == 0)
        {
            char tmp[CFG_STR_MAX];

            copy_rest(rest, tmp, sizeof(tmp));
            strip_quotes(tmp);

            if (tmp[0] != '\0')
            {
                strncpy(cfg->thes_dict_path, tmp, sizeof(cfg->thes_dict_path) - 1);
                cfg->thes_dict_path[sizeof(cfg->thes_dict_path) - 1] = '\0';
            }
        }
        else if (strcasecmp(word, "THES_DICT_NAME") == 0)
        {
            copy_rest(rest, cfg->thes_dict_name, sizeof(cfg->thes_dict_name));
        }
#endif /* HAVE_MYTHES */
#endif /* HAVE_HUNSPELL */

#ifdef HAVE_TRANSLATE
        else if (strcasecmp(word, "TRANSLATE_ENABLED") == 0)
        {
            cfg->translate_enabled = parse_yesno(rest);
        }
        else if (strcasecmp(word, "TRANSLATE_BACKEND") == 0)
        {
            char val[16];
            get_token(rest, val, sizeof(val));

            if (strcasecmp(val, "MYMEMORY") == 0)
                cfg->translate_backend = 0;
            else if (strcasecmp(val, "LIBRETRANSLATE") == 0)
                cfg->translate_backend = 1;
            else if (strcasecmp(val, "LINGVA") == 0)
                cfg->translate_backend = 2;
            else if (strcasecmp(val, "DEEPL") == 0)
                cfg->translate_backend = 4;
            else if (strcasecmp(val, "STARDICT") == 0)
                cfg->translate_backend = 10;
            else
                cfg->translate_backend = 0; /* Default to MyMemory */
        }
        else if (strcasecmp(word, "TRANSLATE_ENDPOINT") == 0)
        {
            char tmp[CFG_STR_MAX];

            copy_rest(rest, tmp, sizeof(tmp));

            strip_quotes(tmp);
            strncpy(cfg->translate_endpoint, tmp, sizeof(cfg->translate_endpoint) - 1);

            cfg->translate_endpoint[sizeof(cfg->translate_endpoint) - 1] = '\0';
        }
        else if (strcasecmp(word, "TRANSLATE_API_KEY") == 0)
        {
            char tmp[CFG_STR_MAX];

            copy_rest(rest, tmp, sizeof(tmp));
            strip_quotes(tmp);

            strncpy(cfg->translate_api_key, tmp, sizeof(cfg->translate_api_key) - 1);

            cfg->translate_api_key[sizeof(cfg->translate_api_key) - 1] = '\0';
        }
        else if (strcasecmp(word, "TRANSLATE_EMAIL") == 0)
        {
            char tmp[CFG_STR_MAX];

            copy_rest(rest, tmp, sizeof(tmp));
            strip_quotes(tmp);

            strncpy(cfg->translate_email, tmp, sizeof(cfg->translate_email) - 1);

            cfg->translate_email[sizeof(cfg->translate_email) - 1] = '\0';
        }
        else if (strcasecmp(word, "TRANSLATE_FROM_LANG") == 0)
        {
            char tmp[16];

            get_token(rest, tmp, sizeof(tmp));

            strncpy(cfg->translate_from_lang, tmp, sizeof(cfg->translate_from_lang) - 1);

            cfg->translate_from_lang[sizeof(cfg->translate_from_lang) - 1] = '\0';
        }
        else if (strcasecmp(word, "TRANSLATE_TO_LANG") == 0)
        {
            char tmp[16];

            get_token(rest, tmp, sizeof(tmp));

            strncpy(cfg->translate_to_lang, tmp, sizeof(cfg->translate_to_lang) - 1);

            cfg->translate_to_lang[sizeof(cfg->translate_to_lang) - 1] = '\0';
        }
        else if (strcasecmp(word, "TRANSLATE_TIMEOUT") == 0)
        {
            cfg->translate_timeout = atoi(rest);

            if (cfg->translate_timeout < 1)
                cfg->translate_timeout = 1;

            if (cfg->translate_timeout > 60)
                cfg->translate_timeout = 60;
        }
        else if (strcasecmp(word, "STARDICT_PATH") == 0)
        {
            strncpy(cfg->stardict_path, rest, sizeof(cfg->stardict_path) - 1);
            cfg->stardict_path[sizeof(cfg->stardict_path) - 1] = '\0';
        }
#endif /* HAVE_TRANSLATE */

        else if (strcasecmp(word, "TEMPLATEFILE") == 0)
        {
            copy_rest(rest, cfg->template_file, sizeof(cfg->template_file));
        }
        else if (strcasecmp(word, "GREETING") == 0)
        {
            cfg->greeting = parse_yesno(rest);
        }
        else if (strcasecmp(word, "GREETINGTEXT") == 0)
        {
            copy_rest(rest, cfg->greeting_text, sizeof(cfg->greeting_text));
            strip_quotes(cfg->greeting_text);
        }
        else if (strcasecmp(word, "ATTRIBUTION") == 0)
        {
            cfg->attribution = parse_yesno(rest);
        }
        else if (strcasecmp(word, "ATTRIBSELF") == 0)
        {
            copy_rest(rest, cfg->attrib_self, sizeof(cfg->attrib_self));
            strip_quotes(cfg->attrib_self);
        }
        else if (strcasecmp(word, "ATTRIBOTHER") == 0)
        {
            copy_rest(rest, cfg->attrib_other, sizeof(cfg->attrib_other));
            strip_quotes(cfg->attrib_other);
        }
        else if (strcasecmp(word, "SIGNATURE") == 0)
        {
            cfg->signature = parse_yesno(rest);
        }
        else if (strcasecmp(word, "SIGNATURETEXT") == 0)
        {
            copy_rest(rest, cfg->signature_text, sizeof(cfg->signature_text));
            strip_quotes(cfg->signature_text);
        }
        else if (strcasecmp(word, "COLORMAP") == 0)
        {
            /* COLORMAP <color_name> <number> */
            char cname[16];
            const char *p = rest;
            int nc = 0;
            int color_num;

            while (*p == ' ' || *p == '\t')
                p++;

            while (*p && *p != ' ' && *p != '\t' && nc < 15)
                cname[nc++] = *p++;

            while (*p == ' ' || *p == '\t')
                p++;

            cname[nc] = '\0';
            color_num = atoi(p);

            /* Map color name to array index */
            if (strcasecmp(cname, "black") == 0)
            {
                cfg->color_map[0] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "red") == 0)
            {
                cfg->color_map[1] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "green") == 0)
            {
                cfg->color_map[2] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "yellow") == 0)
            {
                cfg->color_map[3] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "blue") == 0)
            {
                cfg->color_map[4] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "magenta") == 0)
            {
                cfg->color_map[5] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "cyan") == 0)
            {
                cfg->color_map[6] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "white") == 0)
            {
                cfg->color_map[7] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "brightblack") == 0)
            {
                cfg->color_map[8] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "brightred") == 0)
            {
                cfg->color_map[9] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "brightgreen") == 0)
            {
                cfg->color_map[10] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "brightyellow") == 0)
            {
                cfg->color_map[11] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "brightblue") == 0)
            {
                cfg->color_map[12] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "brightmagenta") == 0)
            {
                cfg->color_map[13] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "brightcyan") == 0)
            {
                cfg->color_map[14] = color_num;
                cfg->color_map_initialized = 1;
            }
            else if (strcasecmp(cname, "brightwhite") == 0)
            {
                cfg->color_map[15] = color_num;
                cfg->color_map_initialized = 1;
            }
        }
        else if (strcasecmp(word, "COLOR") == 0)
        {
            /* COLOR <pair> <fg> <bg> */
            char pname[32], fgname[16], bgname[16];
            const char *p = rest;
            int np, nf, nb;
            np = nf = nb = 0;

            while (*p == ' ' || *p == '\t')
                p++;

            while (*p && *p != ' ' && *p != '\t' && np < 31)
                pname[np++] = *p++;

            while (*p == ' ' || *p == '\t')
                p++;

            while (*p && *p != ' ' && *p != '\t' && nf < 15)
                fgname[nf++] = *p++;

            while (*p == ' ' || *p == '\t')
                p++;

            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && nb < 15)
                bgname[nb++] = *p++;

            pname[np] = fgname[nf] = bgname[nb] = '\0';

            pi = pair_by_name(pname);
            fi = color_by_name_base(fgname);
            bi = color_by_name_base(bgname);

            if (pi >= 1 && pi < CFG_COLOR_MAX && fi >= 0 && bi >= 0)
            {
                cfg->color_fg[pi] = fi;
                cfg->color_bg[pi] = bi;
                cfg->color_explicit[pi] = 1; /* user set this; don't remap through COLORMAP again at apply time */
            }
        }
        else if (strcasecmp(word, "CURSORCOLOR") == 0)
        {
            /* CURSORCOLOR <name|pen|#rgb>: name, pen index, or RGB hex */
            const char *p = rest;
            char val[24];
            int n = 0;

            while (*p == ' ' || *p == '\t')
                p++;

            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && n < 23)
                val[n++] = *p++;

            val[n] = '\0';

            if (val[0] == '#')
            {
                /* Keep literal #RRGGBB for OSC 12 */
                strncpy(cfg->cursor_color_rgb, val, sizeof(cfg->cursor_color_rgb) - 1);

                cfg->cursor_color_rgb[sizeof(cfg->cursor_color_rgb) - 1] = '\0';
                cfg->cursor_color = -1;
            }
            else if ((val[0] >= '0' && val[0] <= '9') || val[0] == '-')
            {
                /* Numeric pen/color index */
                cfg->cursor_color = atoi(val);
                cfg->cursor_color_rgb[0] = '\0';
            }
            else if (val[0])
            {
                /* Color name to ncurses index */
                int ci = color_by_name(val, cfg);
                if (ci >= 0)
                {
                    cfg->cursor_color = ci;
                    cfg->cursor_color_rgb[0] = '\0';
                }
            }
        }
        else if (strcasecmp(word, "MOUSE_ENABLED") == 0)
        {
            char val[16];
            const char *p = rest;
            int n = 0;

            while (*p == ' ' || *p == '\t')
                p++;

            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && n < 15)
                val[n++] = *p++;

            val[n] = '\0';

            if (strcasecmp(val, "0") == 0 || strcasecmp(val, "OFF") == 0 || strcasecmp(val, "NO") == 0 || strcasecmp(val, "FALSE") == 0)
                cfg->mouse_enabled = 0;
            else
                cfg->mouse_enabled = 1;
        }
        else if (strcasecmp(word, "DEFAULT_BG_COLOR") == 0)
        {
            /* Parse DEFAULT_BG_COLOR (name or 0-7) */
            const char *p = rest;
            char val[24];
            int n = 0;

            while (*p == ' ' || *p == '\t')
                p++;

            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && n < 23)
                val[n++] = *p++;

            val[n] = '\0';

            if (val[0] >= '0' && val[0] <= '9')
            {
                cfg->default_bg_color = atoi(val);
            }
            else
            {
                cfg->default_bg_color = color_by_name(val, cfg);
            }
        }
        else if (strcasecmp(word, "FORCEINTL") == 0)
        {
            cfg->forceintl = atoi(rest);

            if (cfg->forceintl < 0)
                cfg->forceintl = 0;

            if (cfg->forceintl > 2)
                cfg->forceintl = 2;
        }
        else if (strcasecmp(word, "AUTOWRAP") == 0)
        {
            cfg->autowrap_col = atoi(rest);

            if (cfg->autowrap_col < 0)
                cfg->autowrap_col = 0;

            if (cfg->autowrap_col > 0 && cfg->autowrap_col < 20)
                cfg->autowrap_col = 75;
        }
        else if (strcasecmp(word, "UNDOLEVELS") == 0)
        {
            cfg->undo_levels = atoi(rest);

            if (cfg->undo_levels < 1)
                cfg->undo_levels = 1;

            if (cfg->undo_levels > 1000)
                cfg->undo_levels = 1000;
        }
        else if (strcasecmp(word, "QUOTEMARGIN") == 0)
        {
            cfg->quotemargin = atoi(rest);

            if (cfg->quotemargin < 0)
                cfg->quotemargin = 0;

            if (cfg->quotemargin > 0 && cfg->quotemargin < 20)
                cfg->quotemargin = 75;

            if (cfg->quotemargin > 200)
                cfg->quotemargin = 200;
        }
        else if (strcasecmp(word, "FONT") == 0)
        {
            copy_rest(rest, cfg->font, sizeof(cfg->font));
            strip_quotes(cfg->font);
        }
        else if (strcasecmp(word, "ANSIFONT") == 0)
        {
            copy_rest(rest, cfg->ansifont, sizeof(cfg->ansifont));
            strip_quotes(cfg->ansifont);
        }
        else if (strcasecmp(word, "TTF_ENABLED") == 0)
        {
            cfg->ttf_enabled = parse_yesno(rest);
        }
        else if (strcasecmp(word, "TTF_FONT") == 0)
        {
            char tmp[CFG_STR_MAX];

            copy_rest(rest, tmp, sizeof(tmp));
            strip_quotes(tmp);
            strncpy(cfg->ttf_font, tmp, sizeof(cfg->ttf_font) - 1);

            cfg->ttf_font[sizeof(cfg->ttf_font) - 1] = '\0';
        }
        else if (strcasecmp(word, "TTF_SIZE") == 0)
        {
            char val[16];

            get_token(rest, val, sizeof(val));

            cfg->ttf_size = atoi(val);

            if (cfg->ttf_size < 6 || cfg->ttf_size > 96)
                cfg->ttf_size = 14;
        }
        else if (strcasecmp(word, "TTF_ANTIALIAS") == 0)
        {
            char val[16];

            get_token(rest, val, sizeof(val));

            if (strcasecmp(val, "ON") == 0 || strcasecmp(val, "YES") == 0)
                cfg->ttf_antialias = 2;
            else if (strcasecmp(val, "OFF") == 0 || strcasecmp(val, "NO") == 0)
                cfg->ttf_antialias = 1;
            else
                cfg->ttf_antialias = 0; /* auto */
        }
        else if (strcasecmp(word, "TTF_USE_UTF8") == 0)
        {
            char val[16];

            get_token(rest, val, sizeof(val));

            if (strcasecmp(val, "ON") == 0 || strcasecmp(val, "YES") == 0 || strcasecmp(val, "1") == 0)
                cfg->ttf_use_utf8 = 1;
            else if (strcasecmp(val, "OFF") == 0 || strcasecmp(val, "NO") == 0 || strcasecmp(val, "0") == 0)
                cfg->ttf_use_utf8 = 0;
            else
                cfg->ttf_use_utf8 = 1; /* default to UTF-8 */
        }
        else if (strncasecmp(word, "TTF_FALLBACK", 12) == 0)
        {
            const char *suffix = word + 12;
            int is_size = 0;
            int slot;

            if (strncasecmp(suffix, "_SIZE", 5) == 0)
            {
                is_size = 1;
                suffix += 5;
            }

            slot = atoi(suffix);

            if (slot >= 1 && slot <= CFG_TTF_FALLBACKS)
            {
                int idx = slot - 1;

                if (is_size)
                {
                    char val[16];
                    int v;

                    get_token(rest, val, sizeof(val));
                    v = atoi(val);

                    if (v >= 6 && v <= 96)
                        cfg->ttf_fallback_size[idx] = v;
                    else
                        cfg->ttf_fallback_size[idx] = 0;
                }
                else
                {
                    char tmp[CFG_STR_MAX];

                    copy_rest(rest, tmp, sizeof(tmp));
                    strip_quotes(tmp);
                    strncpy(cfg->ttf_fallback[idx], tmp, sizeof(cfg->ttf_fallback[idx]) - 1);

                    cfg->ttf_fallback[idx][sizeof(cfg->ttf_fallback[idx]) - 1] = '\0';
                }
            }
        }
    }

    fclose(f);

#ifdef PLATFORM_AMIGA
    /* Fix ttf_use_utf8 for old configs missing this field */
    if (cfg->ttf_use_utf8 != 0 && cfg->ttf_use_utf8 != 1)
        cfg->ttf_use_utf8 = 1; /* default to UTF-8 */
#endif

    return 0;
}

static const char *cfg_mode_str(int mode)
{
    switch (mode)
    {
    case FREQ_MODE_ASO:
        return "aso";
    case FREQ_MODE_BSO:
        return "bso";
    case FREQ_MODE_BSO_EXT:
        return "bso-ext";
    default:
        return "";
    }
}

static const char *color_name(int color)
{
    static const char *names[] =
        {
            "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
            "brightblack", "brightred", "brightgreen", "brightyellow",
            "brightblue", "brightmagenta", "brightcyan", "brightwhite"};

    if (color >= 0 && color < 16)
        return names[color];

    return "black";
}

/* Quote if contains ;, #, or leading/trailing whitespace */
static void cfg_emit(FILE *f, const char *key, const char *val)
{
    int needq = 0;
    size_t n = strlen(val);

    if (n > 0 && (val[0] == ' ' || val[0] == '\t' || val[n - 1] == ' ' || val[n - 1] == '\t'))
        needq = 1;

    if (strchr(val, ';') || strchr(val, '#'))
        needq = 1;

    if (needq)
        fprintf(f, "%s \"%s\"\n", key, val);
    else
        fprintf(f, "%s %s\n", key, val);
}

int cfg_save(const CrashEditCfg *cfg, const char *path)
{
    CfgKV *kv = NULL;
    int nkv = 0;
    int kv_cap = 0;
    FILE *in = NULL;
    FILE *out = NULL;
    char tmp_path[CFG_STR_MAX + 8];
    char line[1024];
    int i;
    const char *aa_str = NULL;
    int fi;

#define KV_STR(k, s)                                       \
    do                                                     \
    {                                                      \
        size_t _n = strlen(s);                             \
        char _buf[CFG_STR_MAX];                            \
        if (_n >= CFG_STR_MAX)                             \
            _n = CFG_STR_MAX - 1;                          \
        memcpy(_buf, (s), _n);                             \
        _buf[_n] = '\0';                                   \
        if (kv_append(&kv, &nkv, &kv_cap, (k), _buf) != 0) \
        {                                                  \
            free(kv);                                      \
            return -1;                                     \
        }                                                  \
    } while (0)
#define KV_INT(k, v)                                       \
    do                                                     \
    {                                                      \
        char _buf[CFG_STR_MAX];                            \
        snprintf(_buf, CFG_STR_MAX, "%d", (v));            \
        if (kv_append(&kv, &nkv, &kv_cap, (k), _buf) != 0) \
        {                                                  \
            free(kv);                                      \
            return -1;                                     \
        }                                                  \
    } while (0)
#define KV_YN(k, v)                                                      \
    do                                                                   \
    {                                                                    \
        if (kv_append(&kv, &nkv, &kv_cap, (k), (v) ? "yes" : "no") != 0) \
        {                                                                \
            free(kv);                                                    \
            return -1;                                                   \
        }                                                                \
    } while (0)

    if (!cfg || !path || !path[0])
        return -1;

    /* Identity and paths */
    KV_STR("SYSOP", cfg->sysop);
    KV_STR("AREAFILE", cfg->areafile);
    KV_STR("OUTBOUND", cfg->freq_outbound);

    if (cfg->freq_mode != FREQ_MODE_UNSET)
        KV_STR("OUTBOUNDMODE", cfg_mode_str(cfg->freq_mode));

    KV_STR("CHARSET", cfg->charset[0] ? cfg->charset : "AUTO");

    if (cfg->timezone_is_manual)
        KV_INT("TIMEZONE", cfg->timezone_offset);

    /* Display settings */
    KV_YN("VIEWHIDDEN", cfg->viewhidden);
    KV_YN("VIEWKLUDGE", cfg->viewkludge);
    KV_YN("VIEWANSI", cfg->viewansi);
    KV_YN("MSGLISTFIRST", cfg->msglistfirst);
    KV_YN("MSGLISTFAST", cfg->msglistfast);
    KV_YN("MSGLISTHEADER", cfg->msglistheader);
    KV_YN("SHOWDELETED", cfg->showdeleted);
    KV_INT("MSGLISTMAX", cfg->msglistmax);
    KV_INT("SEARCHMAX", cfg->search_max);
    KV_STR("AREALISTSORT", cfg->arealistsort_default[0] ? cfg->arealistsort_default : cfg->arealistsort);
    KV_STR("AREALISTFORMAT", cfg->arealistformat);

    /* Editor and signature */
    KV_STR("ORIGIN", cfg->origin);
    KV_STR("TEARLINE", cfg->tearline);
    KV_STR("TAGLINEFILE", cfg->tagline_file);
    KV_STR("TEMPLATEFILE", cfg->template_file);
    KV_INT("FORCEINTL", cfg->forceintl);
    KV_INT("AUTOWRAP", cfg->autowrap_col);
    KV_INT("QUOTEMARGIN", cfg->quotemargin);
    KV_INT("UNDOLEVELS", cfg->undo_levels);
    KV_YN("HARDWRAP", cfg->hard_wrap);
    KV_YN("LINENUMBERS", cfg->show_line_numbers);
    KV_YN("SHOWWHITESPACE", cfg->show_whitespace);
    KV_YN("SHOWBRACKETS", cfg->show_brackets);
    KV_YN("HIGHLIGHTLINE", cfg->highlight_line);
    KV_YN("WORDCOUNT", cfg->word_count);
    KV_YN("AUTOCLOSE", cfg->autoclose);
    KV_YN("SMARTINDENT", cfg->smart_indent);
    KV_INT("RULERCOL", cfg->ruler_col);
    KV_YN("INDENTGUIDES", cfg->indent_guides);
    KV_YN("WRAPINDICATOR", cfg->wrap_indicator);
    KV_INT("TABWIDTH", cfg->tab_width);
    KV_STR("FONT", cfg->font);
    KV_STR("ANSIFONT", cfg->ansifont);

    /* TrueType font (Amiga) */
    KV_YN("TTF_ENABLED", cfg->ttf_enabled);
    KV_STR("TTF_FONT", cfg->ttf_font);
    KV_INT("TTF_SIZE", cfg->ttf_size);

    aa_str = "AUTO";

    if (cfg->ttf_antialias == 2)
        aa_str = "ON";
    else if (cfg->ttf_antialias == 1)
        aa_str = "OFF";

    KV_STR("TTF_ANTIALIAS", aa_str);
    KV_YN("TTF_USE_UTF8", cfg->ttf_use_utf8);

    /* Cursor and background colors */
    if (cfg->cursor_color_rgb[0])
        KV_STR("CURSORCOLOR", cfg->cursor_color_rgb);
    else
        KV_INT("CURSORCOLOR", cfg->cursor_color);

    KV_INT("DEFAULT_BG_COLOR", cfg->default_bg_color);

    /* Mouse support */
    KV_YN("MOUSE_ENABLED", cfg->mouse_enabled);

    /* Editor assists */
    KV_YN("ASSIST_SMART_QUOTES", cfg->assist_smart_quotes);
    KV_YN("ASSIST_AUTO_CAP", cfg->assist_auto_cap);
    KV_YN("ASSIST_REPEAT_CHECK", cfg->assist_repeat_check);
    KV_INT("WORD_MOVE_MODE", cfg->word_move_mode);

    /* Message framing */
    KV_YN("GREETING", cfg->greeting);
    KV_STR("GREETINGTEXT", cfg->greeting_text);
    KV_YN("ATTRIBUTION", cfg->attribution);
    KV_STR("ATTRIBSELF", cfg->attrib_self);
    KV_STR("ATTRIBOTHER", cfg->attrib_other);
    KV_YN("SIGNATURE", cfg->signature);
    KV_STR("SIGNATURETEXT", cfg->signature_text);

#ifdef HAVE_HUNSPELL
    KV_YN("SPELL_ENABLED", cfg->spell_enabled);
    KV_STR("SPELL_DICT_PATH", cfg->spell_dict_path);
    KV_STR("SPELL_DICT_NAME", cfg->spell_dict_name);
    KV_STR("SPELL_CUSTOM_DICT", cfg->spell_custom_dict);

#ifdef HAVE_HYPHEN
    KV_YN("HYPH_ENABLED", cfg->hyph_enabled);
    KV_STR("HYPH_DICT_PATH", cfg->hyph_dict_path);
    KV_STR("HYPH_DICT_NAME", cfg->hyph_dict_name);
    KV_YN("HYPH_WRAP_ENABLED", cfg->hyph_wrap_enabled);
#endif

#ifdef HAVE_MYTHES
    KV_YN("THES_ENABLED", cfg->thes_enabled);
    KV_STR("THES_DICT_PATH", cfg->thes_dict_path);
    KV_STR("THES_DICT_NAME", cfg->thes_dict_name);
#endif
#endif

#ifdef HAVE_TRANSLATE
    const char *backend_name = "MYMEMORY";

    KV_YN("TRANSLATE_ENABLED", cfg->translate_enabled);

    if (cfg->translate_backend == 1)
        backend_name = "LIBRETRANSLATE";
    else if (cfg->translate_backend == 2)
        backend_name = "LINGVA";
    else if (cfg->translate_backend == 4)
        backend_name = "DEEPL";
    else if (cfg->translate_backend == 10)
        backend_name = "STARDICT";
    else
        backend_name = "MYMEMORY";

    KV_STR("TRANSLATE_BACKEND", backend_name);
    KV_STR("TRANSLATE_ENDPOINT", cfg->translate_endpoint);
    KV_STR("TRANSLATE_API_KEY", cfg->translate_api_key);
    KV_STR("TRANSLATE_EMAIL", cfg->translate_email);
    KV_STR("TRANSLATE_FROM_LANG", cfg->translate_from_lang);
    KV_STR("TRANSLATE_TO_LANG", cfg->translate_to_lang);
    KV_INT("TRANSLATE_TIMEOUT", cfg->translate_timeout);
    KV_STR("STARDICT_PATH", cfg->stardict_path);
#endif

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    out = fopen(tmp_path, "w");

    if (!out)
    {
        free(kv);
        return -1;
    }

    /* Stream original, rewriting managed keywords in place */
    in = fopen(path, "r");

    if (in)
    {
        while (fgets(line, sizeof(line), in))
        {
            char word[64];
            char *p = line;
            int wi = 0;
            int matched = 0;

            while (*p == ' ' || *p == '\t')
                p++;

            /* Comment/blank: copy verbatim */
            if (*p == '#' || *p == ';' || *p == '\0' || *p == '\r' || *p == '\n')
            {
                fputs(line, out);
                continue;
            }

            while (p[wi] && p[wi] != ' ' && p[wi] != '\t' && p[wi] != '\r' && p[wi] != '\n' && wi < (int)sizeof(word) - 1)
            {
                word[wi] = p[wi];
                wi++;
            }

            word[wi] = '\0';

            if (cfg->color_map_initialized && strcasecmp(word, "COLORMAP") == 0)
                continue; /* drop; will re-emit below */

            /* Skip TTF_FALLBACK lines - they will be rewritten at the end with fprintf */
            if (strncasecmp(word, "TTF_FALLBACK", 12) == 0)
                continue;

            for (i = 0; i < nkv; i++)
            {
                if (!kv[i].done && strcasecmp(word, kv[i].key) == 0)
                {
                    cfg_emit(out, kv[i].key, kv[i].val);
                    kv[i].done = 1;
                    matched = 1;
                    break;
                }
                else if (kv[i].done && strcasecmp(word, kv[i].key) == 0)
                {
                    /* Skip duplicate lines for keys already processed */
                    matched = 1;
                    break;
                }
            }

            if (!matched)
                fputs(line, out);
        }

        fclose(in);
    }

    /* Append managed keywords not already present */
    for (i = 0; i < nkv; i++)
    {
        if (!kv[i].done)
            cfg_emit(out, kv[i].key, kv[i].val);
    }

    if (cfg->color_map_initialized)
    {
        static const char *cmap_names[16] =
            {
                "black", "red", "green", "yellow",
                "blue", "magenta", "cyan", "white",
                "brightblack", "brightred", "brightgreen", "brightyellow",
                "brightblue", "brightmagenta", "brightcyan", "brightwhite"};
        int ci;

        for (ci = 0; ci < 16; ci++)
            fprintf(out, "COLORMAP %s %d\n", cmap_names[ci], cfg->color_map[ci]);
    }

    /* TTF fallbacks: write directly with fprintf to avoid KV system bugs */
    for (fi = 0; fi < CFG_TTF_FALLBACKS; fi++)
    {
        if (cfg->ttf_fallback[fi][0])
        {
            fprintf(out, "TTF_FALLBACK%d %s\n", fi + 1, cfg->ttf_fallback[fi]);

            if (cfg->ttf_fallback_size[fi] > 0)
                fprintf(out, "TTF_FALLBACK_SIZE%d %d\n", fi + 1, cfg->ttf_fallback_size[fi]);
        }
    }

    /* Save color settings */
    for (i = 1; i < CFG_COLOR_MAX; i++)
    {
        const char *name = NULL;

        switch (i)
        {
        case 1:
            name = "NORMAL";
            break;
        case 2:
            name = "SELECTED";
            break;
        case 3:
            name = "HEADER";
            break;
        case 4:
            name = "STATUS";
            break;
        case 5:
            name = "MENU";
            break;
        case 6:
            name = "MENUHOT";
            break;
        case 7:
            name = "BORDER";
            break;
        case 8:
            name = "QUOTE1";
            break;
        case 9:
            name = "QUOTE2";
            break;
        case 10:
            name = "QUOTE3";
            break;
        case 11:
            name = "QUOTE4";
            break;
        case 12:
            name = "KLUDGE";
            break;
        case 13:
            name = "TEAR";
            break;
        case 14:
            name = "ORIGIN";
            break;
        case 15:
            name = "SEENBY";
            break;
        case 16:
            name = "VIA";
            break;
        case 17:
            name = "UNREAD";
            break;
        case 18:
            name = "DELETED";
            break;
        case 19:
            name = "POPUP";
            break;
        case 20:
            name = "POPUPSEL";
            break;
        case 21:
            name = "ERROR";
            break;
        case 22:
            name = "TAGLINE";
            break;
        case 23:
            name = "SEARCH";
            break;
        case 24:
            name = "SPELLCURRENT";
            break;
        case 25:
            name = "BRACKETMATCH";
            break;
        case 26:
            name = "CURRENTLINE";
            break;
        case 27:
            name = "GUIDES";
            break;
        }

        if (name)
            fprintf(out, "COLOR %s %s %s\n", name, color_name(cfg->color_fg[i]), color_name(cfg->color_bg[i]));
    }

    if (fclose(out) != 0)
    {
        pf_remove_file(tmp_path);
        free(kv);
        return -1;
    }

    /* Atomic replace */
    if (pf_atomic_rename(tmp_path, path) != 0)
    {
        free(kv);
        return -1;
    }

    free(kv);
    return 0;

#undef KV_STR
#undef KV_INT
#undef KV_YN
}
