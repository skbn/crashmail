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

/* ui_internal.h -- Internal types and helpers shared between UI modules */
#define _XOPEN_SOURCE
#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif

#include "ui.h"
#include <wchar.h>
#include <strings.h>
#include <stdio.h>

#define SEARCH_PATTERN_MAX 256

#if defined(PLATFORM_AMIGA)
#include "../ncursesw_amiga.h"
#elif defined(PLATFORM_WIN32)
#include "../ncursesw_win32.h"
#else
#include <ncurses.h>
#endif

#include "../core/keys.h"
#include "../core/nodelist.h"

/* CTRL('B') = 0x02 */
#ifndef CTRL
#define CTRL(ch) ((ch) - 64)
#endif

/* Extended keycodes (above KEY_MAX to avoid collisions) */
#ifndef KEY_PASTE_START
#define KEY_PASTE_START 0x7F1 /* bracketed paste start */
#endif
#ifndef KEY_PASTE_END
#define KEY_PASTE_END 0x7F2 /* bracketed paste end */
#endif
#ifndef KEY_CLEFT
#define KEY_CLEFT 0x7F3 /* Ctrl+Left */
#endif
#ifndef KEY_CRIGHT
#define KEY_CRIGHT 0x7F4 /* Ctrl+Right */
#endif
#ifndef KEY_CUP
#define KEY_CUP 0x7F7 /* Ctrl+Up */
#endif
#ifndef KEY_CDOWN
#define KEY_CDOWN 0x7F8 /* Ctrl+Down */
#endif
#ifndef KEY_CHOME
#define KEY_CHOME 0x7F9 /* Ctrl+Home */
#endif
#ifndef KEY_CEND
#define KEY_CEND 0x7FA /* Ctrl+End */
#endif
#ifndef KEY_ALEFT
#define KEY_ALEFT 0x7F5 /* Alt+Left */
#endif
#ifndef KEY_ARIGHT
#define KEY_ARIGHT 0x7F6 /* Alt+Right */
#endif
#ifndef KEY_SLEFT
#define KEY_SLEFT 0x7FB /* Shift+Left */
#endif
#ifndef KEY_SRIGHT
#define KEY_SRIGHT 0x7FC /* Shift+Right */
#endif
#ifndef KEY_SUP
#define KEY_SUP 0x7FD /* Shift+Up */
#endif
#ifndef KEY_SDOWN
#define KEY_SDOWN 0x7FE /* Shift+Down */
#endif
#ifndef KEY_SHOME
#define KEY_SHOME 0x7FF /* Shift+Home */
#endif
#ifndef KEY_SEND
#define KEY_SEND 0x800 /* Shift+End */
#endif
#ifndef KEY_CSLEFT
#define KEY_CSLEFT 0x801 /* Ctrl+Shift+Left */
#endif
#ifndef KEY_CSRIGHT
#define KEY_CSRIGHT 0x802 /* Ctrl+Shift+Right */
#endif
#ifndef KEY_CSUP
#define KEY_CSUP 0x803 /* Ctrl+Shift+Up */
#endif
#ifndef KEY_CSDOWN
#define KEY_CSDOWN 0x804 /* Ctrl+Shift+Down */
#endif
#ifndef KEY_CSHOME
#define KEY_CSHOME 0x805 /* Ctrl+Shift+Home */
#endif
#ifndef KEY_CSEND
#define KEY_CSEND 0x806 /* Ctrl+Shift+End */
#endif
#ifndef KEY_SPPAGE
#define KEY_SPPAGE 0x807 /* Shift+PageUp */
#endif
#ifndef KEY_SNPAGE
#define KEY_SNPAGE 0x808 /* Shift+PageDown */
#endif
#ifndef KEY_SF12
#define KEY_SF12 0x80C /* Shift+F12 */
#endif
#ifndef KEY_CSUPD
#define KEY_CSUPD 0x809 /* Ctrl+Shift+D */
#endif
#ifndef KEY_CSDOWNU
#define KEY_CSDOWNU 0x80A /* Ctrl+Shift+U */
#endif
#ifndef KEY_ALT_UP
#define KEY_ALT_UP 0xA00 /* Alt+Up */
#endif
#ifndef KEY_ALT_DOWN
#define KEY_ALT_DOWN 0xA01 /* Alt+Down */
#endif
#ifndef KEY_MOUSE_SGR
#define KEY_MOUSE_SGR 0x80B
#endif

