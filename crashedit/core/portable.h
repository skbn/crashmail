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

#ifndef CE_PORTABLE_H
#define CE_PORTABLE_H

#include <stddef.h>
#include <time.h>

typedef struct PfDir PfDir;
typedef struct PfLockFile PfLockFile;

PfDir *pf_dir_open(const char *path);
const char *pf_dir_next(PfDir *d);
void pf_dir_close(PfDir *d);

PfLockFile *pf_lock_create(const char *path);
void pf_lock_release(PfLockFile *lk);

int pf_atomic_rename(const char *from, const char *to);
int pf_remove_file(const char *path);

int port_mkdir_one(const char *path);
int pf_mkdir_path(const char *path);
int pf_ensure_dir(const char *path);

int port_file_create_empty(const char *path);
void port_get_config_dir(char *buf, size_t bufsz);

int pf_path_exists(const char *path);
int pf_is_directory(const char *path);
int pf_is_regular_file(const char *path);

int pf_copy_file(const char *src, const char *dst);
int pf_move_file(const char *src, const char *dst);
long pf_get_file_size(const char *path);
long pf_get_file_mtime(const char *path);

void pf_path_join(char *out, size_t outsize, const char *base, const char *sub);
void pf_safe_strncpy(char *dst, const char *src, size_t dstsize);

int pf_wildmatch(const char *pat, const char *str);
int pf_is_wildcard(const char *s);

void pf_sleep_ms(unsigned ms);

#ifdef PLATFORM_AMIGA
/* Sanitize UTF-8 filename to ASCII for AmigaOS filesystem */
char *port_sanitize_filename(const char *utf8_name);
#endif

#endif
