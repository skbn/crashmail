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

/* ui_arealist.c -- Area list view with sorting and formatting */
#include "ui_internal.h"
#include "ui_arealist_layout.h"
#include "ui_search.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sorting */
typedef struct
{
    const AreaList *list;
    const char *spec;
    const wchar_t *filter; /* wchar_t filter text from user */
    /* Precomputed table: matches[i] != 0 iff entries[i].name matches filter */
    const unsigned char *matches;
} SortCtx;

static SortCtx s_ctx;

/* Help text for area list */
static const char *AREALIST_HELP[] =
    {
        "Area List - Key Bindings:",
        "",
        "  Up/Down,j/k    Move selection",
#ifdef PLATFORM_AMIGA
        "  Ctrl+U/D       Page up/down",
        "  Ctrl+B/E       First / last area",
#else
        "  PgUp/PgDn      Page up/down",
        "  Home/End       First / last area",
#endif
        "  Enter          Open selected area",
        "  /              Search (fuzzy match)",
        "  G              Goto area by name",
        "  s              Change sort order",
        "  Alt+C, C       Catch-up (Y=this area, A=ALL areas)",
        "  Alt+S, R       Rescan all areas",
        "  Ctrl+F         File request",
        "  S              Setup",
        "  P              Search",
        "  ESC, q         Quit application",
        "  F1, ?          This help screen",
        ""};
#define AREALIST_HELP_N ((int)(sizeof(AREALIST_HELP) / sizeof(AREALIST_HELP[0])))

/* Forward decl */
static int field_has_wcs(const char *hay_utf8, const wchar_t *needle);

/* compare_two: like GoldED+, returns +1 if x>y, -1 if x<y, 0 eq */
static int cmp_int(int x, int y) { return (x < y) ? -1 : (x > y) ? 1
                                                                 : 0; }