#ifndef PLATFORM_AMIGA
#define BRACKET_PASTE_ON()            \
    do                                \
    {                                 \
        fputs("\033[?2004h", stdout); \
        fflush(stdout);               \
    } while (0)
#define BRACKET_PASTE_OFF()           \
    do                                \
    {                                 \
        fputs("\033[?2004l", stdout); \
        fflush(stdout);               \
    } while (0)
#else
#define BRACKET_PASTE_ON() ((void)0)
#define BRACKET_PASTE_OFF() ((void)0)
#endif

/* Color pairs */
#define COL_NORMAL 1
#define COL_SELECTED 2
#define COL_HEADER 3
#define COL_STATUS 4
#define COL_MENU 5
#define COL_MENU_HOT 6
#define COL_BORDER 7
#define COL_QUOTE1 8
#define COL_QUOTE2 9
#define COL_QUOTE3 10
#define COL_QUOTE4 11
#define COL_KLUDGE 12
#define COL_TEAR 13
#define COL_ORIGIN 14
#define COL_SEENBY 15
#define COL_VIA 16
#define COL_UNREAD 17
#define COL_DELETED 18
#define COL_POPUP 19
#define COL_POPUP_SEL 20
#define COL_ERROR 21
#define COL_TAGLINE 22
#define COL_SEARCH_MATCH 23
#define COL_SPELL_CURRENT 24
#define COL_BRACKET_MATCH 25
#define COL_CURRENT_LINE 26
#define COL_GUIDE 27

/*
 * UI-level spell-check result cache, keyed by exact wchar_t word
 * UI cache hit > return result
 * UI cache miss > spell_check() > internal cache hit/miss > save result in UI cache
 *
 * UI cache (the first layer):
 * If the word is there, it returns the result without calling Hunspell
 *
 * If it's not there, it moves to the next layer
 * When it's full, it overwrites the oldest entry (it doesn't delegate to the other cache)
 * Hunspell's internal cache (SPELL_CACHE_N): It's checked inside spell_check(), only when the UI cache fails
 * If the word was in the internal cache, it avoids the affix/lowercase handling
 *
 * If the user edited the custom dictionary file directly, reload it so new words are recognized immediately
 */

#ifndef UI_SPELL_CACHE_SIZE
#ifdef PLATFORM_AMIGA
#define UI_SPELL_CACHE_SIZE 2048
#else
#define UI_SPELL_CACHE_SIZE 8192
#endif
#endif

/* View states */
typedef enum
{
    VIEW_AREALIST,
    VIEW_MSGLIST,
    VIEW_READER,
    VIEW_EDITOR,
    VIEW_SEARCH_RESULTS, /* persistent search browser; reader/editor return here */
    VIEW_QUIT
} UiView;

/* Session (per-area state) */
typedef struct
{
    int area_idx;  /* index in app->areas->entries */
    MsgBase mb;    /* open message base */
    int mb_open;   /* 1 if mb_open succeeded */
    MsgInfo *msgs; /* loaded headers */
    int msg_count;
    int *order; /* sort/filter index map (msg_idx -> real) */
    int order_count;
    int msg_sel; /* selected msg in order[] */
    int msg_top; /* topmost visible row in msglist */
    uint32_t user_crc;
    uint32_t lastread;  /* updated only by the reader */
    uint32_t lastseen;  /* highest msgnum acknowledged via msglist; drives "*" indicator */
    wchar_t search[80]; /* current filter text */
} UiSession;

