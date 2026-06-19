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

/* ftn.h -- FTN message logic */
#ifndef WRAPPER_FTN_H
#define WRAPPER_FTN_H

#include <stdint.h>
#include <wchar.h>
#include "msgbase.h"

/* Line type classification */

#define FTN_LT_NORMAL 0x0000
#define FTN_LT_QUOTE1 0x0001
#define FTN_LT_QUOTE2 0x0002
#define FTN_LT_QUOTE3 0x0004
#define FTN_LT_QUOTE4 0x0008
#define FTN_LT_QUOTE 0x000F   /* any quote */
#define FTN_LT_KLUDGE 0x0010  /* ^A kludge */
#define FTN_LT_TEAR 0x0020    /* --- tear */
#define FTN_LT_ORIGIN 0x0040  /* " * Origin:" */
#define FTN_LT_SEENBY 0x0080  /* SEEN-BY: */
#define FTN_LT_VIA 0x0100     /* Via / Recd */
#define FTN_LT_PATH 0x0200    /* PATH: (FTS-5005) */
#define FTN_LT_TAGLINE 0x0400 /* "... " tagline (visible, kept in msg) */
#define FTN_LT_CTRL 0x03F0    /* any control line */
#define FTN_LT_HIDDEN (FTN_LT_KLUDGE | FTN_LT_SEENBY | FTN_LT_VIA | FTN_LT_PATH)

#define FTN_SORT_PRESETS 5

extern const char *ftn_sort_presets[FTN_SORT_PRESETS];
extern const char *ftn_sort_labels[FTN_SORT_PRESETS];

/* Area entry for sorting (caller provides this) */
typedef struct
{
    const char *name;
    const char *desc;
    int total;
    int unread;
} FtnAreaInfo;

/* Classify a line (len = byte count) */
int ftn_classify_line(const char *line, int len);

/* Quote depth (0 = not quoted) */
int ftn_quote_depth(const char *line, int len);

/* Extract quote string from line (GoldED+ GetQuotestr) */
int ftn_get_quotestr(const char *line, int len, char *qbuf, int qbufsz);

/* Check if FTN control line (kludge, SEEN-BY, tear, origin, Via) */
int ftn_is_control(const char *line, int len);

/* True if line starts with known FTN kludge (^A, @, or bare-keyword) */
int ftn_is_kludge_line(const char *line, int len, char *out_kw, int out_max);

/* FTN kludge keywords table */
extern const char *const ftn_kludge_names[];

/* Kludge handling */
const char *ftn_next_line(const char *p, int *line_len);
void ftn_extract_kludges(const char *body, char **kludges_out, char **clean_out);
char *ftn_inject_kludges(const char *saved_kludges, const char *charset, const char *body);
const char *ftn_find_msgid(const char *body);
int ftn_get_kludge_value(const char *body, const char *kludge, char *out, int outsz);

/* Quoting functions */
char *ftn_quote_body(const char *body);
char *ftn_quote_body_named_wrap(const char *body, const char *from_name, int margin);
char *ftn_quote_body_named(const char *body, const char *from_name);
char *ftn_quote_body_named_full_wrap(const char *body, const char *from_name, int margin);

/* Convenience: same as _full_wrap with margin=75 */
char *ftn_quote_body_named_full(const char *body, const char *from_name);

/* FTN address parsing (0 if not parseable) */
int ftn_addr_zone(const char *addr);
int ftn_addr_net(const char *addr);

/* Parse node number from "z:n/f.p" (0 if not parseable) */
int ftn_addr_node(const char *addr);

/* Parse point number from "z:n/f.p" (0 if absent or not parseable) */
int ftn_addr_point(const char *addr);

/* Find first AKA matching zone of dest_addr (returns index or -1) */
int ftn_aka_match_zone(const char *akas_base, int aka_count, size_t stride, const char *dest_addr);

/* Pick random non-empty line from tagline file (skips '#' and ';' comments) */
char *ftn_random_tagline(const char *path);

/* Append GoldED+ signature block to body if no " * Origin:" line present (is_echo only) */
char *ftn_apply_signature(const char *origin, const char *tearline, const char *tagline_file, const char *body, const char *oaddr, int is_echo);

/* Line ending and thread navigation */
void ftn_lf_to_cr(char *body);
int ftn_find_original(const MsgInfo *msgs, int count, int cur_idx);
int ftn_find_original_by_msgid(const MsgInfo *msgs, int count, const char *msgid);
int ftn_find_original_by_msgid(const MsgInfo *msgs, int count, const char *reply_msgid);

/* Find index of first reply to current message (returns index or -1) */
int ftn_find_reply(const MsgInfo *msgs, int count, int cur_idx);

/* Find all replies to current message, fills out[] with raw indices, returns count */
int ftn_find_all_replies(const MsgInfo *msgs, int count, int cur_idx, int *out, int out_max);

/* Find next/prev unread message (msgnum > lastread, returns index or -1) */
int ftn_next_unread(const MsgInfo *msgs, int count, int cur_idx, uint32_t lastread);

int ftn_prev_unread(const MsgInfo *msgs, int count, int cur_idx, uint32_t lastread);

/* Area sorting (GoldED+ style: E=echo, U=unread, u=has-unread, T=total, O=original, -=reverse) */
int ftn_sort_areas(const FtnAreaInfo *areas, int count, const char *spec, const char *filter, int *order_out);

/* Timezone kludge (^ATZUTC) */
int ftn_detect_timezone_offset();                               /* returns minutes (+east/-west) */
int ftn_effective_tz_offset(int cfg_offset, int cfg_is_manual); /* auto unless manual / Amiga */
void ftn_build_tzutc(int offset_mins, char *out, int outsz);
void ftn_build_tzutc(int utc_offset_mins, char *out, int outsz);

/* Build ^AMSGID: kludge (addr: FTN address or NULL->"0:0/0", output ends with \r) */
void ftn_build_msgid(const char *addr, char *out, int outsz);

/* Build ^AREPLY: kludge (msgid_value: value without "^AMSGID:", NULL/empty = no kludge) */
void ftn_build_reply(const char *msgid_value, char *out, int outsz);

/* Extract FTN address from " * Origin: ... (zone:net/node.point)" line (0=success, -1=error) */
int ftn_find_origin_address(const char *body, char *out, int outsz);

int ftn_classify_wcs(const wchar_t *line, int len);

int ftn_quote_depth_wcs(const wchar_t *line, int len);

#endif
