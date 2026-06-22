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

#ifndef SPELL_H
#define SPELL_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Spell checker wrapper */
#ifdef HAVE_HUNSPELL
#if defined(PLATFORM_AMIGA)
/* On AmigaOS, use native spellchecker implementation */
#include "../spellchecker/spell.h"
#elif defined(PLATFORM_BSD)
#include <hunspell.h>
#else
#include <hunspell/hunspell.h>
#endif
#endif

/* LRU cache: smaller on Amiga to reduce memory pressure */
#if defined(PLATFORM_AMIGA)
#ifndef SPELL_CACHE_N
#define SPELL_CACHE_N 256
#endif
#ifndef SPELL_CACHE_KEY_MAX
#define SPELL_CACHE_KEY_MAX 256
#endif
#else
#ifndef SPELL_CACHE_N
#define SPELL_CACHE_N 16384
#endif
#ifndef SPELL_CACHE_KEY_MAX
#define SPELL_CACHE_KEY_MAX 256
#endif
#endif

#ifndef SPELL_SPELLCHECKER_TYPEDEF
#define SPELL_SPELLCHECKER_TYPEDEF
    typedef struct SpellChecker SpellChecker;
#endif

    /* Open dictionary, NULL on failure */
    SpellChecker *spell_new(const char *aff_path, const char *dic_path);

    /* Free dictionary and cache */
    void spell_free(SpellChecker *sc);

    /* Check if word is in dictionary (cached) */
    int spell_check(SpellChecker *sc, const char *word);

    /* Clear cache */
    void spell_cache_clear(SpellChecker *sc);

    /* Get suggestions for word */
    char **spell_suggest(SpellChecker *sc, const char *word, int *n_suggestions);
    void spell_free_suggestions(SpellChecker *sc, char **suggestions, int n_suggestions);

    /* Add word to dictionary (invalidates cache) */
    int spell_add_word(SpellChecker *sc, const char *word);
    int spell_remove_word(SpellChecker *sc, const char *word);

    /* Get dictionary encoding */
    const char *spell_get_encoding(SpellChecker *sc);

    /* List *.dic files in directory */
    char **spell_list_dictionaries(const char *search_path, int *n_dicts);
    void spell_free_dictionaries(char **dicts, int n_dicts);

    /* Check if spell checking is available */
    int spell_is_available(void);

    /* Hunspell stem/generate for thesaurus */
    int spell_stem(SpellChecker *sc, const char *word, char ***out_list);
    int spell_generate(SpellChecker *sc, const char *word, const char *example, char ***out_list);
    void spell_free_list(SpellChecker *sc, char **list, int n);

    /* User custom dictionary: one word per line */
    int spell_load_custom(SpellChecker *sc, const char *path);
    int spell_add_to_custom_dict(SpellChecker *sc, const char *word, const char *custom_dict_path);

#ifdef __cplusplus
}
#endif

#endif /* SPELL_H */
