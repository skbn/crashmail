/*
 * crashedit - Message area editor for AmigaOS
 *
 * This file is part of the crashedit project.
 *
 * Copyright (C) 2026 Tanausú M.
 *
 * Released under the GNU GPL v2 or later.
 */

/* search.h -- selective full-text search across one or many areas */

#ifndef CRASHEDIT_SEARCH_H
#define CRASHEDIT_SEARCH_H

#include <stdint.h>
#include "../components/areafile.h"

#define SEARCH_HIT_HEADER 0x01u
#define SEARCH_HIT_BODY 0x02u

/* Default hit cap when the config doesn't say otherwise. NOT a hard
 * ceiling -- the hits array is malloc'd to whatever max_hits the user
 * configures, so the real limit is available RAM. SEARCHMAX <= 0 in
 * the config means "use this default" */
#define SEARCH_DEFAULT_MAX 2048

/* A sanity guard so a bogus/huge config value can't overflow the
 * malloc size computation. ~100k hits * ~108 bytes ~= 11 MB; if you
 * really need more you have bigger problems than this limit. Set it
 * high enough that it's effectively "as much as the box can hold" */
#define SEARCH_HITS_HARD_MAX 1000000

#define SEARCH_PATTERN_MAX 256
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
    int search_headers;
    int search_body;

    /* Results buffer: malloc'd to max_hits entries in search_new
     * The cap is whatever you configure (clamped only by
     * SEARCH_HITS_HARD_MAX to keep the allocation sane). When it
     * fills, hit_limit_reached is set and the scan stops */
    SearchHit *hits;
    int n_hits;
    int max_hits; /* allocated capacity of hits[] */
    int hit_limit_reached;

    /* Progress / cancellation. cancel is set from outside (by the UI)
     * and polled inside the scan loop every N messages */
    int cancel;
    int scanned_areas;
    int total_areas;
    uint32_t scanned_msgs_current; /* reset per area */
} SearchSession;

/* Allocate a session. pattern is copied. Returns NULL on bad args or
 * OOM. case_sensitive=0 enables ASCII-insensitive matching (the
 * locale-aware kind isn't worth the trouble for a per-byte scan)
 * max_hits is the capacity of the results buffer, malloc'd here;
 * values <= 0 use SEARCH_DEFAULT_MAX, and it is capped only by
 * SEARCH_HITS_HARD_MAX. If the allocation for that many hits fails,
 * search_new returns NULL (the caller reports out-of-memory) */
SearchSession *search_new(const char *pattern, int search_headers, int search_body, int case_sensitive, int max_hits);

void search_free(SearchSession *s);

/* Scan a single area. Opens the JAM, walks its headers, optionally
 * reads each body, records hits into s->hits
 * area_idx is the position of `area` inside the caller's AreaList; it
 * is stored verbatim in each SearchHit so the result UI can map back
 * Returns the number of hits APPENDED (>=0), or -1 if cancelled
 * (s->cancel was set during the scan) — partial results are kept */
int search_scan_area(SearchSession *s, AreaEntry *area, int area_idx);

/* How many distinct areas have at least one hit. Cheap walk over
 * s->hits, no allocation */
int search_areas_with_hits(const SearchSession *s);

/* How many hits in a specific area_idx */
int search_count_in_area(const SearchSession *s, int area_idx);

#endif /* CRASHEDIT_SEARCH_H */