/* Input state for text fields (modular input handling) */
typedef struct
{
    wchar_t *buf; /* buffer de entrada (wchar_t) */
    int bufsz;    /* capacidad del buffer */
    int cursor;   /* posición del cursor */
    int len;      /* longitud actual del texto */
} InputState;

/* Search state (unified for reader and editor) */
typedef struct
{
    wchar_t query[64];        /* last search query */
    wchar_t last_replace[64]; /* last replacement text */
    int *rows;                /* malloc'd array of row indices with matches */
    int *cols;                /* malloc'd array of col indices with matches (editor only) */
    int match_count;          /* number of matches */
    int is_mode;              /* 0=normal, 1=search mode */
    int only_mode;            /* 1=only search mode activated by F5 */
    int current_match;        /* Current match index (0-based) for navigation */
    int match_current;        /* Current match number (1-based) for display */
    int case_sensitive;       /* Search case sensitivity flag */
    int whole_word;           /* Search whole word flag */
} UiSearch;

typedef struct
{
    wchar_t *word; /* NULL = empty slot */
    int len;
    int incorrect; /* 1 = misspelled, 0 = correct */
} UiSpellCacheEntry;

typedef struct
{
    UiSpellCacheEntry entries[UI_SPELL_CACHE_SIZE];
} UiSpellCache;

/* Main app state */
struct UiApp
{
    CrashEditCfg *cfg;
    AreaList *areas;

    /* Path to loaded config file for setup screen save, set by main after ui_init */
    const char *cfg_path;

    /* Set by setup screen when user saves changes, signals main loop to tear down and reload from disk, ui_run() returns this value */
    int want_reload;

    UiView view;
    UiSession sess;

    /* Area list state */
    int *area_order; /* sorted indices */
    int area_order_count;
    int area_sel;
    int area_top;
    wchar_t area_search[64];

    /* Reader state */
    Reader *reader;
    MsgHdr *hdr;
    char msg_charset[32];     /* charset detected when reading */
    char original_chrs[32];   /* original CHRS from message (before override) */
    char view_charset[32];    /* charset chosen for display */
    char decoded_charset[32]; /* actual decode charset (may differ from view_charset) */
    uint32_t cur_msgnum;      /* JAM msgnum of currently shown msg */
    char cur_msgid[160];      /* MSGID value of currently shown msg (without ^A) */
    char cur_reply[160];      /* REPLY value of currently shown msg (without ^A) */
    UiSearch reader_search;   /* search state in reader */

    /* Editor state */
    Ed *editor;
    MsgHdr *edit_hdr;
    char *saved_kludges;               /* malloc'd, freed on close */
    char edit_charset[32];             /* charset for save */
    char edit_charset_saved[32];       /* saved edit_charset before auto-detection */
    int edit_charset_manually_changed; /* 1 = user changed charset via F3, 0 = auto-detected */
    int edit_aka_idx;                  /* selected AKA (netmail only) */
    int edit_is_new;                   /* 1 = new, 0 = edit existing */
    int edit_is_reply;                 /* 1 = reply, swap from/to */
    int edit_return_view;              /* VIEW_READER or VIEW_MSGLIST: where to go on cancel/save */
    int edit_active_field;             /* 0..4 = FROM/TO/SUBJ/DADDR/BODY, -1 idle */
    uint32_t edit_reply_to_msgnum;
    uint32_t edit_attr;      /* MSG_PRIVATE / MSG_CRASH / MSG_HOLD */
    UiSearch edit_search;    /* search state in editor */
    AttachList *attach_list; /* file attachments for current message */

    /* Status message (one-line) */
    char status[256];
    int status_dirty;

    int msglist_overlay_from_reader; /* 1 = msglist shown over reader; ESC returns to reader */

