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
#include "ui_editor_softwrap.h"
#include "ui_editor_search.h"
#include "ui_editor_paste.h"
#include "ui_editor_draw.h"
#include "ui_spell.h"
#ifdef HAVE_MYTHES
#include "ui_thes.h"
#endif
#ifdef HAVE_HYPHEN
#include "../core/hyph.h"
#include "ui_hyph.h"
#endif
#include "ui_editor_helper.h"
#include "ui_aka.h"
#include "ui_attr.h"
#include "ui_files.h"
#include "ui_glyph_picker.h"
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
        "    Ctrl+Left/Right Word movement",
        "    Ctrl-W          Rewrap paragraph",
        "    Alt+Q           Toggle wrap mode",
        "    Alt+D           Toggle line numbers",
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
        "    Alt+G           Goto line",
        "    Ctrl-G          Go to start of document",
        "    Ctrl-K          Go to end of document",
        "    F7 Alt+O        Insert file",
        "    F8 Alt+K        Kludges (Enter del)",
        "",
        "  Spell checker:",
        "    Alt+E           Toggle spell panel",
        "    Alt+T           Toggle spell active",
        "    Alt+W           Spell check word",
#ifdef HAVE_MYTHES
        "    Alt+J           Thesaurus / synonyms",
#endif
#ifdef HAVE_HYPHEN
        "    Alt+L           Toggle hyph-wrap (hard-wrap only)",
#endif
        "",
        "  Attachments:",
        "    Alt+A           Add attachment (file)",
        "    Alt+X           Remove attachment",
        "    Alt+M           List attachments",
        "    Alt+C           Clear all attachments",
        "",
        "  Header / send:",
        "    F2 Ctrl-S       Save",
        "    F3 Alt+H        Charset",
        "    F4 Ctrl-A       AKA (netmail)",
        "    F9 Alt+R        Attr (Priv/Crash/Hold)",
        "    F10 Alt+P       Nodelist picker",
#ifdef PLATFORM_AMIGA
        "    Alt+V           Nodelist browser",
#else
        "    F11 Alt+V       Nodelist browser",
#endif
        "    Alt+U           Glyph Picker",
        "    ESC             Cancel (confirm)",
        "    F1 Alt+Y        This help"};
#define EDITOR_HELP_N ((int)(sizeof(EDITOR_HELP) / sizeof(EDITOR_HELP[0])))

/* Cycle header field in direction dir (+1=next, -1=prev), skips DADDR for non-netmail */
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