/* Comparator (sticky +/-, case-sensitive for U/u/P/p/Y/y) */
static int cmp_areas(const void *pa, const void *pb)
{
    int ai = *(const int *)pa;
    int bi = *(const int *)pb;
    const AreaEntry *a_orig = &s_ctx.list->entries[ai];
    const AreaEntry *b_orig = &s_ctx.list->entries[bi];
    const AreaEntry *A = a_orig;
    const AreaEntry *B = b_orig;
    const char *ptr = s_ctx.spec ? s_ctx.spec : "";
    int rev = 0;
    int cmp;

    while (*ptr)
    {
        char c = *ptr++;

        switch (c)
        {
        case '-':
            rev = 1;
            A = b_orig;
            B = a_orig;
            continue;
        case '+':
            rev = 0;
            A = a_orig;
            B = b_orig;
            continue;

        case 'A':
        case 'a': /* AKA */
            if ((cmp = strcasecmp(A->aka ? A->aka : "", B->aka ? B->aka : "")) != 0)
                return cmp;

            break;
        case 'B':
        case 'b': /* board: no board concept here -> original idx */
            if ((cmp = cmp_int(ai, bi)) != 0)
                return rev ? -cmp : cmp;

            break;
        case 'D':
        case 'd': /* description */
            if ((cmp = strcasecmp(A->description ? A->description : "", B->description ? B->description : "")) != 0)
                return cmp;

            break;
        case 'E':
        case 'e': /* echoid (name) */
            if ((cmp = strcasecmp(A->name ? A->name : "", B->name ? B->name : "")) != 0)
                return cmp;

            break;
        case 'F':
        case 'f': /* fuzzy live filter (echoid only) */
            if (s_ctx.matches)
            {
                /* indices into matches[] follow +/- swap convention */
                int A_i = (A == a_orig) ? ai : bi;
                int B_i = (B == a_orig) ? ai : bi;
                int amay = s_ctx.matches[A_i] ? 1 : 0;
                int bmay = s_ctx.matches[B_i] ? 1 : 0;

                if ((cmp = cmp_int(bmay, amay)) != 0) /* compare_two(B,A) */
                    return cmp;
            }

            break;
        case 'G':
        case 'g': /* groupid (sticky sepfirst) */
            if ((cmp = cmp_int(A->groupid, B->groupid)) != 0)
                return cmp;

            break;
        case 'O':
        case 'o': /* original index */
            if ((cmp = cmp_int(ai, bi)) != 0)
                return rev ? -cmp : cmp;

            break;
        case 'T':
        case 't': /* type: netmail<echo<local */
        {
            static const int order[3] = {1, 0, 2}; /* echo, netmail, local */
            int ta = (A->type >= 0 && A->type < 3) ? order[A->type] : 99;
            int tb = (B->type >= 0 && B->type < 3) ? order[B->type] : 99;

            if ((cmp = cmp_int(ta, tb)) != 0)
                return cmp;

            break;
        }

        case 'U': /* Three-tier pending: tier2=new_count>0, tier1=unread>0, tier0=none */
        {
            int a_tier = (A->new_count > 0) ? 2 : ((A->unread > 0) ? 1 : 0);
            int b_tier = (B->new_count > 0) ? 2 : ((B->unread > 0) ? 1 : 0);

            /* Without rev: higher tier first (descending) */
            if ((cmp = cmp_int(b_tier, a_tier)) != 0)
                return cmp;

            /* Same tier — tiebreak by the count that defines that tier */
            if (a_tier == 2)
            {
                if ((cmp = cmp_int(B->new_count, A->new_count)) != 0)
                    return cmp;
            }
            else if (a_tier == 1)
            {
                if ((cmp = cmp_int(B->unread, A->unread)) != 0)
                    return cmp;
            }

            break;
        }
        case 'u': /* Pending boolean: has unread (new_count>0 implies unread>0) */
            if ((cmp = cmp_int(B->unread > 0 ? 1 : 0, A->unread > 0 ? 1 : 0)) != 0)
                return cmp;

            break;
        case 'P': /* personal count: we don't track, treat as 0 */
        case 'p':
            break;
        case 'X':
        case 'x': /* basetype: always JAM */
            break;
        case 'Y':
        case 'y': /* Three-tier pending without tiebreaker (compose with secondary key) */
        {
            int a_tier = (A->new_count > 0) ? 2 : ((A->unread > 0) ? 1 : 0);
            int b_tier = (B->new_count > 0) ? 2 : ((B->unread > 0) ? 1 : 0);

            if ((cmp = cmp_int(b_tier, a_tier)) != 0)
                return cmp;

            break;
        }
        case 'Z':
        case 'z': /* msgbase path */
            if ((cmp = strcasecmp(A->path ? A->path : "", B->path ? B->path : "")) != 0)
                return cmp;

            break;
        case 'S':
        case 's': /* separator: not applicable */
        case 'M':
        case 'm': /* marked: not tracked */
        default:
            break;
        }
    }

    /* Tie-break: original order */
    return cmp_int(ai, bi);
}

/* Test if UTF-8 field contains wchar_t needle (case-insensitive) */
static int field_has_wcs(const char *hay_utf8, const wchar_t *needle)
{
    wchar_t *w;
    int found;

    if (!hay_utf8)
        return 0;

    w = utf8_to_wcs(hay_utf8, NULL);

    if (!w)
        return 0;

    found = wcs_casestr(w, needle) != NULL;
    free(w);

    return found;
}

static int area_matches_filter(const AreaEntry *a, const wchar_t *flt)
{
    if (!flt || !flt[0])
        return 1;

    if (field_has_wcs(a->name, flt))
        return 1;

    if (field_has_wcs(a->description, flt))
        return 1;

    if (field_has_wcs(a->aka, flt))
        return 1;

    return 0;
}

