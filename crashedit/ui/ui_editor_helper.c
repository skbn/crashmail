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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include "ui_editor_helper.h"
#include "ui.h"
#include "ui_internal.h"

#if defined(PLATFORM_AMIGA) && !defined(wcswidth)
/* wcswidth implementation based on Markus Kuhn's wcwidth.c
 * https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 * Returns number of column positions needed for wide-character string
 * Compatible with POSIX.1-2001 standard for Unicode terminal display
 */
int wcswidth(const wchar_t *wcs, size_t n)
{
    int width = 0;
    size_t i;

    for (i = 0; i < n && wcs[i] != L'\0'; i++)
    {
        /* Based on Markus Kuhn's wcwidth.c implementation
         * Wide (W) and Full-width (F) characters = 2 columns
         * All other printable characters = 1 column
         * Implementation follows Unicode TR#11 and POSIX standards
         */
        if ((wcs[i] >= 0x1100 && wcs[i] <= 0x115F) ||   /* Hangul Jamo */
            (wcs[i] >= 0x2E80 && wcs[i] <= 0xA4CF) ||   /* CJK...Yi */
            (wcs[i] >= 0xAC00 && wcs[i] <= 0xD7A3) ||   /* Hangul Syllables */
            (wcs[i] >= 0xF900 && wcs[i] <= 0xFAFF) ||   /* CJK Compatibility */
            (wcs[i] >= 0xFE30 && wcs[i] <= 0xFE6F) ||   /* CJK Compatibility Forms */
            (wcs[i] >= 0xFF00 && wcs[i] <= 0xFF60) ||   /* Fullwidth Forms */
            (wcs[i] >= 0x20000 && wcs[i] <= 0x2FFFD) || /* Supplementary Planes */
            (wcs[i] >= 0x30000 && wcs[i] <= 0x3FFFD))   /* More Supplementary */
            width += 2;
        else
            width += 1;
    }

    return width;
}
#endif

int wcs_vwidth(const wchar_t *s, int n)
{
    int v = 0;
    int i;

    if (!s || n <= 0)
        return 0;

    for (i = 0; i < n; i++)
    {
        int w = wcswidth(&s[i], 1);

        if (w == 2)
            v += 2;
        else
            v += 1; /* narrow, zero-width, control -> 1 */
    }

    return v;
}

/* Effective wrap column. Clamp AUTOWRAP to COLS-1; 0=disabled */
int editor_eff_wrap(const UiApp *app)
{
    int cfgw = (app && app->cfg) ? app->cfg->autowrap_col : 0;
    int limit = COLS - 1; /* leave one column of margin */

    if (cfgw <= 0)
        return 0; /* AUTOWRAP disabled in config: never wrap */

    if (COLS <= 10)
        return 0; /* Unusably narrow: scroll instead of wrapping */

    if (cfgw > limit)
        return limit; /* Clamp to screen width */

    return cfgw;
}