/* Handle function keys (F1-F11), returns 1 if handled, 0 otherwise */
static int handle_function_keys(UiApp *app, int ch, int is_key)
{
    /* F1 / Alt+Y: help */
    if ((is_key && ch == KEY_F(1)) || (is_key && ch == KEY_ALT('Y')))
    {
        ui_popup_help("Editor Help", EDITOR_HELP, EDITOR_HELP_N);
        return 1;
    }

    /* F2 / Ctrl+S : save */
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

            return 2; /* Special return: exit editor */
        }
        return 1;
    }

    /* F3 / Alt+H / Alt+P : charset OR Previous match in search mode */
    if ((is_key && ch == KEY_F(3)) || (is_key && ch == KEY_ALT('H')) || (is_key && ch == KEY_ALT('P')))
    {
        if (app->edit_search.is_mode || app->edit_search.only_mode)
        {
            if (search_prev_editor(app))
                return 1;
        }
        else
        {
            /* Alt+H: charset picker */
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
        }
        return 1;
    }

    /* F4 / Ctrl+A / Alt+N : AKA picker OR Next match in search mode */
    if ((is_key && ch == KEY_F(4)) || (!is_key && ch == CTRL('A')) || (is_key && ch == KEY_ALT('N')))
    {
        if (((is_key && ch == KEY_ALT('N')) || (is_key && ch == KEY_F(4))) && (app->edit_search.is_mode || app->edit_search.only_mode))
        {
            if (search_next_editor(app))
                return 1;
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
                return 1;
            }

            sel = ui_popup_aka(app, app->edit_aka_idx);

            if (sel < 0)
                return 1;

            picked = ui_aka_at(app->areas, app->cfg, sel);

            if (!picked)
                return 1;

            strncpy(aka_buf, picked, CFG_AKA_MAX - 1);
            aka_buf[CFG_AKA_MAX - 1] = '\0';

            app->edit_aka_idx = sel;
            msghdr_set_utf8(app->edit_hdr, HDR_OADDR, aka_buf);

            /* Regenerate kludges with new AKA, re-read MSGID for replies */
            if (app->saved_kludges)
            {
                free(app->saved_kludges);
                app->saved_kludges = NULL;
            }

            reply_msgid[0] = '\0';

            if (app->edit_is_reply && app->edit_reply_to_msgnum > 0)
            {
                UiSession *s = &app->sess;
                char detected[CHARSET_NAME_MAX];
                char *body_utf8 = NULL;

                detected[0] = '\0';
                body_utf8 = wrapper_read_utf8_ex(&s->mb, app->edit_reply_to_msgnum, app->view_charset[0] ? app->view_charset : NULL, NULL, detected, sizeof(detected));

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
        }

        return 1;
    }

    /* F5 / Alt+S : search */
    if ((is_key && ch == KEY_F(5)) || (is_key && ch == KEY_ALT('S')))
    {
        if (app->edit_search.is_mode)
            replace_current(app);

        return 1;
    }

    /* F6 / Alt+B : toggle block anchor OR Replace All in search mode */
    if ((is_key && ch == KEY_F(6)) || (ch == KEY_ALT('B')))
    {
        if (app->edit_search.is_mode)
            replace_all(app);

        else
            ed_block_anchor(app->editor);

        return 1;
    }

    /* F7 / Alt+O : insert file */
    if ((is_key && ch == KEY_F(7)) || (ch == KEY_ALT('O')))
    {
        char path[512];
        char in_cs[CHARSET_NAME_MAX];

        path[0] = '\0';
        in_cs[0] = '\0';

        if (ui_files_pick("Insert file", NULL, path, sizeof(path)) != 0)
            return 1;

        if (ui_popup_charset("Input charset", "", in_cs, sizeof(in_cs)) != 0)
            return 1;

        if (ed_load_file_at_cursor(app->editor, path, in_cs[0] ? in_cs : NULL) == 0)
        {
            reset_search(app);
            ui_status(app, "Inserted %s (%s)", path, in_cs[0] ? in_cs : "UTF-8");
        }
        else
            ui_status(app, "Cannot read %s", path);

        return 1;
    }

    /* F8 / Alt+K : view kludges */
    if ((is_key && ch == KEY_F(8)) || (ch == KEY_ALT('K')))
    {
        editor_kludge_popup(app);
        return 1;
    }

    /* F9 / Alt+R : attribute flags toggle */
    if ((is_key && ch == KEY_F(9)) || (ch == KEY_ALT('R')))
    {
        editor_attr_popup(app);
        return 1;
    }

    /* Alt+Q : toggle wrapmode */
    if (ch == KEY_ALT('Q'))
    {
        app->cfg->hard_wrap = !app->cfg->hard_wrap;

        return 1;
    }

    /* F10 / Alt+P : nodelist picker (header only) */
    if (is_key && (int)ch == KEY_F(10) && app->edit_active_field != EF_BODY)
    {
        char picked_name[NODELIST_NAME_MAX];
        char picked_addr[NODELIST_ADDR_MAX];

        picked_name[0] = '\0';
        picked_addr[0] = '\0';

        if (ui_popup_nodelist(app, 1, picked_name, sizeof(picked_name), picked_addr, sizeof(picked_addr)))
        {
            if (picked_name[0])
                msghdr_set_utf8(app->edit_hdr, HDR_TO, picked_name);

            /* Destination address is meaningful only on netmail */
            if (ui_is_netmail(app) && picked_addr[0])
                msghdr_set_utf8(app->edit_hdr, HDR_DADDR, picked_addr);

            ui_status(app, "Recipient set: %s%s%s", picked_name, picked_addr[0] ? "  " : "", picked_addr[0] ? picked_addr : "");
        }

        return 1;
    }

    /* F11 / Alt+V : nodelist browser */
    if ((is_key && (int)ch == KEY_F(11)) || (is_key && ch == KEY_ALT('V')))
    {
        ui_popup_nodelist(app, 0, NULL, 0, NULL, 0);
        return 1;
    }

    return 0;
}

#ifdef HAVE_HYPHEN
/* Thunk: adapt hyph_breakpoints to EdHyphenFn signature */
static int rewrap_hyph_thunk(void *user_data, const char *word, int word_len, int *out_pos, int *out_count)
{
    HyphDict *h = (HyphDict *)user_data;
    int n = 0;

    if (!out_count)
        return 0;

    if (!hyph_breakpoints(h, word, word_len, out_pos, &n) || n == 0)
    {
        *out_count = 0;
        return 0;
    }

    *out_count = n;
    return 1;
}
#endif

