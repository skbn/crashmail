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

/* attach.h -- FTN file attachment handling */
#ifndef WRAPPER_ATTACH_H
#define WRAPPER_ATTACH_H

#include <stdint.h>

#define MAX_ATTACHMENTS 32

/* FTS-1 subject line length limit (71 chars, as used by golded-plus) */
#define ATTACH_SUBJ_LIMIT 71

/* Whether list represents file-attach or file-request messages */
typedef enum
{
    ATTACH_MODE_ATTACH = 0,
    ATTACH_MODE_FILEREQUEST = 1
} AttachMode;

typedef struct
{
    char *path;    /* File path (dynamically allocated) */
    uint32_t size; /* File size in bytes */
} AttachEntry;

typedef struct
{
    AttachEntry entries[MAX_ATTACHMENTS];
    int count;
    AttachMode mode; /* attach vs filerequest */
} AttachList;

AttachList *attach_new(void);
void attach_free(AttachList *list);

/* Attachment operations (0=success, -1=error) */
int attach_add(AttachList *list, const char *path);
int attach_remove(AttachList *list, int index);
void attach_clear(AttachList *list);

/* Mode getters/setters (does not affect entries, only JAMATTR flag) */
void attach_set_mode(AttachList *list, AttachMode mode);
AttachMode attach_get_mode(const AttachList *list);

/* Subject parsing/building: build_subject returns all filenames joined, build_subjects splits into limit-capped subjects */
int attach_parse_from_subject(AttachList *list, const char *subject);
char *attach_build_subject(const AttachList *list);

char **attach_build_subjects(const AttachList *list, int *out_count);
void attach_free_subjects(char **subjects, int count);

#endif /* WRAPPER_ATTACH_H */
