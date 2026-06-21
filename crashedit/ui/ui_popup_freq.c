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

#include "ui_internal.h"
#include "../core/freq.h"
#include "../components/config.h"

#define FREQ_UI_MAX_FILES 32
#define FREQ_UI_NAME_MAX 128
#define FREQ_UI_ADDR_MAX 64

#define FREQ_FOCUS_ADDR 0
#define FREQ_FOCUS_LIST 1

static const char *freq_mode_name(int mode)
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

/* Ask user to pick outbound layout when none configured, returns FREQ_MODE_* value or FREQ_MODE_UNSET if cancelled */
static int freq_pick_mode(void)
{
    int key;

    for (;;)
    {
        int y, x, h, w, i, j;

        ui_popup_center(7, 50, &y, &x, &h, &w);
        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < h; i++)
        {
            move(y + i, x);

            for (j = 0; j < w; j++)
                addch(' ');
        }

        ui_box(y, x, h, w);

        mvaddnstr(y, x + 2, " Outbound layout ", 17);
        mvaddnstr(y + 2, x + 2, "(A) ASO flat", w - 4);
        mvaddnstr(y + 3, x + 2, "(B) BSO BinkleyStyle", w - 4);
        mvaddnstr(y + 4, x + 2, "(E) BSO + zone extension", w - 4);
        mvaddnstr(y + h - 2, x + 2, "Pick a layout, ESC to cancel", w - 4);

        attroff(COLOR_PAIR(COL_POPUP));
        refresh();

        key = wrapper_getch();

        if (key == 27)
            return FREQ_MODE_UNSET;

        if (key == 'a' || key == 'A')
            return FREQ_MODE_ASO;

        if (key == 'b' || key == 'B')
            return FREQ_MODE_BSO;

        if (key == 'e' || key == 'E')
            return FREQ_MODE_BSO_EXT;
    }
}

/* Is c a character we accept into the FTN address field, digits and zone/net/node/point separators only */
static int freq_addr_char(int c)
{
    return (c >= '0' && c <= '9') || c == ':' || c == '/' || c == '.';
}

