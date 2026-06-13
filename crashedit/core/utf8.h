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

/* utf8.h -- UTF-8 utilities and codepage conversion */
#ifndef WRAPPER_UTF8_H
#define WRAPPER_UTF8_H
#include <stdint.h>
#include <wchar.h>

int utf8_strlen(const char *s);
int utf8_offset(const char *s, int charpos);
uint32_t utf8_next(const char **p);
int utf8_encode(uint32_t cp, char *buf);

int latin1_to_utf8(const char *src, int srclen, char *dst, int dstmax);
int utf8_to_latin1(const char *src, int srclen, char *dst, int dstmax);

uint32_t cp437_to_unicode(unsigned char c);
uint32_t cp850_to_unicode(unsigned char c);
uint32_t cp865_to_unicode(unsigned char c);
uint32_t cp866_to_unicode(unsigned char c);
uint32_t cp1252_to_unicode(unsigned char c);
uint32_t latin2_to_unicode(unsigned char c);
unsigned char unicode_to_cp437(uint32_t cp);
unsigned char unicode_to_cp850(uint32_t cp);
unsigned char unicode_to_cp865(uint32_t cp);
unsigned char unicode_to_cp866(uint32_t cp);
unsigned char unicode_to_cp1252(uint32_t cp);
unsigned char unicode_to_latin2(uint32_t cp);

/* Convert between named charset and UTF-8 (CP437/850/865/866/1252, LATIN-1/2, IBMPC) */
int charset_to_utf8(const char *cs, const char *src, int srclen, char *dst, int dstmax);
int utf8_to_charset(const char *cs, const char *src, int srclen, char *dst, int dstmax);

/* wchar_t conversion (shared by editor and reader) */

/* UTF-8 <-> wchar_t conversion (caller frees returned memory) */
wchar_t *utf8_to_wcs(const char *utf8, int *out_len);
char *wcs_to_utf8(const wchar_t *wcs, int len);

/* Single-byte charset to Unicode (0x00-0x7F pass-through), NOT for UTF-8 */
uint32_t charset_byte_to_unicode(const char *charset, unsigned char byte);

/* Case-insensitive substring search for wchar_t (returns pointer or NULL, empty needle returns hay) */
const wchar_t *wcs_casestr(const wchar_t *hay, const wchar_t *needle);

/* wcswidth implementation based on Markus Kuhn's wcwidth.c, returns column positions for wide-character string, POSIX.1-2001 compatible */
#if (defined(PLATFORM_AMIGA) || defined(PLATFORM_WIN32)) && !defined(wcswidth)
int wcswidth(const wchar_t *wcs, size_t n);
#endif

#endif
