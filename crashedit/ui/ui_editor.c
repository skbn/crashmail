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

/* ui_editor.c -- Message editor rendering and event loop */

#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif

#include "../core/clipboard.h"
#include "../components/editor.h"
#include "../../src/jamlib/jam.h"
#include "ui_editor_internal.h"
#include "ui_aka.h"
#include "ui_attr.h"
#include "ui_files.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

static const char *EDITOR_HELP[] =
    {
        "Editor - Key Bindings:",
        "",
        "  Navigation / fields:",
        "    TAB Ctrl+N      Next field",
        "    S-TAB Ctrl+P    Previous field",
        "    Arrows          Move cursor",
#ifdef PLATFORM_AMIGA
        "    Ctrl+B/E        Home / End",
        "    Ctrl+U/D        Page Up/Dn",
#else
        "    Home/End        Line start/end",
        "    PgUp/PgDn       Page scroll",
#endif
        "    (Fields cycle: From, To, Subj, Body)",
        "",
        "  Edit:",
        "    Ctrl-Y          Delete line",
        "    Ctrl-Z          Undo",
        "    Alt+Z           Redo",
        "    Ctrl-T          Del word right",
        "    Ctrl-_          Del word left",
#ifdef PLATFORM_AMIGA
        "    Alt+I           Toggle insert",
#else
        "    Ins Alt+I       Toggle insert",
#endif
        "    Ctrl-W          Rewrap paragraph",
        "    Ctrl+Left/Right Word movement",
        "",
        "  Block (selection):",
        "    F6 Alt+B        Mark/unmark block at cursor",
        "    Ctrl-C          Copy block",
        "    Ctrl-X          Cut block",
        "    BS Del          Delete block (no clipboard)",
        "    Ctrl-O          Export block",
#ifdef PLATFORM_AMIGA
        "    Ctrl-V          Paste from system clipboard",
        "    RAmiga-V        Paste from system clipboard",
#else
        "    Ctrl-V          Paste block",
#endif
        "",
        "  Search / nav:",
        "    F5 Alt+S        Search (show all matches)",
        "    Ctrl-R          Find & replace",
        "    F3 Alt+P        Prev match (search mode)",
        "    F4 Alt+N        Next match (search mode)",
        "    Alt+M           Goto line",
        "    Alt+G           Clear search highlights",
        "    Ctrl-G          Go to start of document",
        "    Ctrl-K          Go to end of document",
        "    F7 Alt+O        Insert file",
        "    F8 Alt+K        Kludges (Enter del)",
        "",
        "  Attachments:",
        "    Ctrl+F          Add attachment (file)",
        "    Ctrl-Q          Remove attachment",
        "    Ctrl-L          List attachments",
        "    Alt+L           Clear all attachments",
        "",
        "  Header / send:",
        "    F2 Ctrl-S       Save",
        "    F3 ALT-C        Charset",
        "    F4 Ctrl-A       AKA (netmail)",
        "    F9 Alt+A        Attr (Priv/Crash/Hold)",
        "    ESC F10         Cancel (confirm)",
        "    F1 ?            This help"};
#define EDITOR_HELP_N ((int)(sizeof(EDITOR_HELP) / sizeof(EDITOR_HELP[0])))

/* Soft-wrap viewport state (reset on editor entry, only for soft-wrap mode) */
static int s_soft_vtop = 0;
static int s_soft_desired_vcol = -1;
static int s_soft_last_width = -1;

/* Header field drawing */
static void draw_edit_header(UiApp *app)
{
    AreaEntry *ae;
    const wchar_t *from = msghdr_get(app->edit_hdr, HDR_FROM);
    const wchar_t *to = msghdr_get(app->edit_hdr, HDR_TO);
    static const char SPACES[256] =
        "                                                                "
        "                                                                "
        "                                                                "
        "                                                                ";
    const wchar_t *subj = msghdr_get(app->edit_hdr, HDR_SUBJECT);
    const char *oaddr = ui_wcs2u8(msghdr_get(app->edit_hdr, HDR_OADDR));
    const char *daddr = ui_wcs2u8(msghdr_get(app->edit_hdr, HDR_DADDR));
    const char *area = ui_wcs2u8(msghdr_get(app->edit_hdr, HDR_AREA));
    char attr_str[40];
    int i;
    const char *cs;
    int cl;
    const char *attach_mode = "";

    ae = &app->areas->entries[app->sess.area_idx];

    /* Build attribute string */
    ui_attr_build(app->edit_attr, attr_str, sizeof(attr_str));

    attron(COLOR_PAIR(COL_HEADER));

    for (i = 1; i < 7; i++)
    {
        int written = 0;

        move(i, 0);
        clrtoeol();

        while (written < COLS)
        {
            int chunk = COLS - written;

            if (chunk > (int)sizeof(SPACES))
                chunk = (int)sizeof(SPACES);

            mvaddnstr(i, written, SPACES, chunk);
            written += chunk;
        }
    }

    /* Area on left, edit-mode label in menu bar */
    /*mvprintw(1, 1, "Area: %-30.30s  %s", area, app->edit_is_new ? (app->edit_is_reply ? "Reply" : "New Message") : "Edit");*/

    if (app->attach_list && app->attach_list->count > 0)
        attach_mode = "[ATT]";

    mvprintw(1, 1, "Area: %-30.30s  %s  %s", area, app->edit_is_new ? (app->edit_is_reply ? "Reply" : "New Message") : "Edit", attach_mode);

    /* From + origin AKA */
    if (app->edit_active_field == EF_FROM)
        attron(A_REVERSE);

    mvprintw(2, 1, "From: %-25.25s  %-20.20s %s", ui_wcs2u8(from), oaddr, "[locked]");

    if (app->edit_active_field == EF_FROM)
    {
        attroff(A_REVERSE);
        move(msghdr_field_row(app->edit_hdr, HDR_FROM), msghdr_field_col(app->edit_hdr, HDR_FROM) + msghdr_edit_col(app->edit_hdr));
        curs_set(1);
    }

    /* To + destination AKA (netmail only) */
    if (app->edit_active_field == EF_TO)
        attron(A_REVERSE);

    mvprintw(3, 1, "  To: %-25.25s", ui_wcs2u8(to));

    if (app->edit_active_field == EF_TO)
    {
        attroff(A_REVERSE);
        move(msghdr_field_row(app->edit_hdr, HDR_TO), msghdr_field_col(app->edit_hdr, HDR_TO) + msghdr_edit_col(app->edit_hdr));
        curs_set(1);
    }

    /* Subject (or Dest for netmail) */
    if (ae->type != AREATYPE_NETMAIL)
    {
        if (app->edit_active_field == EF_SUBJECT)
            attron(A_REVERSE);

        mvprintw(4, 1, "Subj: %s", ui_wcs2u8(subj));

        if (app->edit_active_field == EF_SUBJECT)
        {
            attroff(A_REVERSE);
            move(msghdr_field_row(app->edit_hdr, HDR_SUBJECT), msghdr_field_col(app->edit_hdr, HDR_SUBJECT) + msghdr_edit_col(app->edit_hdr));
            curs_set(1);
        }
    }
    else
    {
        /* Destination AKA (netmail only, editable) */
        if (app->edit_active_field == EF_DADDR)
            attron(A_REVERSE);

        mvprintw(4, 1, "Dest: %-25.25s", daddr[0] ? daddr : "");

        if (app->edit_active_field == EF_DADDR)
        {
            attroff(A_REVERSE);
            move(msghdr_field_row(app->edit_hdr, HDR_DADDR), msghdr_field_col(app->edit_hdr, HDR_DADDR) + msghdr_edit_col(app->edit_hdr));
            curs_set(1);
        }

        /* Subject (netmail only) */
        if (app->edit_active_field == EF_SUBJECT)
            attron(A_REVERSE);

        mvprintw(5, 1, "Subj: %s", ui_wcs2u8(subj));

        if (app->edit_active_field == EF_SUBJECT)
        {
            attroff(A_REVERSE);
            move(msghdr_field_row(app->edit_hdr, HDR_SUBJECT), msghdr_field_col(app->edit_hdr, HDR_SUBJECT) + msghdr_edit_col(app->edit_hdr));
            curs_set(1);
        }
    }

    /* Attr on left, Charset on right */
    mvprintw(ae->type == AREATYPE_NETMAIL ? 6 : 5, 1, "Attr: %s", attr_str[0] ? attr_str : "-");

    /* Show effective output charset (resolve Auto to cfg->charset) */
    cs = app->edit_charset[0] ? app->edit_charset : (app->cfg && app->cfg->charset[0] ? app->cfg->charset : "UTF-8");
    cl = (int)strlen(cs);

    mvprintw(ae->type == AREATYPE_NETMAIL ? 6 : 5, COLS - cl - 10, "Charset: %s", cs);

    /* Hide cursor if not in header edit mode */
    if (app->edit_active_field != EF_FROM && app->edit_active_field != EF_TO && app->edit_active_field != EF_SUBJECT && app->edit_active_field != EF_DADDR)
        curs_set(0);

    attroff(COLOR_PAIR(COL_HEADER));

    attron(COLOR_PAIR(COL_BORDER));
    ui_hline(ae->type == AREATYPE_NETMAIL ? 7 : 6, 0, COLS);
    attroff(COLOR_PAIR(COL_BORDER));
}

/* Editor body drawing and soft-wrap helpers */
static void soft_cursor_vpos(UiApp *app, int width, int *out_vrow, int *out_vcol);

/* Reset search state in editor */
static void reset_search(UiApp *app)
{
    if (app->edit_search.rows)
    {
        free(app->edit_search.rows);
        app->edit_search.rows = NULL;
    }

    if (app->edit_search.cols)
    {
        free(app->edit_search.cols);
        app->edit_search.cols = NULL;
    }

    app->edit_search.match_count = 0;
    app->edit_search.is_mode = 0;
    app->edit_search.only_mode = 0;
    app->edit_search.current_match = 0;
    app->edit_search.match_current = 0;
}

