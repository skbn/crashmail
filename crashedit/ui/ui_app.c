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

/* ui.c -- Main app, init/cleanup, run loop, shared helpers */
#include "../components/config.h"
#include "../core/ansi.h"
#include "../core/ftn.h"
#include "ui.h"
#include "ui_internal.h"
#include "ui_spell.h"
#include "ui_grammar.h"

#ifdef HAVE_HYPHEN
#include "ui_hyph.h"
#endif
#ifdef HAVE_MYTHES
#include "ui_thes.h"
#endif
#ifdef HAVE_TRANSLATE
#include "ui_translate.h"
#endif
#ifdef HAVE_TTS
#include "ui_tts.h"
#endif
#include <locale.h>

#if !defined(PLATFORM_WIN32) && !defined(PLATFORM_AMIGA)
#include <langinfo.h> /* nl_langinfo(CODESET) for UTF-8 locale detection (fix BSD) */
#endif
#ifdef PLATFORM_WIN32
#include <windows.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Cursor color setup (platform-specific) */
static void ui_apply_cursor_color(const CrashEditCfg *cfg)
{
    int color_to_use;

#if !defined(PLATFORM_AMIGA) && !defined(PLATFORM_WIN32)
    FILE *tty = NULL;
#endif

#ifdef PLATFORM_AMIGA
    /* Amiga: use pen index */
    if (cfg && cfg->cursor_color >= 0)
        color_to_use = cfg->cursor_color;
    else
        color_to_use = 3; /* default pen 3 if cfg NULL or unset */

    amiga_set_cursor_pen(color_to_use);

#elif defined(PLATFORM_WIN32)
    /* Windows: use ncurses color index */
    if (cfg && cfg->cursor_color >= 0)
        color_to_use = cfg->cursor_color;
    else
        color_to_use = COLOR_WHITE; /* default white if cfg NULL or unset */

    win32_set_cursor_pen(color_to_use);
#else

    /* Linux/Unix: send OSC 12 escape sequence to terminal emulator via /dev/tty */
    tty = fopen("/dev/tty", "w");

    if (!tty)
        return;

    if (cfg && cfg->cursor_color_rgb[0] != '\0')
    {
        /* RGB hex form: #RRGGBB */
        fprintf(tty, "\033]12;%s\007", cfg->cursor_color_rgb);
    }
    else
    {
        const char *color_names[] = {"black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"};

        /* Determine color: from config or default */
        if (cfg && cfg->cursor_color >= 0 && cfg->cursor_color <= 7)
            color_to_use = cfg->cursor_color;
        else
            color_to_use = 7; /* default white if cfg NULL or unset */

        fprintf(tty, "\033]12;%s\007", color_names[color_to_use]);
    }

    fflush(tty);
    fclose(tty);
#endif
}

