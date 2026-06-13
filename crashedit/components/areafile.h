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

/* areafile.h -- Area file parser */
#ifndef WRAPPER_AREAFILE_H
#define WRAPPER_AREAFILE_H

#include <stdint.h>

/* Area type constants */
#define AREATYPE_ECHO 0
#define AREATYPE_NETMAIL 1
#define AREATYPE_LOCAL 2

typedef struct
{
    char *name;        /* Area tag (e.g., "CRASHMAIL.ESP") - dynamically allocated */
    char *description; /* Human-readable description - dynamically allocated */
    char *path;        /* Path to JAM message base - dynamically allocated */
    char *aka;         /* AKA address for this area - dynamically allocated */
    int type;          /* 0=echo, 1=netmail, 2=local */
    int groupid;       /* Group ID (0-255, like GoldEd) */
    int total_msgs;    /* Total messages in area */
    int unread;        /* Count of messages with msgnum > lastread (true "unread") */
    uint32_t lastread; /* Last message read in the reader */
    uint32_t lastseen; /* Highest msgnum the user has acknowledged via msglist */
    int new_count;     /* Count of messages with msgnum > lastseen */
} AreaEntry;

typedef struct
{
    int count;
    int capacity;
    AreaEntry *entries; /* Dynamically allocated */
} AreaList;

/* Load areas from file, returns count or 0 on error */
int areafile_load(AreaList *list, const char *path);
void areafile_calculate_counts(AreaList *list, const char *sysop);
void areafile_refresh_one(AreaEntry *ae, const char *sysop);
void areafile_free(AreaList *list);

#endif