/* Navigate to previous match in editor */
static int search_prev_editor(UiApp *app)
{
    int match_row;
    int match_col;

    if (!app->edit_search.rows || app->edit_search.match_count == 0)
        return 0;

    app->edit_search.current_match = (app->edit_search.current_match - 1 + app->edit_search.match_count) % app->edit_search.match_count;
    app->edit_search.match_current = app->edit_search.current_match + 1;

    match_row = app->edit_search.rows[app->edit_search.current_match];
    match_col = app->edit_search.cols[app->edit_search.current_match];

    ed_set_pos(app->editor, match_row, match_col);
    ed_ensure_visible(app->editor);

    return 1;
}

/* Navigate to next match in editor */
static int search_next_editor(UiApp *app)
{
    int match_row;
    int match_col;

    if (!app->edit_search.rows || app->edit_search.match_count == 0)
        return 0;

    app->edit_search.current_match = (app->edit_search.current_match + 1) % app->edit_search.match_count;
    app->edit_search.match_current = app->edit_search.current_match + 1;

    match_row = app->edit_search.rows[app->edit_search.current_match];
    match_col = app->edit_search.cols[app->edit_search.current_match];

    ed_set_pos(app->editor, match_row, match_col);
    ed_ensure_visible(app->editor);

    return 1;
}

/* Helper: replace all occurrences of needle with repl (for replace_all) */
static int do_replace(UiApp *app, const wchar_t *needle, const wchar_t *repl)
{
    int count = 0;
    int nlen = (int)wcslen(needle);
    int rlen = (int)wcslen(repl);
    int *rows = NULL, *cols = NULL;
    int match_count, i;

    /* Find all matches with current case-sensitive and whole-word options */
    match_count = ed_search_all_custom(app->editor, needle, app->edit_search.case_sensitive, app->edit_search.whole_word, &rows, &cols);

    if (match_count == 0)
        return 0;

    /* Replace from end to start to avoid position shifts */
    for (i = match_count - 1; i >= 0; i--)
    {
        int j;

        ed_set_pos(app->editor, rows[i], cols[i]);

        /* Delete needle */
        for (j = 0; j < nlen; j++)
            ed_delete(app->editor);

        /* Insert replacement */
        for (j = 0; j < rlen; j++)
            ed_insert_char(app->editor, repl[j]);

        count++;
    }

    free(rows);
    free(cols);

    return count;
}

/* Interactive replace - uses ui_popup_replace with case/whole-word options */
static int replace(UiApp *app)
{
    wchar_t needle[64], repl[64];
    int case_sensitive = app->edit_search.case_sensitive;
    int whole_word = app->edit_search.whole_word;
    int *rows = NULL, *cols = NULL;
    int match_count;

    wcsncpy(needle, app->edit_search.query, 63);
    needle[63] = L'\0';
    wcsncpy(repl, app->edit_search.last_replace, 63);
    repl[63] = L'\0';

    if (ui_popup_replace(needle, repl, needle, 64, repl, 64, &case_sensitive, &whole_word) != 0 || !needle[0])
        return 1;

    wcsncpy(app->edit_search.query, needle, 63);
    app->edit_search.query[63] = L'\0';
    wcsncpy(app->edit_search.last_replace, repl, 63);
    app->edit_search.last_replace[63] = L'\0';
    app->edit_search.case_sensitive = case_sensitive;
    app->edit_search.whole_word = whole_word;

    /* Perform search and highlight matches */
    match_count = ed_search_all_custom(app->editor, app->edit_search.query, case_sensitive, whole_word, &rows, &cols);

    reset_search(app);

    if (match_count > 0)
    {
        app->edit_search.rows = rows;
        app->edit_search.cols = cols;
        app->edit_search.match_count = match_count;
        app->edit_search.is_mode = 1;
        app->edit_search.current_match = 0;
        app->edit_search.match_current = 1;

        /* Move cursor to first match and ensure it's visible */
        ed_set_pos(app->editor, rows[0], cols[0]);
        ed_ensure_visible(app->editor);
    }
    else
    {
        free(rows);
        free(cols);
        app->edit_search.is_mode = 0;
        ui_status(app, "No matches found");
    }

    return 1;
}

/* Replace current match - uses last replacement text without popup */
static int replace_current(UiApp *app)
{
    if (app->edit_search.last_replace[0] != L'\0')
    {
        /* Use the last replacement text - no popup */
        wchar_t repl[64];
        wcsncpy(repl, app->edit_search.last_replace, 63);
        repl[63] = L'\0';

        /* Move cursor to current match position */
        if (!app->edit_search.rows || app->edit_search.match_count == 0)
            return 0;

        int match_row = app->edit_search.rows[app->edit_search.current_match];
        int match_col = app->edit_search.cols[app->edit_search.current_match];
        ed_set_pos(app->editor, match_row, match_col);

        /* Save undo state */
        ed_save_undo(app->editor);

        /* Replace the current occurrence */
        int nlen = (int)wcslen(app->edit_search.query);
        int rlen = (int)wcslen(repl);
        int i;

        /* Delete the search text */
        for (i = 0; i < nlen; i++)
            ed_delete(app->editor);

        /* Insert replacement */
        for (i = 0; i < rlen; i++)
            ed_insert_char(app->editor, repl[i]);

        /* Update search results since text changed */
        free(app->edit_search.rows);
        free(app->edit_search.cols);
        app->edit_search.rows = NULL;
        app->edit_search.cols = NULL;
        app->edit_search.match_count = 0;

        /* Re-run search to find remaining matches */
        int *new_rows = NULL, *new_cols = NULL;
        int new_match_count = ed_search_all_custom(app->editor, app->edit_search.query, app->edit_search.case_sensitive, app->edit_search.whole_word, &new_rows, &new_cols);

        if (new_match_count > 0)
        {
            app->edit_search.rows = new_rows;
            app->edit_search.cols = new_cols;
            app->edit_search.match_count = new_match_count;
            app->edit_search.current_match = 0;
            app->edit_search.match_current = 1;

            /* Move cursor to first remaining match */
            ed_set_pos(app->editor, new_rows[0], new_cols[0]);
            ed_ensure_visible(app->editor);
            ui_status(app, "Replaced. %d match(es) remaining", new_match_count);
        }
        else
        {
            free(new_rows);
            free(new_cols);
            app->edit_search.is_mode = 0;

            ui_status(app, "Replaced 1 occurrence (no more matches)");
        }
        return 1;
    }

    if (app->edit_search.last_replace[0] == L'\0')
        ui_status(app, "No replacement text set (use Ctrl+R first)");

    return 0;
}

/* Replace all matches - uses last replacement text with Yes/No confirmation */
static int replace_all(UiApp *app)
{
    if (app->edit_search.is_mode && app->edit_search.match_count > 0)
    {
        char msg[128];
        int n;

        snprintf(msg, sizeof(msg), "Replace all %d occurrences?", app->edit_search.match_count);

        if (ui_popup_confirm("Replace All", msg) == 1)
        {
            ed_save_undo(app->editor);
            n = do_replace(app, app->edit_search.query, app->edit_search.last_replace);
            reset_search(app);
            app->edit_search.is_mode = 0;
            ui_status(app, "Replaced %d occurrence(s)", n);
        }
        else
        {
            ui_status(app, "Replace All cancelled");
        }
        return 1;
    }
    return 0;
}

/* Position cursor in active field after status bar redraw */
static void position_edit_cursor(UiApp *app)
{
    int ec;

    if (!app)
        return;

    ec = msghdr_edit_col(app->edit_hdr);

    switch (app->edit_active_field)
    {
    case EF_FROM:
        move(msghdr_field_row(app->edit_hdr, HDR_FROM), msghdr_field_col(app->edit_hdr, HDR_FROM) + ec);
        curs_set(1);
        return;
    case EF_TO:
        move(msghdr_field_row(app->edit_hdr, HDR_TO), msghdr_field_col(app->edit_hdr, HDR_TO) + ec);
        curs_set(1);
        return;
    case EF_DADDR:
        move(msghdr_field_row(app->edit_hdr, HDR_DADDR), msghdr_field_col(app->edit_hdr, HDR_DADDR) + ec);
        curs_set(1);
        return;
    case EF_SUBJECT:
        move(msghdr_field_row(app->edit_hdr, HDR_SUBJECT), msghdr_field_col(app->edit_hdr, HDR_SUBJECT) + ec);
        curs_set(1);
        return;
    case EF_BODY:
    {
        EdInfo info;
        AreaEntry *ae;
        int start_row, rows;
        int cy, cx;
        int max_y;
        int soft;

        ae = &app->areas->entries[app->sess.area_idx];
        start_row = (ae->type == AREATYPE_NETMAIL) ? 8 : 7;
        rows = LINES - start_row - 1;
        soft = !(app->cfg && app->cfg->hard_wrap);
        ed_get_info(app->editor, &info);

        if (soft)
        {
            int vrow = 0, vcol = 0;
            int width = COLS < 1 ? 1 : COLS;

            soft_cursor_vpos(app, width, &vrow, &vcol);
            cy = start_row + (vrow - s_soft_vtop);
            cx = vcol;
        }
        else
        {
            cy = start_row + (info.row - info.top);
            cx = info.col;
        }

        /* Always move; clamp to body region */
        max_y = start_row + rows - 1;

        if (max_y < start_row)
            max_y = start_row;

        if (cy < start_row)
            cy = start_row;

        if (cy > max_y)
            cy = max_y;

        if (cx < 0)
            cx = 0;

        if (cx > COLS - 1)
            cx = COLS - 1;

        move(cy, cx);
        curs_set(1);

        return;
    }
    default:
        curs_set(0);
        return;
    }
}

/* Soft-wrap: returns the end of the current visual segment (exclusive)
 * Breaks at the last space boundary that fits within width columns
 * If no space fits (word longer than width), hard-cuts at start+width
 * The next segment starts exactly at the returned position - no chars skipped */