/* Handle control key combinations (Ctrl+...), returns 1 if handled, 0 otherwise */
static int handle_control_keys(UiApp *app, int ch, int is_key)
{
    /* Ctrl+V : paste */
    if (!is_key && ch == CTRL('V'))
    {
        char *clip = NULL;

        /* On Amiga/Windows, always use external clipboard. On Unix, use external only if not in SSH session */
        if (clipboard_use_external())
        {
            clip = clipboard_paste();

            if (!clip || !clip[0])
            {
                ui_status(app, "Clipboard: empty or no backend (install xclip/wl-clipboard, or check clipboard.device)");
                free(clip);

                return 1;
            }

            deliver_paste(app, clip);
            free(clip);

            return 1;
        }
        else
        {
            /* SSH/headless: use internal block only */
            if (app->edit_active_field == EF_BODY && ed_block_paste(app->editor) == 0)
            {
                reset_search(app);
                ui_status(app, "Pasted (internal block)");
                return 1;
            }
            else
            {
                ui_status(app, "No internal block to paste (external clipboard unavailable in SSH)");
                return 1;
            }
        }
    }

    /* Ctrl+C : block copy */
    if (!is_key && ch == CTRL('C'))
    {
        EdInfo info;
        ed_get_info(app->editor, &info);

        if (info.block.active)
        {
            char *block_utf8 = ed_block_get_utf8(app->editor);

            if (ed_block_copy(app->editor) == 0)
            {
                /* Copy to external clipboard if available */
                if (clipboard_use_external() && block_utf8)
                {
                    clipboard_copy(block_utf8);
                    ui_status(app, "Block copied to clipboard");
                }
                else
                {
                    ui_status(app, "Block copied (internal only)");
                }

                free(block_utf8);
            }
            else
                free(block_utf8);
        }

        return 1;
    }

    /* Ctrl+X : block cut */
    if (!is_key && ch == CTRL('X'))
    {
        EdInfo info;
        ed_get_info(app->editor, &info);

        if (info.block.active)
        {
            char *block_utf8 = ed_block_get_utf8(app->editor);

            if (ed_block_cut(app->editor) == 0)
            {
                reset_search(app);

                /* Copy to external clipboard if available */
                if (clipboard_use_external() && block_utf8)
                {
                    clipboard_copy(block_utf8);
                    ui_status(app, "Block cut to clipboard");
                }
                else
                {
                    ui_status(app, "Block cut (internal only)");
                }

                free(block_utf8);
            }
            else
                free(block_utf8);
        }

        return 1;
    }

    /* Ctrl+W : rewrap paragraph */
    if (!is_key && ch == CTRL('W'))
    {
        int rwrap = app->cfg->autowrap_col > 0 ? app->cfg->autowrap_col : 75;
        int rc;

#ifdef HAVE_HYPHEN
        if (app->hyph_wrap_enabled && app->hyph_handle)
            rc = ed_rewrap_paragraph_ex(app->editor, rwrap, rewrap_hyph_thunk, app->hyph_handle);
        else
#endif
            rc = ed_rewrap_paragraph(app->editor, rwrap);

        if (rc == 0)
        {
            reset_search(app);
            ui_status(app, "Paragraph rewrapped");
        }

        return 1;
    }

    /* Ctrl+R : find & replace */
    if (!is_key && ch == CTRL('R'))
    {
        replace(app);
        return 1;
    }

    /* Ctrl+O : export block to file */
    if (!is_key && ch == CTRL('O'))
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

            if (ui_files_save("Export block", NULL, "block_export.txt", path, sizeof(path)) != 0)
                return 1;

            if (ui_popup_charset("Output charset", "", out_cs, sizeof(out_cs)) != 0)
                return 1;

            if (ed_export_block_to_file(app->editor, path, out_cs[0] ? out_cs : NULL) == 0)
                ui_status(app, "Block written to %s (%s)", path, out_cs[0] ? out_cs : "UTF-8");
            else
                ui_status(app, "Cannot write %s", path);
        }

        return 1;
    }

    /* Alt+A : Add attachment */
    if (ch == KEY_ALT('A'))
    {
        ui_popup_attach_add(app);
        return 1;
    }

    /* Alt+X : Remove attachment */
    if (ch == KEY_ALT('X'))
    {
        ui_popup_attach_remove(app);
        return 1;
    }

    /* Alt+M : List attachments */
    if (ch == KEY_ALT('M'))
    {
        ui_popup_attach_list(app);
        return 1;
    }

    /* Ctrl+G : Go to start of document */
    if (!is_key && ch == CTRL('G'))
    {
        ed_set_pos(app->editor, 0, 0);
        ed_ensure_visible(app->editor);

        /* In soft-wrap mode, reset viewport to cursor to avoid slow walking */
        if (app->cfg && !app->cfg->hard_wrap)
        {
            EdInfo info;
            const wchar_t *l;
            int len;

            ed_get_info(app->editor, &info);

            l = ed_line_wcs(app->editor, info.row);
            len = ed_line_len(app->editor, info.row);
            s_soft_top_line = info.row;
            s_soft_top_sub = line_subrow_of_col(l ? l : L"", l ? len : 0, COLS, info.col);

            soft_reset_desired();
        }

        return 1;
    }

    /* Ctrl+K : Go to end of document */
    if (!is_key && ch == CTRL('K'))
    {
        EdInfo info;
        ed_get_info(app->editor, &info);

        if (info.line_count > 0)
        {
            int last_line = info.line_count - 1;
            int last_len = ed_line_len(app->editor, last_line);

            ed_set_pos(app->editor, last_line, last_len);
            ed_ensure_visible(app->editor);

            /* In soft-wrap mode, reset viewport to cursor to avoid slow walking */
            if (app->cfg && !app->cfg->hard_wrap)
            {
                const wchar_t *l;
                int len;

                l = ed_line_wcs(app->editor, last_line);
                len = ed_line_len(app->editor, last_line);
                s_soft_top_line = last_line;
                s_soft_top_sub = line_subrow_of_col(l ? l : L"", l ? len : 0, COLS, last_len);

                soft_reset_desired();
            }
        }

        return 1;
    }

    /* Ctrl+Y : delete line */
    if (!is_key && ch == CTRL('Y'))
    {
        ed_delete_line(app->editor);
        reset_search(app);

        return 1;
    }

    /* Ctrl+Z : undo */
    if (!is_key && ch == CTRL('Z'))
    {
        ed_undo(app->editor);

        if (app->cfg)
            app->cfg->hard_wrap = ed_get_hard_wrap(app->editor);

        reset_search(app);

        return 1;
    }

    /* Ctrl+T : delete word right */
    if (!is_key && ch == CTRL('T'))
    {
        ed_delete_word_right(app->editor);
        reset_search(app);

        return 1;
    }

    /* Ctrl+_ : delete word left */
    if (!is_key && ch == CTRL('_'))
    {
        ed_delete_word_left(app->editor);
        reset_search(app);

        return 1;
    }

    return 0;
}