void ui_arealist_rebuild_order(UiApp *app)
{
    int i, n;

    if (!app || !app->areas)
        return;

    free(app->area_order);

    app->area_order = NULL;
    app->area_order_count = 0;

    if (app->areas->count <= 0)
        return;

    app->area_order = (int *)malloc((size_t)app->areas->count * sizeof(int));

    if (!app->area_order)
        return;

    n = 0;

    for (i = 0; i < app->areas->count; i++)
    {
        if (!area_matches_filter(&app->areas->entries[i], app->area_search))
            continue;

        app->area_order[n++] = i;
    }

    app->area_order_count = n;

    s_ctx.list = app->areas;
    s_ctx.spec = app->cfg->arealistsort;
    s_ctx.filter = app->area_search;
    s_ctx.matches = NULL;

    /* Only sort if user specified sort order in config */
    if (app->cfg->arealistsort[0] != '\0')
    {
        /* Precompute fuzzy-match table if 'F'/'f' in sort spec AND active filter */
        unsigned char *tbl = NULL;

        if (app->area_search[0] && (strchr(app->cfg->arealistsort, 'F') || strchr(app->cfg->arealistsort, 'f')))
        {
            int total = app->areas->count;

            tbl = (unsigned char *)malloc((size_t)(total > 0 ? total : 1));

            if (tbl)
            {
                int k;

                for (k = 0; k < total; k++)
                    tbl[k] = field_has_wcs(app->areas->entries[k].name, app->area_search) ? 1 : 0;

                s_ctx.matches = tbl;
            }
        }

        qsort(app->area_order, (size_t)n, sizeof(int), cmp_areas);

        if (tbl)
        {
            s_ctx.matches = NULL;
            free(tbl);
        }
    }

    /* If no sort spec, keep original order from areafile */
    if (app->area_sel >= n)
        app->area_sel = n - 1;

    if (app->area_sel < 0)
        app->area_sel = 0;
}

/* Column layout calculation (GoldED+ AREALISTFORMAT style) */
static void compute_layout_cached(const UiApp *app, int maxcol, ArealistLayout *L)
{
    ui_arealist_layout_get(app->areas, app->cfg ? app->cfg->arealistformat : NULL, maxcol, L);
}

/* Write UTF-8 string left-aligned into buf[start..start+width]; fast ASCII path, wide-char fallback */
static void put_field_utf8(char *buf, int maxcol, int start, int width, const char *utf8_in)
{
    const char *src;
    wchar_t *w;
    char *out;
    int wl, i;
    int is_ascii = 1;

    if (start < 0 || width <= 0 || start >= maxcol)
        return;

    if (start + width > maxcol)
        width = maxcol - start;

    src = utf8_in ? utf8_in : "";

    /* Cheap scan for high bits. break-early on first non-ASCII byte */
    for (i = 0; src[i]; i++)
    {
        if ((unsigned char)src[i] >= 0x80)
        {
            is_ascii = 0;
            break;
        }
    }

    if (is_ascii)
    {
        for (i = 0; src[i] && i < width && start + i < maxcol; i++)
            buf[start + i] = src[i];

        return;
    }

    /* Slow path: real UTF-8 with multi-byte sequences */
    w = utf8_to_wcs(src, NULL);

    if (!w)
        return;

    wl = (int)wcslen(w);

    if (wl > width)
        wl = width;

    out = wcs_to_utf8(w, wl);

    if (out)
    {
        for (i = 0; out[i] && i < width && start + i < maxcol; i++)
            buf[start + i] = out[i];

        free(out);
    }

    free(w);
}

/* Right-align an ASCII string of width n into buf at [pos..pos+width] */
static void put_field_right_ascii(char *buf, int maxcol, int pos, int width, const char *s)
{
    int n = (int)strlen(s);
    int start;
    int i;

    if (pos < 0 || width <= 0)
        return;

    if (n > width)
    {
        s += (n - width);
        n = width;
    }

    start = pos + width - n;

    for (i = 0; i < n && start + i < maxcol; i++)
        buf[start + i] = s[i];
}

