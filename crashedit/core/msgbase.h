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

#ifndef CE_MSGBASE_H
#define CE_MSGBASE_H

#include <stdint.h>
#include "jam_wrap.h"
#include "msg_wrap.h"

#define MB_FORMAT_JAM 1
#define MB_FORMAT_MSG 2

#define MB_LOCK_RETRIES 5

typedef JamMsgInfo MsgInfo;

#define MB_FIELD_MAX JAM_FIELD_MAX
#define MBATTR_FILEREQUEST JAMATTR_FILEREQUEST
#define MBATTR_FILEATTACH JAMATTR_FILEATTACH

typedef struct
{
    int format; /* MB_FORMAT_JAM | MB_FORMAT_MSG */
    int is_open;
    char path[256];

    union
    {
        JamArea jam;
        MsgArea msg;
    } u;
} MsgBase;

int mb_open(MsgBase *m, const char *path, int format);
void mb_close(MsgBase *m);

int mb_lock(MsgBase *m, int retries);
void mb_unlock(MsgBase *m);

char *mb_read_body(MsgBase *m, uint32_t msgnum, uint32_t *out_len);

int mb_read_lastread_pair(MsgBase *m, uint32_t ucrc, uint32_t *out_last, uint32_t *out_high);
int mb_write_lastread(MsgBase *m, uint32_t ucrc, uint32_t last, uint32_t high);

uint32_t mb_write_msg(MsgBase *m, const char *from, const char *to, const char *subject, const char *body, uint32_t bodylen, uint32_t attr, uint32_t reply_to, uint32_t date_written, const char *oaddr, const char *daddr);

int mb_delete_msg(MsgBase *m, uint32_t msgnum);
int mb_mark_sent(MsgBase *m, uint32_t msgnum);

MsgInfo *mb_load_headers(MsgBase *m, int *out_count, uint32_t filter_mask, uint32_t max_msgs);
int mb_count_msgs(MsgBase *m, uint32_t lastread, uint32_t lastseen, int *out_total, int *out_unread, int *out_new);

uint32_t mb_username_crc(const char *name);
int mb_find_by_msgnum(const MsgInfo *msgs, int count, uint32_t msgnum);

#endif
