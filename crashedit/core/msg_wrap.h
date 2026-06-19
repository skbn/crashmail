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

/* msg_wrap.h -- FTS-1 *.MSG (Opus/SDM) backend */

#ifndef CE_MSG_WRAP_H
#define CE_MSG_WRAP_H

#include <stdint.h>
#include "portable.h"
#include "jam_wrap.h" /* JamMsgInfo */

/* Public constants */
#define MSG_STORED_HDR_SIZE 190
#define MSG_LOCK_FILENAME "crashedit.lock"
#define MSG_LR_FILENAME "crashedit.lr"
#define MSG_LR_MAGIC 0x524C5243UL /* 'CRLR' little-endian */
#define MSG_LR_VERSION 1

/* Leading body bytes msg_load_headers reads when scanning kludges; 4 KB is larger than any sane kludge block */
#define MSG_KLUDGE_SCAN_BYTES 4096

typedef struct
{
    char path[256];
    int is_open;
    int is_locked;
    PfLockFile *lock;

    /* Sorted cache of valid msgnums on disk (>= 2; 1 is HWM) */
    uint32_t *nums;
    int nums_count;
    int nums_cap;
    uint32_t low_msg;
    uint32_t high_msg;

    /* HighWater Mark from 1.msg.ReplyTo; new msgnums must exceed this */
    uint32_t hwm;
} MsgArea;

int msg_open(MsgArea *a, const char *path);
void msg_close(MsgArea *a);

int msg_lock(MsgArea *a, int retries);
void msg_unlock(MsgArea *a);

char *msg_read_body(MsgArea *a, uint32_t msgnum, uint32_t *out_len);

int msg_read_lastread_pair(MsgArea *a, uint32_t ucrc, uint32_t *out_last, uint32_t *out_high);
int msg_write_lastread(MsgArea *a, uint32_t ucrc, uint32_t last, uint32_t high);

uint32_t msg_write_msg(MsgArea *a, const char *from, const char *to, const char *subject, const char *body, uint32_t bodylen, uint32_t attr, uint32_t reply_to, uint32_t date_written, const char *oaddr, const char *daddr);

int msg_delete_msg(MsgArea *a, uint32_t msgnum);
int msg_mark_sent(MsgArea *a, uint32_t msgnum);

JamMsgInfo *msg_load_headers(MsgArea *a, int *out_count, uint32_t filter_mask, uint32_t max_msgs);

int msg_count_msgs(MsgArea *a, uint32_t lastread, uint32_t lastseen, int *out_total, int *out_unread, int *out_new);

#endif