static int wrap_next(const wchar_t *line, int len, int width, int start)
{
    int hard_end;
    int k;

    if (width < 1)
        width = 1;

    hard_end = start + width;

    if (hard_end >= len)
        return len;

    /* Search backwards from hard_end for a space to break at */
    for (k = hard_end; k > start; k--)
    {
        if (line[k - 1] == L' ' || line[k - 1] == L'\t')
            return k;
    }

    /* No space found: hard cut */
    return hard_end;
}

/* Number of visual sub-rows a logical line occupies (>= 1) */
static int wrap_count(const wchar_t *line, int len, int width)
{
    int pos = 0;
    int rows = 1;

    if (len <= 0)
        return 1;

    for (;;)
    {
        int end = wrap_next(line, len, width, pos);

        if (end >= len)
            break;

        if (end <= pos)
            end = pos + (width < 1 ? 1 : width); /* never stall */

        pos = end;
        rows++;
    }

    return rows;
}

/* Visual row/col of cursor within soft-wrapped text */
static void soft_cursor_vpos(UiApp *app, int width, int *out_vrow, int *out_vcol)
{
    EdInfo info;
    int li, vrow = 0;
    const wchar_t *l;
    int len;
    int pos;
    int col;

    ed_get_info(app->editor, &info);

    for (li = 0; li < info.row && li < info.line_count; li++)
    {
        const wchar_t *l = ed_line_wcs(app->editor, li);
        int len = ed_line_len(app->editor, li);

        vrow += wrap_count(l ? l : L"", l ? len : 0, width);
    }

    l = ed_line_wcs(app->editor, info.row);
    len = ed_line_len(app->editor, info.row);
    pos = 0;
    col = info.col;

    if (l && len > 0)
    {
        for (;;)
        {
            int end = wrap_next(l, len, width, pos);

            /* Cursor is on this sub-row if it sits before the next
             * sub-row's start, or this is the last sub-row */
            if (end >= len || col < end)
            {
                if (out_vcol)
                {
                    int vc = col - pos;

                    if (vc < 0)
                        vc = 0;

                    *out_vcol = vc;
                }

                break;
            }

            if (end <= pos)
                end = pos + (width < 1 ? 1 : width);

            pos = end;
            vrow++;
        }
    }
    else if (out_vcol)
    {
        *out_vcol = 0;
    }

    if (out_vrow)
        *out_vrow = vrow;
}

/* Soft-wrap vertical movement: cursor walks visual rows, not logical lines
 *
 * Model: editor cursor (ed->row, ed->col) is still the LOGICAL position;
 * the UI only translates to/from VISUAL (vrow, vcol) coordinates when
 * the user presses UP/DOWN/PgUp/PgDn so they behave like in a regular
 * editor that wraps text on screen. Resize is automatic because the
 * cursor is anchored to the logical character - the next draw recomputes
 * the visual position with the new width. We only invalidate the
 * "desired_vcol" goal on resize so it gets re-synced from the new layout
 *
 * Goal column ("desired_vcol"): the visual column the user STARTED their
 * vertical streak from. Lets the cursor "remember" its column when
 * traversing shorter visual rows - e.g. col 30 -> short row at col 5
 * -> back to col 30 on the next long row. Same idea as Emacs/vi
 * -1 means unset; the next vertical move re-syncs it from the cursor */
static void soft_reset_desired()
{
    s_soft_desired_vcol = -1;
}

/* Total visual rows occupied by logical lines [0..upto) at the given width */
static int soft_count_rows_before(Ed *ed, int upto, int width)
{
    int total = 0;
    int li;

    for (li = 0; li < upto; li++)
    {
        const wchar_t *l = ed_line_wcs(ed, li);
        int len = ed_line_len(ed, li);

        total += wrap_count(l ? l : L"", l ? len : 0, width);
    }

    return total;
}

/* Within one logical line, find the logical column that corresponds to
 * (vrow_in_line, target_vcol). vrow_in_line is the sub-row offset within
 * this line (0 = first sub-row)
 *
 * Returns 0 on success, -1 if vrow_in_line is beyond this line's sub-rows
 * If target_vcol overruns the sub-row's content, the result is clamped to
 * the end of that sub-row's segment - matching what the user sees */
static int soft_seg_at(const wchar_t *l, int len, int width, int vrow_in_line, int target_vcol, int *out_col)
{
    int pos = 0;
    int sub = 0;

    if (!l || len <= 0)
    {
        if (vrow_in_line == 0)
        {
            *out_col = 0;
            return 0;
        }

        return -1;
    }

    for (;;)
    {
        int seg_end = wrap_next(l, len, width, pos);
        int seg_len = seg_end - pos;
        int is_last = (seg_end >= len);
        int v;

        if (sub == vrow_in_line)
        {
            v = target_vcol;
            if (v < 0)
                v = 0;

            /* End on last segment: go to insertion point (seg_len)
             * End on interior segment: stay on last visible char (seg_len-1)
             * so cursor doesn't jump to next row */
            if (v >= seg_len)
            {
                if (is_last)
                    v = seg_len;
                else
                    v = (seg_len > 0) ? seg_len - 1 : 0;
            }

            *out_col = pos + v;

            return 0;
        }

        sub++;

        if (seg_end >= len)
            return -1; /* requested vrow past last sub-row */

        if (seg_end <= pos)
            seg_end = pos + (width < 1 ? 1 : width);

        pos = seg_end;
    }
}

/* Convert an absolute visual position (target_vrow, target_vcol) - across
 * all logical lines - to (row, col) for ed_set_pos
 *
 * Out-of-range vrow clamps to the last visual row of the document
 * Out-of-range vcol clamps to the end of the destination sub-row */
static void soft_visual_to_logical(Ed *ed, int width, int target_vrow, int target_vcol, int *out_row, int *out_col)
{
    EdInfo info;
    int li;
    int vacc = 0;

    ed_get_info(ed, &info);

    if (target_vrow < 0)
        target_vrow = 0;

    for (li = 0; li < info.line_count; li++)
    {
        const wchar_t *l = ed_line_wcs(ed, li);
        int len = ed_line_len(ed, li);
        int wc = wrap_count(l ? l : L"", l ? len : 0, width);

        if (target_vrow < vacc + wc)
        {
            int vrow_in_line = target_vrow - vacc;
            int col = 0;

            if (soft_seg_at(l, len, width, vrow_in_line, target_vcol, &col) == 0)
            {
                *out_row = li;
                *out_col = col;

                return;
            }
        }

        vacc += wc;
    }

    /* Past the end: clamp to last line's last sub-row */
    if (info.line_count > 0)
    {
        int last = info.line_count - 1;
        const wchar_t *l = ed_line_wcs(ed, last);
        int len = ed_line_len(ed, last);
        int wc = wrap_count(l ? l : L"", l ? len : 0, width);
        int col = 0;

        if (wc > 0 && soft_seg_at(l, len, width, wc - 1, target_vcol, &col) == 0)
        {
            *out_row = last;
            *out_col = col;

            return;
        }

        *out_row = last;
        *out_col = len;

        return;
    }

    *out_row = 0;
    *out_col = 0;
}

/* Move cursor one visual row up. Preserves desired_vcol so consecutive
 * UPs over shorter rows still land back on the same column on long rows */
static void soft_move_up_visual(UiApp *app, int width)
{
    int vr, vc, nr, nc;

    soft_cursor_vpos(app, width, &vr, &vc);

    /* Already on the first visual row: nothing to do (matches ed_move_up) */
    if (vr <= 0)
        return;

    if (s_soft_desired_vcol < 0)
        s_soft_desired_vcol = vc;

    soft_visual_to_logical(app->editor, width, vr - 1, s_soft_desired_vcol, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);
    ed_ensure_visible(app->editor);
}

/* Move cursor one visual row down. Mirror of soft_move_up_visual */
static void soft_move_down_visual(UiApp *app, int width)
{
    EdInfo info;
    int total_vrows;
    int vr, vc, nr, nc;

    ed_get_info(app->editor, &info);

    if (info.line_count <= 0)
        return;

    total_vrows = soft_count_rows_before(app->editor, info.line_count, width);
    soft_cursor_vpos(app, width, &vr, &vc);

    /* Already on the last visual row: nothing to do (matches ed_move_down) */
    if (vr >= total_vrows - 1)
        return;

    if (s_soft_desired_vcol < 0)
        s_soft_desired_vcol = vc;

    soft_visual_to_logical(app->editor, width, vr + 1, s_soft_desired_vcol, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);
    ed_ensure_visible(app->editor);
}

/* HOME in soft-wrap: go to column 0 of current visual row */
static void soft_move_home_visual(UiApp *app, int width)
{
    int vr, vc, nr, nc;

    soft_cursor_vpos(app, width, &vr, &vc);
    soft_visual_to_logical(app->editor, width, vr, 0, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);
    ed_ensure_visible(app->editor);
}

/* END in soft-wrap: go to last column of current visual row */
static void soft_move_end_visual(UiApp *app, int width)
{
    int vr, vc, nr, nc;

    soft_cursor_vpos(app, width, &vr, &vc);

    /* Use large value (not INT_MAX) to clamp to segment end */
    soft_visual_to_logical(app->editor, width, vr, width + 1000000, &nr, &nc);

    ed_set_pos(app->editor, nr, nc);
    ed_ensure_visible(app->editor);
}

/* Move pg visual rows up. Preserves desired_vcol */
static void soft_move_pgup_visual(UiApp *app, int width, int pg)
{
    int vr, vc, nr, nc;
    int target;

    if (pg <= 0)
        pg = 1;

    soft_cursor_vpos(app, width, &vr, &vc);

    if (s_soft_desired_vcol < 0)
        s_soft_desired_vcol = vc;

    target = vr - pg;

    if (target < 0)
        target = 0;

    soft_visual_to_logical(app->editor, width, target, s_soft_desired_vcol, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);

    /* Scroll viewport so the cursor stays at roughly the same screen row */
    s_soft_vtop -= pg;

    if (s_soft_vtop < 0)
        s_soft_vtop = 0;

    ed_ensure_visible(app->editor);
}

