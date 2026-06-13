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

/* nodelist.h -- In-memory list of (sysop_name, ftn_address) entries */

#ifndef CRASHEDIT_NODELIST_H
#define CRASHEDIT_NODELIST_H

#include <stdint.h>

#define NODELIST_NAME_MAX 64 /* "Firstname Lastname" room */
#define NODELIST_ADDR_MAX 40 /* "Z:N/F.P@network" room   */

typedef struct
{
    char name[NODELIST_NAME_MAX]; /* sysop full name, spaces restored */
    char addr[NODELIST_ADDR_MAX]; /* canonical FTN, e.g. "2:341/207" */
} NodelistEntry;

typedef struct
{
    NodelistEntry *entries;
    int count;
    int capacity;
} Nodelist;

/* Lifecycle */
void nodelist_init(Nodelist *nl);
void nodelist_free(Nodelist *nl);

/* Parse FTS-5000 nodelist/pointlist file, append entries, default network suffix appended to addresses without one */
int nodelist_load_file(Nodelist *nl, const char *path, const char *default_network);

/* Returns 1 if entry's name OR addr contains needle (case-insensitive), empty needle matches everything */
int nodelist_entry_matches(const NodelistEntry *e, const char *needle);

#endif
