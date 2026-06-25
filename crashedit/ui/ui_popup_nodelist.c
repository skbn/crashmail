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

/* ui_popup_nodelist.c -- Type-ahead picker for INCLUDEd nodelists and pointlists */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ui_internal.h"
#include "../core/nodelist.h"
#include "../core/keys.h"

/* Match current needle against entry, returns 1 on match, needle is plain ASCII (case-insensitive), empty needle = match all */
static int entry_matches(const NodelistEntry *e, const char *needle)
{
    return nodelist_entry_matches(e, needle);
}

int ui_popup_nodelist(UiApp *app, int allow_pick, char *out_name, int name_sz, char *out_addr, int addr_sz)
{
    Nodelist *nl = NULL;
    int y, x, h, w;
    int row_top, row_sel;
    int filtered_count;
    int *filtered = NULL; /* indices into nl->entries */
    int filtered_cap = 0;
    char needle[64];
    int nlen = 0;
    WINDOW *saved = NULL;
    int rc_picked = 0;
    int rebuild = 1;
    int want_h, want_w;

    if (!app)
        return 0;

    nl = &app->nodelist;

    if (nl->count == 0)
    {
        ui_status(app, "No nodelist loaded. Add INCLUDE <path> in config");
        return 0;
    }

    /* Initial filter buffer */
    needle[0] = '\0';
    row_top = 0;
    row_sel = 0;

    /* Window geometry: full-ish modal */
    want_h = LINES - 4;
    want_w = COLS - 8;

    if (want_h < 8)
        want_h = 8;

    if (want_w < 50)
        want_w = 50;

    ui_popup_center(want_h, want_w, &y, &x, &h, &w);

    saved = newwin(h, w, y, x);

    if (saved)
        copywin(stdscr, saved, y, x, 0, 0, h - 1, w - 1, 0);

    /* Allocate filtered index array up-front */
    filtered_cap = nl->count;
    filtered = (int *)malloc((size_t)filtered_cap * sizeof(int));

    if (!filtered)
    {
        if (saved)
            delwin(saved);

        return 0;
    }

    filtered_count = 0;

    curs_set(0);

    for (;;)
    {
        wint_t wch;
        int krc;
        int body_rows;
        int rr;
        int i;
        int name_w;
        int addr_w;
        const char *t;
        int tl;
        int tx;
        char prompt[128];
        char hdr[256];
        int cc;
        const char *hint;

        if (rebuild)
        {
            filtered_count = 0;

            for (i = 0; i < nl->count; i++)
            {
                if (entry_matches(&nl->entries[i], needle))
                    filtered[filtered_count++] = i;
            }

            if (row_sel >= filtered_count)
                row_sel = filtered_count - 1;

            if (row_sel < 0)
                row_sel = 0;

            if (row_top > row_sel)
                row_top = row_sel;

            rebuild = 0;
        }

        /* Layout: top border, title, search line, header, body, bottom hint */
        body_rows = h - 5;

        if (body_rows < 1)
            body_rows = 1;

        /* Scroll so selection is visible */
        if (row_sel < row_top)
            row_top = row_sel;

        if (row_sel >= row_top + body_rows)
            row_top = row_sel - body_rows + 1;

        if (row_top < 0)
            row_top = 0;

        standend();

        /* Clear the popup area */
        attron(COLOR_PAIR(COL_POPUP));

        for (rr = 0; rr < h; rr++)
        {
            int cc;

            move(y + rr, x);

            for (cc = 0; cc < w; cc++)
                addch(' ');
        }

        /* Borders */
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

        /* Title */

        t = allow_pick ? " Pick recipient (F10) " : " Browse nodelist (Ctrl-F10) ";
        tl = (int)strlen(t);
        tx = x + (w - tl) / 2;

        if (tx < x + 2)
            tx = x + 2;

        mvaddnstr(y, tx, t, w - 4);

        /* Search prompt */
        snprintf(prompt, sizeof(prompt), " Search: %s_  (%d/%d)  ", needle, filtered_count, nl->count);
        mvaddnstr(y + 1, x + 2, prompt, w - 4);

        /* Column header */
        name_w = (w - 4) / 2;

        if (name_w < 16)
            name_w = 16;

        addr_w = (w - 4) - name_w - 2;

        if (addr_w < 12)
            addr_w = 12;

        attroff(COLOR_PAIR(COL_POPUP));
        attron(COLOR_PAIR(COL_HEADER));

        snprintf(hdr, sizeof(hdr), " %-*s  %-*s", name_w, "Sysop name", addr_w, "FTN address");

        move(y + 2, x + 1);

        for (cc = 1; cc < w - 1; cc++)
            addch(' ');

        mvaddnstr(y + 2, x + 1, hdr, w - 2);

        attroff(COLOR_PAIR(COL_HEADER));
        attron(COLOR_PAIR(COL_POPUP));

        /* Body rows */
        for (rr = 0; rr < body_rows; rr++)
        {
            int idx = row_top + rr;
            int is_sel = (idx == row_sel);
            char buf[256];
            const NodelistEntry *e;
            int cc;

            if (idx >= filtered_count)
            {
                /* Blank row */
                int cc;

                move(y + 3 + rr, x + 1);

                for (cc = 1; cc < w - 1; cc++)
                    addch(' ');

                continue;
            }

            e = &nl->entries[filtered[idx]];
            snprintf(buf, sizeof(buf), " %-*.*s  %-*.*s",
                     name_w, name_w, e->name,
                     addr_w, addr_w, e->addr);

            if (is_sel)
            {
                attroff(COLOR_PAIR(COL_POPUP));
                attron(COLOR_PAIR(COL_POPUP_SEL));
            }

            move(y + 3 + rr, x + 1);

            for (cc = 1; cc < w - 1; cc++)
                addch(' ');

            mvaddnstr(y + 3 + rr, x + 1, buf, w - 2);

            if (is_sel)
            {
                attroff(COLOR_PAIR(COL_POPUP_SEL));
                attron(COLOR_PAIR(COL_POPUP));
            }
        }

        /* Bottom hint */

        hint = allow_pick ? "  Enter=pick   Esc=cancel   BS=erase  " : "  Esc=close   BS=erase  ";
        mvaddnstr(y + h - 1, x + 2, hint, w - 4);

        attroff(COLOR_PAIR(COL_POPUP));
        refresh();

        krc = wrapper_read_key(&wch);

        if (krc == ERR)
            continue;

        /* Exit keys */
        if ((int)wch == 27) /* Esc */
            break;

        if ((int)wch == '\n' || (int)wch == '\r' || (int)wch == KEY_ENTER)
        {
            if (allow_pick && filtered_count > 0 && row_sel >= 0 && row_sel < filtered_count)
            {
                const NodelistEntry *e = &nl->entries[filtered[row_sel]];

                if (out_name && name_sz > 0)
                {
                    strncpy(out_name, e->name, (size_t)(name_sz - 1));
                    out_name[name_sz - 1] = '\0';
                }

                if (out_addr && addr_sz > 0)
                {
                    strncpy(out_addr, e->addr, (size_t)(addr_sz - 1));
                    out_addr[addr_sz - 1] = '\0';
                }

                rc_picked = 1;
            }

            break;
        }

        /* Navigation */
        if (krc == KEY_CODE_YES)
        {
            switch ((int)wch)
            {
            case KEY_UP:
                if (row_sel > 0)
                    row_sel--;

                continue;
            case KEY_DOWN:
                if (row_sel < filtered_count - 1)
                    row_sel++;

                continue;
            case KEY_PPAGE:
                row_sel -= body_rows;

                if (row_sel < 0)
                    row_sel = 0;

                continue;
            case KEY_NPAGE:
                row_sel += body_rows;

                if (row_sel > filtered_count - 1)
                    row_sel = filtered_count - 1;

                if (row_sel < 0)
                    row_sel = 0;

                continue;
            case KEY_HOME:
                row_sel = 0;
                continue;
            case KEY_END:
                row_sel = filtered_count - 1;

                if (row_sel < 0)
                    row_sel = 0;

                continue;
            case KEY_BACKSPACE:
                if (nlen > 0)
                {
                    needle[--nlen] = '\0';
                    row_sel = 0;
                    row_top = 0;
                    rebuild = 1;
                }

                continue;
            }

            continue;
        }

        /* Ctrl key navigation shortcuts */
        if (krc != KEY_CODE_YES)
        {
            switch ((int)wch)
            {
            case CTRL('D'): /* PageDown */
                row_sel += body_rows;

                if (row_sel > filtered_count - 1)
                    row_sel = filtered_count - 1;

                if (row_sel < 0)
                    row_sel = 0;

                continue;
            case CTRL('U'): /* PageUp */
                row_sel -= body_rows;

                if (row_sel < 0)
                    row_sel = 0;

                continue;
            case CTRL('E'): /* End */
                row_sel = filtered_count - 1;

                if (row_sel < 0)
                    row_sel = 0;

                continue;
            case CTRL('B'): /* Home */
                row_sel = 0;
                continue;
            }
        }

        /* Plain keys */
        if ((int)wch == 8 || (int)wch == 127)
        {
            if (nlen > 0)
            {
                needle[--nlen] = '\0';

                row_sel = 0;
                row_top = 0;
                rebuild = 1;
            }

            continue;
        }

        /* Printable: append to needle, restricted to 7-bit ASCII to keep substring matcher simple, entries' names are romanised */
        if (wch >= 0x20 && wch < 0x7F && nlen < (int)(sizeof(needle) - 1))
        {
            needle[nlen++] = (char)wch;
            needle[nlen] = '\0';

            row_sel = 0;
            row_top = 0;
            rebuild = 1;
        }
    }

    free(filtered);

    /* Restore underlying screen */
    if (saved)
    {
        copywin(saved, stdscr, 0, 0, y, x, h - 1, w - 1, 0);
        delwin(saved);
    }

    curs_set(1);
    refresh();

    return rc_picked;
}