static void setup_colors(const CrashEditCfg *cfg)
{
    int i;
    short ansi_col[16];
    int fg, bg;

    if (!has_colors())
        return;

    start_color();
    use_default_colors();

    /* Apply cursor color after ncurses initialization */
    ui_apply_cursor_color(cfg);

    if (cfg)
    {
        /* Apply colors, remap non-explicit slots via color_map */
        for (i = 1; i < CFG_COLOR_MAX; i++)
        {
            int fg = cfg->color_fg[i];
            int bg = cfg->color_bg[i];

            if (!cfg->color_explicit[i] && cfg->color_map_initialized)
            {
                if (fg >= 0 && fg < 16)
                    fg = cfg->color_map[fg];

                if (bg >= 0 && bg < 16)
                    bg = cfg->color_map[bg];
            }

            init_pair(i, fg, bg);
        }

        /* Apply default background to entire screen */
        bkgd(COLOR_PAIR(COL_NORMAL) | ' ');

        erase();
        refresh();
    }
    else
    {
        /* Fallback hardcoded defaults */
        init_pair(COL_NORMAL, COLOR_WHITE, COLOR_BLACK);
        init_pair(COL_SELECTED, COLOR_BLACK, COLOR_CYAN);
        init_pair(COL_HEADER, COLOR_YELLOW, COLOR_BLUE);
        init_pair(COL_STATUS, COLOR_BLACK, COLOR_WHITE);
        init_pair(COL_MENU, COLOR_BLACK, COLOR_WHITE);
        init_pair(COL_MENU_HOT, COLOR_RED, COLOR_WHITE);
        init_pair(COL_BORDER, COLOR_CYAN, COLOR_BLACK);
        init_pair(COL_QUOTE1, COLOR_GREEN, COLOR_BLACK);
        init_pair(COL_QUOTE2, COLOR_CYAN, COLOR_BLACK);
        init_pair(COL_QUOTE3, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(COL_QUOTE4, COLOR_BLUE, COLOR_BLACK);
        init_pair(COL_KLUDGE, COLOR_CYAN, COLOR_BLACK);
        init_pair(COL_TEAR, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COL_ORIGIN, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COL_SEENBY, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(COL_VIA, COLOR_BLUE, COLOR_BLACK);
        init_pair(COL_UNREAD, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COL_DELETED, COLOR_RED, COLOR_BLACK);
        init_pair(COL_POPUP, COLOR_WHITE, COLOR_BLUE);
        init_pair(COL_POPUP_SEL, COLOR_BLACK, COLOR_CYAN);
        init_pair(COL_ERROR, COLOR_WHITE, COLOR_RED);
        init_pair(COL_TAGLINE, COLOR_CYAN, COLOR_BLACK);
        init_pair(COL_SEARCH_MATCH, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COL_SPELL_CURRENT, COLOR_WHITE, COLOR_MAGENTA);
        init_pair(COL_BRACKET_MATCH, COLOR_BLACK, COLOR_YELLOW);
        init_pair(COL_CURRENT_LINE, COLOR_WHITE, COLOR_BLUE);
        init_pair(COL_GUIDE, COLOR_CYAN, COLOR_BLACK);
    }

    if (cfg && cfg->color_map_initialized)
    {
        ansi_col[0] = cfg->color_map[0];
        ansi_col[1] = cfg->color_map[1];
        ansi_col[2] = cfg->color_map[2];
        ansi_col[3] = cfg->color_map[3];
        ansi_col[4] = cfg->color_map[4];
        ansi_col[5] = cfg->color_map[5];
        ansi_col[6] = cfg->color_map[6];
        ansi_col[7] = cfg->color_map[7];
        ansi_col[8] = cfg->color_map[8];
        ansi_col[9] = cfg->color_map[9];
        ansi_col[10] = cfg->color_map[10];
        ansi_col[11] = cfg->color_map[11];
        ansi_col[12] = cfg->color_map[12];
        ansi_col[13] = cfg->color_map[13];
        ansi_col[14] = cfg->color_map[14];
        ansi_col[15] = cfg->color_map[15];
    }
    else
    {
        ansi_col[0] = COLOR_BLACK;
        ansi_col[1] = COLOR_RED;
        ansi_col[2] = COLOR_GREEN;
        ansi_col[3] = COLOR_YELLOW;
        ansi_col[4] = COLOR_BLUE;
        ansi_col[5] = COLOR_MAGENTA;
        ansi_col[6] = COLOR_CYAN;
        ansi_col[7] = COLOR_WHITE;
        ansi_col[8] = 8;
        ansi_col[9] = 9;
        ansi_col[10] = 10;
        ansi_col[11] = 11;
        ansi_col[12] = 12;
        ansi_col[13] = 13;
        ansi_col[14] = 14;
        ansi_col[15] = 15;
    }

    for (bg = 0; bg < 16; bg++)
    {
        for (fg = 0; fg < 16; fg++)
            init_pair((short)ANSI_PAIR(fg, bg), ansi_col[fg], ansi_col[bg]);
    }

    erase();
    refresh();
}

#if !defined(PLATFORM_AMIGA) && !defined(PLATFORM_WIN32)
/* Configure mouse based on setting */
static void ui_configure_mouse(int enabled)
{
    if (enabled)
    {
        mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
        mouseinterval(0);

        printf("\033[?1002h");
        printf("\033[?1006h");
        fflush(stdout);
    }
    else
    {
        mousemask(0, NULL);

        printf("\033[?1002l");
        printf("\033[?1006l");
        fflush(stdout);
    }
}
#endif

/* Apply config changes without restarting */
void ui_reapply_config(UiApp *app)
{
    if (!app || !app->cfg)
        return;

#ifdef PLATFORM_AMIGA
    if (app->cfg->ttf_enabled)
    {
        extern int amiga_reload_ttf(const char *font_path, int new_size);
        extern void amiga_clear_ttf_fallbacks(void);
        extern int amiga_add_ttf_fallback(const char *path, int size);
        int fi;

        /* Reload primary font */
        amiga_reload_ttf(app->cfg->ttf_font, app->cfg->ttf_size);

        /* Reload fallback fonts */
        amiga_clear_ttf_fallbacks();

        for (fi = 0; fi < CFG_TTF_FALLBACKS; fi++)
        {
            if (app->cfg->ttf_fallback[fi][0])
            {
                int sz = app->cfg->ttf_fallback_size[fi] > 0 ? app->cfg->ttf_fallback_size[fi] : app->cfg->ttf_size;
                amiga_add_ttf_fallback(app->cfg->ttf_fallback[fi], sz);
            }
        }
    }

    amiga_set_default_bg_color(app->cfg->default_bg_color);
#endif

    setup_colors(app->cfg);

#ifdef HAVE_MYTHES
    /* Load thesaurus FIRST (spell checker needs to attach to it) */
    ui_thes_load_from_config(app);
#endif

#ifdef HAVE_HUNSPELL
    /* Reload Hunspell dictionary if config changed */
    ui_spell_load_from_config(app);
#endif

#ifdef HAVE_HYPHEN
    ui_hyph_load_from_config(app);
#endif

#ifdef HAVE_TRANSLATE
    /* Reload translator config */
    app->translate_enabled = app->cfg->translate_enabled;
    app->translate_active = app->cfg->translate_enabled;

    /* Load translator from config */
    ui_translate_load_from_config(app);
#endif

#ifdef HAVE_TTS
    /* Load TTS from config, idempotent, degrades silently if unavailable */
    ui_tts_load_from_config(app);
#endif

#if !defined(PLATFORM_AMIGA) && !defined(PLATFORM_WIN32)
    /* Reconfigure mouse if setting changed */
    ui_configure_mouse(app->cfg->mouse_enabled);
#endif

#ifdef PLATFORM_AMIGA
    extern void amiga_force_redraw(void);
    amiga_force_redraw();
#endif

    /* Propagate word move mode to editor */
    if (app->editor)
        ed_set_word_move_mode(app->editor, app->cfg->word_move_mode);
}

/* Status bar */
void ui_status(UiApp *app, const char *fmt, ...)
{
    va_list ap;

    if (!app)
        return;

    memset(app->status, 0, sizeof(app->status));

    va_start(ap, fmt);
    vsnprintf(app->status, sizeof(app->status), fmt, ap);
    va_end(ap);

    app->status_dirty = 1;
}

/* Build right-hand status bar (INS/OVR + time) */
static void build_status_right(const UiApp *app, char *out, int outsz)
{
    char tbuf[8];
    time_t t = time(NULL);
    struct tm *lt;
    tbuf[0] = '\0';

    /* Apply TZ offset, then gmtime() for wall-clock */
#ifndef PLATFORM_AMIGA
    if (app->cfg)
    {
        int off = ftn_effective_tz_offset(app->cfg->timezone_offset, app->cfg->timezone_is_manual);
        t += (time_t)off * 60;
    }
#endif

    lt = gmtime(&t);

    if (lt)
        strftime(tbuf, sizeof(tbuf), "%H:%M", lt);

    if (app->view == VIEW_EDITOR && app->editor)
    {
        EdInfo info;
        char sp_buf[32];
        char hy_buf[32];
        char tr_buf[32];

        sp_buf[0] = '\0';
        hy_buf[0] = '\0';
        tr_buf[0] = '\0';

        ed_get_info(app->editor, &info);

#ifdef HAVE_HUNSPELL
        if (app->spell_enabled && app->spell_active && app->spell_handle)
            snprintf(sp_buf, sizeof(sp_buf), "SP ");
#endif
#ifdef HAVE_HYPHEN
        if (app->hyph_wrap_enabled && app->hyph_handle)
            snprintf(hy_buf, sizeof(hy_buf), "HY ");
#endif
#ifdef HAVE_TRANSLATE
        if (app->translate_active && app->translate_handle)
            snprintf(tr_buf, sizeof(tr_buf), "TR [%.7s]->[%.7s] ", app->cfg->translate_from_lang, app->cfg->translate_to_lang);
#endif
        snprintf(out, (size_t)outsz, " %s %s%s%s%s %s ", app->cfg->hard_wrap ? "HARD" : "SOFT", sp_buf, hy_buf, tr_buf, info.insert_mode ? "INS" : "OVR", tbuf);
    }
    else
    {
        snprintf(out, (size_t)outsz, " %s ", tbuf);
    }
}

/* Build centre status bar (msg/line counters) */
static void build_status_center(const UiApp *app, char *out, int outsz)
{
    out[0] = '\0';

    switch (app->view)
    {
    case VIEW_AREALIST:
        if (app->areas)
            snprintf(out, (size_t)outsz, "Area %d/%d", app->area_sel + 1, app->areas->count);
        break;
    case VIEW_MSGLIST:
        if (app->sess.order_count > 0)
            snprintf(out, (size_t)outsz, "Msg %d/%d", app->sess.msg_sel, app->sess.order_count);
        break;
    case VIEW_READER:
        if (app->reader)
            snprintf(out, (size_t)outsz, "Ln %d%%", rd_percent(app->reader));
        break;
    case VIEW_EDITOR:
        if (app->editor)
        {
            EdInfo info;

            ed_get_info(app->editor, &info);

            if (app->cfg && app->cfg->word_count)
                snprintf(out, (size_t)outsz, "Ln %d/%d Col %d W:%d", info.row + 1, info.line_count, info.col + 1, ed_word_count(app->editor));
            else
                snprintf(out, (size_t)outsz, "Ln %d/%d Col %d", info.row + 1, info.line_count, info.col + 1);
        }
        break;
    default:
        break;
    }
}

void ui_draw_statusbar(UiApp *app)
{
    int y = LINES - 1;
    int x;
    attr_t saved_attr;
    short saved_pair;
    char center[64];
    char right[64];
    char rzone[128];
    int left_len, rzone_len;
    int rzone_start;
    int max_left;

    if (!app)
        return;

    standend();

    attr_get(&saved_attr, &saved_pair, NULL);

    build_status_center(app, center, sizeof(center));
    build_status_right(app, right, sizeof(right));

    /* Status bar: left=status, right=counter+time */
    if (center[0])
        snprintf(rzone, sizeof(rzone), " %s %s", center, right);
    else
        snprintf(rzone, sizeof(rzone), "%s", right);

    attron(COLOR_PAIR(COL_STATUS));
    move(y, 0);

    for (x = 0; x < COLS; x++)
        addch(' ');

    left_len = (int)strlen(app->status);
    rzone_len = (int)strlen(rzone);

    /* Right block flush to edge, drop if too narrow */
    if (rzone_len + 2 < COLS)
        rzone_start = COLS - rzone_len - 1;
    else
        rzone_start = COLS; /* no room */

    if (rzone_start < COLS)
        mvaddnstr(y, rzone_start, rzone, rzone_len);

    /* Left status, truncated before right block */
    max_left = (rzone_start < COLS ? rzone_start : COLS) - 2;

    if (max_left > left_len)
        max_left = left_len;

    if (max_left > 0)
        mvaddnstr(y, 1, app->status, max_left);

    attroff(COLOR_PAIR(COL_STATUS));

    attr_set(saved_attr, saved_pair, NULL);
    app->status_dirty = 0;
}

/* Menu bar (top line) */
void ui_draw_menubar(UiApp *app, const char *title)
{
    int x;
    int left_offset = 0;
    char hint[64] = "";

    standend();
    attron(COLOR_PAIR(COL_MENU));
    move(0, 0);

    for (x = 0; x < COLS; x++)
        addch(' ');

    /* Left: key hints based on search mode */
    if (app)
    {
        /* Check editor/reader search mode */
        if (app->edit_search.is_mode && app->edit_search.match_count > 0)
            snprintf(hint, sizeof(hint), "Match %d/%d | F3=Prev F4=Next F5=Change F6=ALL ESC=Exit", app->edit_search.match_current, app->edit_search.match_count);
        else if (app->edit_search.only_mode && app->edit_search.match_count > 0)
            snprintf(hint, sizeof(hint), "Match %d/%d | F3=Prev F4=Next ESC=Exit", app->edit_search.match_current, app->edit_search.match_count);
        else if (app->reader_search.only_mode && app->reader_search.match_count > 0)
            snprintf(hint, sizeof(hint), "Match %d/%d | F3=Prev F4=Next ESC=Exit", app->reader_search.match_current, app->reader_search.match_count);

        if (hint[0])
        {
            mvaddnstr(0, 2, hint, (int)strlen(hint));
            left_offset = (int)strlen(hint) + 4;
        }
    }

    /* Right: title */
    if (title && title[0])
    {
        int tl = (int)strlen(title);
        int tx = COLS - tl - 2;

        if (tx > left_offset)
            mvaddnstr(0, tx, title, tl);
    }

    attroff(COLOR_PAIR(COL_MENU));
}

/* Drawing helpers */
void ui_box(int y, int x, int h, int w)
{
    int i;

    if (h < 2 || w < 2)
        return;

    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);

    for (i = 1; i < w - 1; i++)
    {
        mvaddch(y, x + i, ACS_HLINE);
        mvaddch(y + h - 1, x + i, ACS_HLINE);
    }

    for (i = 1; i < h - 1; i++)
    {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + w - 1, ACS_VLINE);
    }
}