    char search_opt_pattern[SEARCH_PATTERN_MAX]; /* last search pattern */
    int search_opt_headers;                      /* 1=search headers */
    int search_opt_body;                         /* 1=search body */
    int search_opt_all_areas;                    /* 1=all areas */
    int search_opt_case;                         /* 1=case sensitive */
    int search_opt_whole;                        /* 1=whole word */

    void *search;
    void *search_runs;
    int search_n_runs;
    int search_mode; /* 0 = areas list, 1 = hits in picked area */
    int search_area_pick;
    int search_area_top;
    int search_hit_pick;
    int search_hit_top;
    UiView search_from_view;
    int from_search;

    /* Force setup mode on first run or missing config, ui_run() drops into setup screen */
    int force_setup;

    /* Nodelist/pointlist entries loaded from INCLUDE directives at startup */
    Nodelist nodelist;

    /* Spell checker state (compiled in, all builds; fields are inert when no Hunspell) */
    int show_spell; /* 1 = panel visible (overlay) */

#ifdef HAVE_HUNSPELL
    void *spell_handle;              /* opaque SpellChecker* */
    int spell_enabled;               /* mirrors cfg.spell_enabled once loaded */
    int spell_active;                /* spell checker active (manual toggle) */
    wchar_t spell_current_word[256]; /* last word checked, for the panel */
    int spell_word_status;           /* 0=none, 1=correct, 2=incorrect */
    char **spell_suggestions;        /* most recent suggestions */
    int spell_suggestion_count;
    UiSpellCache spell_cache; /* Cache for visible-word spell-check results */
#endif

#ifdef HAVE_HYPHEN
    void *hyph_handle;     /* opaque HyphDict* */
    int hyph_wrap_enabled; /* mirrors cfg.hyph_wrap_enabled; runtime toggle */
#endif

#ifdef HAVE_MYTHES
    void *thes_handle; /* opaque ThesHandle* */
#endif

    /* Dictionary panel (always present; StarDict backend is optional) */
    int show_dict;       /* 1 = panel visible */
    char *dict_result;   /* malloc'd, NULL when empty */
    char dict_word[128]; /* header label */
    int dict_scroll;     /* first visible line of dict_result */

#ifdef HAVE_TRANSLATE
    void *translate_handle;    /* TranslateHandle* (opaque) */
    int translate_enabled;     /* mirrors cfg.translate_enabled; runtime toggle */
    int translate_active;      /* translator active (manual toggle) */
    int translate_http_inited; /* flag: http_client_init was called */
#endif

    /* Bracket matching: row/col of partner of bracket under cursor.
     * -1 = no match. Recomputed each render */
    int bracket_match_row;
    int bracket_match_col;
};

/* Status and UI helpers */
void ui_status(UiApp *app, const char *fmt, ...);
void ui_draw_statusbar(UiApp *app);
void ui_draw_menubar(UiApp *app, const char *title);
void ui_draw_statusbar(UiApp *app);

/* Border helpers */
void ui_box(int y, int x, int h, int w);
void ui_hline(int y, int x, int len);

/* wchar_t -> UTF-8 via static rotating buffers (multiple args per printf safe) */
const char *ui_wcs2u8(const wchar_t *wcs);

/* Color helper: returns color pair for FTN line type (FTN_LT_*) */
int ui_color_for_type(int ftn_line_type);

/* Is current area netmail? (type == 1) */
int ui_is_netmail(const UiApp *app);

/* View entry points (one per source file) */
UiView ui_arealist_run(UiApp *app);
UiView ui_msglist_run(UiApp *app);
UiView ui_reader_run(UiApp *app);
UiView ui_editor_run(UiApp *app);

/* Persistent search-result browser, driven by search_* fields in UiApp, bounces back to VIEW_AREALIST when app->search is NULL */
UiView ui_search_results_run(UiApp *app);

