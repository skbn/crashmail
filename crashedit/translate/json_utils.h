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

#ifndef JSON_UTILS_H
#define JSON_UTILS_H

/* Extract a string field from JSON response by key */
char *json_extract_string(const char *json, const char *key);

/* Unescape JSON string (handles \n, \r, \t, \\, \", \uXXXX) */
void json_unescape_string(char *str);

/* Build simple JSON request body for POST */
char *json_build_simple_request(const char **keys, const char **values, int count);

#endif /* JSON_UTILS_H */
