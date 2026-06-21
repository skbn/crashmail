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

#ifndef HYPH_H
#define HYPH_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Hyphenation wrapper with LRU cache to reduce libhyphen allocations */
#if defined(PLATFORM_AMIGA)
#ifndef HYPH_CACHE_N
#define HYPH_CACHE_N 128
#endif
#ifndef HYPH_CACHE_KEY_MAX
#define HYPH_CACHE_KEY_MAX 512
#endif
#ifndef HYPH_MAX_BREAKS
#define HYPH_MAX_BREAKS 16
#endif
#else
#ifndef HYPH_CACHE_N
#define HYPH_CACHE_N 4096
#endif
#ifndef HYPH_CACHE_KEY_MAX
#define HYPH_CACHE_KEY_MAX 512
#endif
#ifndef HYPH_MAX_BREAKS
#define HYPH_MAX_BREAKS 64
#endif
#endif

    typedef struct HyphDict HyphDict;

    /* Open dictionary file, NULL on failure */
    HyphDict *hyph_new(const char *dict_path);

    /* Free dictionary and cache */
    void hyph_free(HyphDict *h);

    /* Get break points for word (cached) */
    int hyph_breakpoints(HyphDict *h, const char *word, int word_len, int *out_pos, int *out_count);

    /* Clear cache */
    void hyph_cache_clear(HyphDict *h);

    /* List *.dic files in directory */
    char **hyph_list_dictionaries(const char *dir_path, int *n_dicts);
    void hyph_free_dictionaries(char **dicts, int n_dicts);

    /* Check if hyphenation is available */
    int hyph_is_available(void);

#ifdef __cplusplus
}
#endif

#endif /* HYPH_H */
