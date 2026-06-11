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
 */

/* ui_setup.c -- full-screen, tabbed configuration editor */

#include <stdio.h>
#include <stdlib.h>
#include "ui_files.h"
#include <string.h>

#include "ui_internal.h"
#include "../components/config.h"
#include "../core/ftn.h"
#include "../core/freq.h"

#define ST_TAB_COUNT 7

static const char *st_tab_names[ST_TAB_COUNT] =
    {
        "Identity", "Paths", "Display", "Editor", "Messages", "Colour/Font", "TTF Fallbacks"};

typedef enum
{
    FT_STR,      /* free text */
    FT_STR_AUTO, /* free text, but empty shows/saves as AUTO (charset) */
    FT_INT,      /* integer */
    FT_BOOL,     /* yes/no toggle */
    FT_MODE,     /* outbound layout: ASO / BSO / BSO+ext */
    FT_TRI,      /* tri-state 0/1/2 (e.g. forceintl: auto/always/never) */
    FT_TZ,       /* timezone offset in minutes; editing marks it manual */
    FT_TZAUTO,   /* yes/no: auto-detect TZ from OS (toggles timezone_is_manual) */
    FT_COLORMAP, /* integer 0-255, edit also marks color_map_initialized */
    FT_CYCLE,    /* cycle through predefined string options (e.g. TTF antialias: AUTO/OFF/ON) */
    FT_COLORPAIR /* color pair configuration (fg/bg) */
} FieldType;

typedef struct
{
    int tab;
    const char *label;
    FieldType type;
    size_t off; /* offsetof() into CrashEditCfg */
    int maxlen; /* for FT_STR */
} SetupField;

#define F_OFF(m) offsetof(CrashEditCfg, m)

