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

/* ui_aka.h -- Unique AKA enumeration from areas */
#ifndef UI_AKA_H
#define UI_AKA_H

#include "../wrapper.h"

/* Get Nth unique AKA (returns AKA string from areas->entries[*] or cfg->aka[*], NULL if idx out of range) */
const char *ui_aka_at(const AreaList *areas, const CrashEditCfg *cfg, int idx);

/* Count of unique AKAs available */
int ui_aka_count(const AreaList *areas, const CrashEditCfg *cfg);

/* Walk each unique AKA via visitor callback (stops early if cb returns non-zero) */
typedef int (*ui_aka_visit_fn)(int idx, const char *aka, void *user);
int ui_aka_walk(const AreaList *areas, const CrashEditCfg *cfg, ui_aka_visit_fn cb, void *user);

#endif /* UI_AKA_H */