void ui_hline(int y, int x, int len)
{
    int i;

    for (i = 0; i < len; i++)
        mvaddch(y, x + i, ACS_HLINE);
}

/* wchar_t -> UTF-8 with rotating static buffers */
const char *ui_wcs2u8(const wchar_t *wcs)
{
    static char pool[8][512];
    static int slot = 0;
    char *out = NULL;
    int i, n;

    if (!wcs)
        return "";

    out = pool[slot];
    slot = (slot + 1) & 7;
    n = 0;

    for (i = 0; wcs[i] && n < (int)sizeof(pool[0]) - 4; i++)
    {
        unsigned long cp = (unsigned long)wcs[i];

        if (cp < 0x80)
            out[n++] = (char)cp;
        else if (cp < 0x800)
        {
            out[n++] = (char)(0xC0 | (cp >> 6));
            out[n++] = (char)(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out[n++] = (char)(0xE0 | (cp >> 12));
            out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[n++] = (char)(0x80 | (cp & 0x3F));
        }
        else
        {
            out[n++] = (char)(0xF0 | (cp >> 18));
            out[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[n++] = (char)(0x80 | (cp & 0x3F));
        }
    }

    out[n] = '\0';

    return out;
}

int ui_color_for_type(int t)
{
    if (t & FTN_LT_KLUDGE)
        return COL_KLUDGE;

    if (t & FTN_LT_SEENBY)
        return COL_SEENBY;

    if (t & FTN_LT_VIA)
        return COL_VIA;

    if (t & FTN_LT_TEAR)
        return COL_TEAR;

    if (t & FTN_LT_ORIGIN)
        return COL_ORIGIN;

    if (t & FTN_LT_TAGLINE)
        return COL_TAGLINE;

    if (t & FTN_LT_QUOTE4)
        return COL_QUOTE4;

    if (t & FTN_LT_QUOTE3)
        return COL_QUOTE3;

    if (t & FTN_LT_QUOTE2)
        return COL_QUOTE2;

    if (t & FTN_LT_QUOTE1)
        return COL_QUOTE1;

    return COL_NORMAL;
}

int ui_is_netmail(const UiApp *app)
{
    if (!app || !app->areas)
        return 0;

    if (app->sess.area_idx < 0 || app->sess.area_idx >= app->areas->count)
        return 0;

    return app->areas->entries[app->sess.area_idx].type == AREATYPE_NETMAIL;
}

/* UTF-8 locale init, Amiga/Win32 skip */
static void ui_init_locale(void)
{
#if defined(PLATFORM_AMIGA) || defined(PLATFORM_WIN32)
    /* Wrapper layers handle encoding */
    setlocale(LC_ALL, "");
#else

    static const char *utf8_fallbacks[] =
        {
            "C.UTF-8",
            "en_US.UTF-8",
            "POSIX.UTF-8",
            NULL};

    const char *codeset;
    int i;

    /* First honour the environment */
    setlocale(LC_ALL, "");

    codeset = nl_langinfo(CODESET);

    if (codeset && (strcmp(codeset, "UTF-8") == 0 || strcmp(codeset, "utf8") == 0 || strcmp(codeset, "UTF8") == 0))
        return; /* environment already UTF-8 */

    /* Try to upgrade LC_CTYPE to UTF-8 locale */
    for (i = 0; utf8_fallbacks[i]; i++)
    {
        if (setlocale(LC_CTYPE, utf8_fallbacks[i]))
        {
            codeset = nl_langinfo(CODESET);

            if (codeset && (strcmp(codeset, "UTF-8") == 0 || strcmp(codeset, "utf8") == 0 || strcmp(codeset, "UTF8") == 0))
                return; /* success */
        }
    }
#endif
}

UiApp *ui_init(CrashEditCfg *cfg, AreaList *areas)
{
    UiApp *app = NULL;
    int k;

#if !defined(PLATFORM_AMIGA) && !defined(PLATFORM_WIN32)
    char *s = NULL;
#endif
    const char *default_net = NULL;

#ifdef PLATFORM_AMIGA
    int fi;
#endif

    if (!cfg || !areas)
        return NULL;

    /* Locale init for wide-character ncursesw */
    ui_init_locale();

#ifdef PLATFORM_AMIGA
    /* Amiga setup (before initscr) */
    amiga_set_default_bg_color(cfg->default_bg_color);
    amiga_set_font_name(cfg->font);
    amiga_set_ansi_font_name(cfg->ansifont);

    /* Optional TrueType (ignored if disabled) */
    if (cfg->ttf_enabled)
    {
        amiga_set_ttf(cfg->ttf_font, cfg->ttf_size, cfg->ttf_antialias);
        amiga_set_ttf_encoding(cfg->ttf_use_utf8);

        /* Pass TTF_FALLBACK<N> entries to engine */
        amiga_clear_ttf_fallbacks();

        for (fi = 0; fi < CFG_TTF_FALLBACKS; fi++)
        {
            if (cfg->ttf_fallback[fi][0])
            {
                int sz = cfg->ttf_fallback_size[fi] > 0 ? cfg->ttf_fallback_size[fi] : cfg->ttf_size;
                amiga_add_ttf_fallback(cfg->ttf_fallback[fi], sz);
            }
        }
    }
    else
    {
        /* Explicitly disable TTF when disabled */
        amiga_set_ttf(NULL, 0, 0);
        amiga_clear_ttf_fallbacks();
    }
#endif

#ifdef PLATFORM_WIN32
    /* Windows setup (before initscr) */
    if (cfg->font[0] && (strchr(cfg->font, '\\') || strchr(cfg->font, '/') || strchr(cfg->font, ':')))
    {
        /* FONT holds a TTF path: split into family name and TTF slot */
        char family[256];
        char path[CFG_STR_MAX];

        strncpy(path, cfg->font, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';

        if (win32_get_font_family_name(path, family, sizeof(family)) == 0)
        {
            strncpy(cfg->font, family, CFG_STR_MAX - 1);
            cfg->font[CFG_STR_MAX - 1] = '\0';
        }
        else
        {
            cfg->font[0] = '\0';
        }

        if (!cfg->ttf_enabled || !cfg->ttf_font[0])
        {
            strncpy(cfg->ttf_font, path, CFG_STR_MAX - 1);
            cfg->ttf_font[CFG_STR_MAX - 1] = '\0';
            cfg->ttf_enabled = 1;
        }
    }

    win32_set_font_name(cfg->font);

    /* Load optional TTF files; FONT must be the family name */
    if (cfg->ttf_enabled)
    {
        int fi;

        win32_set_font_size(cfg->ttf_size);
        win32_add_font_file(cfg->ttf_font);

        for (fi = 0; fi < CFG_TTF_FALLBACKS; fi++)
        {
            if (cfg->ttf_fallback[fi][0])
                win32_add_font_file(cfg->ttf_fallback[fi]);
        }
    }
#endif

    if (!initscr())
    {
        fprintf(stderr, "ui_init: initscr failed\n");
        return NULL;
    }

    raw();
    noecho();

    /* fix with paste text */
    /*nonl();*/ /* Disable carriage return to newline translation */

    keypad(stdscr, TRUE);
    curs_set(0);
    setup_colors(cfg);

    set_tabsize(cfg->tab_width > 0 ? cfg->tab_width : 4);

#if !defined(PLATFORM_AMIGA) && !defined(PLATFORM_WIN32)
    /* Enable mouse if configured */
    ui_configure_mouse(cfg->mouse_enabled);

    set_escdelay(25);

    /* Register custom key sequences */
    define_key("\033[<", KEY_MOUSE_SGR);      /* SGR mouse escape sequence */
    define_key("\033[200~", KEY_PASTE_START); /* bracketed paste start */
    define_key("\033[201~", KEY_PASTE_END);   /* bracketed paste end */
    define_key("\033z", KEY_ALT('Z'));        /* Alt+Z redo */
    define_key("\033Z", KEY_ALT('Z'));        /* Alt+Shift+Z */

    /* Ctrl+Arrow keys for navigation */

    s = tigetstr("kLFT5"); /* Ctrl+Left */

    if (s && s != (char *)-1)
        define_key(s, KEY_CLEFT);

    s = tigetstr("kRIT5"); /* Ctrl+Right */

    if (s && s != (char *)-1)
        define_key(s, KEY_CRIGHT);

    /* xterm/vte/etc. fallback sequences */
    define_key("\033[1;5D", KEY_CLEFT);
    define_key("\033[1;5C", KEY_CRIGHT);
    define_key("\033[1;5A", KEY_CUP);   /* Ctrl+Up */
    define_key("\033[1;5B", KEY_CDOWN); /* Ctrl+Down */
    define_key("\033[1;5H", KEY_CHOME); /* Ctrl+Home */
    define_key("\033[1;5F", KEY_CEND);  /* Ctrl+End */

    /* Shift+Arrow keys */
    define_key("\033[1;2A", KEY_SUP);    /* Shift+Up */
    define_key("\033[1;2B", KEY_SDOWN);  /* Shift+Down */
    define_key("\033[1;2D", KEY_SLEFT);  /* Shift+Left */
    define_key("\033[1;2C", KEY_SRIGHT); /* Shift+Right */
    define_key("\033[1;2H", KEY_SHOME);  /* Shift+Home */
    define_key("\033[1;2F", KEY_SEND);   /* Shift+End */

    /* Ctrl+Shift+Arrow keys */
    define_key("\033[1;6A", KEY_CSUP);    /* Ctrl+Shift+Up */
    define_key("\033[1;6B", KEY_CSDOWN);  /* Ctrl+Shift+Down */
    define_key("\033[1;6D", KEY_CSLEFT);  /* Ctrl+Shift+Left */
    define_key("\033[1;6C", KEY_CSRIGHT); /* Ctrl+Shift+Right */
    define_key("\033[1;6H", KEY_CSHOME);  /* Ctrl+Shift+Home */
    define_key("\033[1;6F", KEY_CSEND);   /* Ctrl+Shift+End */
    define_key("\033[5;2~", KEY_SPPAGE);  /* Shift+PageUp */
    define_key("\033[6;2~", KEY_SNPAGE);  /* Shift+PageDown */
    define_key("\033[1;6d", KEY_CSUPD);   /* Ctrl+Shift+D */
    define_key("\033[1;6u", KEY_CSDOWNU); /* Ctrl+Shift+U */
    define_key("\033[24;2~", KEY_SF12);   /* Shift+F12 */

    /* Alt+Left/Right: word movement */
    s = tigetstr("kLFT3"); /* Alt+Left */

    if (s && s != (char *)-1)
        define_key(s, KEY_ALEFT);

    s = tigetstr("kRIT3"); /* Alt+Right */

    if (s && s != (char *)-1)
        define_key(s, KEY_ARIGHT);

    /* Common Alt+Left/Right sequences */
    /* $ showkey -a
    Pulse cualquier tecla -- o Ctrl-D para salir de este programa
    ^[b      27 0033 0x1b
             98 0142 0x62
    ^[f      27 0033 0x1b
            102 0146 0x66
    ^D        4 0004 0x04
    */

    define_key("\033[1;3D", KEY_ALEFT);
    define_key("\033[1;3C", KEY_ARIGHT);
    define_key("\033b", KEY_ALEFT);      /* Alt+Left */
    define_key("\033f", KEY_ARIGHT);     /* Alt+Right */
    define_key("\033B", KEY_ALEFT);      /* Alt+Shift+Left  */
    define_key("\033F", KEY_ARIGHT);     /* Alt+Shift+Right */
    define_key("\033[3D", KEY_ALEFT);    /* Some terminals */
    define_key("\033[3C", KEY_ARIGHT);   /* Some terminals */
    define_key("\033[1;9D", KEY_ALEFT);  /* Some terminals */
    define_key("\033[1;9C", KEY_ARIGHT); /* Some terminals */
    define_key("\033[1;7D", KEY_ALEFT);  /* Some terminals */
    define_key("\033[1;7C", KEY_ARIGHT); /* Some terminals */

    /* Alt+Up/Down: move line up/down */
    define_key("\033[1;3A", KEY_ALT_UP);
    define_key("\033[1;3B", KEY_ALT_DOWN);
    define_key("\033\033[A", KEY_ALT_UP);   /* rxvt */
    define_key("\033\033[B", KEY_ALT_DOWN); /* rxvt */

    define_key("\033c", KEY_ALT('C'));       /* Alt+C */
    define_key("\033C", KEY_ALT('C'));       /* Alt+Shift+C */
    define_key("\033f", KEY_ALT('F'));       /* Alt+F */
    define_key("\033F", KEY_ALT('F'));       /* Alt+Shift+F */
    define_key("\033s", KEY_ALT('S'));       /* Alt+S */
    define_key("\033S", KEY_ALT('S'));       /* Alt+Shift+S */
    define_key("\033l", KEY_ALT('L'));       /* Alt+L */
    define_key("\033L", KEY_SHIFT('L'));     /* Alt+Shift+L: TTS speak (Leer) */
    define_key("\033[1;4L", KEY_SHIFT('L')); /* xterm Alt+Shift+L */
    define_key("\033K", KEY_SHIFT('K'));     /* Alt+Shift+K: TTS speak whole document */
    define_key("\033i", KEY_ALT('I'));       /* Alt+I */
    define_key("\033I", KEY_ALT('I'));       /* Alt+Shift+I */
    define_key("\033v", KEY_ALT('V'));       /* Alt+V nodelist view */
    define_key("\033V", KEY_SHIFT('V'));     /* Shift+Alt+V sort lines */
    define_key("\033[1;4V", KEY_SHIFT('V')); /* xterm Shift+Alt+V */
    define_key("\033t", KEY_ALT('T'));       /* Alt+T nodelist picker */
    define_key("\033T", KEY_ALT('T'));       /* Alt+Shift+T */
    define_key("\033y", KEY_ALT('Y'));       /* Alt+Y help */
    define_key("\033Y", KEY_ALT('Y'));       /* Alt+Shift+Y */
    define_key("\033d", KEY_ALT('D'));       /* Alt+D line numbers */
    define_key("\033D", KEY_ALT('D'));       /* Alt+Shift+D */
    define_key("\033a", KEY_ALT('A'));       /* Alt+A add attachment */
    define_key("\033A", KEY_ALT('A'));       /* Alt+Shift+A */
    define_key("\033x", KEY_ALT('X'));       /* Alt+X remove attachment */
    define_key("\033X", KEY_SHIFT('X'));     /* Shift+Alt+X convert case */
    define_key("\033[1;4X", KEY_SHIFT('X')); /* xterm Shift+Alt+X */
    define_key("\033g", KEY_ALT('G'));       /* Alt+G goto line */
    define_key("\033G", KEY_ALT('G'));       /* Alt+Shift+G */
    define_key("\033h", KEY_ALT('H'));       /* Alt+H charset */
    define_key("\033H", KEY_ALT('H'));       /* Alt+Shift+H */
    define_key("\033r", KEY_ALT('R'));       /* Alt+R translate */
    define_key("\033R", KEY_ALT('R'));       /* Alt+Shift+R */
    define_key("\033p", KEY_ALT('P'));       /* Alt+P nodelist picker */
    define_key("\033P", KEY_SHIFT('P'));     /* Alt+Shift+P: TTS pause/resume */
    define_key("\033[1;4P", KEY_SHIFT('P')); /* xterm Alt+Shift+P */
    define_key("\033q", KEY_ALT('Q'));       /* Alt+Q toggle wrap */
    define_key("\033Q", KEY_ALT('Q'));       /* Alt+Shift+Q */
    define_key("\033b", KEY_ALT('B'));       /* Alt+B toggle block */
    define_key("\033B", KEY_ALT('B'));       /* Alt+Shift+B */
    define_key("\033n", KEY_ALT('N'));       /* Alt+N next match */
    define_key("\033N", KEY_ALT('N'));       /* Alt+Shift+N */
    define_key("\033o", KEY_ALT('O'));       /* Alt+O insert file */
    define_key("\033O", KEY_SHIFT('O'));     /* Alt+Shift+O: TTS stop (Off) */
    define_key("\033[1;4O", KEY_SHIFT('O')); /* xterm Alt+Shift+O */
    define_key("\033k", KEY_ALT('K'));       /* Alt+K kludges */
    define_key("\033K", KEY_ALT('K'));       /* Alt+Shift+K */
    define_key("\033m", KEY_ALT('M'));       /* Alt+M list attachments */
    define_key("\033M", KEY_ALT('M'));       /* Alt+Shift+M */
    define_key("\033e", KEY_ALT('E'));       /* Alt+E spell panel */
    define_key("\033E", KEY_ALT('E'));       /* Alt+Shift+E */
    define_key("\033w", KEY_ALT('W'));       /* Alt+W spell word */
    define_key("\033W", KEY_ALT('W'));       /* Alt+Shift+W */
    define_key("\033j", KEY_ALT('J'));       /* Alt+J thesaurus */
    define_key("\033J", KEY_SHIFT('J'));     /* Alt+Shift+J: TTS settings popup */
    define_key("\033[1;4J", KEY_SHIFT('J')); /* xterm Alt+Shift+J */
    define_key("\033z", KEY_ALT('Z'));       /* Alt+Z redo */
    define_key("\033Z", KEY_ALT('Z'));       /* Alt+Shift+Z */
#endif

    app = (UiApp *)calloc(1, sizeof(UiApp));

    if (!app)
    {
#if !defined(PLATFORM_AMIGA) && !defined(PLATFORM_WIN32)
        /* Disable mouse if it was enabled */
        if (cfg->mouse_enabled)
            ui_configure_mouse(0);
#endif
#ifndef PLATFORM_WIN32
        endwin();
#endif
        return NULL;
    }

    app->cfg = cfg;
    app->areas = areas;
    app->view = VIEW_AREALIST;
    app->sess.area_idx = -1;
    app->edit_aka_idx = cfg->aka_selected;
    app->edit_return_view = VIEW_MSGLIST;

    app->search_opt_headers = 1;

    /* view_charset: runtime override, init EMPTY for Auto */
    app->view_charset[0] = '\0';

    /* edit_charset seeded from cfg->charset */
    snprintf(app->edit_charset, sizeof(app->edit_charset), "%s", cfg->charset);

    /* Initialize saved charset and manual change flag */
    app->edit_charset_saved[0] = '\0';
    app->edit_charset_manually_changed = 0;

#ifdef HAVE_HUNSPELL
    /* Initialize spell checker state */
    app->spell_active = 0; /* Disabled by default */
    ui_spell_cache_init(&app->spell_cache);
#endif

#ifdef HAVE_TRANSLATE
    /* Initialize translator state */
    app->translate_enabled = app->cfg->translate_enabled;
    app->translate_active = app->cfg->translate_enabled;

    /* Load translator if enabled in config */
    if (app->translate_active)
        ui_translate_load_from_config(app);
#endif

#ifdef HAVE_TTS
    /* Initialize TTS state */
    app->tts_enabled = app->cfg->tts_enabled;

    /* Load TTS if enabled in config */
    if (app->tts_enabled)
        ui_tts_load_from_config(app);
#endif

    app->reader = rd_new(cfg->viewkludge, cfg->viewhidden);
    app->hdr = msghdr_new();
    app->editor = ed_new();

    if (app->editor)
    {
        ed_set_hard_wrap(app->editor, cfg->hard_wrap);
        ed_set_word_move_mode(app->editor, cfg->word_move_mode);
    }

    app->edit_hdr = msghdr_new();
    app->attach_list = attach_new();

    if (!app->reader || !app->hdr || !app->editor || !app->edit_hdr || !app->attach_list)
    {
        if (app->reader)
            rd_free(app->reader);

        if (app->hdr)
            msghdr_free(app->hdr);

        if (app->editor)
            ed_free(app->editor);

        if (app->edit_hdr)
            msghdr_free(app->edit_hdr);

        if (app->attach_list)
            attach_free(app->attach_list);

        free(app);

#ifndef PLATFORM_WIN32
        endwin();
#endif

        fprintf(stderr, "ui_init: out of memory creating components\n");

        return NULL;
    }

    /* Apply config-driven editor settings */
    if (cfg->undo_levels > 0)
        ed_set_undo_levels(app->editor, cfg->undo_levels);

    ui_arealist_rebuild_order(app);

    /* Load FTS-5000 nodelists from config INCLUDE */
    nodelist_init(&app->nodelist);

    default_net = NULL; /* No @network suffix */

    if (cfg->aka_count > 0)
    {
        const char *at = strchr(cfg->aka[0], '@');

        if (at && at[1])
            default_net = at + 1;
    }

    for (k = 0; k < cfg->nodelist_includes_count; k++)
    {
        const char *p = cfg->nodelist_includes[k];

        if (p && p[0])
            nodelist_load_file(&app->nodelist, p, default_net);
    }

    /* MSGLISTFIRST: if exactly one area, open it directly */
    if (app->cfg->msglistfirst && areas->count == 1)
    {
        if (ui_session_open(app, 0) == 0)
            app->view = VIEW_MSGLIST;
    }

#ifdef HAVE_HUNSPELL
    /* Load Hunspell dictionary if configured */
    ui_spell_load_from_config(app);
#endif

#ifdef HAVE_HYPHEN
    ui_hyph_load_from_config(app);
#endif
#ifdef HAVE_MYTHES
    ui_thes_load_from_config(app);
#endif

#ifdef HAVE_GRAMMAR
    app->grammar_enabled = cfg->grammar_enabled;
    app->grammar_active = 0;
    app->grammar_handle = NULL;

    ui_grammar_load_from_config(app);
#endif

    ui_status(app, "Welcome to CrashEdit. Press F1 or ? for help");

    return app;
}

void ui_set_cfg_path(UiApp *app, const char *path)
{
    if (app)
        app->cfg_path = path;
}

void ui_force_setup(UiApp *app)
{
    if (app)
        app->force_setup = 1;
}

int ui_run(UiApp *app)
{
    if (!app)
        return 0;

    /* First-run / no-usable-config: go to setup */
    if (app->force_setup)
    {
        int saved = ui_setup_run(app);

        app->force_setup = 0;

        if (saved == 1)
        {
            /* First-run: reload areas from disk */
            return 1;
        }

        return 0; /* quit without saving */
    }

    while (app->view != VIEW_QUIT)
    {
        switch (app->view)
        {
        case VIEW_AREALIST:
            app->view = ui_arealist_run(app);
            break;
        case VIEW_MSGLIST:
            app->view = ui_msglist_run(app);
            break;
        case VIEW_READER:
            app->view = ui_reader_run(app);
            break;
        case VIEW_EDITOR:
            app->view = ui_editor_run(app);
            break;
        case VIEW_SEARCH_RESULTS:
            app->view = ui_search_results_run(app);
            break;
        case VIEW_QUIT:
        default:
            return app->want_reload;
        }
    }

    return app->want_reload;
}

void ui_cleanup(UiApp *app)
{
    if (!app)
        return;

    /* Tear-down order: detach speller, then free thes, hyph, spell */
#ifdef HAVE_MYTHES
    ui_thes_unload(app);
#endif

#ifdef HAVE_HYPHEN
    ui_hyph_unload(app);
#endif

#ifdef HAVE_HUNSPELL
    ui_spell_cache_clear(&app->spell_cache);
    ui_spell_unload(app);
#endif

#ifdef HAVE_GRAMMAR
    ui_grammar_free_app(app);
#endif

#ifdef HAVE_TRANSLATE
    ui_translate_unload(app);
#endif

#ifdef HAVE_TTS
    /* Stop in-flight speech before freeing to avoid dangling I/O */
    ui_tts_unload(app);
#endif

    ui_session_close(app);

    if (app->reader)
        rd_free(app->reader);

    if (app->hdr)
        msghdr_free(app->hdr);

    if (app->editor)
        ed_free(app->editor);

    if (app->edit_hdr)
        msghdr_free(app->edit_hdr);

    if (app->attach_list)
        attach_free(app->attach_list);

    if (app->saved_kludges)
        free(app->saved_kludges);

    /* Release pending search session */
    ui_search_cleanup(app);

    nodelist_free(&app->nodelist);

    free(app->area_order);
    free(app);

#if !defined(PLATFORM_AMIGA) && !defined(PLATFORM_WIN32)
    /* Disable mouse if it was enabled */
    ui_configure_mouse(0);
#endif

    endwin();
}
