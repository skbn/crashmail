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

#ifndef WRAPPER_FREQ_H
#define WRAPPER_FREQ_H

/* Values shared with config parser (OUTBOUNDMODE keyword) and freq popup, UNSET means not configured */
#define FREQ_MODE_UNSET 0
#define FREQ_MODE_ASO 1
#define FREQ_MODE_BSO 2
#define FREQ_MODE_BSO_EXT 3

/* Write a file request */
#define FREQ_MAX_PATH 1024

int freq_write(const char *outbound, int mode, unsigned int zone, unsigned int net, unsigned int node, unsigned int point, char *const *filenames, int nfiles, const char *password, long newer_than, int update, char *out_req_path, int out_req_path_sz);

/* Parse "z:n/f" or "z:n/f.p" into components */
int freq_parse_addr(const char *addr, unsigned int *zone, unsigned int *net, unsigned int *node, unsigned int *point);

#endif /* WRAPPER_FREQ_H */
