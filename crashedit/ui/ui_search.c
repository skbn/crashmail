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

/* ui_search.c -- popup + result browser for selective search */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_internal.h"
#include "ui_search.h"
#include "../core/search.h"
#include "../core/keys.h"

static const char *SEARCH_HELP[] =
    {
        "Search Results - Help",
        "",
        "Area List Mode:",
        "  UP/DOWN, j/k   Move cursor",
#ifndef PLATFORM_AMIGA
        "  PgUp/PgDn      Page up/down",
        "  Home/End       First / last area",
#endif
#ifdef PLATFORM_AMIGA
        "  Ctrl+U/D       Page up/down",
        "  Ctrl+B/E       First / last area",
#endif
        "  Enter          Open area (show hits)",
        "  ESC, q         Quit search",
        "",
        "Message List Mode:",
        "  UP/DOWN, j/k   Move cursor",
#ifndef PLATFORM_AMIGA
        "  PgUp/PgDn      Page up/down",
        "  Home/End       First / last message",
#endif
#ifdef PLATFORM_AMIGA
        "  Ctrl+U/D       Page up/down",
        "  Ctrl+B/E       First / last message",
#endif
        "  Enter          Read message",
        "  Backspace      Back to area list",
        "  ESC, q         Quit search",
        "",
        "Both Modes:",
        "  F1, ?          This help screen",
        ""};
#define SEARCH_HELP_N ((int)(sizeof(SEARCH_HELP) / sizeof(SEARCH_HELP[0])))

/* Internal "summary": one entry per area that has hits, with its
 * count and the index of its first hit in ss->hits[]. Built once
 * after the scan. Hits are emitted in area order, so the runs are
 * contiguous */
typedef struct
{
    int area_idx;
    int first_hit; /* index into ss->hits[] */
    int hit_count;
} AreaRun;

/* Non-blocking ESC poll. Used during long scans so the user can
 * cancel without waiting for the message loop to come around */
static int poll_esc()
{
    int ch;
    timeout(0);
    ch = wrapper_getch();
    timeout(-1);
    return (ch == 27) ? 1 : 0;
}

/* Force a fresh paint of the background. The search popups are modal
 * and we paint over a black background -- no point trying to show the
 * underlying view */
static void wipe_background()
{
    erase();
}

/* Center popup and return geometry */
static void popup_geo(int want_h, int want_w, int *y, int *x, int *h, int *w)
{
    ui_popup_center(want_h, want_w, y, x, h, w);
}

/* Draw the search progress line. Updated rarely so it stays readable
 * even on slow terminals/serial */
static void draw_progress(int y, int x, int w, const SearchSession *s, AreaList *areas)
{
    char buf[160];
    const char *areatag = "?";
    int j;

    if (areas && s->scanned_areas < areas->count)
    {
        const AreaEntry *ae = &areas->entries[s->scanned_areas];

        if (ae->name)
            areatag = ae->name;
    }

    snprintf(buf, sizeof(buf), "Searching %-24s [%d/%d]   msgs:%lu  hits:%d  (ESC cancels)", areatag, s->scanned_areas + 1, s->total_areas, (unsigned long)s->scanned_msgs_current, s->n_hits);
    attron(COLOR_PAIR(COL_STATUS));

    /* Clear the status row and paint our line */
    for (j = 0; j < w; j++)
        mvaddch(y, x + j, ' ');

    mvaddnstr(y, x, buf, w - 1);
    attroff(COLOR_PAIR(COL_STATUS));
    refresh();
}