/* Handle Alt key combinations (Alt+...), returns 1 if handled, 0 otherwise */
static int handle_alt_keys(UiApp *app, int ch, int is_key)
{
    /* Alt+E : toggle spell-check overlay panel */
    if (ch == KEY_ALT('E'))
    {
#ifdef HAVE_HUNSPELL
        return ui_spell_toggle_panel(app);
#else
        ui_status(app, "Spell support not built in");
        return 1;
#endif
    }

    /* Alt+T : toggle spell checker active */
    if (ch == KEY_ALT('T'))
    {
#ifdef HAVE_HUNSPELL
        if (app->spell_handle)
        {
            app->spell_active = !app->spell_active;
            ui_status(app, "Spell checker %s", app->spell_active ? "enabled" : "disabled");
        }
        else
        {
            ui_status(app, "No dictionary loaded");
        }

        return 1;
#else
        ui_status(app, "Spell support not built in");
        return 1;
#endif
    }

    /* Alt+W : spell-check word under cursor */
    if (ch == KEY_ALT('W'))
    {
#ifdef HAVE_HUNSPELL
        if (!app->spell_handle)
        {
            ui_status(app, "No dictionary loaded (configure SPELL_DICT_*)");
            return 1;
        }

        if (app->edit_active_field != EF_BODY)
        {
            ui_status(app, "Spell check only available in body");
            return 1;
        }

        ui_spell_check_word_at_cursor(app);
        return 1;
#else
        ui_status(app, "Spell support not built in");
        return 1;
#endif
    }

    /* Alt+G : goto line */
    if (ch == KEY_ALT('G'))
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
            {
                ed_goto_line(app->editor, n - 1);

                /* In soft-wrap mode, reset viewport to cursor to avoid slow walking */
                if (app->cfg && !app->cfg->hard_wrap)
                {
                    EdInfo info;
                    const wchar_t *l;
                    int len;

                    ed_get_info(app->editor, &info);

                    l = ed_line_wcs(app->editor, info.row);
                    len = ed_line_len(app->editor, info.row);
                    s_soft_top_line = info.row;
                    s_soft_top_sub = line_subrow_of_col(l ? l : L"", l ? len : 0, COLS, info.col);

                    soft_reset_desired();
                }
            }
        }

        return 1;
    }

    /* Alt+U : Unicode glyph picker, pop grid of glyphs, hex input */
    if (ch == KEY_ALT('U'))
    {
        long cp = ui_glyph_pick();

        if (cp >= 0)
        {
            ed_insert_char(app->editor, (wchar_t)cp);
            reset_search(app);
        }

        return 1;
    }

    /* Alt+C : clear all attachments */
    if (ch == KEY_ALT('C'))
    {
        ui_popup_attach_clear(app);
        return 1;
    }

    /* Alt+D : toggle line numbers */
    if (ch == KEY_ALT('D'))
    {
        if (app->cfg)
        {
            app->cfg->show_line_numbers = !app->cfg->show_line_numbers;
            ui_status(app, "Line numbers: %s", app->cfg->show_line_numbers ? "ON" : "OFF");
        }

        return 1;
    }

#ifdef HAVE_MYTHES
    /* Alt+J : look up synonyms for the word under cursor */
    if (ch == KEY_ALT('J'))
    {
        if (!app->thes_handle)
        {
            ui_status(app, "No thesaurus loaded (configure THES_DICT_*)");
            return 1;
        }

        if (app->edit_active_field != EF_BODY)
        {
            ui_status(app, "Thesaurus only available in body");
            return 1;
        }

        ui_thes_lookup_word(app);
        return 1;
    }
