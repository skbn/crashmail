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

/* ui_aka.c -- AKA selection utilities */
#include <string.h>
#include "../components/config.h"
#include "ui_aka.h"

/* Visitor for ui_aka_at: stop on match, capture pointer */
typedef struct
{
    int want;
    const char *got;
} AtCtx;

/* Check if s already appeared in first seen_n slots */
static int already_seen(const char *const *seen, int seen_n, const char *s)
{
    int i;

    for (i = 0; i < seen_n; i++)
    {
        if (seen[i] && strcmp(seen[i], s) == 0)
            return 1;
    }

    return 0;
}

/* AreaList AKAs come first, cfg->aka[] fills in if areas had none */
int ui_aka_walk(const AreaList *areas, const CrashEditCfg *cfg, ui_aka_visit_fn cb, void *user)
{
    /* Bounded local table: AKAs max out below 64, no malloc/VLA */
    const char *seen[CFG_AKA_MAX];
    int n = 0;
    int i;
    int rc;

    if (!areas || !cb)
        return 0;

    for (i = 0; i < areas->count && n < CFG_AKA_MAX; i++)
    {
        const AreaEntry *ae = &areas->entries[i];

        if (!ae->aka || !ae->aka[0])
            continue;

        if (already_seen(seen, n, ae->aka))
            continue;

        seen[n] = ae->aka;
        rc = cb(n, ae->aka, user);
        n++;

        if (rc)
            return n;
    }

    if (n == 0 && cfg && cfg->aka_count > 0)
    {
        for (i = 0; i < cfg->aka_count && n < CFG_AKA_MAX; i++)
        {
            const char *a = cfg->aka[i];

            if (!a || !a[0])
                continue;

            if (already_seen(seen, n, a))
                continue;

            seen[n] = a;
            rc = cb(n, a, user);
            n++;

            if (rc)
                return n;
        }
    }

    return n;
}

static int at_cb(int idx, const char *aka, void *user)
{
    AtCtx *c = (AtCtx *)user;

    if (idx == c->want)
    {
        c->got = aka;
        return 1;
    }

    return 0;
}

const char *ui_aka_at(const AreaList *areas, const CrashEditCfg *cfg, int idx)
{
    AtCtx c;
    c.want = idx;
    c.got = NULL;

    if (idx < 0)
        return NULL;

    ui_aka_walk(areas, cfg, at_cb, &c);

    return c.got;
}

static int count_cb(int idx, const char *aka, void *user)
{
    return 0;
}

int ui_aka_count(const AreaList *areas, const CrashEditCfg *cfg)
{
    return ui_aka_walk(areas, cfg, count_cb, NULL);
}
