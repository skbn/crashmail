/*
 * crashedit - Message area editor for AmigaOS
 *
 * This file is part of the crashedit project.
 *
 * Copyright (C) 2026 Tanausú M.
 *
 * Released under the GNU GPL v2 or later.
 */

/* search.c -- motor for the selective full-text search.
 *
 * Header-only matching uses the already-decoded subject/from/to/msgid
 * strings that jam_load_headers stuffs into JamMsgInfo — these are
 * small and they live in RAM only for the duration of the area scan,
 * so no streaming work is needed there.
 *
 * Body matching does need streaming. jam_read_body() returns the full
 * body as a single malloc, which is the path of least resistance and
 * keeps us out of the JAM library's internals. Peak RAM is one body
 * at a time — typically a few KB, rarely above 64 KB. We free it
 * before reading the next, so the actual cost is "size of the
 * single largest message we scan", not "sum of all bodies".
 *
 * The pattern matcher is a tiny case-insensitive substring search
 * (ASCII-fold for the !case_sensitive case). For the message sizes
 * crashedit deals with, naive O(n*m) is fine; Boyer-Moore would be
 * a couple hundred lines for ~no observable win below a megabyte
 * per message.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "search.h"
#include "jam_wrap.h"

/* Pattern matching */

/* Case-insensitive (ASCII) memmem. Returns first match offset in
 * hay or -1. n=length of hay, m=length of needle */
static int memcasestr(const char *hay, int n, const char *needle, int m)
{
    int i, j;

    if (m <= 0)
        return 0; /* empty needle matches at start */

    if (m > n)
        return -1;

    for (i = 0; i <= n - m; i++)
    {
        for (j = 0; j < m; j++)
        {
            unsigned char a = (unsigned char)hay[i + j];
            unsigned char b = (unsigned char)needle[j];

            if (a >= 'A' && a <= 'Z')
                a = (unsigned char)(a + 32);

            if (b >= 'A' && b <= 'Z')
                b = (unsigned char)(b + 32);

            if (a != b)
                break;
        }

        if (j == m)
            return i;
    }

    return -1;
}

/* Case-SENSITIVE memmem variant */
static int memmemstr(const char *hay, int n, const char *needle, int m)
{
    int i;

    if (m <= 0)
        return 0;

    if (m > n)
        return -1;

    for (i = 0; i <= n - m; i++)
    {
        if (memcmp(hay + i, needle, (size_t)m) == 0)
            return i;
    }

    return -1;
}

static int contains(const char *hay, int n, const char *needle, int m, int csens)
{
    if (!hay || n <= 0 || !needle || m <= 0)
        return 0;

    if (csens)
        return memmemstr(hay, n, needle, m) >= 0;

    return memcasestr(hay, n, needle, m) >= 0;
}

SearchSession *search_new(const char *pattern, int search_headers, int search_body, int case_sensitive, int max_hits)
{
    SearchSession *s;
    size_t pl;

    if (!pattern || !pattern[0])
        return NULL;

    if (!search_headers && !search_body)
        return NULL;

    pl = strlen(pattern);

    if (pl >= SEARCH_PATTERN_MAX)
        pl = SEARCH_PATTERN_MAX - 1;

    s = (SearchSession *)calloc(1, sizeof(SearchSession));

    if (!s)
        return NULL;

    /* Clamp into 1..HARD_MAX. <=0 means "use the default" */
    if (max_hits <= 0)
        max_hits = SEARCH_DEFAULT_MAX;

    if (max_hits > SEARCH_HITS_HARD_MAX)
        max_hits = SEARCH_HITS_HARD_MAX;

    s->hits = (SearchHit *)malloc((size_t)max_hits * sizeof(SearchHit));

    if (!s->hits)
    {
        /* Couldn't get a buffer that big -- tell the caller */
        free(s);
        return NULL;
    }

    memcpy(s->pattern, pattern, pl);

    s->pattern[pl] = '\0';
    s->pat_len = (int)pl;
    s->case_sensitive = case_sensitive ? 1 : 0;
    s->search_headers = search_headers ? 1 : 0;
    s->search_body = search_body ? 1 : 0;
    s->max_hits = max_hits;

    return s;
}

void search_free(SearchSession *s)
{
    if (s)
    {
        free(s->hits);
        free(s);
    }
}

/* Copy at most cap-1 chars + NUL terminator from src into dst
 * src is assumed null-terminated; we just trim */