#endif

#ifdef HAVE_HYPHEN
    /* Alt+L : toggle automatic hyphenation when hard-wrap is on */
    if (ch == KEY_ALT('L'))
    {
        if (!app->hyph_handle)
        {
            ui_status(app, "No hyphenation dictionary loaded (configure HYPH_DICT_*)");
            return 1;
        }

        if (!app->cfg || !app->cfg->hard_wrap)
        {
            ui_status(app, "Hyphen wrap requires hard-wrap mode");
            return 1;
        }

        app->hyph_wrap_enabled = !app->hyph_wrap_enabled;
        ui_status(app, "Hyphenation wrap: %s", app->hyph_wrap_enabled ? "ON" : "OFF");
        return 1;
    }
#endif

    return 0;
}

/* Handle navigation keys, returns 1 if handled, preserve_desired=1 for vertical moves */
static int handle_navigation_keys(UiApp *app, int ch, int is_key, int soft_active, int body_width, int body_rows, int *preserve_desired)
{
    switch (ch)
    {
    case KEY_UP:
        if (soft_active)
        {
            soft_move_up_visual(app, body_width);
            *preserve_desired = 1;
        }
        else
            ed_move_up(app->editor);

        return 1;

    case KEY_DOWN:
        if (soft_active)
        {
            soft_move_down_visual(app, body_width);
            *preserve_desired = 1;
        }
        else
            ed_move_down(app->editor);

        return 1;

    case KEY_LEFT:
        ed_move_left(app->editor);
        return 1;

    case KEY_RIGHT:
        ed_move_right(app->editor);
        return 1;

    case KEY_HOME:
        if (soft_active)
            soft_move_home_visual(app, body_width);
        else
            ed_move_home(app->editor);

        return 1;

    case KEY_END:
        if (soft_active)
            soft_move_end_visual(app, body_width);
        else
            ed_move_end(app->editor);

        return 1;

    case KEY_PPAGE:
        if (soft_active)
        {
            soft_move_pgup_visual(app, body_width, body_rows);
            *preserve_desired = 1;
        }
        else
            ed_move_pgup(app->editor, 0);

        return 1;

    case KEY_NPAGE:
        if (soft_active)
        {
            soft_move_pgdn_visual(app, body_width, body_rows);
            *preserve_desired = 1;
        }
        else
            ed_move_pgdn(app->editor, 0);

        return 1;

    case KEY_CLEFT:
        ed_word_left(app->editor);

        return 1;

    case KEY_CRIGHT:
        ed_word_right(app->editor);

        return 1;

    default:
        return 0;
    }
}

/* Handle header field input, returns 1 if handled, 0 otherwise */
static int handle_header_input(UiApp *app, int ch, int is_key)
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

        return 1;
    }

    if (ch == KEY_UP || ch == CTRL('P'))
    {
        editor_cycle_field(app, -1);

        return 1;
    }

    if (is_key)
    {
        switch (ch)
        {
        case KEY_ENTER:
            msghdr_edit_stop(app->edit_hdr);
            app->edit_active_field = EF_BODY;
            return 1;
        case KEY_BACKSPACE:
            msghdr_edit_key(app->edit_hdr, HDR_KEY_BS);
            return 1;
        case KEY_DC:
            msghdr_edit_key(app->edit_hdr, HDR_KEY_DEL);
            return 1;
        case KEY_LEFT:
            msghdr_edit_key(app->edit_hdr, HDR_KEY_LEFT);
            return 1;
        case KEY_RIGHT:
            msghdr_edit_key(app->edit_hdr, HDR_KEY_RIGHT);
            return 1;
        case KEY_HOME:
            msghdr_edit_key(app->edit_hdr, HDR_KEY_HOME);
            return 1;
        case KEY_END:
            msghdr_edit_key(app->edit_hdr, HDR_KEY_END);
            return 1;
        default:
            return 0;
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
            return 1;
        case 127:
        case 8:
            msghdr_edit_key(app->edit_hdr, HDR_KEY_BS);
            return 1;
        case CTRL('B'):
            msghdr_edit_key(app->edit_hdr, HDR_KEY_HOME);
            return 1;
        case CTRL('E'):
            msghdr_edit_key(app->edit_hdr, HDR_KEY_END);
            return 1;
        default:
            /* Regular character input */
            if (ch >= 32 && ch < 127)
            {
                msghdr_edit_key(app->edit_hdr, ch);
                return 1;
            }

            return 0;
        }
    }
}