/* Ask for pattern + search options; return 0=run, -1=ESC */
static int search_params_popup(UiApp *app, char *pattern, int patternsz, int *opt_headers, int *opt_body, int *opt_all_areas, int *opt_max)
{
    int y, x, h, w;
    int want_w = 64, want_h = 13;
    int rc;
    int field = 0; /* 0=pattern, 1=hdr, 2=body, 3=scope, 4=maxhits */
    char inbuf[SEARCH_PATTERN_MAX];
    char maxbuf[16];
    int cx;

    inbuf[0] = '\0';

    if (pattern && pattern[0])
    {
        strncpy(inbuf, pattern, sizeof(inbuf) - 1);
        inbuf[sizeof(inbuf) - 1] = '\0';
    }

    snprintf(maxbuf, sizeof(maxbuf), "%d", *opt_max > 0 ? *opt_max : SEARCH_DEFAULT_MAX);
    maxbuf[sizeof(maxbuf) - 1] = '\0';

    popup_geo(want_h, want_w, &y, &x, &h, &w);

    for (;;)
    {
        int i, j;
        int row;
        wint_t wch;
        const char *t;
        int tl, tx;
        int fieldx, fieldw, k;
        int len;

        wipe_background();

        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < h; i++)
        {
            move(y + i, x);

            for (j = 0; j < w; j++)
                addch(' ');
        }

        ui_box(y, x, h, w);

        t = " Search ";
        tl = (int)strlen(t);
        tx = x + (w - tl) / 2;
        mvaddnstr(y, tx, t, tl);

        row = y + 2;

        /* Pattern row */
        mvaddnstr(row, x + 2, "Pattern:", 8);

        fieldx = x + 12;
        fieldw = w - 14;

        if (field == 0)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        for (k = 0; k < fieldw; k++)
            mvaddch(row, fieldx + k, ' ');

        mvaddnstr(row, fieldx, inbuf, fieldw);

        if (field == 0)
            attroff(COLOR_PAIR(COL_POPUP_SEL));

        attron(COLOR_PAIR(COL_POPUP));

        /* Header checkbox */
        row = y + 4;

        if (field == 1)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        mvprintw(row, x + 4, "[%c] Search headers (subject/from/to/msgid)", *opt_headers ? 'X' : ' ');

        if (field == 1)
            attroff(COLOR_PAIR(COL_POPUP_SEL));

        attron(COLOR_PAIR(COL_POPUP));

        /* Body checkbox */
        row = y + 5;

        if (field == 2)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        mvprintw(row, x + 4, "[%c] Search body  (slower, reads disk)", *opt_body ? 'X' : ' ');

        if (field == 2)
            attroff(COLOR_PAIR(COL_POPUP_SEL));

        attron(COLOR_PAIR(COL_POPUP));

        /* Scope */
        row = y + 6;

        if (field == 3)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        mvprintw(row, x + 4, "[%c] Search ALL areas  (else current only)", *opt_all_areas ? 'X' : ' ');

        if (field == 3)
            attroff(COLOR_PAIR(COL_POPUP_SEL));

        attron(COLOR_PAIR(COL_POPUP));

        /* Max hits to keep */
        row = y + 7;

        mvaddnstr(row, x + 4, "Max hits:", 9);

        fieldx = x + 14;
        fieldw = 8;

        if (field == 4)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        for (k = 0; k < fieldw; k++)
            mvaddch(row, fieldx + k, ' ');

        mvaddnstr(row, fieldx, maxbuf, fieldw);

        if (field == 4)
            attroff(COLOR_PAIR(COL_POPUP_SEL));

        attron(COLOR_PAIR(COL_POPUP));

        mvaddnstr(row, fieldx + fieldw + 1, "(0=default)", 14);

        /* Footer */
        mvaddnstr(y + h - 2, x + 2, "TAB next  SPACE toggle  ENTER run  ESC cancel", w - 4);

        attroff(COLOR_PAIR(COL_POPUP));

        /* Position cursor in the focused text field */
        if (field == 0)
        {
            cx = x + 12 + (int)strlen(inbuf);

            if (cx >= x + w - 2)
                cx = x + w - 3;

            move(y + 2, cx);
            curs_set(1);
        }
        else if (field == 4)
        {
            cx = x + 14 + (int)strlen(maxbuf);
            move(y + 7, cx);
            curs_set(1);
        }
        else
        {
            curs_set(0);
        }

        refresh();

        rc = wrapper_read_key(&wch);

        if (rc == ERR)
            continue;

        if (wch == 27)
        {
            curs_set(0);
            return -1;
        }

        if ((wch == '\n' || wch == '\r' || wch == KEY_ENTER))
        {
            if (!inbuf[0])
            {
                /* Require non-empty pattern */
                continue;
            }

            if (!*opt_headers && !*opt_body)
            {
                /* Default to headers if no fields selected */
                *opt_headers = 1;
            }

            /* Parse max-hits; <=0 uses default */
            *opt_max = atoi(maxbuf);

            if (*opt_max < 0)
                *opt_max = 0;

            strncpy(pattern, inbuf, (size_t)(patternsz - 1));
            pattern[patternsz - 1] = '\0';
            curs_set(0);

            return 0;
        }

        if (wch == '\t' || wch == KEY_DOWN)
        {
            field = (field + 1) % 5;
            continue;
        }

        if (wch == KEY_BTAB || wch == KEY_UP)
        {
            field = (field + 4) % 5;
            continue;
        }

        /* Editing actions per-field */
        switch (field)
        {
        case 0:
            /* Pattern editing */
            if (wch == KEY_BACKSPACE || wch == 8 || wch == 127)
            {
                len = (int)strlen(inbuf);

                if (len > 0)
                    inbuf[len - 1] = '\0';
            }
            else if (wch >= 0x20 && wch < 0x7f)
            {
                len = (int)strlen(inbuf);

                if (len + 1 < (int)sizeof(inbuf))
                {
                    inbuf[len] = (char)wch;
                    inbuf[len + 1] = '\0';
                }
            }
            break;

        case 4:
            /* Max-hits: digits only, max 7 chars */
            if (wch == KEY_BACKSPACE || wch == 8 || wch == 127)
            {
                len = (int)strlen(maxbuf);

                if (len > 0)
                    maxbuf[len - 1] = '\0';
            }
            else if (wch >= '0' && wch <= '9')
            {
                len = (int)strlen(maxbuf);

                if (len < 7)
                {
                    maxbuf[len] = (char)wch;
                    maxbuf[len + 1] = '\0';
                }
            }
            break;

        case 1:
        case 2:
        case 3:
            /* Toggles: SPACE or 'x'/'X' */
            if (wch == ' ' || wch == 'x' || wch == 'X')
            {
                switch (field)
                {
                case 1:
                    *opt_headers = !*opt_headers;
                    break;
                case 2:
                    *opt_body = !*opt_body;
                    break;
                case 3:
                    *opt_all_areas = !*opt_all_areas;
                    break;
                }
            }
            break;

        default:
            break;
        }
    }
}