/* Render one row using the precomputed layout */
static void draw_row(int y, int x_start, int width, const UiApp *app, const AreaEntry *a, int area_num, int is_sel, const ArealistLayout *L)
{
    char buf[512];
    int maxcol = (width < 510) ? width : 510;
    int i;

    for (i = 0; i < maxcol; i++)
        buf[i] = ' ';

    buf[maxcol] = '\0';

    /* Area number (right) */
    if (L->area_pos >= 0)
    {
        char tmp[16];

        snprintf(tmp, sizeof(tmp), "%u", (unsigned)area_num);
        put_field_right_ascii(buf, maxcol, L->area_pos, L->area_width, tmp);
    }

    /* Marked / Personal: not tracked, blank */
    if (L->marked_pos >= 0)
        buf[L->marked_pos] = ' ';

    if (L->pmark_pos >= 0)
        buf[L->pmark_pos] = ' ';

    /* Count (right) - '-' if no msgs yet */
    if (L->count_pos >= 0)
    {
        char tmp[16];
        int maxmsgs = app->cfg ? app->cfg->msglistmax : 0;
        int shown = (maxmsgs > 0 && a->total_msgs > maxmsgs) ? maxmsgs : a->total_msgs;

        if (shown > 0)
            snprintf(tmp, sizeof(tmp), "%u", (unsigned)shown);
        else
        {
            tmp[0] = '-';
            tmp[1] = '\0';
        }

        put_field_right_ascii(buf, maxcol, L->count_pos, L->count_width, tmp);
    }

    /* Unread (right) - '-' if zero */
    if (L->unread_pos >= 0)
    {
        char tmp[16];

        if (a->unread > 0)
            snprintf(tmp, sizeof(tmp), "%u", (unsigned)a->unread);
        else
        {
            tmp[0] = '-';
            tmp[1] = '\0';
        }

        put_field_right_ascii(buf, maxcol, L->unread_pos, L->unread_width, tmp);
    }

    /* Changed flag: "*" if new_count > 0 (msgs above lastseen) */
    if (L->changed_pos >= 0)
        buf[L->changed_pos] = (a->new_count > 0) ? '*' : ' ';

    /* Echoid */
    if (L->echoid_pos >= 0)
        put_field_utf8(buf, maxcol, L->echoid_pos, L->echoid_width, a->name ? a->name : "");

    /* Description */
    if (L->desc_pos >= 0)
        put_field_utf8(buf, maxcol, L->desc_pos, L->desc_width, a->description ? a->description : "");

    /* GroupID: GoldED+ shows A-Z (or 1-3 digit number); we use 1..26 -> letter */
    if (L->groupid_pos >= 0 && a->groupid > 0)
    {
        if (L->groupid_width >= 3 || a->groupid > 26)
        {
            char tmp[16];

            snprintf(tmp, sizeof(tmp), "%u", (unsigned)a->groupid);
            put_field_right_ascii(buf, maxcol, L->groupid_pos, L->groupid_width, tmp);
        }
        else if (a->groupid >= 1 && a->groupid <= 26)
        {
            buf[L->groupid_pos] = (char)('A' + a->groupid - 1);
        }
    }

    mvaddnstr(y, x_start, buf, maxcol);
}

/* Draw header row using the same layout (col titles align with data) */
static void draw_header(int y, int width, const ArealistLayout *L)
{
    char buf[512];
    int maxcol = (width < 510) ? width : 510;
    int i;

    for (i = 0; i < maxcol; i++)
        buf[i] = ' ';

    buf[maxcol] = '\0';

    /* GoldED+ uses wmessage to overlay column titles on the border */
    if (L->area_pos >= 0 && L->area_width > 0)
        put_field_utf8(buf, maxcol, L->area_pos, L->area_width, "Area");

    if (L->desc_pos >= 0 && L->desc_width > 0)
        put_field_utf8(buf, maxcol, L->desc_pos, L->desc_width, "Description");

    if (L->count_pos >= 0 && L->count_width > 0)
        put_field_right_ascii(buf, maxcol, L->count_pos, L->count_width, "Msgs");

    if (L->unread_pos >= 0 && L->unread_width > 0)
        put_field_right_ascii(buf, maxcol, L->unread_pos, L->unread_width, "New");

    if (L->echoid_pos >= 0 && L->echoid_width > 0)
        put_field_utf8(buf, maxcol, L->echoid_pos, L->echoid_width, "EchoID");

    if (L->groupid_pos >= 0 && L->groupid_width > 0)
    {
        const char *t = (L->groupid_width >= 3) ? "Grp" : "G";
        put_field_utf8(buf, maxcol, L->groupid_pos, L->groupid_width, t);
    }

    /* M / P / N have width 1, no room for a title */
    mvaddnstr(y, 0, buf, maxcol);
}

