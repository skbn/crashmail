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

/* attach.c -- FTN file attachment handling */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "attach.h"

/* Create/destroy */
AttachList *attach_new()
{
    AttachList *list = (AttachList *)calloc(1, sizeof(AttachList));
    return list;
}

void attach_free(AttachList *list)
{
    if (list)
        free(list);
}

/* Operations */
int attach_add(AttachList *list, const char *path)
{
    FILE *f;
    long size;
    int i;

    if (!list || !path || !path[0])
        return -1;

    /* Guard against fixed-size array overflow */
    if (list->count >= MAX_ATTACHMENTS)
        return -1;

    /* Check if file exists and get size */
    f = fopen(path, "rb");

    if (!f)
        return -1;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);

    if (size < 0)
        return -1;

    /* Check for duplicate */
    for (i = 0; i < list->count; i++)
    {
        if (strcmp(list->entries[i].path, path) == 0)
            return -1; /* Already attached */
    }

    /* Add new attachment */
    list->entries[list->count].path = strdup(path);

    if (!list->entries[list->count].path)
        return -1;

    list->entries[list->count].size = (uint32_t)size;
    list->count++;

    return 0;
}

int attach_remove(AttachList *list, int index)
{
    int i;

    if (!list || index < 0 || index >= list->count)
        return -1;

    /* Free the path */
    free(list->entries[index].path);

    /* Shift remaining entries */
    for (i = index; i < list->count - 1; i++)
    {
        list->entries[i] = list->entries[i + 1];
    }

    list->count--;

    return 0;
}

void attach_clear(AttachList *list)
{
    int i;

    if (!list)
        return;

    for (i = 0; i < list->count; i++)
    {
        free(list->entries[i].path);
        list->entries[i].path = NULL;
    }

    list->count = 0;
}

/* Subject handling */
int attach_parse_from_subject(AttachList *list, const char *subject)
{
    char *buf;
    char *ptr;
    int count = 0;

    if (!list || !subject)
        return 0;

    attach_clear(list);

    /* Copy subject to buffer for tokenization */
    buf = strdup(subject);

    if (!buf)
        return 0;

    /* Tokenize by spaces */
    ptr = strtok(buf, " ");

    while (ptr && list->count < MAX_ATTACHMENTS)
    {
        const char *path = ptr;

        /* Check for ^ prefix (ignore for compatibility) */
        if (*ptr == '^')
            path++;

        /* Skip empty paths */
        if (*path)
        {
            if (attach_add(list, path) == 0)
                count++;
        }

        ptr = strtok(NULL, " ");
    }

    free(buf);
    return count;
}

char *attach_build_subject(const AttachList *list)
{
    char *result = NULL;
    size_t total_len = 0;
    int i;

    if (!list || list->count == 0)
        return NULL;

    /* Calculate total length needed */
    for (i = 0; i < list->count; i++)
    {
        total_len += strlen(list->entries[i].path) + 1; /* path + space */
    }

    if (total_len == 0)
        return NULL;

    result = (char *)malloc(total_len);

    if (!result)
        return NULL;

    result[0] = '\0';

    for (i = 0; i < list->count; i++)
    {
        if (i > 0)
            strcat(result, " ");

        strcat(result, list->entries[i].path);
    }

    return result;
}

static void subj_free(char **subjects, int count)
{
    int j;

    for (j = 0; j < count; j++)
        free(subjects[j]);

    free(subjects);
}

static int subj_flush(char ***subjects, int *count, int *cap, const char *buf)
{
    if (*count >= *cap)
    {
        int new_cap = *cap ? *cap * 2 : 4;
        char **np = (char **)realloc(*subjects, (size_t)new_cap * sizeof(char *));

        if (!np)
            return -1;

        *subjects = np;
        *cap = new_cap;
    }

    (*subjects)[*count] = strdup(buf);

    if (!(*subjects)[*count])
        return -1;

    (*count)++;

    return 0;
}

/* Build subject lines for file-attach msgs: pack filenames
 * space-separated up to ATTACH_SUBJ_LIMIT per subject, one msg per subject */
char **attach_build_subjects(const AttachList *list, int *out_count)
{
    char **subjects = NULL;
    int subj_count = 0;
    int subj_cap = 0;
    char buf[ATTACH_SUBJ_LIMIT + 256]; /* worst-case overrun headroom */
    size_t buf_len = 0;
    int i;

    if (out_count)
        *out_count = 0;

    if (!list || list->count == 0)
        return NULL;

    buf[0] = '\0';

    for (i = 0; i < list->count; i++)
    {
        const char *name = list->entries[i].path ? list->entries[i].path : "";
        size_t name_len;
        size_t want;

        if (!name[0])
            continue;

        name_len = strlen(name);
        /* +1 for the separating space between filenames (only when the
         * buffer already has content) */
        want = buf_len + (buf_len ? 1 : 0) + name_len;

        if (buf_len > 0 && want > (size_t)ATTACH_SUBJ_LIMIT)
        {
            /* Flush current buffer as a finished subject and start a
             * fresh one with this filename */
            if (subj_flush(&subjects, &subj_count, &subj_cap, buf) != 0)
            {
                subj_free(subjects, subj_count);
                return NULL;
            }

            /* Reset buffer with the new filename; oversized names are allowed
             * and flushed on the next iteration */
            memcpy(buf, name, name_len + 1);
            buf_len = name_len;
        }
        else
        {
            if (buf_len > 0)
                buf[buf_len++] = ' ';

            memcpy(buf + buf_len, name, name_len + 1);
            buf_len += name_len;
        }
    }

    /* Flush whatever is still in buf as the final subject */
    if (buf_len > 0)
    {
        if (subj_flush(&subjects, &subj_count, &subj_cap, buf) != 0)
        {
            subj_free(subjects, subj_count);
            return NULL;
        }
    }

    if (subj_count == 0)
    {
        free(subjects);
        return NULL;
    }

    if (out_count)
        *out_count = subj_count;

    return subjects;
}

void attach_free_subjects(char **subjects, int count)
{
    int i;

    if (!subjects)
        return;

    for (i = 0; i < count; i++)
        free(subjects[i]);

    free(subjects);
}