static void run_scan(UiApp *app, SearchSession *ss, int scope_all_areas)
{
    int sty;
    int i;
    int idx;
    const char *banner = "Searching... ESC to cancel";

    /* Status row at bottom of screen. LINES/COLS are kept current by
     * ncurses on KEY_RESIZE, so we use them directly */
    sty = LINES - 1;

    /* Clear the screen, leave a small banner */
    wipe_background();

    attron(COLOR_PAIR(COL_POPUP));
    mvaddnstr(LINES / 2, (COLS - (int)strlen(banner)) / 2, banner, (int)strlen(banner));
    attroff(COLOR_PAIR(COL_POPUP));

    if (scope_all_areas)
    {
        ss->total_areas = app->areas->count;

        for (i = 0; i < app->areas->count; i++)
        {
            ss->scanned_areas = i;
            draw_progress(sty, 0, COLS, ss, app->areas);

            if (poll_esc())
                ss->cancel = 1;

            if (ss->cancel)
                break;

            search_scan_area(ss, &app->areas->entries[i], i);

            if (ss->hit_limit_reached)
                break;
        }
    }
    else
    {
        idx = app->area_sel;

        if (idx < 0 || idx >= app->areas->count)
            return;

        ss->total_areas = 1;
        ss->scanned_areas = 0;
        draw_progress(sty, 0, COLS, ss, app->areas);
        search_scan_area(ss, &app->areas->entries[idx], idx);
    }
}