/* Actions reachable from key bindings (sharable between hotkeys) */
/* Catch up area: set LASTREAD to highest msgnum. Returns msg count or -1 on error */
static int catchup_one_area(UiApp *app, int area_idx)
{
    AreaEntry *ae;
    JamArea jam;
    JamMsgInfo *msgs;
    uint32_t hi = 0, ucrc;
    int n = 0, i;

    if (!app || area_idx < 0 || area_idx >= app->areas->count)
        return -1;

    ae = &app->areas->entries[area_idx];

    if (jam_open(&jam, ae->path) != 0)
        return -1;

    msgs = jam_load_headers(&jam, &n, 0, (uint32_t)app->cfg->msglistmax);

    if (!msgs)
    {
        jam_close(&jam);
        return -1;
    }

    for (i = 0; i < n; i++)
    {
        if (msgs[i].msgnum > hi)
            hi = msgs[i].msgnum;
    }

    ucrc = jam_username_crc(app->cfg->sysop);

    if (jam_lock(&jam, JAM_LOCK_RETRIES) == 0)
    {
        jam_write_lastread(&jam, ucrc, hi, hi);
        jam_unlock(&jam);
    }

    free(msgs);

    jam_close(&jam);

    /* Catchup marks area as read AND seen: sync in-memory AreaEntry with JAM */
    ae->lastread = hi;
    ae->lastseen = hi;
    ae->unread = 0;
    ae->new_count = 0;

    return n;
}

/* Alt+C catch-up: Y = current area, A = all areas */
static void arealist_action_catchup(UiApp *app)
{
    int rc, idx;
    AreaEntry *ae;
    char prompt[160];

    if (!app || app->area_order_count <= 0)
        return;

    if (app->area_sel < 0 || app->area_sel >= app->area_order_count)
        return;

    idx = app->area_order[app->area_sel];
    ae = &app->areas->entries[idx];
    snprintf(prompt, sizeof(prompt), "Catch up '%s'?  (A = ALL %d areas)", ae->name ? ae->name : "(area)", app->area_order_count);

    rc = ui_popup_confirm_all("Catch-up", prompt);

    if (rc < 0 || rc == 0)
        return;

    if (rc == 1)
    {
        /* Current area only */
        int n = catchup_one_area(app, idx);

        if (n < 0)
            ui_status(app, "Cannot open area");
        else
            ui_status(app, "%s: marked %d msg(s) as read", ae->name ? ae->name : "?", n);
    }
    else /* rc == 2: ALL */
    {
        int i, total_msgs = 0, ok_areas = 0, failed_areas = 0;

        ui_status(app, "Catching up all areas...");
        ui_draw_statusbar(app);
        refresh();

        for (i = 0; i < app->area_order_count; i++)
        {
            int real_idx = app->area_order[i];
            int n = catchup_one_area(app, real_idx);

            if (n < 0)
                failed_areas++;
            else
            {
                ok_areas++;
                total_msgs += n;
            }
        }

        if (failed_areas == 0)
            ui_status(app, "Caught up %d area(s), %d msg(s) marked read", ok_areas, total_msgs);
        else
            ui_status(app, "Caught up %d area(s), %d msg(s); %d area(s) failed", ok_areas, total_msgs, failed_areas);
    }
}

/* Refresh message counts for all areas */
static void arealist_action_rescan(UiApp *app)
{
    if (!app)
        return;

    ui_status(app, "Rescanning areas...");
    ui_draw_statusbar(app);

    refresh();

    areafile_calculate_counts(app->areas, app->cfg->sysop);
    ui_arealist_rebuild_order(app);
    ui_status(app, "Rescan complete: %d areas", app->areas->count);
}

