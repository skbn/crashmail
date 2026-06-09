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

/* ui_msglist.c -- Message list view (inside an area) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ui_internal.h"
#include "../core/ftn.h"
#include "ui_search.h"
#include "../core/keys.h"

static const char *MSGLIST_HELP[] =
    {
        "Message List - Key Bindings:",
        "",
        "  Up/Down,j/k    Move selection",
#ifdef PLATFORM_AMIGA
        "  Ctrl+U/D       Page up/down",
        "  Ctrl+B/E       First / last message",
#else
        "  PgUp/PgDn      Page up/down",
        "  Home/End       First / last message",
#endif
        "  < , / > .      First / last message",
        "  Space/Enter    Read message",
        "  n              NEW message",
        "  r              REPLY to selected message",
        "  e              EDIT selected message",
        "  d              DELETE selected message (confirm)",
        "  g              Goto message number",
        "  /              Search by From/To/Subject",
        "  Ctrl+F         File request",
        "  S              Setup",
        "  P              Search",
        "  ESC, q         Back to area list",
        "  F1, ?          This help"};
#define MSGLIST_HELP_N ((int)(sizeof(MSGLIST_HELP) / sizeof(MSGLIST_HELP[0])))

/* Editor preparation helpers (forward declarations) */
void ui_editor_prep_new(UiApp *app);
void ui_editor_prep_reply(UiApp *app, uint32_t orig_msgnum);
void ui_editor_prep_edit(UiApp *app, uint32_t msgnum);

