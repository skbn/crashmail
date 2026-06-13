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

/* jam_wrap.h -- Safe JAM message base wrapper */
#ifndef WRAPPER_JAM_WRAP_H
#define WRAPPER_JAM_WRAP_H

#include <stdint.h>
#include <stddef.h>

#define JAM_FIELD_MAX 80
#define JAM_LOCK_RETRIES 5
#define JAM_LOCK_DELAY_MS 200
#define JAM_MAX_MSGS 500000

/* JAM message attributes */
#define JAMATTR_FILEREQUEST 0x00001000L
#define JAMATTR_FILEATTACH 0x00002000L

typedef struct
{
    uint32_t msgnum;
    uint32_t attr;
    uint32_t date_written;
    uint32_t txt_offset;
    uint32_t txt_len;
    uint32_t reply_to;
    uint32_t reply_crc; /* ReplyCRC from JAM header for fallback linking */
    uint32_t msgid_crc; /* MsgIdCRC from JAM header for fallback linking */
    char from[JAM_FIELD_MAX];
    char to[JAM_FIELD_MAX];
    char subject[JAM_FIELD_MAX];
    char oaddress[JAM_FIELD_MAX];
    char daddress[JAM_FIELD_MAX];
    char msgid[JAM_FIELD_MAX];
    int tzutc_offset; /* TZUTC offset in minutes, -1 if not available */
} JamMsgInfo;

typedef struct
{
    void *base;      /* s_JamBase* -- opaque */
    void *hdr_cache; /* s_JamBaseHeader* -- opaque */
    char path[256];
    uint32_t msg_count;
    int is_open;
    int is_locked;
} JamArea;

int jam_open(JamArea *a, const char *path);
void jam_close(JamArea *a);
int jam_lock(JamArea *a, int retries);
void jam_unlock(JamArea *a);

/* Read (no lock needed) */
char *jam_read_body(JamArea *a, uint32_t msgnum, uint32_t *out_len);
uint32_t jam_read_lastread(JamArea *a, uint32_t user_crc);
int jam_read_lastread_pair(JamArea *a, uint32_t user_crc, uint32_t *out_last, uint32_t *out_high);

/* Write (lock required) */
int jam_write_lastread(JamArea *a, uint32_t ucrc, uint32_t last, uint32_t high);
uint32_t jam_write_msg(JamArea *a, const char *from, const char *to, const char *subject, const char *body, uint32_t bodylen, uint32_t attr, uint32_t reply_to, uint32_t date_written, const char *oaddr, const char *daddr);
int jam_delete_msg(JamArea *a, uint32_t msgnum);
int jam_mark_sent(JamArea *a, uint32_t msgnum);

/* Bulk read: loads non-deleted headers (caller frees, filter_mask=extra bits to skip, max_msgs=0 means all else last N) */
JamMsgInfo *jam_load_headers(JamArea *a, int *out_count, uint32_t filter_mask, uint32_t max_msgs);

/* Lightweight counting: reads only fixed-size headers (no subfield I/O, faster than jam_load_headers) */
int jam_count_msgs(JamArea *a, uint32_t lastread, uint32_t lastseen, int *out_total, int *out_unread, int *out_new);

/* Utilities */
uint32_t jam_username_crc(const char *name);
int jam_find_by_msgnum(const JamMsgInfo *msgs, int count, uint32_t msgnum);

#endif