/* Main loop */
UiView ui_arealist_run(UiApp *app)
{
    int ch;
    int rows;
    int i;
    wint_t wch;
    int wrc;

    if (!app)
        return VIEW_QUIT;

    for (;;)
    {
        ArealistLayout layout;

        erase();
        ui_draw_menubar(app, "Area List");

        rows = LINES - 3; /* leave menubar(0) + status(LINES-1) + header(1) */

        if (rows < 1)
            rows = 1;

        /* Compute column layout for current screen width */
        compute_layout_cached(app, COLS, &layout);

        /* Header bar (column titles) - follows AREALISTFORMAT layout exactly */
        attron(COLOR_PAIR(COL_HEADER));
        move(1, 0);

        for (i = 0; i < COLS; i++)
            addch(' ');

        draw_header(1, COLS, &layout);
        attroff(COLOR_PAIR(COL_HEADER));

        /* Adjust scroll */
        if (app->area_sel < app->area_top)
            app->area_top = app->area_sel;

        if (app->area_sel >= app->area_top + rows)
            app->area_top = app->area_sel - rows + 1;

        if (app->area_top < 0)
            app->area_top = 0;

        if (app->area_order_count == 0)
        {
            attron(COLOR_PAIR(COL_NORMAL));

            mvaddnstr(LINES / 2, (COLS - 30) / 2, "(no areas match the filter)", 30);

            attroff(COLOR_PAIR(COL_NORMAL));
        }
        else
        {
            for (i = 0; i < rows && app->area_top + i < app->area_order_count; i++)
            {
                int idx = app->area_order[app->area_top + i];
                AreaEntry *a = &app->areas->entries[idx];
                int sel = (app->area_top + i == app->area_sel);

                if (sel)
                    attron(COLOR_PAIR(COL_SELECTED));
                else
                    attron(COLOR_PAIR(COL_NORMAL));

                draw_row(2 + i, 0, COLS, app, a, idx + 1, sel, &layout);

                if (sel)
                    attroff(COLOR_PAIR(COL_SELECTED));
                else
                    attroff(COLOR_PAIR(COL_NORMAL));
            }
        }

        if (app->area_search[0])
        {
            char *sbuf = wcs_to_utf8(app->area_search, (int)wcslen(app->area_search));

            ui_status(app, "Filter: \"%s\"  |  %d areas  | /=search G=goto s=sort", sbuf ? sbuf : "", app->area_order_count);
            free(sbuf);
        }
        else
            ui_status(app, "%s | %d areas | /=search G=goto s=sort", WRAPPER_PID, app->area_order_count);

        ui_draw_statusbar(app);
        refresh();

        /* wrapper_read_key() folds ESC+letter -> KEY_ALT() on both Linux and Amiga */
        wint_t wch;
        int wrc = wrapper_read_key(&wch);

        if (wrc == ERR)
            continue;

        ch = (int)wch;

        switch (ch)
        {
        case KEY_UP:
        case 'k':
            if (app->area_sel > 0)
                app->area_sel--;

            break;
        case KEY_DOWN:
        case 'j':
            if (app->area_sel < app->area_order_count - 1)
                app->area_sel++;

            break;
        case KEY_PPAGE:
        case CTRL('U'): /* Page Up (Amiga) */
            app->area_sel -= rows;

            if (app->area_sel < 0)
                app->area_sel = 0;

            break;
        case KEY_NPAGE:
        case CTRL('D'): /* Page Down (Amiga) */
            app->area_sel += rows;

            if (app->area_sel >= app->area_order_count)
                app->area_sel = app->area_order_count - 1;

            break;
        case KEY_HOME:
        case CTRL('B'): /* Beginning/Home (Amiga) */
            app->area_sel = 0;
            break;
        case KEY_END:
        case CTRL('E'): /* End (Amiga) */
            app->area_sel = app->area_order_count - 1;
            break;
        case '\n':
        case '\r':
        case KEY_ENTER:
        case KEY_RIGHT:
            if (app->area_order_count > 0 && app->area_sel >= 0 && app->area_sel < app->area_order_count)
            {
                int idx = app->area_order[app->area_sel];

                if (ui_session_open(app, idx) == 0)
                    return VIEW_MSGLIST;
            }

            break;
        case '/':
        {
            wchar_t tmp[64];
            wcsncpy(tmp, app->area_search, 63);
            tmp[63] = L'\0';

            if (ui_popup_input("Search areas", "Substring (empty to clear):", tmp, 64) == 0)
            {
                wcsncpy(app->area_search, tmp, 63);

                app->area_search[63] = L'\0';

                ui_arealist_rebuild_order(app);
            }

            break;
        }
        case 'G':
        case 'g':
        {
            char maybe[64];
            int mlen = 0;
            int *saved_order = NULL;
            int saved_count = app->area_order_count;

            /* Snapshot the current display order so Esc can restore it */
            if (saved_count > 0)
            {
                saved_order = (int *)malloc((size_t)saved_count * sizeof(int));

                if (saved_order)
                    memcpy(saved_order, app->area_order, (size_t)saved_count * sizeof(int));
            }

            maybe[0] = '\0';

            for (;;)
            {
                wint_t k;
                int krc;
                int j;
                int vis_rows = LINES - 3;
                ArealistLayout ly;
                char hdr[128];

                if (vis_rows < 1)
                    vis_rows = 1;

                erase();
                ui_draw_menubar(app, "Area List");

                /* Type-ahead bar across the header row */
                attron(COLOR_PAIR(COL_HEADER));
                move(1, 0);

                for (j = 0; j < COLS; j++)
                    addch(' ');

                snprintf(hdr, sizeof(hdr), ">>Pick area: %s_", maybe);
                mvaddnstr(1, 1, hdr, COLS - 2);

                attroff(COLOR_PAIR(COL_HEADER));

                compute_layout_cached(app, COLS, &ly);

                if (app->area_sel < app->area_top)
                    app->area_top = app->area_sel;

                if (app->area_sel >= app->area_top + vis_rows)
                    app->area_top = app->area_sel - vis_rows + 1;

                if (app->area_top < 0)
                    app->area_top = 0;

                for (j = 0; j < vis_rows && app->area_top + j < app->area_order_count; j++)
                {
                    int aidx = app->area_order[app->area_top + j];
                    AreaEntry *a = &app->areas->entries[aidx];
                    int sel = (app->area_top + j == app->area_sel);

                    attron(COLOR_PAIR(sel ? COL_SELECTED : COL_NORMAL));

                    draw_row(2 + j, 0, COLS, app, a, aidx + 1, sel, &ly);

                    attroff(COLOR_PAIR(sel ? COL_SELECTED : COL_NORMAL));
                }

                ui_draw_statusbar(app);
                refresh();

                krc = wrapper_read_key(&k);

                if (krc == ERR)
                    continue;

                /* Enter: open the currently selected area */
                if ((int)k == '\n' || (int)k == '\r' || (int)k == KEY_ENTER)
                {
                    if (app->area_order_count > 0 && app->area_sel >= 0 && app->area_sel < app->area_order_count)
                    {
                        int idx = app->area_order[app->area_sel];

                        free(saved_order);

                        if (ui_session_open(app, idx) == 0)
                            return VIEW_MSGLIST;
                    }
                    else
                    {
                        free(saved_order);
                    }

                    break;
                }

                /* Esc: restore original order */
                if ((int)k == 27)
                {
                    if (saved_order && saved_count > 0)
                    {
                        free(app->area_order);
                        app->area_order = saved_order;
                        app->area_order_count = saved_count;
                        saved_order = NULL;

                        if (app->area_sel >= app->area_order_count)
                            app->area_sel = app->area_order_count - 1;

                        if (app->area_sel < 0)
                            app->area_sel = 0;
                    }

                    free(saved_order);
                    break;
                }

                /* Backspace */
                if ((int)k == KEY_BACKSPACE || (int)k == 8 || (int)k == 127)
                {
                    if (mlen > 0)
                        maybe[--mlen] = '\0';
                }
                /* Up / Down: manual navigation */
                else if ((int)k == KEY_UP)
                {
                    if (app->area_sel > 0)
                        app->area_sel--;

                    continue;
                }
                else if ((int)k == KEY_DOWN)
                {
                    if (app->area_sel < app->area_order_count - 1)
                        app->area_sel++;

                    continue;
                }
                /* Printable ASCII char: append to type-ahead buffer */
                else if (krc != KEY_CODE_YES && k >= 0x20 && k < 0x7F && mlen < 62)
                {
                    maybe[mlen++] = (char)k;
                    maybe[mlen] = '\0';
                }
                else
                {
                    continue; /* ignore everything else */
                }

                /* Re-partition area_order: matching areas first (in their
                 * original relative order), then non-matching. Use the
                 * saved snapshot as the source of truth so each keystroke
                 * starts from the same base order */
                if (saved_order && saved_count > 0)
                {
                    int t;
                    int n = 0;

                    if (mlen == 0)
                    {
                        /* Empty needle: restore original order verbatim */
                        memcpy(app->area_order, saved_order, (size_t)saved_count * sizeof(int));

                        app->area_sel = 0;
                        app->area_top = 0;
                    }
                    else
                    {
                        /* Matches */
                        for (t = 0; t < saved_count; t++)
                        {
                            int aidx = saved_order[t];
                            const char *nm = app->areas->entries[aidx].name;
                            int matches = 0;
                            int q;

                            if (nm)
                            {
                                const char *h;

                                for (h = nm; *h && !matches; h++)
                                {
                                    int ok = 1;

                                    for (q = 0; q < mlen; q++)
                                    {
                                        if (!h[q] || tolower((unsigned char)h[q]) != tolower((unsigned char)maybe[q]))
                                        {
                                            ok = 0;
                                            break;
                                        }
                                    }

                                    if (ok)
                                        matches = 1;
                                }
                            }

                            if (matches)
                                app->area_order[n++] = aidx;
                        }

                        /* Non-matches */
                        for (t = 0; t < saved_count; t++)
                        {
                            int aidx = saved_order[t];
                            const char *nm = app->areas->entries[aidx].name;
                            int matches = 0;
                            int q;

                            if (nm)
                            {
                                const char *h;

                                for (h = nm; *h && !matches; h++)
                                {
                                    int ok = 1;

                                    for (q = 0; q < mlen; q++)
                                    {
                                        if (!h[q] || tolower((unsigned char)h[q]) != tolower((unsigned char)maybe[q]))
                                        {
                                            ok = 0;
                                            break;
                                        }
                                    }

                                    if (ok)
                                        matches = 1;
                                }
                            }

                            if (!matches)
                                app->area_order[n++] = aidx;
                        }

                        /* Cursor on the first match (top of list) */
                        app->area_sel = 0;
                        app->area_top = 0;
                    }
                }
            }

            break;
        }
        case 's':
        {
            char tmp[CFG_SORT_MAX];

            strncpy(tmp, app->cfg->arealistsort, sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';

            if (ui_popup_sort(tmp, sizeof(tmp), app->cfg->arealistsort_default) == 0)
            {
                size_t l = strlen(tmp);

                if (l >= sizeof(app->cfg->arealistsort))
                    l = sizeof(app->cfg->arealistsort) - 1;

                memcpy(app->cfg->arealistsort, tmp, l);
                app->cfg->arealistsort[l] = '\0';

                ui_arealist_rebuild_order(app);
            }

            break;
        }
        case KEY_F(1):
        case '?':
            ui_popup_help("Area List Help", AREALIST_HELP, AREALIST_HELP_N);
            break;
        case 'C':
        case KEY_ALT('C'):
            arealist_action_catchup(app);
            break;
        case 'R':
        case KEY_ALT('S'):
            arealist_action_rescan(app);
            break;
        case CTRL('F'):
            ui_popup_freq(app);
            break;
        case 'P':
        {
            /* Search across all areas */
            UiView next = ui_search_run(app, 1);

            if (next != VIEW_AREALIST)
                return next;
        }
        break;
        case 'S':
            if (ui_setup_run(app) == 1)
            {
                flushinp();
                return VIEW_QUIT;
            }
            break;
        case 27:
        case 'q':
        case 'Q':
            if (ui_popup_confirm("Quit", "Exit CrashEdit?") == 1)
                return VIEW_QUIT;
            break;
        case KEY_RESIZE:
            flushinp();
            break;
        default:
            break;
        }
    }
}