/* Move pg visual rows down. Mirror of soft_move_pgup_visual */
static void soft_move_pgdn_visual(UiApp *app, int width, int pg)
{
    EdInfo info;
    int total_vrows;
    int vr, vc, nr, nc;
    int target;

    if (pg <= 0)
        pg = 1;

    ed_get_info(app->editor, &info);

    if (info.line_count <= 0)
        return;

    total_vrows = soft_count_rows_before(app->editor, info.line_count, width);
    soft_cursor_vpos(app, width, &vr, &vc);

    if (s_soft_desired_vcol < 0)
        s_soft_desired_vcol = vc;

    target = vr + pg;

    if (target > total_vrows - 1)
        target = total_vrows - 1;

    if (target < 0)
        target = 0;

    soft_visual_to_logical(app->editor, width, target, s_soft_desired_vcol, &nr, &nc);
    ed_set_pos(app->editor, nr, nc);

    s_soft_vtop += pg;

    if (s_soft_vtop < 0)
        s_soft_vtop = 0;

    ed_ensure_visible(app->editor);
}

static void draw_edit_body(UiApp *app)
{
    EdInfo info;
    AreaEntry *ae;
    int rows, start_row, i;
    int b_r1 = -1, b_c1 = 0, b_r2 = -1, b_c2 = 0;
    int soft;
    int width;
    int cur_vrow, cur_vcol;
    int li, sr;
    int vrow;

    ae = &app->areas->entries[app->sess.area_idx];
    start_row = (ae->type == AREATYPE_NETMAIL) ? 8 : 7;
    rows = LINES - start_row - 1;
    soft = !(app->cfg && app->cfg->hard_wrap);
    width = COLS; /* visual wrap fits the terminal */

    if (width < 1)
        width = 1;

    ed_set_page(app->editor, rows);
    ed_get_info(app->editor, &info);

    /* Normalize block range (anchor vs cursor) for highlight */
    if (info.block.active)
    {
        if (info.block.anchor_row < info.row || (info.block.anchor_row == info.row && info.block.anchor_col <= info.col))
        {
            b_r1 = info.block.anchor_row;
            b_c1 = info.block.anchor_col;
            b_r2 = info.row;
            b_c2 = info.col;
        }
        else
        {
            b_r1 = info.row;
            b_c1 = info.col;
            b_r2 = info.block.anchor_row;
            b_c2 = info.block.anchor_col;
        }
    }

    if (!soft)
    {
        /* HARD-WRAP: classic 1 logical line == 1 screen row */
        for (i = 0; i < rows; i++)
        {
            int line_idx = info.top + i;
            int line_len;
            const wchar_t *wl;

            move(start_row + i, 0);
            clrtoeol();

            if (line_idx >= info.line_count)
                continue;

            /* mvaddnwstr: n is in wide chars, no UTF-8 conversion needed */
            wl = ed_line_wcs(app->editor, line_idx);
            line_len = ed_line_len(app->editor, line_idx);

            if (wl && line_len > 0)
                mvaddnwstr(start_row + i, 0, wl, line_len);

            /* Highlight search matches */
            if (app->edit_search.rows && app->edit_search.match_count > 0)
            {
                int j;

                for (j = 0; j < app->edit_search.match_count; j++)
                {
                    if (app->edit_search.rows[j] == line_idx)
                    {
                        int match_col = app->edit_search.cols[j];
                        int match_len = (int)wcslen(app->edit_search.query);

                        if (match_col >= 0 && match_col + match_len <= line_len)
                        {
                            attron(COLOR_PAIR(COL_SEARCH_MATCH));
                            mvaddnwstr(start_row + i, match_col, &wl[match_col], match_len);
                            attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                        }
                    }
                }
            }

            if (b_r1 >= 0 && line_idx >= b_r1 && line_idx <= b_r2)
            {
                const wchar_t *wcs;
                int hs, he;

                hs = (line_idx == b_r1) ? b_c1 : 0;
                he = (line_idx == b_r2) ? b_c2 : line_len;

                if (hs < 0)
                    hs = 0;

                if (he > line_len)
                    he = line_len;

                wcs = wl;

                if (wcs && hs < he)
                {
                    attron(A_REVERSE);
                    mvaddnwstr(start_row + i, hs, &wcs[hs], he - hs);
                    attroff(A_REVERSE);
                }
            }
        }

        if (app->edit_active_field == EF_BODY)
        {
            int cy = start_row + (info.row - info.top);
            int cx = info.col;

            if (cy >= start_row && cy < start_row + rows)
                move(cy, cx);

            curs_set(1);
        }
        return;
    }

    /* SOFT-WRAP: one logical line spans several screen rows */
    soft_cursor_vpos(app, width, &cur_vrow, &cur_vcol);

    /* Keep the cursor's visual row inside [s_soft_vtop, +rows) */
    if (cur_vrow < s_soft_vtop)
        s_soft_vtop = cur_vrow;
    else if (cur_vrow >= s_soft_vtop + rows)
        s_soft_vtop = cur_vrow - rows + 1;

    if (s_soft_vtop < 0)
        s_soft_vtop = 0;

    /* Walk logical lines, emitting visual sub-rows, skipping the
     * first s_soft_vtop of them, filling `rows` screen rows */
    for (sr = 0; sr < rows; sr++)
    {
        move(start_row + sr, 0);
        clrtoeol();
    }

    vrow = 0; /* absolute visual row being produced */
    sr = 0;   /* screen row index 0..rows-1 */

    for (li = 0; li < info.line_count && sr < rows; li++)
    {
        const wchar_t *l = ed_line_wcs(app->editor, li);
        int len = ed_line_len(app->editor, li);
        int pos = 0;
        int done = 0;

        while (!done && sr < rows)
        {
            int seg_start = pos;
            int seg_end, seg_len, np;

            if (l && len > 0)
            {
                seg_end = wrap_next(l, len, width, pos);
                np = seg_end;
            }
            else
            {
                seg_end = 0;
                np = len; /* empty line: one (blank) sub-row */
            }

            if (vrow >= s_soft_vtop)
            {
                seg_len = seg_end - seg_start;

                if (seg_len < 0)
                    seg_len = 0;

                if (l && seg_len > 0)
                    mvaddnwstr(start_row + sr, 0, &l[seg_start], seg_len);

                /* Highlight search matches in softwrap */
                if (app->edit_search.rows && app->edit_search.match_count > 0)
                {
                    int j;

                    for (j = 0; j < app->edit_search.match_count; j++)
                    {
                        if (app->edit_search.rows[j] == li)
                        {
                            int match_col = app->edit_search.cols[j];
                            int match_len = (int)wcslen(app->edit_search.query);
                            int match_end = match_col + match_len;

                            /* Check if match is within this segment */
                            if (match_col >= seg_start && match_end <= seg_end)
                            {
                                /* Match entirely within this segment */
                                attron(COLOR_PAIR(COL_SEARCH_MATCH));
                                mvaddnwstr(start_row + sr, match_col - seg_start, &l[match_col], match_len);
                                attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                            }
                            else if (match_col >= seg_start && match_col < seg_end)
                            {
                                /* Match starts in this segment, continues to next */
                                int partial_len = seg_end - match_col;
                                attron(COLOR_PAIR(COL_SEARCH_MATCH));
                                mvaddnwstr(start_row + sr, match_col - seg_start, &l[match_col], partial_len);
                                attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                            }
                            else if (match_end > seg_start && match_end <= seg_end)
                            {
                                /* Match ends in this segment, started in previous */
                                int partial_len = match_end - seg_start;

                                attron(COLOR_PAIR(COL_SEARCH_MATCH));
                                mvaddnwstr(start_row + sr, 0, &l[seg_start], partial_len);
                                attroff(COLOR_PAIR(COL_SEARCH_MATCH));
                            }
                        }
                    }
                }

                /* Block-selection overlay (logical-span) */
                if (b_r1 >= 0 && li >= b_r1 && li <= b_r2 && l)
                {
                    int hs = (li == b_r1) ? b_c1 : 0;
                    int he = (li == b_r2) ? b_c2 : len;

                    if (hs < seg_start)
                        hs = seg_start;

                    if (he > seg_end)
                        he = seg_end;

                    if (hs < he)
                    {
                        attron(A_REVERSE);
                        mvaddnwstr(start_row + sr, hs - seg_start, &l[hs], he - hs);
                        attroff(A_REVERSE);
                    }
                }
                sr++;
            }

            vrow++;

            if (np >= len)
                done = 1;
            else
            {
                if (np <= pos)
                    np = pos + (width < 1 ? 1 : width);

                pos = np;
            }
        }
    }

    if (app->edit_active_field == EF_BODY)
    {
        int cy = start_row + (cur_vrow - s_soft_vtop);
        int cx = cur_vcol;

        if (cy < start_row)
            cy = start_row;

        if (cy > start_row + rows - 1)
            cy = start_row + rows - 1;

        if (cx < 0)
            cx = 0;

        if (cx > COLS - 1)
            cx = COLS - 1;

        move(cy, cx);
        curs_set(1);
    }
}

/* Word-wrap UTF-8 paste to col columns, preserving newlines. No hard-breaks for URLs/code */
static int paste_char_width(wchar_t c)
{
    /* For FTN editing: does this char take 1 column or 0? Zero-width cases: combining marks, BOM. CJK treated as 1 */
    if (c == 0)
        return 0;

    if (c < 0x20)
        return 0; /* control chars, including \r */

    if (c >= 0x0300 && c <= 0x036F)
        return 0; /* combining diacritical marks */

    if (c == 0x200B || c == 0xFEFF)
        return 0; /* ZWSP / BOM */

    return 1;
}