static void clip_copy(char *dst, int cap, const char *src)
{
    int i = 0;

    if (!dst || cap <= 0)
        return;

    if (src)
    {
        for (; i < cap - 1 && src[i]; i++)
            dst[i] = src[i];
    }

    dst[i] = '\0';
}

/* Append one hit to the session. Returns 1 if added, 0 if cap hit */
static int record_hit(SearchSession *s, int area_idx, const JamMsgInfo *msg, uint16_t flags)
{
    SearchHit *h;

    if (s->n_hits >= s->max_hits)
    {
        s->hit_limit_reached = 1;
        return 0;
    }

    h = &s->hits[s->n_hits++];
    h->area_idx = area_idx;
    h->msgnum = msg->msgnum;
    h->flags = flags;

    clip_copy(h->subject, SEARCH_PREVIEW_MAX, msg->subject);
    clip_copy(h->from, SEARCH_FROM_MAX, msg->from);

    return 1;
}

/* Test one header bundle against the pattern */
static int header_matches(const SearchSession *s, const JamMsgInfo *msg)
{
    const char *fields[] =
        {
            msg->subject, msg->from, msg->to, msg->msgid, msg->oaddress,
            msg->daddress};

    int nfields = (int)(sizeof(fields) / sizeof(fields[0]));
    int i;

    for (i = 0; i < nfields; i++)
    {
        const char *fv = fields[i];

        if (fv && contains(fv, (int)strlen(fv), s->pattern, s->pat_len, s->case_sensitive))
            return 1;
    }

    return 0;
}

/* Read body bytes for msgnum and test against pattern. Returns 1 on
 * match. Frees the body buffer before returning */
static int body_matches(const SearchSession *s, JamArea *area, uint32_t msgnum)
{
    uint32_t body_len = 0;
    char *body;
    int rc;

    body = jam_read_body(area, msgnum, &body_len);

    if (!body || body_len == 0)
    {
        if (body)
            free(body);

        return 0;
    }

    rc = contains(body, (int)body_len, s->pattern, s->pat_len, s->case_sensitive) ? 1 : 0;

    free(body);
    return rc;
}

int search_scan_area(SearchSession *s, AreaEntry *area, int area_idx)
{
    JamArea ja;
    JamMsgInfo *msgs;
    int nmsgs = 0;
    int i;
    int hits_before;

    if (!s || !area || !area->path)
        return 0;

    if (s->cancel)
        return -1;

    if (jam_open(&ja, area->path) != 0)
        return 0; /* Skip unreadable areas, don't fail the whole search */

    hits_before = s->n_hits;
    msgs = jam_load_headers(&ja, &nmsgs, 0, 0);
    s->scanned_msgs_current = 0;

    if (msgs && nmsgs > 0)
    {
        for (i = 0; i < nmsgs; i++)
        {
            uint16_t flags = 0;

            /* Cancel check every 64 messages — cheap enough that we
             * don't lag while still letting the user ESC out of a
             * long scan in well under a second */
            if ((i & 63) == 0 && s->cancel)
                break;

            if (s->search_headers && header_matches(s, &msgs[i]))
                flags |= SEARCH_HIT_HEADER;

            if (s->search_body && body_matches(s, &ja, msgs[i].msgnum))
                flags |= SEARCH_HIT_BODY;

            if (flags)
            {
                if (!record_hit(s, area_idx, &msgs[i], flags))
                {
                    /* Cap reached — abort the whole search; partial
                     * results stand. The UI sees hit_limit_reached */
                    break;
                }
            }

            s->scanned_msgs_current = (uint32_t)(i + 1);
        }

        free(msgs);
    }

    jam_close(&ja);

    if (s->cancel)
        return -1;

    return s->n_hits - hits_before;
}

int search_areas_with_hits(const SearchSession *s)
{
    /* Walk hits and count distinct area_idx values. Hits are emitted
     * in area-scan order, so a given area's hits are CONTIGUOUS in
     * s->hits[]; that lets us count distinct just by watching for
     * area_idx changes */
    int i;
    int distinct = 0;
    int prev = -1;

    if (!s)
        return 0;

    for (i = 0; i < s->n_hits; i++)
    {
        if (s->hits[i].area_idx != prev)
        {
            distinct++;
            prev = s->hits[i].area_idx;
        }
    }

    return distinct;
}

int search_count_in_area(const SearchSession *s, int area_idx)
{
    int i, c = 0;

    if (!s)
        return 0;

    for (i = 0; i < s->n_hits; i++)
    {
        if (s->hits[i].area_idx == area_idx)
            c++;
    }

    return c;
}