static int build_area_runs(const SearchSession *ss, AreaRun *runs, int max)
{
    int i;
    int n = 0;
    int prev = -2;

    for (i = 0; i < ss->n_hits && n < max; i++)
    {
        int ai = ss->hits[i].area_idx;

        if (ai != prev)
        {
            runs[n].area_idx = ai;
            runs[n].first_hit = i;
            runs[n].hit_count = 1;
            n++;
            prev = ai;
        }
        else
        {
            runs[n - 1].hit_count++;
        }
    }

    return n;
}

/* Tiny ASCII spinner for hit flags */
static const char *flag_letters(uint16_t flags)
{
    if ((flags & SEARCH_HIT_HEADER) && (flags & SEARCH_HIT_BODY))
        return "HB";

    if (flags & SEARCH_HIT_HEADER)
        return "H ";

    if (flags & SEARCH_HIT_BODY)
        return "B ";

    return "  ";
}

/* Persistent result browser: reads search state, preserves cursor/scroll
 * position for reader round-trip, returns VIEW_READER on message open or
 * app->search_from_view on ESC */
UiView ui_search_results_run(UiApp *app)
{
    SearchSession *ss;
    AreaRun *runs;
    int n_runs;
    int mode;
    int area_pick, area_top;
    int hit_pick, hit_top;
    UiView next_view = VIEW_AREALIST;
    int visible;
    int i;
    wint_t wch;
    int rc;
    int idx;
    int row;
    int first;
    int cnt;
    const AreaEntry *ae;
    const SearchHit *h;
    char hdr[160];
    char line[200];
    const char *foot;
    int j;

    if (!app || !app->search || !app->search_runs)
    {
        /* Defensive: if state vanished, bail to arealist. */
        ui_search_cleanup(app);
        return VIEW_AREALIST;
    }

    /* Clear from_search flag after reader excursion to avoid
     * propagating to next reader entry */
    app->from_search = 0;

    ss = (SearchSession *)app->search;
    runs = (AreaRun *)app->search_runs;
    n_runs = app->search_n_runs;

    /* Load picks into locals for hot loop optimization; write back on transitions */
    mode = app->search_mode;
    area_pick = app->search_area_pick;
    area_top = app->search_area_top;
    hit_pick = app->search_hit_pick;
    hit_top = app->search_hit_top;

    if (n_runs == 0)
    {
        ui_search_cleanup(app);
        return app->search_from_view;
    }

    for (;;)
    {
        wipe_background();
        visible = LINES - 4;

        if (visible < 4)
            visible = 4;

        /* Header */
        attron(COLOR_PAIR(COL_HEADER));

        for (i = 0; i < COLS; i++)
            mvaddch(0, i, ' ');

        if (mode == 0)
        {
            snprintf(hdr, sizeof(hdr), " Search results -- %d hits in %d areas%s   pattern: \"%s\"", ss->n_hits, n_runs, ss->hit_limit_reached ? " (LIMIT REACHED)" : "", ss->pattern);
            mvaddnstr(0, 0, hdr, COLS - 1);
        }
        else
        {
            ae = &app->areas->entries[runs[area_pick].area_idx];
            snprintf(hdr, sizeof(hdr), " %s -- %d hits   (BACKSPACE = areas, ENTER = read)", ae->name ? ae->name : "?", runs[area_pick].hit_count);
            mvaddnstr(0, 0, hdr, COLS - 1);
        }

        attroff(COLOR_PAIR(COL_HEADER));

        if (mode == 0)
        {
            /* Areas summary */
            row = 2;

            if (area_pick < area_top)
                area_top = area_pick;

            if (area_pick >= area_top + visible)
                area_top = area_pick - visible + 1;

            for (i = 0; i < visible && area_top + i < n_runs; i++)
            {
                idx = area_top + i;
                ae = &app->areas->entries[runs[idx].area_idx];

                snprintf(line, sizeof(line), "  %5d  %-32s  %s", runs[idx].hit_count, ae->name ? ae->name : "?", ae->description ? ae->description : "");

                if (idx == area_pick)
                    attron(COLOR_PAIR(COL_SELECTED));

                for (j = 0; j < COLS; j++)
                    mvaddch(row + i, j, ' ');

                mvaddnstr(row + i, 0, line, COLS - 1);

                if (idx == area_pick)
                    attroff(COLOR_PAIR(COL_SELECTED));
            }
        }
        else
        {
            /* Hit list for the picked area */
            row = 2;
            first = runs[area_pick].first_hit;
            cnt = runs[area_pick].hit_count;

            if (hit_pick < hit_top)
                hit_top = hit_pick;

            if (hit_pick >= hit_top + visible)
                hit_top = hit_pick - visible + 1;

            for (i = 0; i < visible && hit_top + i < cnt; i++)
            {
                idx = hit_top + i;
                h = &ss->hits[first + idx];

                snprintf(line, sizeof(line), " [%s] %6lu  %-20.20s  %s", flag_letters(h->flags), (unsigned long)h->msgnum, h->from, h->subject);

                if (idx == hit_pick)
                    attron(COLOR_PAIR(COL_SELECTED));

                for (j = 0; j < COLS; j++)
                    mvaddch(row + i, j, ' ');

                mvaddnstr(row + i, 0, line, COLS - 1);

                if (idx == hit_pick)
                    attroff(COLOR_PAIR(COL_SELECTED));
            }
        }

        /* Footer */
        foot = mode == 0 ? " UP/DOWN  ENTER open area  ESC quit " : " UP/DOWN  ENTER read  BACKSPACE back  ESC quit ";

        attron(COLOR_PAIR(COL_STATUS));

        for (j = 0; j < COLS; j++)
            mvaddch(LINES - 1, j, ' ');

        mvaddnstr(LINES - 1, 0, foot, COLS - 1);
        attroff(COLOR_PAIR(COL_STATUS));

        refresh();

        rc = wrapper_read_key(&wch);

        if (rc == ERR)
            continue;

        if (wch == 27)
        {
            next_view = VIEW_AREALIST;
            break;
        }

        if (mode == 0)
        {
            switch (wch)
            {
            case KEY_UP:
            case 'k':
                if (area_pick > 0)
                    area_pick--;
                break;

            case KEY_DOWN:
            case 'j':
                if (area_pick < n_runs - 1)
                    area_pick++;
                break;

            case KEY_HOME:
            case CTRL('B'):
                area_pick = 0;
                break;

            case KEY_END:
            case CTRL('E'):
                area_pick = n_runs - 1;
                break;

            case KEY_NPAGE:
            case CTRL('D'):
                area_pick += visible;

                if (area_pick >= n_runs)
                    area_pick = n_runs - 1;
                break;

            case KEY_PPAGE:
            case CTRL('U'):
                area_pick -= visible;

                if (area_pick < 0)
                    area_pick = 0;
                break;

            case '\n':
            case '\r':
            case KEY_ENTER:
                mode = 1;
                hit_pick = 0;
                hit_top = 0;
                break;

            case KEY_F(1):
            case '?':
                ui_popup_help("Search Results Help", SEARCH_HELP, SEARCH_HELP_N);
                break;

            default:
                break;
            }
        }
        else
        {
            int cnt = runs[area_pick].hit_count;

            switch (wch)
            {
            case KEY_UP:
            case 'k':
                if (hit_pick > 0)
                    hit_pick--;
                break;

            case KEY_DOWN:
            case 'j':
                if (hit_pick < cnt - 1)
                    hit_pick++;
                break;

            case KEY_HOME:
            case CTRL('B'):
                hit_pick = 0;
                break;

            case KEY_END:
            case CTRL('E'):
                hit_pick = cnt - 1;
                break;

            case KEY_NPAGE:
            case CTRL('D'):
                hit_pick += visible;

                if (hit_pick >= cnt)
                    hit_pick = cnt - 1;
                break;

            case KEY_PPAGE:
            case CTRL('U'):
                hit_pick -= visible;

                if (hit_pick < 0)
                    hit_pick = 0;
                break;

            case KEY_BACKSPACE:
            case 8:
            case 127:
                mode = 0;
                break;

            case '\n':
            case '\r':
            case KEY_ENTER:
                /* Save cursor state before opening reader to resume on
                 * same hit when reader returns */
                h = &ss->hits[runs[area_pick].first_hit + hit_pick];

                if (ui_session_open(app, h->area_idx) == 0)
                {
                    app->cur_msgnum = h->msgnum;
                    app->from_search = 1; /* tells reader/editor to NOT touch lastread and to return here */
                    app->search_mode = mode;
                    app->search_area_pick = area_pick;
                    app->search_area_top = area_top;
                    app->search_hit_pick = hit_pick;
                    app->search_hit_top = hit_top;
                    next_view = VIEW_READER;

                    /* DO NOT free runs/search — we want them when
                     * we come back. Just return */
                    return next_view;
                }

                ui_status(app, "Cannot open area for msg %lu", (unsigned long)h->msgnum);
                break;

            case KEY_F(1):
            case '?':
                ui_popup_help("Search Results Help", SEARCH_HELP, SEARCH_HELP_N);
                break;
            default:
                break;
            }
        }
    }

    /* ESC path: persist nothing, free the session entirely */
    next_view = app->search_from_view;
    ui_search_cleanup(app);

    return next_view;
}