int ui_popup_freq(UiApp *app)
{
    char files[FREQ_UI_MAX_FILES][FREQ_UI_NAME_MAX];
    int nfiles = 0;
    char password[64];
    char addr[FREQ_UI_ADDR_MAX];
    int addr_len = 0;
    int mode;
    int focus = FREQ_FOCUS_ADDR;
    int sel = 0, top = 0;
    int y, x, h, w, visible;
    int key;

    if (!app || !app->cfg)
        return 0;

    password[0] = '\0';
    addr[0] = '\0';

    /* OUTBOUND must be configured; warn and bail if missing */
    if (!app->cfg->freq_outbound[0])
    {
        ui_popup_message("Error", "File request needs OUTBOUND configured in config");
        return 0;
    }

    /* Mode from config, or ask once up front. */
    mode = app->cfg->freq_mode;

    if (mode == FREQ_MODE_UNSET)
    {
        mode = freq_pick_mode();

        if (mode == FREQ_MODE_UNSET)
            return 0; /* cancelled */
    }

    ui_popup_center(20, 64, &y, &x, &h, &w);
    visible = h - 7; /* address line + separators + footer reserved */

    if (visible < 1)
        visible = 1;

    for (;;)
    {
        int i, j, tl, tx;
        char foot[128];
        const char *t;
        int cursor_x = 0; /* where to park the caret if address pane has focus */
        int fieldx;
        int fieldw;
        int k;
        char meta[80];

        /* Clear screen each iteration to remove sub-popup artefacts, arealist redraws on return */
        /*erase();*/

        attron(COLOR_PAIR(COL_POPUP));

        for (i = 0; i < h; i++)
        {
            move(y + i, x);

            for (j = 0; j < w; j++)
                addch(' ');
        }

        ui_box(y, x, h, w);

        t = " File request ";
        tl = (int)strlen(t);
        tx = x + (w - tl) / 2;
        mvaddnstr(y, tx, t, tl);

        /* Top pane: boxed address field with label; cursor visible when focused */
        attron(COLOR_PAIR(COL_POPUP));
        mvaddnstr(y + 1, x + 2, "Node:", 5);

        fieldx = x + 8;  /* where the address box starts */
        fieldw = w - 10; /* width of the address box */

        if (fieldw < 8)
            fieldw = 8;

        if (focus == FREQ_FOCUS_ADDR)
            attron(COLOR_PAIR(COL_POPUP_SEL));

        /* Draw the field background so it reads as an input box. */
        for (k = 0; k < fieldw; k++)
            mvaddch(y + 1, fieldx + k, ' ');

        mvaddnstr(y + 1, fieldx, addr[0] ? addr : "", fieldw);

        if (focus == FREQ_FOCUS_ADDR)
            attroff(COLOR_PAIR(COL_POPUP_SEL));
        else
            attroff(COLOR_PAIR(COL_POPUP));

        attron(COLOR_PAIR(COL_POPUP));

        /* Mode + password summary on their own line. */
        snprintf(meta, sizeof(meta), "Mode: %s   Password: %s", freq_mode_name(mode), password[0] ? password : "-");
        mvaddnstr(y + 2, x + 2, meta, w - 4);

        /* Defer cursor positioning; mvaddnstr below would drag it away. Final move() after paint */
        cursor_x = fieldx + addr_len;

        if (cursor_x > fieldx + fieldw - 1)
            cursor_x = fieldx + fieldw - 1;

        attron(COLOR_PAIR(COL_POPUP));

        /* Separator under the address area */
        for (j = 1; j < w - 1; j++)
            mvaddch(y + 3, x + j, '-');

        /* Bottom pane: file list */
        if (nfiles == 0)
        {
            mvaddnstr(y + 5, x + 2, "(no files yet -- TAB here, then A to add)", w - 4);
        }
        else
        {
            for (i = 0; i < visible && top + i < nfiles; i++)
            {
                int idx = top + i;

                if (focus == FREQ_FOCUS_LIST && idx == sel)
                    attron(COLOR_PAIR(COL_POPUP_SEL));
                else
                    attron(COLOR_PAIR(COL_POPUP));

                mvaddnstr(y + 5 + i, x + 2, files[idx], w - 4);
                attroff(COLOR_PAIR(COL_POPUP_SEL));
            }
        }

        attron(COLOR_PAIR(COL_POPUP));

        snprintf(foot, sizeof(foot), "TAB pane  A add  D del  P pass  M mode  W write(%d)  ESC", nfiles);
        mvaddnstr(y + h - 2, x + 2, foot, w - 4);

        attroff(COLOR_PAIR(COL_POPUP));

        /* Cursor positioning must be last; drawing calls move it */
        if (focus == FREQ_FOCUS_ADDR)
        {
            move(y + 1, cursor_x);
            curs_set(1);
        }
        else
        {
            curs_set(0);
        }

        refresh();

        key = wrapper_getch();

        /* Keys common to both panels */
        if (key == 27)
            break;

        if (key == '\t' || key == KEY_BTAB)
        {
            focus = (focus == FREQ_FOCUS_ADDR) ? FREQ_FOCUS_LIST : FREQ_FOCUS_ADDR;

            /* Force cursor update immediately after focus change */
            curs_set(focus == FREQ_FOCUS_ADDR ? 1 : 0);
            refresh();
            continue;
        }

        if (key == 'p' || key == 'P')
        {
            wchar_t wpassword[64];
            wchar_t *w_initial;

            w_initial = utf8_to_wcs(password, NULL);
            wpassword[0] = L'\0';

            if (w_initial)
            {
                wcsncpy(wpassword, w_initial, 63);
                wpassword[63] = L'\0';
                free(w_initial);
            }

            if (ui_popup_input("Request password", "Password (blank = none):", wpassword, 64) == 0)
            {
                char *u = wcs_to_utf8(wpassword, (int)wcslen(wpassword));

                if (u)
                {
                    strncpy(password, u, sizeof(password) - 1);
                    password[sizeof(password) - 1] = '\0';
                    free(u);
                }
            }

            continue;
        }

        if (key == 'm' || key == 'M')
        {
            int nm = freq_pick_mode();

            if (nm != FREQ_MODE_UNSET)
                mode = nm;

            continue;
        }

        if (key == 'w' || key == 'W' || ((key == '\n' || key == '\r' || key == KEY_ENTER) && focus == FREQ_FOCUS_LIST))
        {
            char *ptrs[FREQ_UI_MAX_FILES];
            char reqpath[FREQ_MAX_PATH];
            unsigned int zone = 0, net = 0, node = 0, point = 0;
            int rc, n;

            if (freq_parse_addr(addr, &zone, &net, &node, &point) != 0)
            {
                ui_status(app, "Invalid address: %s", addr[0] ? addr : "(empty)");
                focus = FREQ_FOCUS_ADDR;
                continue;
            }

            if (nfiles == 0)
            {
                ui_status(app, "No files to request");
                focus = FREQ_FOCUS_LIST;
                continue;
            }

            for (n = 0; n < nfiles; n++)
                ptrs[n] = files[n];

            rc = freq_write(app->cfg->freq_outbound, mode, zone, net, node, point, ptrs, nfiles, password[0] ? password : NULL, 0, 0, reqpath, sizeof(reqpath));

            if (rc == 0)
            {
                ui_status(app, "Wrote %d-file request: %s", nfiles, reqpath);
                curs_set(0);
                return 1;
            }

            ui_status(app, "Failed to write request (check OUTBOUND path/filenames)");
            continue;
        }

        /* Address pane: in-place editing */
        if (focus == FREQ_FOCUS_ADDR)
        {
            if ((key == KEY_BACKSPACE || key == 127 || key == 8) && addr_len > 0)
            {
                addr[--addr_len] = '\0';
                continue;
            }

            if (freq_addr_char(key) && addr_len < FREQ_UI_ADDR_MAX - 1)
            {
                addr[addr_len++] = (char)key;
                addr[addr_len] = '\0';
                continue;
            }

            continue; /* ignore anything else while editing the address */
        }

        /* List panel: manage files */
        if (key == 'a' || key == 'A')
        {
            wchar_t wname[FREQ_UI_NAME_MAX];
            char name[FREQ_UI_NAME_MAX];
            size_t nl;
            wname[0] = L'\0';

            if (nfiles >= FREQ_UI_MAX_FILES)
            {
                ui_status(app, "Request list full (%d)", FREQ_UI_MAX_FILES);
                continue;
            }

            if (ui_popup_input_width("Add file to request", "Remote filename:", wname, FREQ_UI_NAME_MAX, 64) == 0 && wname[0])
            {
                char *u = wcs_to_utf8(wname, (int)wcslen(wname));

                if (u)
                {
                    strncpy(name, u, sizeof(name) - 1);
                    name[sizeof(name) - 1] = '\0';
                    free(u);

                    nl = strlen(name);

                    if (nl >= FREQ_UI_NAME_MAX)
                        nl = FREQ_UI_NAME_MAX - 1;

                    memcpy(files[nfiles], name, nl);
                    files[nfiles][nl] = '\0';
                    nfiles++;
                }
            }

            continue;
        }

        if ((key == 'd' || key == 'D' || key == KEY_DC) && nfiles > 0)
        {
            int k;

            for (k = sel; k < nfiles - 1; k++)
                memcpy(files[k], files[k + 1], FREQ_UI_NAME_MAX);

            nfiles--;

            if (sel >= nfiles)
                sel = nfiles > 0 ? nfiles - 1 : 0;

            if (sel < top)
                top = sel;

            continue;
        }

        if ((key == KEY_UP || key == 'k') && sel > 0)
        {
            sel--;

            if (sel < top)
                top = sel;
        }

        if ((key == KEY_DOWN || key == 'j') && sel < nfiles - 1)
        {
            sel++;

            if (sel >= top + visible)
                top = sel - visible + 1;
        }
    }

    curs_set(0);
    return 0;
}
