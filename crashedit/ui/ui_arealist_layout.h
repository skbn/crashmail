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

/* ui_arealist_layout.h -- Area list column layout */
#ifndef UI_AREALIST_LAYOUT_H
#define UI_AREALIST_LAYOUT_H

#include "../wrapper.h"

/* Per-field position+width (pos = -1 means field absent) */
typedef struct
{
    int area_pos, area_width;
    int marked_pos, marked_width;
    int desc_pos, desc_width;
    int count_pos, count_width;
    int pmark_pos, pmark_width;
    int unread_pos, unread_width;
    int changed_pos, changed_width;
    int echoid_pos, echoid_width;
    int groupid_pos, groupid_width;
    int total_used; /* total columns consumed (diagnostics) */
} ArealistLayout;

/* Compute layout from cfg->arealistformat against current areas (caller passes max column width) */
void ui_arealist_layout_compute(const AreaList *areas, const char *format, int maxcol, ArealistLayout *out);

/* Cached wrapper: keyed on (areas, format, maxcol, areas->count), recompute only when changes (drop-in for compute() on hot paths) */
void ui_arealist_layout_get(const AreaList *areas, const char *format, int maxcol, ArealistLayout *out);

#endif /* UI_AREALIST_LAYOUT_H */