static const SetupField st_fields[] =
    {
        /* Identity */
        {0, "Sysop name", FT_STR, F_OFF(sysop), 80},
        {0, "Origin", FT_STR, F_OFF(origin), 80},
        {0, "Save Charset", FT_STR_AUTO, F_OFF(charset), CHARSET_NAME_MAX},
        {0, "Timezone auto", FT_TZAUTO, F_OFF(timezone_is_manual), 0},
        {0, "Timezone (min)", FT_TZ, F_OFF(timezone_offset), 0},

        /* Paths */
        {1, "Area file", FT_STR, F_OFF(areafile), CFG_STR_MAX},
        {1, "Freq outbound dir", FT_STR, F_OFF(freq_outbound), CFG_STR_MAX},
        {1, "Freq outbound mode", FT_MODE, F_OFF(freq_mode), 0},
        {1, "Tagline file", FT_STR, F_OFF(tagline_file), CFG_STR_MAX},
        {1, "Template file", FT_STR, F_OFF(template_file), CFG_STR_MAX},

        /* Display */
        {2, "View hidden", FT_BOOL, F_OFF(viewhidden), 0},
        {2, "View kludges", FT_BOOL, F_OFF(viewkludge), 0},
        {2, "Render ANSI", FT_BOOL, F_OFF(viewansi), 0},
        {2, "Msglist first", FT_BOOL, F_OFF(msglistfirst), 0},
        {2, "Msglist fast", FT_BOOL, F_OFF(msglistfast), 0},
        {2, "Msglist header", FT_BOOL, F_OFF(msglistheader), 0},
        {2, "Show deleted", FT_BOOL, F_OFF(showdeleted), 0},
        {2, "Msglist max", FT_INT, F_OFF(msglistmax), 0},
        {2, "Search max hits", FT_INT, F_OFF(search_max), 0},
        {2, "Area sort", FT_STR, F_OFF(arealistsort_default), CFG_SORT_MAX},
        {2, "Area format", FT_STR, F_OFF(arealistformat), CFG_FORMAT_MAX},

        /* Editor */
        {3, "Tearline", FT_STR, F_OFF(tearline), 80},
        {3, "Force INTL 0/1/2", FT_TRI, F_OFF(forceintl), 0},
        {3, "Auto-wrap col", FT_INT, F_OFF(autowrap_col), 0},
        {3, "Quote margin", FT_INT, F_OFF(quotemargin), 0},
        {3, "Undo levels", FT_INT, F_OFF(undo_levels), 0},
        {3, "Hard wrap", FT_BOOL, F_OFF(hard_wrap), 0},
        {3, "Line numbers", FT_BOOL, F_OFF(show_line_numbers), 0},

        /* Messages */
        {4, "Greeting on", FT_BOOL, F_OFF(greeting), 0},
        {4, "Greeting text", FT_STR, F_OFF(greeting_text), CFG_STR_MAX},
        {4, "Attribution on", FT_BOOL, F_OFF(attribution), 0},
        {4, "Attrib (self)", FT_STR, F_OFF(attrib_self), CFG_STR_MAX},
        {4, "Attrib (other)", FT_STR, F_OFF(attrib_other), CFG_STR_MAX},
        {4, "Signature on", FT_BOOL, F_OFF(signature), 0},
        {4, "Signature text", FT_STR, F_OFF(signature_text), CFG_STR_MAX},

/* Colours and fonts */
#if defined(PLATFORM_AMIGA) || defined(PLATFORM_WIN32)
        {5, "Font", FT_STR, F_OFF(font), CFG_STR_MAX},
#endif

#if defined(PLATFORM_AMIGA)
        {5, "ANSI font", FT_STR, F_OFF(ansifont), CFG_STR_MAX},
        {5, "TTF enabled", FT_BOOL, F_OFF(ttf_enabled), 0},
        {5, "TTF font", FT_STR, F_OFF(ttf_font), CFG_STR_MAX},
        {5, "TTF size", FT_INT, F_OFF(ttf_size), 0},
        {5, "TTF antialias", FT_CYCLE, F_OFF(ttf_antialias), 0},
        {5, "TTF encoding", FT_CYCLE, F_OFF(ttf_use_utf8), 0},
#endif
        {5, "Cursor color", FT_INT, F_OFF(cursor_color), 0},
        {5, "Default BG color", FT_INT, F_OFF(default_bg_color), 0},
        {5, "Pen 0 (black)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 0 * sizeof(int), 0},
        {5, "Pen 1 (red)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 1 * sizeof(int), 0},
        {5, "Pen 2 (green)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 2 * sizeof(int), 0},
        {5, "Pen 3 (yellow)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 3 * sizeof(int), 0},
        {5, "Pen 4 (blue)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 4 * sizeof(int), 0},
        {5, "Pen 5 (magenta)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 5 * sizeof(int), 0},
        {5, "Pen 6 (cyan)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 6 * sizeof(int), 0},
        {5, "Pen 7 (white)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 7 * sizeof(int), 0},
        {5, "Pen 8 (br-black)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 8 * sizeof(int), 0},
        {5, "Pen 9 (br-red)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 9 * sizeof(int), 0},
        {5, "Pen 10 (br-green)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 10 * sizeof(int), 0},
        {5, "Pen 11 (br-yellow)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 11 * sizeof(int), 0},
        {5, "Pen 12 (br-blue)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 12 * sizeof(int), 0},
        {5, "Pen 13 (br-magenta)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 13 * sizeof(int), 0},
        {5, "Pen 14 (br-cyan)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 14 * sizeof(int), 0},
        {5, "Pen 15 (br-white)", FT_COLORMAP, offsetof(CrashEditCfg, color_map) + 15 * sizeof(int), 0},
        {5, "Normal", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_NORMAL * sizeof(int), 0},
        {5, "Selected", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_SELECTED * sizeof(int), 0},
        {5, "Header", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_HEADER * sizeof(int), 0},
        {5, "Status", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_STATUS * sizeof(int), 0},
        {5, "Menu", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_MENU * sizeof(int), 0},
        {5, "Menu Hot", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_MENU_HOT * sizeof(int), 0},
        {5, "Border", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_BORDER * sizeof(int), 0},
        {5, "Quote1", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_QUOTE1 * sizeof(int), 0},
        {5, "Quote2", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_QUOTE2 * sizeof(int), 0},
        {5, "Quote3", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_QUOTE3 * sizeof(int), 0},
        {5, "Quote4", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_QUOTE4 * sizeof(int), 0},
        {5, "Kludge", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_KLUDGE * sizeof(int), 0},
        {5, "Tear", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_TEAR * sizeof(int), 0},
        {5, "Origin", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_ORIGIN * sizeof(int), 0},
        {5, "Seenby", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_SEENBY * sizeof(int), 0},
        {5, "Via", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_VIA * sizeof(int), 0},
        {5, "Unread", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_UNREAD * sizeof(int), 0},
        {5, "Deleted", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_DELETED * sizeof(int), 0},
        {5, "Popup", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_POPUP * sizeof(int), 0},
        {5, "Popup Sel", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_POPUP_SEL * sizeof(int), 0},
        {5, "Error", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_ERROR * sizeof(int), 0},
        {5, "Tagline", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_TAGLINE * sizeof(int), 0},
        {5, "Match Search", FT_COLORPAIR, offsetof(CrashEditCfg, color_fg) + COL_SEARCH_MATCH * sizeof(int), 0},

/* TTF Fallbacks */
#ifdef PLATFORM_AMIGA
        {6, "Fallback 1", FT_STR, offsetof(CrashEditCfg, ttf_fallback[0]), CFG_STR_MAX},
        {6, "Fallback 1 Size", FT_INT, offsetof(CrashEditCfg, ttf_fallback_size[0]), 0},
        {6, "Fallback 2", FT_STR, offsetof(CrashEditCfg, ttf_fallback[1]), CFG_STR_MAX},
        {6, "Fallback 2 Size", FT_INT, offsetof(CrashEditCfg, ttf_fallback_size[1]), 0},
        {6, "Fallback 3", FT_STR, offsetof(CrashEditCfg, ttf_fallback[2]), CFG_STR_MAX},
        {6, "Fallback 3 Size", FT_INT, offsetof(CrashEditCfg, ttf_fallback_size[2]), 0},
        {6, "Fallback 4", FT_STR, offsetof(CrashEditCfg, ttf_fallback[3]), CFG_STR_MAX},
        {6, "Fallback 4 Size", FT_INT, offsetof(CrashEditCfg, ttf_fallback_size[3]), 0},
        {6, "Fallback 5", FT_STR, offsetof(CrashEditCfg, ttf_fallback[4]), CFG_STR_MAX},
        {6, "Fallback 5 Size", FT_INT, offsetof(CrashEditCfg, ttf_fallback_size[4]), 0},
        {6, "Fallback 6", FT_STR, offsetof(CrashEditCfg, ttf_fallback[5]), CFG_STR_MAX},
        {6, "Fallback 6 Size", FT_INT, offsetof(CrashEditCfg, ttf_fallback_size[5]), 0},
        {6, "Fallback 7", FT_STR, offsetof(CrashEditCfg, ttf_fallback[6]), CFG_STR_MAX},
        {6, "Fallback 7 Size", FT_INT, offsetof(CrashEditCfg, ttf_fallback_size[6]), 0},
        {6, "Fallback 8", FT_STR, offsetof(CrashEditCfg, ttf_fallback[7]), CFG_STR_MAX},
        {6, "Fallback 8 Size", FT_INT, offsetof(CrashEditCfg, ttf_fallback_size[7]), 0},
#endif
};

#define ST_FIELD_COUNT ((int)(sizeof(st_fields) / sizeof(st_fields[0])))

static const char *st_mode_name(int mode)
{
    switch (mode)
    {
    case FREQ_MODE_ASO:
        return "ASO";
    case FREQ_MODE_BSO:
        return "BSO";
    case FREQ_MODE_BSO_EXT:
        return "BSO+ext";
    default:
        return "(unset)";
    }
}

/* Render the value of a field into buf for display. */
static void st_format_value(const CrashEditCfg *w, const SetupField *fld, char *buf, int bufsz)
{
    const char *base = (const char *)w;

    switch (fld->type)
    {
    case FT_STR:
    {
        const char *s = base + fld->off;
        snprintf(buf, bufsz, "%s", s[0] ? s : "(empty)");
        break;
    }
    case FT_STR_AUTO:
    {
        /* Empty value means auto-detect; show AUTO so user sees what empty CHARSET means */
        const char *s = base + fld->off;
        snprintf(buf, bufsz, "%s", s[0] ? s : "AUTO");
        break;
    }
    case FT_INT:
    {
        int v = *(const int *)(base + fld->off);
        snprintf(buf, bufsz, "%d", v);
        break;
    }
    case FT_COLORMAP:
    {
        int v = *(const int *)(base + fld->off);
        snprintf(buf, bufsz, "%d", v);
        break;
    }
    case FT_TRI:
    {
        /* 0/1/2 — used by forceintl: auto/always/never */
        int v = *(const int *)(base + fld->off);
        const char *n = (v == 0) ? "auto" : (v == 1) ? "always"
                                        : (v == 2)   ? "never"
                                                     : "?";
        snprintf(buf, bufsz, "%d (%s)", v, n);
        break;
    }
    case FT_CYCLE:
    {
        /* Cycle through predefined string options */
        int v = *(const int *)(base + fld->off);
        const char *label = "";

        if (strcmp(fld->label, "TTF encoding") == 0)
        {
            /* TTF encoding: 0=UTF-16 BE (BMP only), 1=UTF-8 (full Unicode) */
            if (v == 0)
                label = "UTF-16";
            else
                label = "UTF-8";
        }
        else
        {
            /* TTF antialias: 0=AUTO, 1=OFF, 2=ON */
            label = (v == 0) ? "AUTO" : (v == 1) ? "OFF"
                                    : (v == 2)   ? "ON"
                                                 : "AUTO";
        }

        snprintf(buf, bufsz, "%s", label);
        break;
    }
    case FT_TZ:
    {
        int v = *(const int *)(base + fld->off);

        /* Show effective value: auto mode shows OS-detected value with "(auto)" tag, manual shows pinned value */
        if (w->timezone_is_manual)
            snprintf(buf, bufsz, "%d (manual)", v);
        else
            snprintf(buf, bufsz, "%d (auto)", ftn_effective_tz_offset(v, 0));

        break;
    }
    case FT_TZAUTO:
    {
        /* Bound to timezone_is_manual, shown inverted: auto = NOT manual. Toggling returns to OS auto-detect */
        int manual = *(const int *)(base + fld->off);
        snprintf(buf, bufsz, "%s", manual ? "no" : "yes");
        break;
    }
    case FT_BOOL:
    {
        int v = *(const int *)(base + fld->off);
        snprintf(buf, bufsz, "%s", v ? "yes" : "no");
        break;
    }
    case FT_MODE:
    {
        int v = *(const int *)(base + fld->off);
        snprintf(buf, bufsz, "%s", st_mode_name(v));
        break;
    }
    case FT_COLORPAIR:
    {
        int pair_index = (fld->off - offsetof(CrashEditCfg, color_fg)) / sizeof(int);
        int fg = *(const int *)(base + fld->off);
        int bg = *(const int *)(base + offsetof(CrashEditCfg, color_bg) + pair_index * sizeof(int));

        /* Color names for display */
        const char *fg_name = "unknown";
        const char *bg_name = "unknown";

        switch (fg)
        {
        case 0:
            fg_name = "black";
            break;
        case 1:
            fg_name = "red";
            break;
        case 2:
            fg_name = "green";
            break;
        case 3:
            fg_name = "yellow";
            break;
        case 4:
            fg_name = "blue";
            break;
        case 5:
            fg_name = "magenta";
            break;
        case 6:
            fg_name = "cyan";
            break;
        case 7:
            fg_name = "white";
            break;
        case 8:
            fg_name = "br-black";
            break;
        case 9:
            fg_name = "br-red";
            break;
        case 10:
            fg_name = "br-green";
            break;
        case 11:
            fg_name = "br-yellow";
            break;
        case 12:
            fg_name = "br-blue";
            break;
        case 13:
            fg_name = "br-magenta";
            break;
        case 14:
            fg_name = "br-cyan";
            break;
        case 15:
            fg_name = "br-white";
            break;
        }

        switch (bg)
        {
        case 0:
            bg_name = "black";
            break;
        case 1:
            bg_name = "red";
            break;
        case 2:
            bg_name = "green";
            break;
        case 3:
            bg_name = "yellow";
            break;
        case 4:
            bg_name = "blue";
            break;
        case 5:
            bg_name = "magenta";
            break;
        case 6:
            bg_name = "cyan";
            break;
        case 7:
            bg_name = "white";
            break;
        case 8:
            bg_name = "br-black";
            break;
        case 9:
            bg_name = "br-red";
            break;
        case 10:
            bg_name = "br-green";
            break;
        case 11:
            bg_name = "br-yellow";
            break;
        case 12:
            bg_name = "br-blue";
            break;
        case 13:
            bg_name = "br-magenta";
            break;
        case 14:
            bg_name = "br-cyan";
            break;
        case 15:
            bg_name = "br-white";
            break;
        }

        snprintf(buf, bufsz, "%s on %s", fg_name, bg_name);
        break;
    }
    }
}

/* Edit one field in place on the working copy. */
static void st_edit_field(CrashEditCfg *w, const SetupField *fld)
{
    char *base = (char *)w;

    switch (fld->type)
    {
    case FT_STR:
    {
        char *s = base + fld->off;

        /* File fields: use file picker */
        if (strcmp(fld->label, "Area file") == 0 ||
            strcmp(fld->label, "Tagline file") == 0 ||
            strcmp(fld->label, "Template file") == 0 ||
            strcmp(fld->label, "Font") == 0 ||
            strcmp(fld->label, "ANSI font") == 0 ||
            strcmp(fld->label, "TTF font") == 0 ||
            strncmp(fld->label, "Fallback", 8) == 0)
        {
            char tmp[CFG_STR_MAX];
            strncpy(tmp, s, sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';

            if (ui_files_pick(fld->label, NULL, tmp, sizeof(tmp)) == 0)
            {
                strncpy(s, tmp, CFG_STR_MAX - 1);
                s[CFG_STR_MAX - 1] = '\0';
            }
        }
        /* Directory fields: use directory picker */
        else if (strcmp(fld->label, "Freq outbound dir") == 0)
        {
            char tmp[CFG_STR_MAX];
            strncpy(tmp, s, sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';

            if (ui_files_pick_dir(fld->label, NULL, tmp, sizeof(tmp)) == 0)
            {
                strncpy(s, tmp, CFG_STR_MAX - 1);
                s[CFG_STR_MAX - 1] = '\0';
            }
        }
        else
        {
            wchar_t wtmp[CFG_STR_MAX];
            int cap = fld->maxlen > 0 && fld->maxlen < (int)sizeof(wtmp) ? fld->maxlen : (int)sizeof(wtmp);
            wchar_t *w_initial;

            w_initial = utf8_to_wcs(s, NULL);
            wtmp[0] = L'\0';

            if (w_initial)
            {
                wcsncpy(wtmp, w_initial, (size_t)(cap - 1));
                wtmp[cap - 1] = L'\0';
                free(w_initial);
            }

            if (ui_popup_input(fld->label, "New value:", wtmp, cap) == 0)
            {
                char *u = wcs_to_utf8(wtmp, (int)wcslen(wtmp));
                if (u)
                {
                    size_t n = strlen(u);

                    if (n >= (size_t)cap)
                        n = cap - 1;

                    memcpy(s, u, n);

                    s[n] = '\0';
                    free(u);
                }
            }
        }

        break;
    }
    case FT_STR_AUTO:
    {
        /* For charset field: cycle between AUTO and available charsets */
        if (strstr(fld->label, "Charset") != NULL)
        {
            char *s = base + fld->off;
            const char **charsets;
            int count, i, current = -1;

            charsets = charset_get_list(&count);

            if (charsets && count > 0)
            {
                /* Find current charset in list (empty = AUTO is position -1) */
                for (i = 0; i < count; i++)
                {
                    if (s[0] && strcasecmp(s, charsets[i]) == 0)
                    {
                        current = i;
                        break;
                    }
                }

                /* Move to next: if at last or not found (AUTO), go to first */
                current++;

                if (current >= count)
                    s[0] = '\0'; /* Back to AUTO (empty) */
                else
                {
                    strncpy(s, charsets[current], CHARSET_NAME_MAX - 1);
                    s[CHARSET_NAME_MAX - 1] = '\0';
                }
            }
        }
        else
        {
            /* Other FT_STR_AUTO fields: normal popup editing */
            char *s = base + fld->off;
            wchar_t wtmp[CFG_STR_MAX];
            int cap = fld->maxlen > 0 && fld->maxlen < (int)sizeof(wtmp) ? fld->maxlen : (int)sizeof(wtmp);
            wchar_t *w_initial;

            w_initial = utf8_to_wcs(s, NULL);
            wtmp[0] = L'\0';

            if (w_initial)
            {
                wcsncpy(wtmp, w_initial, (size_t)(cap - 1));
                wtmp[cap - 1] = L'\0';
                free(w_initial);
            }

            if (ui_popup_input(fld->label, "New value (empty = AUTO):", wtmp, cap) == 0)
            {
                char *u = wcs_to_utf8(wtmp, (int)wcslen(wtmp));

                if (u)
                {
                    size_t n = strlen(u);

                    if (n >= (size_t)cap)
                        n = cap - 1;

                    memcpy(s, u, n);

                    s[n] = '\0';
                    free(u);
                }
            }
        }

        break;
    }
    case FT_INT:
    case FT_TZ:
    case FT_COLORMAP:
    {
        int *v = (int *)(base + fld->off);
        wchar_t wtmp[32];
        char tmp[32];

        snprintf(tmp, sizeof(tmp), "%d", *v);

        wchar_t *w_initial = utf8_to_wcs(tmp, NULL);
        wtmp[0] = L'\0';

        if (w_initial)
        {
            wcsncpy(wtmp, w_initial, 31);
            wtmp[31] = L'\0';
            free(w_initial);
        }

        if (ui_popup_input(fld->label, "New value (integer):", wtmp, 32) == 0)
        {
            char *u = wcs_to_utf8(wtmp, (int)wcslen(wtmp));
            int parsed = 0;

            if (u)
            {
                parsed = (int)strtol(u, NULL, 10);
                free(u);
            }

            /* Amiga COLORMAP pens are physical pen numbers and can never be negative, so clamp those.
             * Everything else -- timezone (zones west of UTC) AND colour fields cursor_color/default_bg_color,
             * where -1 is the "unset, use terminal default" sentinel -- is left as typed */
            if (fld->type == FT_COLORMAP && parsed < 0)
                parsed = 0;

            *v = parsed;

            /* Special validation for TTF_SIZE (6-96) */
            if (fld->off == F_OFF(ttf_size))
            {
                if (parsed < 6 || parsed > 96)
                    *v = 14;
            }

            /* Editing timezone implies user wants manual offset rather than auto-detected one */
            if (fld->type == FT_TZ)
                w->timezone_is_manual = 1;

            /* Editing any Amiga pen flips "use configured palette" switch so cfg_save emits fresh COLORMAP lines */
            if (fld->type == FT_COLORMAP)
                w->color_map_initialized = 1;
        }

        break;
    }
    case FT_TRI:
    {
        /* Cycle 0 -> 1 -> 2 -> 0; for forceintl this maps to
         * auto -> always -> never -> auto. */
        int *v = (int *)(base + fld->off);
        *v = (*v + 1) % 3;
        break;
    }
    case FT_BOOL:
    {
        int *v = (int *)(base + fld->off);
        *v = !*v; /* toggle */
        break;
    }
    case FT_TZAUTO:
    {
        /* Toggle timezone_is_manual: flipping to auto (manual=0)
         * returns to OS detection; Timezone field shows detected
         * value with "(auto)" tag. No popup needed -- SPACE/ENTER flips it like a bool */
        int *v = (int *)(base + fld->off);
        *v = !*v;
        break;
    }
    case FT_MODE:
    {
        int *v = (int *)(base + fld->off);

        /* Cycle UNSET -> ASO -> BSO -> BSO_EXT -> ASO ... */
        switch (*v)
        {
        case FREQ_MODE_ASO:
            *v = FREQ_MODE_BSO;
            break;
        case FREQ_MODE_BSO:
            *v = FREQ_MODE_BSO_EXT;
            break;
        case FREQ_MODE_BSO_EXT:
            *v = FREQ_MODE_ASO;
            break;
        default:
            *v = FREQ_MODE_ASO;
            break;
        }

        break;
    }
    case FT_CYCLE:
    {
        /* Cycle through predefined options */
        int *v = (int *)(base + fld->off);

        if (strcmp(fld->label, "TTF encoding") == 0)
            /* TTF encoding: 0=UTF-16, 1=UTF-8 (toggle between 2 options) */
            *v = (*v == 0) ? 1 : 0;
        else
            /* TTF antialias: 0=AUTO, 1=OFF, 2=ON (cycle through 3 options) */
            *v = (*v + 1) % 3;

        break;
    }
    case FT_COLORPAIR:
    {
        int pair_index = (fld->off - offsetof(CrashEditCfg, color_fg)) / sizeof(int);
        int *fg = (int *)(base + fld->off);
        int *bg = (int *)(base + offsetof(CrashEditCfg, color_bg) + pair_index * sizeof(int));

        /* Color names and their values */
        static const char *color_names[] =
            {
                "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
                "bright black", "bright red", "bright green", "bright yellow",
                "bright blue", "bright magenta", "bright cyan", "bright white"};

        /* Select foreground color */
        int fg_result = ui_popup_list("Select Foreground", color_names, 16, *fg);

        if (fg_result < 0)
            return; /* User canceled */

        /* Select background color */
        int bg_result = ui_popup_list("Select Background", color_names, 16, *bg);

        if (bg_result < 0)
            return; /* User canceled */

        /* Apply colors only if both selections were confirmed */
        *fg = fg_result;
        *bg = bg_result;

        break;
    }
    }
}

/* Count fields on a given tab and map a within-tab index to a global
 * st_fields[] index. Returns the global index, or -1. */
static int st_field_on_tab(int tab, int within)
{
    int i, c = 0;

    for (i = 0; i < ST_FIELD_COUNT; i++)
    {
        if (st_fields[i].tab == tab)
        {
            if (c == within)
                return i;
            c++;
        }
    }

    return -1;
}

static int st_tab_field_count(int tab)
{
    int i, c = 0;

    for (i = 0; i < ST_FIELD_COUNT; i++)
        if (st_fields[i].tab == tab)
            c++;

    return c;
}

int ui_setup_run(UiApp *app)
{
    CrashEditCfg work; /* working copy: cancel discards */
    int tab = 0;
    int sel = 0; /* within-tab selection */
    int scroll_offset = 0;
    int key;
    int dirty = 0;
    char orig_ttf_font[512];
    int orig_ttf_size;

    if (!app || !app->cfg)
        return 0;

    work = *app->cfg; /* struct copy (shallow; all fields are inline arrays/ints) */

    /* Save original font values to detect changes */
    strncpy(orig_ttf_font, work.ttf_font, sizeof(orig_ttf_font) - 1);
    orig_ttf_font[sizeof(orig_ttf_font) - 1] = '\0';
    orig_ttf_size = work.ttf_size;

    for (;;)
    {
        int i, c, row;
        int tabx;
        int nfields = st_tab_field_count(tab);
        int valw;

        erase();

        /* Title bar */
        attron(COLOR_PAIR(COL_HEADER));

        for (i = 0; i < COLS; i++)
            mvaddch(0, i, ' ');

        mvprintw(0, 1, "CrashEdit Setup%s   (%s)", dirty ? " *" : "", app->cfg_path ? app->cfg_path : "?");
        attroff(COLOR_PAIR(COL_HEADER));

        /* Tab row */
        tabx = 1;

        for (i = 0; i < ST_TAB_COUNT; i++)
        {
            if (i == tab)
                attron(COLOR_PAIR(COL_SELECTED));
            else
                attron(COLOR_PAIR(COL_NORMAL));

            mvprintw(2, tabx, " %s ", st_tab_names[i]);
            tabx += (int)strlen(st_tab_names[i]) + 3;

            attroff(COLOR_PAIR(COL_SELECTED));
            attroff(COLOR_PAIR(COL_NORMAL));
        }

        /* Fields of the current tab */
        int visible_fields = LINES - 6;

        if (visible_fields < 1)
            visible_fields = 1;

        row = 4;
        c = 0;

        for (i = 0; i < ST_FIELD_COUNT; i++)
        {
            char val[CFG_STR_MAX + 16];

            if (st_fields[i].tab != tab)
                continue;

            if (c < scroll_offset || c >= scroll_offset + visible_fields)
            {
                c++;
                continue;
            }

            st_format_value(&work, &st_fields[i], val, sizeof(val));

            if (c == sel)
                attron(COLOR_PAIR(COL_SELECTED));
            else
                attron(COLOR_PAIR(COL_NORMAL));

            int valw = COLS - 24;

            if (valw < 1)
                valw = 1; /* never pass a negative precision to printf */

#ifdef PLATFORM_AMIGA
            /* Workaround for AmigaOS mvprintw bug with "0" value */
            if (strcmp(val, "0") == 0)
                mvprintw(row, 2, "%-16s : %s", st_fields[i].label, "0");
            else
                mvprintw(row, 2, "%-16s : %-.*s", st_fields[i].label, valw, val);
#else
            mvprintw(row, 2, "%-16s : %-.*s", st_fields[i].label, valw, val);
#endif

            attroff(COLOR_PAIR(COL_SELECTED));
            attroff(COLOR_PAIR(COL_NORMAL));

            row++;
            c++;
        }

        if (nfields > visible_fields)
        {
            /* Show up arrow if not at top */
            if (scroll_offset > 0)
                mvprintw(LINES - 2, COLS - 3, "↑");

            /* Show down arrow if not at bottom */
            if (scroll_offset + visible_fields < nfields)
                mvprintw(LINES - 2, COLS - 2, "↓");
        }

        /* Footer */
        attron(COLOR_PAIR(COL_STATUS));

        for (i = 0; i < COLS; i++)
            mvaddch(LINES - 1, i, ' ');

        mvprintw(LINES - 1, 1, "Up/Dn field  Left/Right tab  Enter edit  F10/S save+reload  ESC cancel");
        attroff(COLOR_PAIR(COL_STATUS));

        refresh();

        key = wrapper_getch();

        /* ESC: cancel — confirm unsaved edits, mirroring message
         * editor's "modified, discard?" prompt */
        if (key == 27)
        {
            if (dirty)
            {
                if (ui_popup_confirm("Setup", "Discard unsaved changes?") != 1)
                    continue; /* user said no — stay in setup */
            }

            return 0;
        }

        if (key == KEY_F(10) || key == 'S' || key == 's')
        {
            /* Save working copy back to the file (preserving the rest),
             * then ask the main loop to reload from disk */
            if (!app->cfg_path)
            {
                ui_status(app, "No config path known; cannot save");
                continue;
            }

            if (cfg_save(&work, app->cfg_path) != 0)
            {
                ui_status(app, "Save failed: %s", app->cfg_path);
                continue;
            }

            /* Apply new config in-place: update live struct, reapply colors/font */
            *app->cfg = work;
            ui_reapply_config(app);

            return 1;
        }

        if (key == KEY_LEFT)
        {
            if (tab > 0)
                tab--;
            else
                tab = ST_TAB_COUNT - 1;

            sel = 0;
            scroll_offset = 0;
            continue;
        }

        if (key == KEY_RIGHT || key == '\t')
        {
            tab = (tab + 1) % ST_TAB_COUNT;
            sel = 0;
            scroll_offset = 0;
            continue;
        }

        if (key == KEY_UP || key == 'k')
        {
            if (sel > 0)
            {
                sel--;

                if (sel < scroll_offset)
                    scroll_offset = sel;
            }

            continue;
        }

        if (key == KEY_DOWN || key == 'j')
        {
            if (sel < nfields - 1)
            {
                sel++;

                if (sel >= scroll_offset + visible_fields)
                    scroll_offset = sel - visible_fields + 1;
            }

            continue;
        }

        if (key == KEY_HOME || key == CTRL('B'))
        {
            sel = 0;
            scroll_offset = 0;
            continue;
        }

        if (key == KEY_END || key == CTRL('E'))
        {
            sel = nfields - 1;

            if (sel >= scroll_offset + visible_fields)
                scroll_offset = sel - visible_fields + 1;

            continue;
        }

        if (key == KEY_PPAGE || key == CTRL('U'))
        {
            sel -= visible_fields;

            if (sel < 0)
                sel = 0;

            if (sel < scroll_offset)
                scroll_offset = sel;

            continue;
        }

        if (key == KEY_NPAGE || key == CTRL('D'))
        {
            sel += visible_fields;

            if (sel >= nfields)
                sel = nfields - 1;

            if (sel >= scroll_offset + visible_fields)
                scroll_offset = sel - visible_fields + 1;

            continue;
        }

        if (key == '\n' || key == '\r' || key == KEY_ENTER ||
            key == ' ')
        {
            int gi = st_field_on_tab(tab, sel);

            if (gi >= 0)
            {
                st_edit_field(&work, &st_fields[gi]);
                dirty = 1;
            }

            continue;
        }
    }
}