/* Tear down search state; safe to call when empty, used on ESC and shutdown */
void ui_search_cleanup(UiApp *app)
{
    if (!app)
        return;

    if (app->search_runs)
    {
        free(app->search_runs);
        app->search_runs = NULL;
    }

    if (app->search)
    {
        search_free((SearchSession *)app->search);
        app->search = NULL;
    }

    app->search_n_runs = 0;
    app->search_mode = 0;
    app->search_area_pick = 0;
    app->search_area_top = 0;
    app->search_hit_pick = 0;
    app->search_hit_top = 0;
    app->from_search = 0;
}

UiView ui_search_run(UiApp *app, int scope_all_areas)
{
    char pattern[SEARCH_PATTERN_MAX] = {0};
    int opt_headers = 1;
    int opt_body = 0;
    int opt_all_areas = scope_all_areas ? 1 : 0;
    int opt_max;
    SearchSession *ss;
    AreaRun *runs;
    int n_runs;
    UiView caller_view;

    if (!app || !app->areas || app->areas->count == 0)
        return VIEW_AREALIST;

    /* Seed the editable max-hits from config (SEARCHMAX) */
    opt_max = (app->cfg && app->cfg->search_max > 0) ? app->cfg->search_max : SEARCH_DEFAULT_MAX;

    /* Remember caller view for ESC return path */
    caller_view = app->view;

    /* Clean up any previous search before starting fresh */
    ui_search_cleanup(app);

    /* Parameters popup */
    if (search_params_popup(app, pattern, sizeof(pattern), &opt_headers, &opt_body, &opt_all_areas, &opt_max) != 0)
        return caller_view;

    /* Persist limit change to config for session stickiness */
    if (app->cfg)
        app->cfg->search_max = opt_max;

    /* Build session, run the scan with progress + cancel polling */
    ss = search_new(pattern, opt_headers, opt_body, 0, opt_max);

    if (!ss)
    {
        ui_status(app, "Search: out of memory or invalid pattern");
        return caller_view;
    }

    run_scan(app, ss, opt_all_areas);

    if (ss->n_hits == 0)
    {
        ui_status(app, "No matches for \"%s\"%s", pattern, ss->cancel ? " (cancelled)" : "");
        search_free(ss);
        return caller_view;
    }

    /* Pre-build per-area runs and stash in app for persistent browser;
     * size to n_hits (worst case: one run per hit) */
    runs = (AreaRun *)malloc((size_t)ss->n_hits * sizeof(AreaRun));

    if (!runs)
    {
        ui_status(app, "Search: out of memory building results");
        search_free(ss);
        return caller_view;
    }

    n_runs = build_area_runs(ss, runs, ss->n_hits);

    app->search = ss;
    app->search_runs = runs;
    app->search_n_runs = n_runs;
    app->search_mode = 0;
    app->search_area_pick = 0;
    app->search_area_top = 0;
    app->search_hit_pick = 0;
    app->search_hit_top = 0;
    app->search_from_view = caller_view;
    app->from_search = 0;

    return VIEW_SEARCH_RESULTS;
}