/* Render one message line */
static void format_date_short(uint32_t epoch, int reader_offset, int tzutc_offset, char *buf, int bufsz)
{
    time_t t = (time_t)epoch;

    /* crashmail stores header as UTC, but it's sender's local time
     * TZUTC: -0400 = local is 4h behind UTC
     * Example: header "11:20" with TZUTC -0400 -> UTC = 11:20 + 240min = 15:20 */
    if (tzutc_offset != -1)
        t += (time_t)tzutc_offset * 60; /* Convert to real UTC */

    /* Convert UTC to reader's local time */
    if (reader_offset != 0)
        t -= (time_t)reader_offset * 60;

    struct tm *tm = gmtime(&t);

    if (tm)
        snprintf(buf, (size_t)bufsz, "%02d/%02d/%02d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year % 100);
    else
        snprintf(buf, (size_t)bufsz, "        ");
}

/* Copy at most <width> bytes of <src> into <dst>, padding with spaces (counts bytes, not display columns) */
static void pad_field(char *dst, int width, const char *src, int maxlen)
{
    int i, n;

    if (width <= 0)
        return;

    n = src ? (int)strlen(src) : 0;

    if (n > maxlen)
        n = maxlen;

    if (n > width)
        n = width;

    for (i = 0; i < n; i++)
        dst[i] = src[i];

    for (; i < width; i++)
        dst[i] = ' ';
}

/* Render message list row (hot path: build entire row in stack buffer, single mvaddnstr, zero mallocs) */
static void draw_msg_row(int y, int width, const JamMsgInfo *m, uint32_t lastread, int is_sel, int reader_offset)
{
    char buf[512];
    char date[32];
    int unread = (m->msgnum > lastread);
    int maxcol = (width < 510) ? width : 510;
    int p = 0;
    int n;
    char num[16];

    format_date_short(m->date_written, reader_offset, m->tzutc_offset, date, sizeof(date));

    if (is_sel)
        attron(COLOR_PAIR(COL_SELECTED));
    else if (unread)
        attron(COLOR_PAIR(COL_UNREAD));
    else
        attron(COLOR_PAIR(COL_NORMAL));

    /* " %5lu  %-8s  %-20.20s  %-20.20s  %s" -- built by hand */
    if (p < maxcol)
        buf[p++] = ' ';

    n = snprintf(num, sizeof(num), "%5lu", (unsigned long)m->msgnum);
    if (n < 0)
        n = 0;

    if (n > maxcol - p)
        n = maxcol - p;

    memcpy(buf + p, num, (size_t)n);

    p += n;

    if (p < maxcol)
        buf[p++] = ' ';

    if (p < maxcol)
        buf[p++] = ' ';

    n = (int)strlen(date);

    if (n > 8)
        n = 8;

    if (n > maxcol - p)
        n = maxcol - p;

    memcpy(buf + p, date, (size_t)n);
    p += n;

    /* date column is fixed 8 wide */
    while (n < 8 && p < maxcol)
    {
        buf[p++] = ' ';
        n++;
    }

    if (p < maxcol)
        buf[p++] = ' ';

    if (p < maxcol)
        buf[p++] = ' ';

    /* From: 20 cols */
    if (p < maxcol)
    {
        int avail = maxcol - p;
        int w = avail < 20 ? avail : 20;

        pad_field(buf + p, w, m->from, JAM_FIELD_MAX);
        p += w;
    }

    if (p < maxcol)
        buf[p++] = ' ';

    if (p < maxcol)
        buf[p++] = ' ';

    /* To: 20 cols */
    if (p < maxcol)
    {
        int avail = maxcol - p;
        int w = avail < 20 ? avail : 20;

        pad_field(buf + p, w, m->to, JAM_FIELD_MAX);
        p += w;
    }

    if (p < maxcol)
        buf[p++] = ' ';

    if (p < maxcol)
        buf[p++] = ' ';

    /* Subject: rest of the row */
    if (p < maxcol)
    {
        int avail = maxcol - p;

        pad_field(buf + p, avail, m->subject, JAM_FIELD_MAX);
        p += avail;
    }

    /* Trailing fill (in case trimmed early) */
    while (p < maxcol)
        buf[p++] = ' ';

    mvaddnstr(y, 0, buf, maxcol);

    if (is_sel)
        attroff(COLOR_PAIR(COL_SELECTED));
    else if (unread)
        attroff(COLOR_PAIR(COL_UNREAD));
    else
        attroff(COLOR_PAIR(COL_NORMAL));
}

/* Mark area seen on entry; update lastseen to max msgnum, clear "*" indicator */
static void msglist_mark_seen(UiApp *app)
{
    UiSession *s;
    AreaEntry *ae;
    uint32_t hi = 0;
    int i;

    if (!app)
        return;

    s = &app->sess;

    if (s->area_idx < 0 || s->area_idx >= app->areas->count)
        return;

    /* Find max msgnum from loaded headers; no JAM access needed */
    for (i = 0; i < s->msg_count; i++)
        if (s->msgs[i].msgnum > hi)
            hi = s->msgs[i].msgnum;

    if (hi <= s->lastseen)
        return; /* nothing new to acknowledge */

    s->lastseen = hi;

    /* Persist lastseen, keep lastread untouched */
    if (jam_lock(&s->jam, JAM_LOCK_RETRIES) == 0)
    {
        jam_write_lastread(&s->jam, s->user_crc, s->lastread, s->lastseen);
        jam_unlock(&s->jam);
    }

    ae = &app->areas->entries[s->area_idx];
    ae->lastseen = s->lastseen;

    /* Recompute new_count; with lastseen=hi, this clears "*" indicator */
    ae->new_count = 0;
}

/* Main loop */
UiView ui_msglist_run(UiApp *app)
{
    UiSession *s;
    AreaEntry *ae;
    int rows;
    int ch;
    int i;

    if (!app)
        return VIEW_QUIT;

    s = &app->sess;

    if (s->area_idx < 0)
        return VIEW_AREALIST;

    ae = &app->areas->entries[s->area_idx];

    /* Mark list as seen on entry; fast-out if already up to date */
    msglist_mark_seen(app);

    for (;;)
    {
        char title[80];
        snprintf(title, sizeof(title), "%s", ae->description ? ae->description : (ae->name ? ae->name : ""));
        erase();
        ui_draw_menubar(app, title);

        rows = LINES - 3;

        if (rows < 1)
            rows = 1;

        /* Header bar: pre-fill single buffer instead of COLS addch
         * calls (slow under Amiga curses) */
        if (app->cfg->msglistheader)
        {
            char hdr[512];
            int maxcol = (COLS < 510) ? COLS : 510;
            const char *titles =
                "   Num   Date      From                  "
                "To                    Subject";

            int tl = (int)strlen(titles);

            for (i = 0; i < maxcol; i++)
                hdr[i] = ' ';

            if (tl > maxcol)
                tl = maxcol;

            memcpy(hdr, titles, (size_t)tl);
            attron(COLOR_PAIR(COL_HEADER));
            mvaddnstr(1, 0, hdr, maxcol);
            attroff(COLOR_PAIR(COL_HEADER));
        }
        else
        {
            move(1, 0);
            clrtoeol();
        }

        /* Adjust scroll */
        if (s->msg_sel < s->msg_top)
            s->msg_top = s->msg_sel;

        if (s->msg_sel >= s->msg_top + rows)
            s->msg_top = s->msg_sel - rows + 1;

        if (s->msg_top < 1)
            s->msg_top = 1;

        if (s->order_count == 0)
        {
            attron(COLOR_PAIR(COL_NORMAL));
            mvaddnstr(LINES / 2 - 1, (COLS - 36) / 2, "(no messages in this area)", 36);
            mvaddnstr(LINES / 2 + 1, (COLS - 36) / 2, "Press 'n' to create the first message.", 38);
            attroff(COLOR_PAIR(COL_NORMAL));
        }
        else
        {
            for (i = 0; i < rows && s->msg_top + i - 1 < s->order_count; i++)
            {
                int real = s->order[s->msg_top + i - 1];
                const JamMsgInfo *m = &s->msgs[real];
                int sel = (s->msg_top + i == s->msg_sel);

                draw_msg_row(2 + i, COLS, m, s->lastread, sel, ftn_effective_tz_offset(app->cfg->timezone_offset, app->cfg->timezone_is_manual));
            }
        }

        ui_status(app, "Area: %s | n=new r=reply e=edit d=del", ae->name ? ae->name : "?", s->order_count, s->msg_sel);
        ui_draw_statusbar(app);
        refresh();

        ch = wrapper_getch();

        switch (ch)
        {
        case KEY_UP:
        case 'k':
            if (s->msg_sel > 1)
                s->msg_sel--;

            break;
        case KEY_DOWN:
        case 'j':
            if (s->msg_sel < s->order_count)
                s->msg_sel++;

            break;
        case KEY_PPAGE:
        case CTRL('U'): /* Ctrl+U: Page Up (Amiga compatibility) */
            s->msg_sel -= rows;
            if (s->msg_sel < 1)
                s->msg_sel = 1;

            break;
        case KEY_NPAGE:
        case CTRL('D'): /* Ctrl+D: Page Down (Amiga compatibility) */
            s->msg_sel += rows;
            if (s->msg_sel > s->order_count)
                s->msg_sel = s->order_count;

            break;
        case KEY_HOME:
        case CTRL('B'): /* Ctrl+B: Beginning/Home (Amiga compatibility) */
            s->msg_sel = 1;
            break;
        case KEY_END:
        case CTRL('E'): /* Ctrl+E: End (Amiga compatibility) */
            s->msg_sel = s->order_count;
            break;
        case '<':
        case ',':
            s->msg_sel = 1;
            break;
        case '>':
        case '.':
            if (s->order_count > 0)
                s->msg_sel = s->order_count;
            break;
        case ' ':
        case '\n':
        case '\r':
        case KEY_ENTER:
        case KEY_RIGHT:
            if (s->order_count > 0 && s->msg_sel >= 1 && s->msg_sel <= s->order_count)
            {
                int real = s->order[s->msg_sel - 1];
                app->cur_msgnum = s->msgs[real].msgnum;
                app->msglist_overlay_from_reader = 0;

                return VIEW_READER;
            }
            break;
        case 'n':
        case 'N':
            app->msglist_overlay_from_reader = 0;
            app->edit_return_view = VIEW_MSGLIST;
            ui_editor_prep_new(app);

            return VIEW_EDITOR;
        case 'r':
        case 'R':
            if (s->order_count > 0 && s->msg_sel >= 1 && s->msg_sel <= s->order_count)
            {
                int real = s->order[s->msg_sel - 1];
                app->msglist_overlay_from_reader = 0;
                app->edit_return_view = VIEW_MSGLIST;

                ui_editor_prep_reply(app, s->msgs[real].msgnum);

                return VIEW_EDITOR;
            }
            break;
        case 'e':
        case 'E':
            if (s->order_count > 0 && s->msg_sel >= 1 && s->msg_sel <= s->order_count)
            {
                int real = s->order[s->msg_sel - 1];
                app->msglist_overlay_from_reader = 0;
                app->edit_return_view = VIEW_MSGLIST;

                ui_editor_prep_edit(app, s->msgs[real].msgnum);

                return VIEW_EDITOR;
            }
            break;
        case 'g':
        case 'G':
        {
            wchar_t wbuf[16];
            wbuf[0] = L'\0';

            if (ui_popup_input("Goto", "Message number:", wbuf, 16) == 0 && wbuf[0])
            {
                char *u = wcs_to_utf8(wbuf, (int)wcslen(wbuf));
                long mn = 0;

                if (u)
                {
                    mn = strtol(u, NULL, 10);
                    free(u);
                }

                if (mn > 0)
                {
                    int i;
                    int found = -1;

                    for (i = 0; i < s->msg_count; i++)
                    {
                        if ((long)s->msgs[i].msgnum == mn)
                        {
                            found = i;
                            break;
                        }
                    }

                    if (found >= 0)
                    {
                        for (i = 0; i < s->order_count; i++)
                        {
                            if (s->order[i] == found)
                            {
                                s->msg_sel = i + 1;
                                ui_status(app, "Moved to message %ld", mn);
                                break;
                            }
                        }
                    }
                    else
                        ui_status(app, "Message %ld not in list", mn);
                }
            }

            break;
        }
        case 'd':
        case 'D':
        case KEY_DC:
            if (s->order_count > 0 && s->msg_sel >= 1 && s->msg_sel <= s->order_count)
            {
                int real = s->order[s->msg_sel - 1];

                if (ui_popup_confirm("Delete message", "Mark this message as deleted?") == 1)
                {
                    if (jam_lock(&s->jam, JAM_LOCK_RETRIES) == 0)
                    {
                        if (jam_delete_msg(&s->jam, s->msgs[real].msgnum) == 0)
                            ui_status(app, "Message deleted");
                        else
                            ui_status(app, "Delete failed");

                        jam_unlock(&s->jam);

                        /* Reload */
                        free(s->msgs);

                        s->msgs = NULL;
                        s->msgs = jam_load_headers(&s->jam, &s->msg_count, 0, (uint32_t)app->cfg->msglistmax);

                        if (!s->msgs)
                        {
                            s->msg_count = 0;
                            ui_status(app, "Reload failed after delete");
                        }
                        else
                        {
                            ui_session_rebuild_order(app);
                        }
                    }
                    else
                    {
                        ui_status(app, "Cannot lock area");
                    }
                }
            }
            break;
        case 'S':
            ui_setup_run(app);
            flushinp();
            break;
        case CTRL('F'):
            ui_popup_freq(app);
            break;
        case '/':
        {
            wchar_t tmp[64];
            wcsncpy(tmp, s->search, 63);
            tmp[63] = L'\0';

            if (ui_popup_input("Search messages", "Substring in From/To/Subject:", tmp, 64) == 0)
            {
                wcsncpy(s->search, tmp, 63);
                s->search[63] = L'\0';

                ui_session_rebuild_order(app);
                s->msg_top = 1;
                s->msg_sel = 1;
            }
            break;
        }

        case 'P':
        {
            /* Full-text search; defaults to current area (scope=0)
             * user can select ALL areas */
            UiView next = ui_search_run(app, 0);

            if (next != VIEW_MSGLIST)
                return next;
        }
        break;

        case KEY_F(1):
        case '?':
            ui_popup_help("Message List Help", MSGLIST_HELP, MSGLIST_HELP_N);
            break;
        case 27:
        case 'q':
        case 'Q':
        case KEY_LEFT:
            if (app->msglist_overlay_from_reader)
            {
                app->msglist_overlay_from_reader = 0;
                return VIEW_READER;
            }

            /* Exit to area list without updating lastread; browsing ≠ reading */
            return VIEW_AREALIST;
        case KEY_RESIZE:
            flushinp();
            break;
        default:
            break;
        }
    }
}