static char *wrap_paste_text(const char *utf8, int col)
{
    wchar_t *w;
    int wlen = 0;
    int i;
    int line_start = 0;
    int last_space = -1;
    int col_pos = 0;
    int out_cap, out_len = 0;
    wchar_t *out;
    char *result;

    if (col <= 0)
        return NULL;

    w = utf8_to_wcs(utf8, &wlen);

    if (!w)
        return NULL;

    out_cap = wlen + (wlen / col) + 16;
    out = (wchar_t *)malloc((size_t)out_cap * sizeof(wchar_t));

    if (!out)
    {
        free(w);
        return NULL;
    }

    for (i = 0; i < wlen; i++)
    {
        wchar_t ch = w[i];
        int cw;

        if (ch == L'\n')
        {
            if (out_len < out_cap)
                out[out_len++] = L'\n';

            line_start = out_len;
            last_space = -1;
            col_pos = 0;

            continue;
        }
        if (ch == L'\r')
            continue;

        cw = paste_char_width(ch);

        if (ch == L'\t')
            cw = 1;

        if (out_len < out_cap)
            out[out_len++] = ch;

        if (ch == L' ' || ch == L'\t')
            last_space = out_len - 1;

        col_pos += cw;

        if (col_pos > col && last_space > line_start)
        {
            int new_start;
            int j;
            int width_after = 0;

            out[last_space] = L'\n';
            new_start = last_space + 1;

            for (j = new_start; j < out_len; j++)
            {
                int w2 = (out[j] == L'\t') ? 1 : paste_char_width(out[j]);
                width_after += w2;
            }

            line_start = new_start;
            last_space = -1;
            col_pos = width_after;
        }
    }

    free(w);

    if (out_len < out_cap)
        out[out_len] = L'\0';
    else
        out[out_cap - 1] = L'\0';

    result = wcs_to_utf8(out, out_len);
    free(out);

    return result;
}

/* Forward declaration */
static int editor_eff_wrap(const UiApp *app);

/* Paste UTF-8 buffer at cursor: body preserves newlines, header strips them */
static void deliver_paste(UiApp *app, const char *utf8)
{
    if (!utf8 || !utf8[0])
        return;

    if (app->edit_active_field == EF_BODY)
    {
        char *wrapped = NULL;
        const char *to_insert = utf8;
        int reported_len;

        ed_save_undo(app->editor);

        /* HARD-WRAP only: reflow pasted text; soft-wrap inserts verbatim */
        if (app->cfg && app->cfg->hard_wrap)
        {
            int pw = editor_eff_wrap(app);

            if (pw > 0)
            {
                wrapped = wrap_paste_text(utf8, pw);

                if (wrapped)
                    to_insert = wrapped;
            }
        }

        ed_paste_text(app->editor, to_insert);
        reset_search(app);
        reported_len = (int)strlen(to_insert);

        if (wrapped)
            free(wrapped);

        ui_status(app, "Pasted %d bytes into body", reported_len);
    }
    else
    {
        wchar_t *w = utf8_to_wcs(utf8, NULL);

        if (w)
        {
            int i, n = 0;

            for (i = 0; w[i] && w[i] != L'\n' && w[i] != L'\r'; i++)
            {
                if (w[i] >= 0x20)
                {
                    msghdr_edit_key(app->edit_hdr, (int)w[i]);
                    n++;
                }
            }

            free(w);

            ui_status(app, "Pasted %d chars into header", n);
        }
    }
}

/* Read characters until KEY_PASTE_END. Returns malloc'd UTF-8 buffer or NULL. Keeps tabs, line feeds, printable; drops other control codes */
static char *collect_bracketed_paste()
{
    wchar_t *wbuf = NULL;
    int wlen = 0, wcap = 0;
    char *out;

    for (;;)
    {
        wint_t wch;
        int wrc = wrapper_read_key(&wch);

        if (wrc == ERR)
            break;

        if (wrc == KEY_CODE_YES && (int)wch == KEY_PASTE_END)
            break;

        if (wrc == KEY_CODE_YES)
            continue; /* ignore stray special keys during paste */

        if (wch != L'\n' && wch != L'\t' && wch < 0x20)
            continue;

        if (wlen + 1 >= wcap)
        {
            int ncap = wcap ? wcap * 2 : 256;
            wchar_t *nb = (wchar_t *)realloc(wbuf, (size_t)ncap * sizeof(wchar_t));

            if (!nb)
            {
                free(wbuf);
                return NULL;
            }

            wbuf = nb;
            wcap = ncap;
        }

        wbuf[wlen++] = (wchar_t)wch;
    }

    if (!wbuf || wlen == 0)
    {
        free(wbuf);
        return NULL;
    }

    out = wcs_to_utf8(wbuf, wlen);
    free(wbuf);

    if (!out)
        return NULL;

    return out;
}

/* Effective wrap column. Clamp AUTOWRAP to COLS-1; 0=disabled */
static int editor_eff_wrap(const UiApp *app)
{
    int cfgw = (app && app->cfg) ? app->cfg->autowrap_col : 0;
    int limit = COLS - 1; /* leave one column of margin */

    if (cfgw <= 0)
        return 0; /* AUTOWRAP disabled in config: never wrap */

    if (COLS <= 10)
        return 0; /* Unusably narrow: scroll instead of wrapping */

    if (cfgw > limit)
        return limit; /* Screen narrower than the configured column */

    return cfgw;
}

/* Cycle header field in direction dir (+1=next, -1=prev). Skips DADDR for non-netmail */
static void editor_cycle_field(UiApp *app, int dir)
{
    AreaEntry *ae = &app->areas->entries[app->sess.area_idx];
    int start = EF_TO;
    int next;
    int hdr;

    /* From body: jump into header */
    if (app->edit_active_field == EF_BODY)
        next = (dir < 0) ? EF_SUBJECT : start;
    else
        next = app->edit_active_field + dir;

    if (next > EF_SUBJECT)
        next = start;

    if (next < start)
        next = EF_SUBJECT;

    if (ae->type != AREATYPE_NETMAIL && next == EF_DADDR)
        next = (dir > 0) ? EF_SUBJECT : EF_TO;

    app->edit_active_field = next;

    hdr = (next == EF_FROM) ? HDR_FROM : (next == EF_TO)    ? HDR_TO
                                     : (next == EF_SUBJECT) ? HDR_SUBJECT
                                                            : HDR_DADDR;
    msghdr_edit_start(app->edit_hdr, hdr);
}