/* Free active search session if any, called by ui_cleanup and result browser when user ESCs out */
void ui_search_cleanup(UiApp *app);

/* Session helpers */
int ui_session_open(UiApp *app, int area_idx);
void ui_session_close(UiApp *app);
void ui_session_rebuild_order(UiApp *app);

/* Area list helpers */
void ui_arealist_rebuild_order(UiApp *app);

/* Popups (blocking, return value = user choice) */
void ui_popup_info(const char *title, const char *msg);       /* non-blocking info popup */
int ui_popup_confirm(const char *title, const char *msg);     /* 1=yes, 0=no, -1=ESC */
int ui_popup_confirm_all(const char *title, const char *msg); /* 1=yes, 0=no, -1=ESC, 2=all */
void ui_popup_message(const char *title, const char *msg);    /* simple message, waits for key */

/* Choose one item from string array (-1=cancel, initial=preselected) */
int ui_popup_list(const char *title, const char **items, int count, int initial);

/* Search results popup with line numbers and context */
int ui_popup_search_results(const char *title, const int *line_nums, const char **contexts, int count, int initial);

/* Charset pickers (0=ok, -1=cancel) */
int ui_popup_charset(const char *title, const char *cur, char *out, int outsz);
int ui_popup_charset_pair(const char *view_in, const char *output_in, const char *view_def, const char *output_def, char *view_out, int view_outsz, char *output_out, int output_outsz);

/* Choose AKA from cfg->aka[] (cur_idx=preselected, returns selected index, -1=cancel) */
int ui_popup_aka(const UiApp *app, int cur_idx);

/* Input text: prompt + prefilled buffer (0=ok, -1=cancel) */
/* Wide-char input (for filters/text with possible accents) */
int ui_popup_input(const char *title, const char *prompt, wchar_t *wbuf, int wcap);
int ui_popup_input_width(const char *title, const char *prompt, wchar_t *wbuf, int wcap, int width);

/* Find & Replace popup with case-sensitive and whole-word options */
int ui_popup_replace(const wchar_t *search_in, const wchar_t *replace_in, wchar_t *search_out, int search_outsz, wchar_t *replace_out, int replace_outsz, int *case_sensitive, int *whole_word);

/* Choose area list sort spec (0=ok, -1=cancel) */
int ui_popup_sort(char *spec, int specsz, const char *cfg_default);

/* Help and attachment popups */
void ui_popup_help(const char *title, const char *const *lines, int n);
int ui_popup_attach_add(UiApp *app);    /* 1=added, 0=canceled, -1=error */
int ui_popup_freq(UiApp *app);          /* file request popup; 1=written, 0=canceled */
int ui_setup_run(UiApp *app);           /* config editor; 1=saved, 0=cancelled */
void ui_reapply_config(UiApp *app);     /* apply config changes in-place (colors, font) */
int ui_popup_attach_remove(UiApp *app); /* 1=removed, 0=canceled */
int ui_popup_attach_clear(UiApp *app);  /* 1=cleared, 0=canceled */
int ui_popup_attach_list(UiApp *app);   /* 0=closed */

/* Nodelist / pointlist picker, type-ahead filtering by name or address */
int ui_popup_nodelist(UiApp *app, int allow_pick, char *out_name, int name_sz, char *out_addr, int addr_sz);

/* Compute centered popup geometry, clamped to LINES/COLS */
void ui_popup_center(int want_h, int want_w, int *y, int *x, int *h, int *w);

/* Draw popup frame (used by ui_glyph_picker) */
void draw_popup_frame(int y, int x, int h, int w, const char *title);
void ui_draw_popup_frame(int y, int x, int h, int w, const char *title);

void input_draw(InputState *state, int y, int x, int width, int is_active);
void input_move_cursor(InputState *state, int y, int x, int width);
int input_handle_key(InputState *state, int ch, int is_key);

#endif /* UI_INTERNAL_H */
