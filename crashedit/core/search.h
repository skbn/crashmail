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

/* search.h -- selective full-text search across one or many areas */

#ifndef CRASHEDIT_SEARCH_H
#define CRASHEDIT_SEARCH_H

#include <stdint.h>
#include "../components/areafile.h"

#define SEARCH_HIT_HEADER 0x01u
#define SEARCH_HIT_BODY 0x02u

/* Default hit cap when config doesn't specify, not a hard ceiling, real limit is available RAM */
#define SEARCH_DEFAULT_MAX 2048

/* Sanity guard to prevent bogus/huge config values from overflowing malloc, effectively "as much as box can hold" */
#define SEARCH_HITS_HARD_MAX 1000000

#ifndef SEARCH_PATTERN_MAX
#define SEARCH_PATTERN_MAX 256
#endif
#define SEARCH_PREVIEW_MAX 64
#define SEARCH_FROM_MAX 32

typedef struct
{
    int area_idx; /* index into AreaList::entries */
    uint32_t msgnum;
    uint16_t flags; /* OR of SEARCH_HIT_* */
    char subject[SEARCH_PREVIEW_MAX];
    char from[SEARCH_FROM_MAX];
} SearchHit;

typedef struct
{
    /* Search parameters (read-only after creation) */
    char pattern[SEARCH_PATTERN_MAX];
    int pat_len; /* cached strlen(pattern), -1 if empty */
    int case_sensitive;
    int whole_word;
    int search_headers;
    int search_body;

    /* Results buffer: malloc'd to max_hits entries, cap is configured (clamped by SEARCH_HITS_HARD_MAX), sets hit_limit_reached when full */
    SearchHit *hits;
    int n_hits;
    int max_hits; /* allocated capacity of hits[] */
    int hit_limit_reached;

    /* Progress/cancellation: cancel set from outside (by UI) and polled inside scan loop every N messages */
    int cancel;
    int scanned_areas;
    int total_areas;
    uint32_t scanned_msgs_current; /* reset per area */
} SearchSession;

/* Allocate session, pattern is copied, returns NULL on bad args or OOM, case_sensitive=0 enables ASCII-insensitive matching */
SearchSession *search_new(const char *pattern, int search_headers, int search_body, int case_sensitive, int whole_word, int max_hits);

void search_free(SearchSession *s);

/* Scan single area, opens JAM, walks headers, optionally reads bodies, records hits, returns hits appended or -1 if cancelled */
int search_scan_area(SearchSession *s, AreaEntry *area, int area_idx);

/* How many distinct areas have at least one hit, cheap walk over s->hits, no allocation */
int search_areas_with_hits(const SearchSession *s);

/* How many hits in specific area_idx */
int search_count_in_area(const SearchSession *s, int area_idx);

#endif /* CRASHEDIT_SEARCH_H */
