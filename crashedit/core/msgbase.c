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

/* msgbase.c -- thin dispatcher.  No detection, no fallback */

#include "msgbase.h"

#include <stdlib.h>
#include <string.h>

int mb_open(MsgBase *m, const char *path, int format)
{
    if (!m || !path)
        return -1;

    memset(m, 0, sizeof(*m));

    if (strlen(path) >= sizeof(m->path))
        return -1;

    if (format != MB_FORMAT_JAM && format != MB_FORMAT_MSG)
        return -1;

    strcpy(m->path, path);
    m->format = format;

    if (format == MB_FORMAT_JAM)
    {
        if (jam_open(&m->u.jam, path) != 0)
        {
            m->format = 0;
            m->path[0] = '\0';
            return -1;
        }
    }
    else /* MB_FORMAT_MSG */
    {
        if (msg_open(&m->u.msg, path) != 0)
        {
            m->format = 0;
            m->path[0] = '\0';
            return -1;
        }
    }

    m->is_open = 1;
    return 0;
}

void mb_close(MsgBase *m)
{
    if (!m || !m->is_open)
        return;

    if (m->format == MB_FORMAT_JAM)
        jam_close(&m->u.jam);
    else if (m->format == MB_FORMAT_MSG)
        msg_close(&m->u.msg);

    m->is_open = 0;
    m->format = 0;
    m->path[0] = '\0';
}

int mb_lock(MsgBase *m, int retries)
{
    if (!m || !m->is_open)
        return -1;

    if (m->format == MB_FORMAT_JAM)
        return jam_lock(&m->u.jam, retries);

    return msg_lock(&m->u.msg, retries);
}

void mb_unlock(MsgBase *m)
{
    if (!m || !m->is_open)
        return;

    if (m->format == MB_FORMAT_JAM)
        jam_unlock(&m->u.jam);
    else
        msg_unlock(&m->u.msg);
}

char *mb_read_body(MsgBase *m, uint32_t msgnum, uint32_t *out_len)
{
    if (out_len)
        *out_len = 0;

    if (!m || !m->is_open)
        return NULL;

    if (m->format == MB_FORMAT_JAM)
        return jam_read_body(&m->u.jam, msgnum, out_len);

    return msg_read_body(&m->u.msg, msgnum, out_len);
}

int mb_read_lastread_pair(MsgBase *m, uint32_t ucrc, uint32_t *out_last, uint32_t *out_high)
{
    if (out_last)
        *out_last = 0;

    if (out_high)
        *out_high = 0;

    if (!m || !m->is_open)
        return -1;

    if (m->format == MB_FORMAT_JAM)
        return jam_read_lastread_pair(&m->u.jam, ucrc, out_last, out_high);

    return msg_read_lastread_pair(&m->u.msg, ucrc, out_last, out_high);
}

int mb_write_lastread(MsgBase *m, uint32_t ucrc, uint32_t last, uint32_t high)
{
    if (!m || !m->is_open)
        return -1;

    if (m->format == MB_FORMAT_JAM)
        return jam_write_lastread(&m->u.jam, ucrc, last, high);

    return msg_write_lastread(&m->u.msg, ucrc, last, high);
}

uint32_t mb_write_msg(MsgBase *m, const char *from, const char *to, const char *subject, const char *body, uint32_t bodylen, uint32_t attr, uint32_t reply_to, uint32_t date_written, const char *oaddr, const char *daddr)
{
    if (!m || !m->is_open)
        return 0;

    if (m->format == MB_FORMAT_JAM)
        return jam_write_msg(&m->u.jam, from, to, subject, body, bodylen, attr, reply_to, date_written, oaddr, daddr);

    return msg_write_msg(&m->u.msg, from, to, subject, body, bodylen, attr, reply_to, date_written, oaddr, daddr);
}

int mb_delete_msg(MsgBase *m, uint32_t msgnum)
{
    if (!m || !m->is_open)
        return -1;

    if (m->format == MB_FORMAT_JAM)
        return jam_delete_msg(&m->u.jam, msgnum);

    return msg_delete_msg(&m->u.msg, msgnum);
}

int mb_mark_sent(MsgBase *m, uint32_t msgnum)
{
    if (!m || !m->is_open)
        return -1;

    if (m->format == MB_FORMAT_JAM)
        return jam_mark_sent(&m->u.jam, msgnum);

    return msg_mark_sent(&m->u.msg, msgnum);
}

MsgInfo *mb_load_headers(MsgBase *m, int *out_count, uint32_t filter_mask, uint32_t max_msgs)
{
    if (out_count)
        *out_count = 0;

    if (!m || !m->is_open || !out_count)
        return NULL;

    if (m->format == MB_FORMAT_JAM)
        return jam_load_headers(&m->u.jam, out_count, filter_mask, max_msgs);

    return msg_load_headers(&m->u.msg, out_count, filter_mask, max_msgs);
}

int mb_count_msgs(MsgBase *m, uint32_t lastread, uint32_t lastseen, int *out_total, int *out_unread, int *out_new)
{
    if (out_total)
        *out_total = 0;

    if (out_unread)
        *out_unread = 0;

    if (out_new)
        *out_new = 0;

    if (!m || !m->is_open)
        return -1;

    if (m->format == MB_FORMAT_JAM)
        return jam_count_msgs(&m->u.jam, lastread, lastseen, out_total, out_unread, out_new);

    return msg_count_msgs(&m->u.msg, lastread, lastseen, out_total, out_unread, out_new);
}

uint32_t mb_username_crc(const char *name)
{
    return jam_username_crc(name);
}

int mb_find_by_msgnum(const MsgInfo *msgs, int count, uint32_t msgnum)
{
    return jam_find_by_msgnum(msgs, count, msgnum);
}