UiView ui_editor_run(UiApp *app)
{
    AreaEntry *ae;
    int eff_wrap;

    if (!app)
        return VIEW_QUIT;

    ae = &app->areas->entries[app->sess.area_idx];

    /* Initial wrap column (handles starting on a small screen) */
    eff_wrap = editor_eff_wrap(app);

    /* Fresh soft-wrap viewport for this editing session */
    s_soft_vtop = 0;
    s_soft_desired_vcol = -1;
    s_soft_last_width = COLS;

    BRACKET_PASTE_ON();

    for (;;)
    {
        wint_t wch;
        int wrc;
        int ch;
        int is_key;
        int soft_active;
        int body_width;
        int body_rows;
        int preserve_desired;
        AreaEntry *ae_body;
        int srow;

        /* Recalculate effective width each frame for resize handling */
        eff_wrap = editor_eff_wrap(app);

        /* On resize, reset goal column to re-sync from new layout */
        if (COLS != s_soft_last_width)
        {
            soft_reset_desired();
            s_soft_last_width = COLS;
        }

        erase();
        ui_draw_menubar(app, app->edit_is_new ? (app->edit_is_reply ? "Reply" : "New Message") : "Edit Message");
        draw_edit_header(app);
        draw_edit_body(app);

        ui_status(app, "%s | F2=Save F3=Charset%s",
                  app->edit_active_field == EF_BODY      ? "Body"
                  : app->edit_active_field == EF_FROM    ? "Header: From"
                  : app->edit_active_field == EF_TO      ? "Header: To"
                  : app->edit_active_field == EF_DADDR   ? "Header: Dest"
                  : app->edit_active_field == EF_SUBJECT ? "Header: Subj"
                                                         : "Idle",
                  "");

        ui_draw_statusbar(app);
        position_edit_cursor(app);
        refresh();

        wrc = wrapper_read_key(&wch);

        if (wrc == ERR)
            continue;

        ch = (int)wch;

        /* Distinguish special key codes (KEY_CODE_YES) from printable chars. Without guard, codepoint matching KEY_F(5) would trigger F5 */
        is_key = (wrc == KEY_CODE_YES);

        /* Force is_key for navigation keys that may not have KEY_CODE_YES on some systems */
        if (!is_key && (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT ||
                        ch == KEY_HOME || ch == KEY_END || ch == KEY_PPAGE || ch == KEY_NPAGE ||
                        ch == KEY_BACKSPACE || ch == KEY_DC || ch == KEY_ENTER))
            is_key = 1;

        /* F1 always; '?' only when not in an input field (so the user can type '?' in headers/body) */
        if (is_key && ch == KEY_F(1))
        {
            ui_popup_help("Editor Help", EDITOR_HELP, EDITOR_HELP_N);
            continue;
        }

        if ((is_key && ch == KEY_F(2)) || (!is_key && ch == CTRL('S')))
        {
            if (ui_editor_save(app) == 0)
            {
                /* Restore edit_charset if not manually changed by user */
                if (!app->edit_charset_manually_changed)
                {
                    strncpy(app->edit_charset, app->edit_charset_saved, sizeof(app->edit_charset) - 1);
                    app->edit_charset[sizeof(app->edit_charset) - 1] = '\0';
                }

                /* Free search matches */
                reset_search(app);

                curs_set(0);
                BRACKET_PASTE_OFF();
                return app->edit_return_view;
            }

            continue;
        }

        if (is_key && ch == KEY_ALT('F'))
            ui_popup_freq(app);

        if ((is_key && ch == KEY_F(3)) || (is_key && ch == KEY_ALT('C')) || (is_key && ch == KEY_ALT('P')))
        {
            /* F3/Alt+P: previous match in search mode, otherwise charset */
            if (app->edit_search.is_mode || app->edit_search.only_mode)
            {
                if (search_prev_editor(app))
                    continue;

                break;
            }
            else
            {
                /* Alt+C: charset picker */
                char new_view[32], new_out[32];

                new_view[0] = '\0';
                new_out[0] = '\0';

                if (ui_popup_charset_pair(app->view_charset, app->edit_charset, CHARSET_READ_DEFAULT, app->cfg->charset, new_view, sizeof(new_view), new_out, sizeof(new_out)) == 0)
                {
                    strncpy(app->view_charset, new_view, sizeof(app->view_charset) - 1);
                    app->view_charset[sizeof(app->view_charset) - 1] = '\0';

                    strncpy(app->edit_charset, new_out, sizeof(app->edit_charset) - 1);
                    app->edit_charset[sizeof(app->edit_charset) - 1] = '\0';

                    /* Mark that user manually changed charset - only if NOT Auto */
                    if (new_out[0] != '\0')
                        app->edit_charset_manually_changed = 1;
                }

                continue;
            }
        }

        /* F4/Ctrl+A: AKA picker (netmail/local only; echo locks AKA to area) */
        if ((is_key && ch == KEY_F(4)) || (!is_key && ch == CTRL('A')) || (is_key && ch == KEY_ALT('N')))
        {
            /* F4/Alt+N: next match in search mode, otherwise AKA picker */
            if ((is_key && ch == KEY_ALT('N')) || (is_key && ch == KEY_F(4)) && app->edit_search.is_mode || app->edit_search.only_mode)
            {
                if (search_next_editor(app))
                    continue;

                break;
            }
            else
            {
                /* Ctrl+A: AKA picker */
                int sel;
                char aka_buf[CFG_AKA_MAX];
                const char *picked;
                char reply_msgid[200];
                const char *daddr_aka;
                AreaEntry *ae = &app->areas->entries[app->sess.area_idx];

                /* Echo areas lock AKA to area configuration */
                if (ae->type == AREATYPE_ECHO)
                {
                    ui_status(app, "AKA is locked by area configuration");
                    continue;
                }

                sel = ui_popup_aka(app, app->edit_aka_idx);

                if (sel < 0)
                    continue;

                picked = ui_aka_at(app->areas, app->cfg, sel);

                if (!picked)
                    continue;

                strncpy(aka_buf, picked, CFG_AKA_MAX - 1);
                aka_buf[CFG_AKA_MAX - 1] = '\0';

                app->edit_aka_idx = sel;
                msghdr_set_utf8(app->edit_hdr, HDR_OADDR, aka_buf);

                /* Regenerate kludges with new AKA; re-read MSGID for replies */
                if (app->saved_kludges)
                {
                    free(app->saved_kludges);
                    app->saved_kludges = NULL;
                }

                reply_msgid[0] = '\0';

                if (app->edit_is_reply && app->edit_reply_to_msgnum > 0)
                {
                    UiSession *s = &app->sess;
                    /* Use a temp buffer for the detected charset so we
                     * don't clobber the user-chosen edit_charset every time
                     * the AKA is changed (this is just re-reading the orig
                     * message to extract its MSGID, not changing the user's
                     * save-charset preference) */
                    char detected[CHARSET_NAME_MAX];
                    char *body_utf8;

                    detected[0] = '\0';
                    body_utf8 = wrapper_read_utf8_ex(&s->jam, app->edit_reply_to_msgnum, app->view_charset[0] ? app->view_charset : NULL, NULL, detected, sizeof(detected));

                    if (body_utf8)
                    {
                        const char *mid = ftn_find_msgid(body_utf8);

                        if (mid)
                        {
                            int i = 0;

                            while (mid[i] && mid[i] != '\r' && mid[i] != '\n' && i < (int)sizeof(reply_msgid) - 1)
                            {
                                reply_msgid[i] = mid[i];
                                i++;
                            }

                            reply_msgid[i] = '\0';
                        }

                        free(body_utf8);
                    }
                }

                daddr_aka = editor_daddr_for_intl(app, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR));
                app->saved_kludges = editor_build_kludge_block(app->cfg, aka_buf, daddr_aka, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR), reply_msgid[0] ? reply_msgid : NULL, ae->type == AREATYPE_NETMAIL);
                ui_status(app, "AKA set to %s", aka_buf);

                continue;
            }
        } /* end F4 / Ctrl+A handler */

        /* ESC in header: jump to body. ESC in body / F10: confirm quit */
        if ((!is_key && ch == 27) || (is_key && ch == KEY_F(10)))
        {
            EdInfo info;

            if (!is_key && ch == 27 && app->edit_active_field != EF_BODY)
            {
                app->edit_active_field = EF_BODY;
                continue;
            }

            /* Exit search mode first before confirming quit */
            if (app->edit_search.is_mode || app->edit_search.only_mode)
            {
                reset_search(app);
                ui_status(app, "Search mode exited");
                continue;
            }

            ed_get_info(app->editor, &info);

            if (info.modified)
            {
                int r = ui_popup_confirm("Cancel", "Discard changes?");

                if (r != 1)
                    continue;
            }

            if (!app->edit_charset_manually_changed)
            {
                strncpy(app->edit_charset, app->edit_charset_saved, sizeof(app->edit_charset) - 1);
                app->edit_charset[sizeof(app->edit_charset) - 1] = '\0';
            }

            /* Free search matches */
            reset_search(app);

            curs_set(0);
            BRACKET_PASTE_OFF();

            return app->edit_return_view;
        }

        /* TAB/S-TAB/Ctrl+P/Ctrl+N cycle header fields (in body: TAB inserts tab) */
        if ((is_key && ch == KEY_STAB) || (is_key && ch == KEY_TAB && app->edit_active_field != EF_BODY) ||
            (!is_key && ch == CTRL('P')) ||
            (!is_key && ch == '\t' && app->edit_active_field != EF_BODY) ||
            (!is_key && ch == CTRL('N')))
        {
            int dir = ((is_key && ch == KEY_STAB) || (!is_key && ch == CTRL('P'))) ? -1 : 1;
            editor_cycle_field(app, dir);
            continue;
        }

        if (is_key && ch == KEY_RESIZE)
        {
            msghdr_resize(app->edit_hdr, COLS);
            continue;
        }

        /* Paste: internal block buffer first, then system clipboard */
        if ((!is_key && ch == CTRL('V')) || (is_key && ch == KEY_SIC))
        {
            /* Fall back to system clipboard */
            char *clip = clipboard_paste();

            /* Try internal block (filled by Ctrl+C/X) first */
            if (app->edit_active_field == EF_BODY && ed_block_paste(app->editor) == 0)
            {
                reset_search(app);
                ui_status(app, "Pasted");
                continue;
            }

            if (!clip || !clip[0])
            {
                ui_status(app, "Clipboard: empty or no backend (install xclip/wl-clipboard, or check clipboard.device)");
                free(clip);

                continue;
            }

            deliver_paste(app, clip);
            free(clip);

            continue;
        }

        if (is_key && ch == KEY_PASTE_START)
        {
            char *buf = collect_bracketed_paste();

            if (buf)
            {
                deliver_paste(app, buf);
                free(buf);
            }
            else
            {
                ui_status(app, "Paste cancelled");
            }

            continue;
        }

        /* F5 and Alt+S : forward search with results list (case-insensitive) */
        if ((is_key && ch == KEY_F(5)) || (is_key && ch == KEY_ALT('S')))
        {
            if (app->edit_search.is_mode)
            {
                replace_current(app);
                continue;
            }
            else
            {
                wchar_t tmp[64];
                int *rows = NULL, *cols = NULL;
                int match_count;
                int i;
                const char **contexts = NULL;
                char **context_bufs = NULL;
                int *line_nums = NULL;

                wcsncpy(tmp, app->edit_search.query, 63);
                tmp[63] = L'\0';

                if (ui_popup_input("Search", "Text:", tmp, 64) == 0 && tmp[0])
                {
                    wcsncpy(app->edit_search.query, tmp, 63);
                    app->edit_search.query[63] = L'\0';

                    /* Find all matches (no limit) */
                    match_count = ed_search_all(app->editor, app->edit_search.query, &rows, &cols);

                    /* Free previous search matches */
                    reset_search(app);

                    if (match_count == 0)
                    {
                        ui_status(app, "Not found");
                    }
                    else if (match_count == 1)
                    {
                        /* Single match: jump directly */
                        ed_set_pos(app->editor, rows[0], cols[0]);
                        ed_ensure_visible(app->editor);
                        ui_status(app, "Found at line %d", rows[0] + 1);

                        /* Save matches for highlighting and activate search mode */
                        app->edit_search.rows = rows;
                        app->edit_search.cols = cols;
                        app->edit_search.match_count = match_count;
                        app->edit_search.only_mode = 1;
                        app->edit_search.current_match = 0;
                        app->edit_search.match_current = 1;
                    }
                    else
                    {
                        /* Allocate arrays for display */
                        contexts = (const char **)malloc((size_t)match_count * sizeof(const char *));
                        context_bufs = (char **)malloc((size_t)match_count * sizeof(char *));
                        line_nums = (int *)malloc((size_t)match_count * sizeof(int));

                        if (!contexts || !context_bufs || !line_nums)
                        {
                            if (contexts)
                                free(contexts);

                            if (context_bufs)
                                free(context_bufs);

                            if (line_nums)
                                free(line_nums);

                            free(rows);
                            free(cols);
                            ui_status(app, "Memory error");

                            continue;
                        }

                        /* Multiple matches: show list */
                        for (i = 0; i < match_count; i++)
                        {
                            const wchar_t *line = ed_line_wcs(app->editor, rows[i]);
                            int line_len = ed_line_len(app->editor, rows[i]);
                            int context_len = line_len - cols[i];
                            int copy_len = context_len;

                            if (copy_len > 60)
                                copy_len = 60;

                            if (copy_len < 0)
                                copy_len = 0;

                            context_bufs[i] = (char *)malloc(128);

                            if (!context_bufs[i])
                            {
                                context_bufs[i] = NULL;
                                contexts[i] = "";
                            }
                            else
                            {
                                /* Convert context to UTF-8 for display */
                                if (line && copy_len > 0)
                                {
                                    char *utf8 = wcs_to_utf8(&line[cols[i]], copy_len);

                                    if (utf8)
                                    {
                                        snprintf(context_bufs[i], 128, "%s", utf8);
                                        free(utf8);
                                    }
                                    else
                                    {
                                        context_bufs[i][0] = '\0';
                                    }
                                }
                                else
                                {
                                    context_bufs[i][0] = '\0';
                                }

                                contexts[i] = context_bufs[i];
                            }

                            line_nums[i] = rows[i] + 1; /* 1-based for display */
                        }

                        /* Show popup with results */
                        int choice = ui_popup_search_results("Search Results", line_nums, contexts, match_count, 0);

                        if (choice >= 0)
                        {
                            ed_set_pos(app->editor, rows[choice], cols[choice]);
                            ed_ensure_visible(app->editor);
                            app->edit_search.current_match = choice;
                            app->edit_search.match_current = choice + 1;
                            ui_status(app, "Jumped to line %d", rows[choice] + 1);
                        }
                        else
                        {
                            ui_status(app, "Search cancelled");
                        }

                        /* Free allocated memory */
                        for (i = 0; i < match_count; i++)
                        {
                            if (context_bufs[i])
                                free(context_bufs[i]);
                        }

                        free(context_bufs);
                        free(contexts);
                        free(line_nums);

                        /* Save matches for highlighting and activate search mode (don't free rows/cols) */
                        app->edit_search.rows = rows;
                        app->edit_search.cols = cols;
                        app->edit_search.match_count = match_count;
                        app->edit_search.only_mode = 1;
                        app->edit_search.current_match = 0;
                        app->edit_search.match_current = 1;
                    }
                }

                continue;
            }
        }

        /* F6: toggle block anchor at cursor */
        if ((is_key && ch == KEY_F(6)) || (ch == KEY_ALT('B')))
        {
            if (app->edit_search.is_mode)
            {
                replace_all(app);
                continue;
            }
            else
            {
                ed_block_anchor(app->editor);
                continue;
            }
        }

        /* F7: insert file at cursor. Uses file browser (ui_files_pick)
         * then asks for source charset so files saved in CP437 / LATIN-1 /
         * etc. import cleanly into the editor's UTF-8 buffer. Empty
         * charset = "the file is already UTF-8" */
        if ((is_key && ch == KEY_F(7)) || (ch == KEY_ALT('O')))
        {
            char path[512];
            char in_cs[CHARSET_NAME_MAX];

            path[0] = '\0';
            in_cs[0] = '\0';

            if (ui_files_pick("Insert file", NULL, path, sizeof(path)) != 0)
                continue;

            if (ui_popup_charset("Input charset", "", in_cs, sizeof(in_cs)) != 0)
                continue;

            if (ed_load_file_at_cursor(app->editor, path, in_cs[0] ? in_cs : NULL) == 0)
            {
                reset_search(app);
                ui_status(app, "Inserted %s (%s)", path, in_cs[0] ? in_cs : "UTF-8");
            }
            else
                ui_status(app, "Cannot read %s", path);

            continue;
        }

        /* F8: view kludges (read-only popup) */
        if ((is_key && ch == KEY_F(8)) || (ch == KEY_ALT('K')))
        {
            editor_kludge_popup(app);
            continue;
        }

        /* F9: attribute flags toggle */
        if ((is_key && ch == KEY_F(9)) || (ch == KEY_ALT('A')))
        {
            editor_attr_popup(app);
            continue;
        }

        /* Alt-M: goto line */
        if (ch == KEY_ALT('M'))
        {
            wchar_t wbuf[16];
            wbuf[0] = L'\0';

            if (ui_popup_input("Goto line", "Line number (1..):", wbuf, 16) == 0 && wbuf[0])
            {
                char *u = wcs_to_utf8(wbuf, (int)wcslen(wbuf));
                int n = 0;

                if (u)
                {
                    n = atoi(u);
                    free(u);
                }

                if (n >= 1)
                    ed_goto_line(app->editor, n - 1);
            }

            continue;
        }

        /* Alt-G: clear search highlights */
        if (ch == KEY_ALT('G'))
        {
            reset_search(app);
            ui_status(app, "Search highlights cleared");

            continue;
        }

        /* ESC: exit search mode */
        if (ch == 27)
        {
            if (app->edit_search.is_mode || app->edit_search.only_mode)
            {
                reset_search(app);
                ui_status(app, "Search mode exited");
                continue;
            }
            break;
        }

        /* Ctrl-C: block copy */
        if (ch == CTRL('C'))
        {
            EdInfo info;
            ed_get_info(app->editor, &info);

            if (info.block.active)
            {
                char *block_utf8 = ed_block_get_utf8(app->editor);

                if (ed_block_copy(app->editor) == 0)
                {
                    if (block_utf8)
                    {
                        clipboard_copy(block_utf8);
                        free(block_utf8);
                    }

                    ui_status(app, "Block copied");
                }
                else
                    free(block_utf8);
            }

            continue;
        }

        /* Ctrl-X: block cut */
        if (ch == CTRL('X'))
        {
            EdInfo info;
            ed_get_info(app->editor, &info);

            if (info.block.active)
            {
                char *block_utf8 = ed_block_get_utf8(app->editor);

                if (ed_block_cut(app->editor) == 0)
                {
                    reset_search(app);

                    if (block_utf8)
                    {
                        clipboard_copy(block_utf8);
                        free(block_utf8);
                    }

                    ui_status(app, "Block cut");
                }
                else
                    free(block_utf8);
            }

            continue;
        }

        /* Ctrl-W: rewrap paragraph to autowrap column */
        if (ch == CTRL('W'))
        {
            ed_save_undo(app->editor);

            if (ed_rewrap_paragraph(app->editor, app->cfg->autowrap_col > 0 ? app->cfg->autowrap_col : 75) == 0)
            {
                reset_search(app);
                ui_status(app, "Paragraph rewrapped");
            }

            continue;
        }

        /* Ctrl-R: interactive find-and-replace with case/whole-word options */
        if (ch == CTRL('R'))
        {
            replace(app);
            continue;
        }

        /* Ctrl-O: export block to file */
        if (ch == CTRL('O'))
        {
            EdInfo info;
            ed_get_info(app->editor, &info);

            if (!info.block.active)
            {
                ui_status(app, "No block marked (F6 to mark)");
            }
            else
            {
                char path[256];
                char out_cs[CHARSET_NAME_MAX];

                path[0] = '\0';
                out_cs[0] = '\0';

                /*if (ui_popup_input("Export block", "Path:", path, sizeof(path)) != 0 || !path[0])
                    continue;*/

                if (ui_files_save("Export block", NULL, "block_export.txt", path, sizeof(path)) != 0)
                    continue;

                /* Charset of the output file. Empty == AUTO == write
                 * UTF-8 verbatim; the user can type "CP437" etc */
                if (ui_popup_charset("Output charset", "", out_cs, sizeof(out_cs)) != 0)
                    continue;

                if (ed_export_block_to_file(app->editor, path, out_cs[0] ? out_cs : NULL) == 0)
                    ui_status(app, "Block written to %s (%s)", path, out_cs[0] ? out_cs : "UTF-8");
                else
                    ui_status(app, "Cannot write %s", path);
            }

            continue;
        }

        /* Ctrl+F: Add attachment */
        if (ch == CTRL('F'))
        {
            ui_popup_attach_add(app);
            continue;
        }

        /* Field-specific input */
        if (app->edit_active_field == EF_FROM || app->edit_active_field == EF_TO || app->edit_active_field == EF_SUBJECT || app->edit_active_field == EF_DADDR)
        {
            int hdrfld = (app->edit_active_field == EF_FROM)      ? HDR_FROM
                         : (app->edit_active_field == EF_TO)      ? HDR_TO
                         : (app->edit_active_field == EF_SUBJECT) ? HDR_SUBJECT
                                                                  : HDR_DADDR;

            if (msghdr_edit_field(app->edit_hdr) != hdrfld)
                msghdr_edit_start(app->edit_hdr, hdrfld);

            /* UP/DOWN navigate header fields */
            if (ch == KEY_DOWN || ch == CTRL('N'))
            {
                editor_cycle_field(app, 1);
                continue;
            }

            if (ch == KEY_UP || ch == CTRL('P'))
            {
                editor_cycle_field(app, -1);
                continue;
            }

            if (is_key)
            {
                switch (ch)
                {
                case KEY_ENTER:
                    msghdr_edit_stop(app->edit_hdr);
                    app->edit_active_field = EF_BODY;
                    continue;
                case KEY_BACKSPACE:
                    msghdr_edit_key(app->edit_hdr, HDR_KEY_BS);
                    break;
                case KEY_DC:
                    msghdr_edit_key(app->edit_hdr, HDR_KEY_DEL);
                    break;
                case KEY_LEFT:
                    msghdr_edit_key(app->edit_hdr, HDR_KEY_LEFT);
                    break;
                case KEY_RIGHT:
                    msghdr_edit_key(app->edit_hdr, HDR_KEY_RIGHT);
                    break;
                case KEY_HOME:
                    msghdr_edit_key(app->edit_hdr, HDR_KEY_HOME);
                    break;
                case KEY_END:
                    msghdr_edit_key(app->edit_hdr, HDR_KEY_END);
                    break;
                default:
                    break;
                }
            }
            else
            {
                switch (ch)
                {
                case '\n':
                case '\r':
                    msghdr_edit_stop(app->edit_hdr);
                    app->edit_active_field = EF_BODY;
                    continue;
                case 127:
                case 8:
                    msghdr_edit_key(app->edit_hdr, HDR_KEY_BS);
                    break;
                case CTRL('B'): /* Ctrl+B: Home (Amiga compatibility) */
                    msghdr_edit_key(app->edit_hdr, HDR_KEY_HOME);
                    break;
                case CTRL('E'): /* Ctrl+E: End (Amiga compatibility) */
                    msghdr_edit_key(app->edit_hdr, HDR_KEY_END);
                    break;
                /* TAB handled by global handler above */
                default:
                    /* Regular character input */
                    if (ch >= 32 && ch < 127)
                        msghdr_edit_key(app->edit_hdr, ch);
                    break;
                }
            }

            continue;
        }

        /* Body input: separate special-key and printable paths to avoid spurious KEY_* matches */
        soft_active = !(app->cfg && app->cfg->hard_wrap);
        body_width = COLS < 1 ? 1 : COLS;
        preserve_desired = 0;

        ae_body = &app->areas->entries[app->sess.area_idx];
        srow = (ae_body->type == AREATYPE_NETMAIL) ? 8 : 7;

        body_rows = LINES - srow - 1;

        if (body_rows < 1)
            body_rows = 1;

        if (is_key)
        {
            switch (ch)
            {
            case KEY_UP:
                if (soft_active)
                {
                    soft_move_up_visual(app, body_width);
                    preserve_desired = 1;
                }
                else
                    ed_move_up(app->editor);
                break;
            case KEY_DOWN:
                if (soft_active)
                {
                    soft_move_down_visual(app, body_width);
                    preserve_desired = 1;
                }
                else
                    ed_move_down(app->editor);
                break;
            case KEY_LEFT:
                ed_move_left(app->editor);
                break;
            case KEY_RIGHT:
                ed_move_right(app->editor);
                break;
            case KEY_HOME:
                if (soft_active)
                    soft_move_home_visual(app, body_width);
                else
                    ed_move_home(app->editor);
                break;
            case KEY_END:
                if (soft_active)
                    soft_move_end_visual(app, body_width);
                else
                    ed_move_end(app->editor);
                break;
            case KEY_PPAGE:
                if (soft_active)
                {
                    soft_move_pgup_visual(app, body_width, body_rows);
                    preserve_desired = 1;
                }
                else
                    ed_move_pgup(app->editor, 0);
                break;
            case KEY_NPAGE:
                if (soft_active)
                {
                    soft_move_pgdn_visual(app, body_width, body_rows);
                    preserve_desired = 1;
                }
                else
                    ed_move_pgdn(app->editor, 0);
                break;
            case KEY_ENTER:
                ed_save_undo(app->editor);
                ed_enter(app->editor);
                reset_search(app);
                break;
            case KEY_BACKSPACE:
            {
                EdInfo info;
                ed_get_info(app->editor, &info);

                if (info.block.active)
                {
                    /* Delete selected block (no clipboard copy) */
                    ed_save_undo(app->editor);
                    ed_block_delete(app->editor);
                    reset_search(app);
                    ui_status(app, "Block deleted");
                }
                else
                {
                    /* Backspace single character */
                    ed_save_undo(app->editor);
                    ed_backspace(app->editor);
                    reset_search(app);
                }

                break;
            }
            case KEY_DC:
            {
                EdInfo info;
                ed_get_info(app->editor, &info);

                if (info.block.active)
                {
                    /* Delete selected block (no clipboard copy) */
                    ed_save_undo(app->editor);
                    ed_block_delete(app->editor);
                    reset_search(app);
                    ui_status(app, "Block deleted");
                }
                else
                {
                    /* Delete single character */
                    ed_save_undo(app->editor);
                    ed_delete(app->editor);
                    reset_search(app);
                }

                break;
            }
            case KEY_IC: /* Insert: toggle insert/overwrite */
            case KEY_ALT('I'):
                ed_toggle_insert(app->editor);
                break;
            case KEY_CLEFT: /* Control+Left: word left */
                ed_word_left(app->editor);
                break;
            case KEY_CRIGHT: /* Control+Right: word right */
                ed_word_right(app->editor);
                break;
            /* Alt-key chords: KEY_ALT() from shim (Amiga) or wrapper_read_key() fold (Linux) */
            case KEY_ALT('L'):
                ui_popup_attach_clear(app);
                break;
            case KEY_ALT('Z'): /* Alt+Z: redo */
                ui_status(app, "Alt+Z detected - trying redo");
                ed_redo(app->editor);
                reset_search(app);
                break;
            default:
                break;
            }
        }
        else
        {
            switch (ch)
            {
            case '\n':
            case '\r':
                ed_save_undo(app->editor);
                ed_enter(app->editor);
                reset_search(app);
                break;
            case 8:
            case 127:
                ed_save_undo(app->editor);
                ed_backspace(app->editor);
                reset_search(app);
                break;
            case CTRL('B'): /* Ctrl+B: Home (Amiga compatibility) */
                if (soft_active)
                    soft_move_home_visual(app, body_width);
                else
                    ed_move_home(app->editor);
                break;
            case CTRL('E'): /* Ctrl+E: End (Amiga compatibility) */
                if (soft_active)
                    soft_move_end_visual(app, body_width);
                else
                    ed_move_end(app->editor);
                break;
            case CTRL('U'): /* Ctrl+U: Page Up (Amiga compatibility) */
                if (soft_active)
                {
                    soft_move_pgup_visual(app, body_width, body_rows);
                    preserve_desired = 1;
                }
                else
                    ed_move_pgup(app->editor, 0);
                break;
            case CTRL('D'): /* Ctrl+D: Page Down (Amiga compatibility) */
                if (soft_active)
                {
                    soft_move_pgdn_visual(app, body_width, body_rows);
                    preserve_desired = 1;
                }
                else
                    ed_move_pgdn(app->editor, 0);
                break;
            case CTRL('Y'): /* Ctrl+Y: delete line */
                ed_save_undo(app->editor);
                ed_delete_line(app->editor);
                reset_search(app);
                break;
            case CTRL('Z'): /* Ctrl+Z: undo */
                ed_undo(app->editor);
                reset_search(app);
                break;
            case CTRL('T'): /* Ctrl+T: delete word right */
                ed_save_undo(app->editor);
                ed_delete_word_right(app->editor);
                reset_search(app);
                break;
            case CTRL('_'): /* Ctrl+_: delete word left */
                ed_save_undo(app->editor);
                ed_delete_word_left(app->editor);
                reset_search(app);
                break;
            case CTRL('Q'): /* Ctrl+Q: Remove attachment */
                ui_popup_attach_remove(app);
                break;
            case CTRL('L'): /* Ctrl+L: List attachments */
                ui_popup_attach_list(app);
                break;
            case CTRL('G'): /* Go to start of document */
                ed_set_pos(app->editor, 0, 0);
                ed_ensure_visible(app->editor);
                break;
            case CTRL('K'): /* Go to end of document */
            {
                EdInfo info;
                ed_get_info(app->editor, &info);

                if (info.line_count > 0)
                {
                    int last_line = info.line_count - 1;
                    int last_len = ed_line_len(app->editor, last_line);

                    ed_set_pos(app->editor, last_line, last_len);
                    ed_ensure_visible(app->editor);
                }

                break;
            }
            /* ESC handled by global handler above */
            case '\t':
                ed_save_undo(app->editor);
                ed_insert_tab(app->editor, 4);
                reset_search(app);
                break;
            default:
                /* Printable wide character: insert into body */
                if (wch >= 0x20 && wch != 127)
                {
                    ed_save_undo(app->editor);

                    /* SOFT-WRAP: If cursor is at end of visual segment, move to start of next segment BEFORE inserting */
                    if (!(app->cfg && app->cfg->hard_wrap) && body_width > 0)
                    {
                        EdInfo info;
                        const wchar_t *line;
                        int len;
                        int pos = 0;
                        int end, np;

                        ed_get_info(app->editor, &info);
                        line = ed_line_wcs(app->editor, info.row);
                        len = ed_line_len(app->editor, info.row);

                        if (line && len > 0)
                        {
                            /* Find the segment where the cursor is located */
                            while (pos < len)
                            {
                                end = wrap_next(line, len, body_width, pos);
                                /*np = wrap_skip_spaces(line, len, end);*/
                                np = pos;

                                if (info.col >= pos && info.col < np)
                                {
                                    /* Cursor is in this segment */
                                    if (info.col >= end && np < len)
                                    {
                                        /* Cursor is at end of segment: move to start of next segment */
                                        ed_set_pos(app->editor, info.row, np);
                                    }
                                    break;
                                }

                                if (np <= pos)
                                    np = pos + (body_width < 1 ? 1 : body_width);

                                pos = np;
                            }
                        }
                    }

                    ed_insert_char(app->editor, (wchar_t)wch);
                    reset_search(app);

                    /* HARD-WRAP only: insert CR at wrap col; soft-wrap leaves line intact */
                    if (app->cfg && app->cfg->hard_wrap && eff_wrap > 0)
                    {
                        EdInfo wi;
                        int linelen;

                        ed_get_info(app->editor, &wi);
                        linelen = ed_line_len(app->editor, wi.row);

                        if (wi.col > eff_wrap && wi.col == linelen)
                        {
                            const wchar_t *line = ed_line_wcs(app->editor, wi.row);
                            int brk = -1;
                            int k;

                            if (line)
                            {
                                int limit = eff_wrap;

                                if (limit > linelen)
                                    limit = linelen;

                                for (k = limit; k > 0; k--)
                                {
                                    if (line[k - 1] == L' ')
                                    {
                                        brk = k - 1;
                                        break;
                                    }
                                }
                            }

                            if (wch == L' ')
                            {
                                ed_backspace(app->editor); /* replace trailing space with newline */
                                ed_enter(app->editor);
                                reset_search(app);
                            }
                            else if (brk >= 0)
                            {
                                int tail = linelen - brk - 1;

                                ed_set_pos(app->editor, wi.row, brk);
                                ed_delete(app->editor);
                                ed_enter(app->editor);
                                reset_search(app);
                                ed_set_pos(app->editor, wi.row + 1, tail);
                            }
                        }
                    }
                }
                break;
            }
        }

        /* Vertical moves (UP/DOWN/PgUp/PgDn) carry the visual goal
         * column across consecutive presses. Anything else - typing,
         * deletes, horizontal moves - invalidates it, so the next
         * vertical move re-syncs from the cursor's actual visual col */
        if (!preserve_desired)
            soft_reset_desired();
    }
}