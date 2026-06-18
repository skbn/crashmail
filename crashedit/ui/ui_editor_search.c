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

#include "ui_editor_search.h"
#include "ui.h"
#include "ui_internal.h"
#include "../components/editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* Reset search state in editor */
void reset_search(UiApp *app)
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
int search_prev_editor(UiApp *app)
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
int search_next_editor(UiApp *app)
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
int do_replace(UiApp *app, const wchar_t *needle, const wchar_t *repl)
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
int replace(UiApp *app)
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
int replace_current(UiApp *app)
{
    int match_row;
    int match_col;
    int nlen;
    int rlen;
    int i;
    int *new_rows;
    int *new_cols;
    int new_match_count;

    if (app->edit_search.last_replace[0] != L'\0')
    {
        /* Use the last replacement text, no popup */
        wchar_t repl[64];
        wcsncpy(repl, app->edit_search.last_replace, 63);
        repl[63] = L'\0';

        /* Move cursor to current match position */
        if (!app->edit_search.rows || app->edit_search.match_count == 0)
            return 0;

        match_row = app->edit_search.rows[app->edit_search.current_match];
        match_col = app->edit_search.cols[app->edit_search.current_match];

        ed_set_pos(app->editor, match_row, match_col);

        /* Save undo state */
        ed_save_undo(app->editor);

        /* Replace the current occurrence */
        nlen = (int)wcslen(app->edit_search.query);
        rlen = (int)wcslen(repl);

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
        new_rows = NULL;
        new_cols = NULL;
        new_match_count = ed_search_all_custom(app->editor, app->edit_search.query, app->edit_search.case_sensitive, app->edit_search.whole_word, &new_rows, &new_cols);

        if (new_match_count > 0)
        {
            int next_idx = 0;

            app->edit_search.rows = new_rows;
            app->edit_search.cols = new_cols;
            app->edit_search.match_count = new_match_count;
            app->edit_search.current_match = 0;
            app->edit_search.match_current = 1;

            /* Find first match after replacement position */
            for (i = 0; i < new_match_count; i++)
            {
                if (new_rows[i] > match_row || (new_rows[i] == match_row && new_cols[i] > match_col))
                {
                    next_idx = i;
                    break;
                }
            }

            app->edit_search.current_match = next_idx;

            /* Move cursor to next match */
            ed_set_pos(app->editor, new_rows[next_idx], new_cols[next_idx]);
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
int replace_all(UiApp *app)
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
            ui_status(app, "Replace all cancelled");
        }
    }
    else
    {
        ui_status(app, "No search active (use F5/Alt+S first)");
    }

    return 1;
}

/* Handle F5/Alt+S search with popup results list */
int handle_search_with_popup(UiApp *app)
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

                return 1;
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

    return 1;
}