/* Handle body input (editing the message body), returns 1 if handled, 0 otherwise */
static int handle_body_input(UiApp *app, int ch, int is_key, wint_t wch, int soft_active, int body_width, int body_rows, int eff_wrap, int *preserve_desired)
{
    if (is_key)
    {
        switch (ch)
        {
        case KEY_UP:
            if (soft_active)
            {
                soft_move_up_visual(app, body_width);
                *preserve_desired = 1;
            }
            else
                ed_move_up(app->editor);

            return 1;

        case KEY_DOWN:
            if (soft_active)
            {
                soft_move_down_visual(app, body_width);
                *preserve_desired = 1;
            }
            else
                ed_move_down(app->editor);

            return 1;

        case KEY_LEFT:
            ed_move_left(app->editor);
            return 1;

        case KEY_RIGHT:
            ed_move_right(app->editor);
            return 1;

        case KEY_HOME:
            if (soft_active)
                soft_move_home_visual(app, body_width);
            else
                ed_move_home(app->editor);

            return 1;

        case KEY_END:
            if (soft_active)
                soft_move_end_visual(app, body_width);
            else
                ed_move_end(app->editor);

            return 1;

        case KEY_PPAGE:
            if (soft_active)
            {
                soft_move_pgup_visual(app, body_width, body_rows);
                *preserve_desired = 1;
            }
            else
                ed_move_pgup(app->editor, 0);

            return 1;

        case KEY_NPAGE:
            if (soft_active)
            {
                soft_move_pgdn_visual(app, body_width, body_rows);
                *preserve_desired = 1;
            }
            else
                ed_move_pgdn(app->editor, 0);

            return 1;

        case KEY_ENTER:
            ed_enter(app->editor);
            reset_search(app);

            return 1;

        case KEY_BACKSPACE:
        {
            EdInfo info;
            ed_get_info(app->editor, &info);

            if (info.block.active)
            {
                /* Delete selected block (no clipboard copy) */
                ed_block_delete(app->editor);
                reset_search(app);
                ui_status(app, "Block deleted");
            }
            else
            {
                /* Backspace single character */
                ed_backspace(app->editor);
                reset_search(app);
            }

            return 1;
        }

        case KEY_DC:
        {
            EdInfo info;
            ed_get_info(app->editor, &info);

            if (info.block.active)
            {
                /* Delete selected block (no clipboard copy) */
                ed_block_delete(app->editor);
                reset_search(app);
                ui_status(app, "Block deleted");
            }
            else
            {
                /* Delete single character */
                ed_delete(app->editor);
                reset_search(app);
            }

            return 1;
        }

        case KEY_IC: /* Insert: toggle insert/overwrite */
        case KEY_ALT('I'):
            ed_toggle_insert(app->editor);
            return 1;

        case KEY_CLEFT: /* Control+Left: word left */
            ed_word_left(app->editor);
            return 1;

        case KEY_CRIGHT: /* Control+Right: word right */
            ed_word_right(app->editor);
            return 1;

        /* Alt-key chords: KEY_ALT() from shim (Amiga) or wrapper_read_key() fold */
        case KEY_ALT('L'):
            ui_popup_attach_clear(app);
            return 1;

        case KEY_ALT('D'):
            if (app->cfg)
            {
                app->cfg->show_line_numbers = !app->cfg->show_line_numbers;
                ui_status(app, "Line numbers: %s", app->cfg->show_line_numbers ? "ON" : "OFF");
            }
            return 1;

        case KEY_ALT('Z'): /* Alt+Z: redo */
            ui_status(app, "Alt+Z detected - trying redo");
            ed_redo(app->editor);

            if (app->cfg)
                app->cfg->hard_wrap = ed_get_hard_wrap(app->editor);

            reset_search(app);

            return 1;

        default:
            return 0;
        }
    }
    else
    {
        switch (ch)
        {
        case '\n':
        case '\r':
            ed_enter(app->editor);
            reset_search(app);

            return 1;

        case 8:
        case 127:
            ed_backspace(app->editor);
            reset_search(app);

            return 1;

        case CTRL('B'):
            if (soft_active)
                soft_move_home_visual(app, body_width);
            else
                ed_move_home(app->editor);

            return 1;

        case CTRL('E'):
            if (soft_active)
                soft_move_end_visual(app, body_width);
            else
                ed_move_end(app->editor);

            return 1;

        case CTRL('U'):
            if (soft_active)
            {
                soft_move_pgup_visual(app, body_width, body_rows);
                *preserve_desired = 1;
            }
            else
                ed_move_pgup(app->editor, 0);

            return 1;

        case CTRL('D'):
            if (soft_active)
            {
                soft_move_pgdn_visual(app, body_width, body_rows);
                *preserve_desired = 1;
            }
            else
                ed_move_pgdn(app->editor, 0);

            return 1;

        case CTRL('Y'):
            ed_delete_line(app->editor);
            reset_search(app);

            return 1;

        case CTRL('Z'):
            ed_undo(app->editor);
            reset_search(app);

            return 1;

        case CTRL('T'):
            ed_delete_word_right(app->editor);
            reset_search(app);

            return 1;

        case CTRL('_'):
            ed_delete_word_left(app->editor);
            reset_search(app);
            return 1;

        case CTRL('Q'):
            ui_popup_attach_remove(app);
            return 1;

        case CTRL('L'):
            ui_popup_attach_list(app);
            return 1;

        case CTRL('G'):
            ed_set_pos(app->editor, 0, 0);
            ed_ensure_visible(app->editor);

            /* In soft-wrap mode, reset viewport to cursor to avoid slow walking */
            if (app->cfg && !app->cfg->hard_wrap)
            {
                const wchar_t *l;
                int len;

                l = ed_line_wcs(app->editor, 0);
                len = ed_line_len(app->editor, 0);
                s_soft_top_line = 0;
                s_soft_top_sub = line_subrow_of_col(l ? l : L"", l ? len : 0, COLS, 0);

                soft_reset_desired();
            }

            return 1;

        case CTRL('K'):
        {
            EdInfo info;
            ed_get_info(app->editor, &info);

            if (info.line_count > 0)
            {
                int last_line = info.line_count - 1;
                int last_len = ed_line_len(app->editor, last_line);

                ed_set_pos(app->editor, last_line, last_len);
                ed_ensure_visible(app->editor);

                /* In soft-wrap mode, reset viewport to cursor to avoid slow walking */
                if (app->cfg && !app->cfg->hard_wrap)
                {
                    const wchar_t *l;
                    int len;

                    l = ed_line_wcs(app->editor, last_line);
                    len = ed_line_len(app->editor, last_line);
                    s_soft_top_line = last_line;
                    s_soft_top_sub = line_subrow_of_col(l ? l : L"", l ? len : 0, COLS, last_len);

                    soft_reset_desired();
                }
            }

            return 1;
        }

        /* ESC handled by global handler above */
        case '\t':
            ed_insert_tab(app->editor, 4);
            reset_search(app);

            return 1;

        default:
            /* Printable wide character: insert into body */
            if (wch >= 0x20 && wch != 127)
            {
                /* Try rapid paste detection first (fallback for terminals without bracketed paste) */
                char *rapid_buf = collect_rapid_paste(wch);

                if (rapid_buf)
                {
                    deliver_paste(app, rapid_buf);

                    free(rapid_buf);

                    return 1;
                }

                /* SOFT-WRAP: If cursor at end of visual segment, move to start of next segment */
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
                            np = pos;

                            if (info.col >= pos && info.col < np)
                            {
                                /* Cursor is in this segment */
                                if (info.col >= end && np < len)
                                {
                                    /* Cursor at end of segment: move to start of next segment */
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

                    /* Activate when cursor is at end of line */
                    if (wi.col == linelen)
                    {
                        const wchar_t *line = ed_line_wcs(app->editor, wi.row);

                        if (line)
                        {
                            int word_start = 0;
                            int word_len;
                            int space_available;
                            int k;
                            int hyphen_performed = 0;

                            /* Identify the current word (from last space to cursor) */
                            for (k = wi.col; k > 0; k--)
                            {
                                if (line[k - 1] == L' ' || line[k - 1] == L'\n' || line[k - 1] == L'\t')
                                {
                                    word_start = k;
                                    break;
                                }
                            }

                            word_len = wi.col - word_start;
                            space_available = eff_wrap - word_start;

#ifdef HAVE_HYPHEN
                            /* If word doesn't fit in available space, try hyphen */
                            if (word_len > space_available && space_available >= 0 && app->hyph_wrap_enabled && app->hyph_handle)
                            {
                                int hyph_break = ui_hyph_find_break(app, &line[word_start], word_len, space_available);

                                /* Validate hyphenation breakpoint: must be valid and leave at least one character after hyphen */
                                if (hyph_break >= 0 && hyph_break > 0 && hyph_break < word_len - 1)
                                {
                                    int break_pos = word_start + hyph_break;

                                    /* Validate that break_pos is within the line */
                                    if (break_pos < linelen)
                                    {
                                        /* Save state for undo */
                                        ed_save_undo(app->editor);

                                        /* Insert '-' at breakpoint */
                                        ed_set_pos(app->editor, wi.row, break_pos);

                                        if (ed_insert_char(app->editor, L'-') == 0)
                                        {
                                            /* Insert '\n' after '-' using ed_enter to properly split the line */
                                            if (ed_enter(app->editor) == 0)
                                            {
                                                /* Calculate tail: characters remaining after breakpoint in the word */
                                                int tail = word_len - hyph_break;

                                                if (tail < 0)
                                                    tail = 0;

                                                ed_set_pos(app->editor, wi.row + 1, tail);
                                                reset_search(app);

                                                hyphen_performed = 1;
                                            }
                                        }
                                    }
                                }
                            }
#endif

                            /* If line exceeds limit, try to break at space (but not if hyphen already handled it) */
                            if (wi.col > eff_wrap && !hyphen_performed)
                            {
                                int brk = -1;
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
                }

                return 1;
            }

            return 0;
        }
    }
}

UiView ui_editor_run(UiApp *app)
{
    int eff_wrap;

    if (!app)
        return VIEW_QUIT;

    /* Initial wrap column (handles starting on a small screen) */
    eff_wrap = editor_eff_wrap(app);

    /* Fresh soft-wrap viewport for this editing session */
    s_soft_top_line = 0;
    s_soft_top_sub = 0;
    s_soft_desired_vcol = -1;
    s_soft_last_width = COLS;

    BRACKET_PASTE_ON();

    for (;;)
    {
        wint_t wch;
        int wrc;
        int ch;
        int is_key;
        int width;
        int func_result;
        int soft_active;
        int body_width;
        int body_rows;
        int preserve_desired;
        AreaEntry *ae_body = NULL;
        int srow;

        /* Recalculate effective width each frame for resize handling */
        eff_wrap = editor_eff_wrap(app);

        /* Calculate width for line numbers BEFORE resize check - TinyEdit style */
        width = COLS;

        if (app->cfg && app->cfg->show_line_numbers)
        {
            EdInfo info;

            ed_get_info(app->editor, &info);
            width = COLS - lineno_width(info.line_count);

            if (width < 1)
                width = 1;
        }

        /* On resize, reset goal column to re-sync from new layout */
        if (COLS != s_soft_last_width)
        {
            soft_reset_desired();
            s_soft_last_width = COLS;
        }

        erase();
        standend();

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

        /* Spell panel overlay (no-op when hidden) */
        ui_spell_draw_panel(app);

        position_edit_cursor(app);
        refresh();

        wrc = wrapper_read_key(&wch);

        if (wrc == ERR)
            continue;

        ch = (int)wch;

        /* Distinguish special key codes (KEY_CODE_YES) from printable chars, without guard codepoint matching KEY_F(5) triggers F5 */
        is_key = (wrc == KEY_CODE_YES);

        /* Force is_key for navigation keys that may not have KEY_CODE_YES */
        if (!is_key && (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT ||
                        ch == KEY_HOME || ch == KEY_END || ch == KEY_PPAGE || ch == KEY_NPAGE ||
                        ch == KEY_BACKSPACE || ch == KEY_DC || ch == KEY_ENTER))
            is_key = 1;

        /* Alt+F: freq request popup (not in handle_function_keys) */
        if (is_key && ch == KEY_ALT('F'))
        {
            ui_popup_freq(app);
            continue;
        }

        /* Handle function keys (F1-F11) */
        func_result = handle_function_keys(app, ch, is_key);

        if (func_result == 2)
        {
            /* Special return: exit editor */
            curs_set(0);
            BRACKET_PASTE_OFF();
            return app->edit_return_view;
        }

        if (func_result == 1)
            continue;

        /* ESC in header: jump to body. ESC in body: confirm quit */
        if (!is_key && ch == 27)
        {
            EdInfo info;

            if (app->edit_active_field != EF_BODY)
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
                handle_search_with_popup(app);
                continue;
            }
        }

        /* Handle control keys (Ctrl+...) */
        if (handle_control_keys(app, ch, is_key))
            continue;

        /* Handle Alt keys (Alt+...) */
        if (handle_alt_keys(app, ch, is_key))
            continue;

        /* ESC: exit search mode (already handled above, but also check here) */
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

        /* Handle header field input */
        if (app->edit_active_field == EF_FROM || app->edit_active_field == EF_TO || app->edit_active_field == EF_SUBJECT || app->edit_active_field == EF_DADDR)
        {
            if (handle_header_input(app, ch, is_key))
                continue;
        }

        /* Body input: separate special-key and printable paths to avoid spurious KEY_* matches */
        soft_active = !(app->cfg && app->cfg->hard_wrap);
        body_width = width; /* Use the width calculated earlier like TinyEdit */
        preserve_desired = 0;

        ae_body = &app->areas->entries[app->sess.area_idx];
        srow = (ae_body->type == AREATYPE_NETMAIL) ? 8 : 7;

        body_rows = LINES - srow - 1;

        /* Reserve space for spell panel at bottom when active */
        if (app->show_spell)
            body_rows -= SPELL_PANEL_H;

        if (body_rows < 1)
            body_rows = 1;

        /* Handle body input */
        if (handle_body_input(app, ch, is_key, wch, soft_active, body_width, body_rows, eff_wrap, &preserve_desired))
        {
            /* Reset desired column unless vertical move, otherwise LEFT/RIGHT/HOME/END leave stale desired_vcol */
            if (!preserve_desired)
                soft_reset_desired();

            continue;
        }

        /* Handle navigation keys in body */
        if (handle_navigation_keys(app, ch, is_key, soft_active, body_width, body_rows, &preserve_desired))
        {
            if (!preserve_desired)
                soft_reset_desired();

            continue;
        }

        /* Reset desired column on horizontal moves */
        if (!preserve_desired)
            soft_reset_desired();
    }

    curs_set(0);
    BRACKET_PASTE_OFF();

    return app->edit_return_view;
}
