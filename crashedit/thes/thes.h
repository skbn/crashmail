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

#ifndef THES_H
#define THES_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Thesaurus wrapper with LRU cache to reduce MyThes allocations */
#if defined(PLATFORM_AMIGA)
#ifndef THES_CACHE_N
#define THES_CACHE_N 32
#endif
#ifndef THES_CACHE_KEY_MAX
#define THES_CACHE_KEY_MAX 64
#endif
#ifndef THES_CACHE_SYNS_MAX
#define THES_CACHE_SYNS_MAX 48
#endif
#ifndef THES_CACHE_TXT_MAX
#define THES_CACHE_TXT_MAX 96
#endif
#else
#ifndef THES_CACHE_N
#define THES_CACHE_N 1024
#endif
#ifndef THES_CACHE_KEY_MAX
#define THES_CACHE_KEY_MAX 96
#endif
#ifndef THES_CACHE_SYNS_MAX
#define THES_CACHE_SYNS_MAX 96
#endif
#ifndef THES_CACHE_TXT_MAX
#define THES_CACHE_TXT_MAX 128
#endif
#endif

    typedef struct ThesHandle ThesHandle;

/* Forward-declared from spell.h for stemming */
#include "../spell/spell.h"

    /* Each meaning has a definition and synonym list */
    typedef struct
    {
        char *def; /* never NULL; "" if no definition */
        int nsyns;
        char **syns; /* nsyns entries, each owned */
    } ThesMeaning;

    /* Load thesaurus files, NULL on failure */
    ThesHandle *thes_new(const char *idx_path, const char *dat_path);

    /* Free thesaurus handle and cache */
    void thes_free(ThesHandle *t);

    /* Attach spell checker for stemming fallback (borrowed pointer) */
    void thes_set_speller(ThesHandle *t, SpellChecker *sc);

    /* Look up word (cached). Returns meaning count or -1 on error */
    int thes_lookup(ThesHandle *t, const char *word, ThesMeaning **out_meanings);

    /* Free meaning array from thes_lookup */
    void thes_free_meanings(ThesHandle *t, ThesMeaning *m, int nmeanings);

    /* Get thesaurus encoding */
    const char *thes_get_encoding(ThesHandle *t);

    /* Clear lookup cache */
    void thes_cache_clear(ThesHandle *t);

    /* List *.idx files in directory */
    char **thes_list_dictionaries(const char *dir_path, int *n_dicts);
    void thes_free_dictionaries(char **dicts, int n_dicts);

    /* Check if thesaurus is available */
    int thes_is_available(void);

#ifdef __cplusplus
}
#endif

#endif /* THES_H */
