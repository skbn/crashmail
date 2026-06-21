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

/* jam_wrap.c -- Safe JAM wrapper */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include "../../src/jamlib/jam.h"
#include "jam_wrap.h"

#define BASE(a) ((s_JamBase *)(a)->base)
#define BHDR(a) ((s_JamBaseHeader *)(a)->hdr_cache)

#define KBUF_CAP 16384
#define VBUF_CAP 8192

/* CRC32 (matches jamlib/crc32.c exactly) */
static uint32_t s_crc_tab[256];
static int s_crc_ready = 0;

static void crc_init(void)
{
    uint32_t i, j, c;

    if (s_crc_ready)
        return;

    for (i = 0; i < 256; i++)
    {
        c = i;

        for (j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);

        s_crc_tab[i] = c;
    }

    s_crc_ready = 1;
}

static uint32_t crc32_str(const char *s, int len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    int i;

    crc_init();

    for (i = 0; i < len; i++)
        crc = (crc >> 8) ^ s_crc_tab[(uint8_t)(crc ^ (uint8_t)s[i])];

    return crc;
}

static void safe_copy(char *d, size_t dsz, const char *s, uint32_t slen)
{
    uint32_t n;

    if (dsz == 0)
        return; /* prevent underflow of dsz-1 */

    n = slen < (uint32_t)(dsz - 1) ? slen : (uint32_t)(dsz - 1);

    if (n > 0)
        memcpy(d, s, n);

    d[n] = '\0';
}

/* O(1) slot lookup using BaseMsgNum, with linear scan fallback */
static uint32_t find_slot(JamArea *a, uint32_t msgnum)
{
    uint32_t total, n, candidate_slot;
    s_JamMsgHeader h;

    if (!a || !a->is_open || msgnum == 0)
        return (uint32_t)-1;

    if (JAM_ReadMBHeader(BASE(a), BHDR(a)) != 0)
        return (uint32_t)-1;

    if (JAM_GetMBSize(BASE(a), &total) != 0)
        return (uint32_t)-1;

    /* O(1) calculation: in JAM, slot = msgnum - BaseMsgNum */
    candidate_slot = msgnum - BHDR(a)->BaseMsgNum;

    /* Try direct read first (1 disk access instead of N) */
    if (candidate_slot < total)
    {
        memset(&h, 0, sizeof(h));

        if (JAM_ReadMsgHeader(BASE(a), candidate_slot, &h, NULL) == 0 && h.MsgNum == msgnum)
            return candidate_slot;
    }

    /* Fallback: linear scan (handles reorganized bases with gaps) */
    for (n = 0; n < total; n++)
    {
        memset(&h, 0, sizeof(h));

        if (JAM_ReadMsgHeader(BASE(a), n, &h, NULL) == 0 && h.MsgNum == msgnum)
            return n;
    }

    return (uint32_t)-1;
}

/* Extract subfields from JAM subpacket into JamMsgInfo */
static void extract_sf(void *vsp, JamMsgInfo *info)
{
    s_JamSubPacket *sp = (s_JamSubPacket *)vsp;
    uint32_t i;

    if (!sp || !info)
        return;

    info->tzutc_offset = -1; /* Default: not available */

    for (i = 0; i < sp->NumFields; i++)
    {
        s_JamSubfield *sf = sp->Fields[i];

        if (!sf || !sf->Buffer)
            continue;

        switch (sf->LoID)
        {
        case JAMSFLD_SENDERNAME:
            safe_copy(info->from, sizeof(info->from), sf->Buffer, sf->DatLen);
            break;
        case JAMSFLD_RECVRNAME:
            safe_copy(info->to, sizeof(info->to), sf->Buffer, sf->DatLen);
            break;
        case JAMSFLD_SUBJECT:
            safe_copy(info->subject, sizeof(info->subject), sf->Buffer, sf->DatLen);
            break;
        case JAMSFLD_OADDRESS:
            safe_copy(info->oaddress, sizeof(info->oaddress), sf->Buffer, sf->DatLen);
            break;
        case JAMSFLD_DADDRESS:
            safe_copy(info->daddress, sizeof(info->daddress), sf->Buffer, sf->DatLen);
            break;
        case JAMSFLD_MSGID:
            safe_copy(info->msgid, sizeof(info->msgid), sf->Buffer, sf->DatLen);
            break;
        case JAMSFLD_TZUTCINFO:
            /* Parse TZUTC: format is "+HHMM" or "-HHMM" */
            if (sf->DatLen >= 5)
            {
                char sign = sf->Buffer[0];
                int hours, mins;

                if (sign == '+' || sign == '-')
                {
                    hours = (sf->Buffer[1] - '0') * 10 + (sf->Buffer[2] - '0');
                    mins = (sf->Buffer[3] - '0') * 10 + (sf->Buffer[4] - '0');
                    info->tzutc_offset = (hours * 60 + mins) * (sign == '-' ? -1 : 1);
                }
            }
            break;
        case JAMSFLD_FTSKLUDGE:
            /* crashmail stores unrecognized kludges (including TZUTC) here, format is "TZUTC: +HHMM" */
            if (sf->DatLen >= 8 && sf->Buffer[0] == 'T' && sf->Buffer[1] == 'Z' && sf->Buffer[2] == 'U' && sf->Buffer[3] == 'T' && sf->Buffer[4] == 'C' && sf->Buffer[5] == ':' && sf->Buffer[6] == ' ')
            {
                char sign = sf->Buffer[7];

                if ((sign == '+' || sign == '-') && sf->DatLen >= 12)
                {
                    int hours = (sf->Buffer[8] - '0') * 10 + (sf->Buffer[9] - '0');
                    int mins = (sf->Buffer[10] - '0') * 10 + (sf->Buffer[11] - '0');

                    info->tzutc_offset = (hours * 60 + mins) * (sign == '-' ? -1 : 1);
                }
            }
            break;
        default:
            break;
        }
    }
}

int jam_open(JamArea *a, const char *path)
{
    s_JamBase *b = NULL;
    s_JamBaseHeader *bh = NULL;

    if (!a || !path)
        return -1;

    memset(a, 0, sizeof(*a));
    strncpy(a->path, path, sizeof(a->path) - 1);

    bh = (s_JamBaseHeader *)malloc(sizeof(s_JamBaseHeader));

    if (!bh)
        return -1;

    memset(bh, 0, sizeof(*bh));
    a->hdr_cache = bh;

    if (JAM_OpenMB((char *)path, &b) != 0)
    {
        if (b)
        {
            JAM_CloseMB(b);
            free(b);
        }

        free(bh);

        a->hdr_cache = NULL;

        return -1;
    }

    a->base = b;

    if (JAM_ReadMBHeader(b, bh) != 0)
    {
        JAM_CloseMB(b);

        free(b);
        free(bh);

        a->base = NULL;
        a->hdr_cache = NULL;

        return -1;
    }

    a->msg_count = bh->ActiveMsgs;
    a->is_open = 1;

    return 0;
}

void jam_close(JamArea *a)
{
    if (!a || !a->is_open)
        return;

    if (a->is_locked)
        jam_unlock(a);

    JAM_CloseMB(BASE(a));

    free(a->base);

    a->base = NULL;
    a->is_open = 0;

    free(a->hdr_cache);

    a->hdr_cache = NULL;
}

int jam_lock(JamArea *a, int retries)
{
    int i;

    if (!a || !a->is_open)
        return -1;

    if (a->is_locked)
        return 0;

    for (i = 0; i <= retries; i++)
    {
        if (JAM_LockMB(BASE(a), 0) == 0)
        {
            a->is_locked = 1;
            JAM_ReadMBHeader(BASE(a), BHDR(a));
            return 0;
        }

        if (i < retries)
        {
#ifdef PLATFORM_AMIGA
            extern void Delay(long);
            Delay(10);
#elif defined(PLATFORM_WIN32)

            /* mingw has no nanosleep; use the Win32 Sleep (ms)
             * Declared locally to avoid pulling all of windows.h
             * into this translation unit */
            extern void __stdcall Sleep(unsigned long);
            Sleep(200);
#else
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 200000000L;
            nanosleep(&ts, NULL);

#endif
        }
    }

    return -1;
}

void jam_unlock(JamArea *a)
{
    s_JamBase *b = NULL;

    if (!a || !a->is_locked)
        return;

    b = BASE(a);

    if (b)
    {
        if (b->HdrFile_PS)
            fflush(b->HdrFile_PS);

        if (b->TxtFile_PS)
            fflush(b->TxtFile_PS);

        if (b->IdxFile_PS)
            fflush(b->IdxFile_PS);

        if (b->LrdFile_PS)
            fflush(b->LrdFile_PS);
    }

    JAM_UnlockMB(b);

    a->is_locked = 0;
}

/* Read functions */
/* Build kludge prefix from JAM subfields (FTS-1: ^A kludges prepended, SEEN-BY/PATH/Via appended) */
static char *build_body_with_kludges(s_JamSubPacket *sp, const char *txt, uint32_t tl, uint32_t *out_len)
{
    char *kbuf = NULL; /* ^A kludges */
    char *bbuf = NULL; /* SEEN-BY/PATH/Via */
    char *vbuf = NULL;
    int klen = 0, blen = 0;
    uint32_t i;
    char *result = NULL;

    kbuf = (char *)malloc(KBUF_CAP);
    bbuf = (char *)malloc(KBUF_CAP);
    vbuf = (char *)malloc(VBUF_CAP);

    if (!kbuf || !bbuf || !vbuf)
    {
        free(kbuf);
        free(bbuf);
        free(vbuf);

        return NULL;
    }

    if (sp)
    {
        for (i = 0; i < sp->NumFields; i++)
        {
            s_JamSubfield *sf = sp->Fields[i];
            const char *tag = NULL;
            char enc = ' ';
            char *target = NULL;
            int *tgt_len = NULL;
            int vlen, wrote;

            if (!sf || !sf->Buffer || sf->DatLen == 0)
                continue;

            /* Format subfields: K=^A kludge->top, R=FTSKLUDGE->top, S=SEEN-BY/PATH->bottom */
            switch (sf->LoID)
            {
            case JAMSFLD_MSGID:
                tag = "MSGID";
                enc = 'K';
                break;
            case JAMSFLD_REPLYID:
                tag = "REPLY";
                enc = 'K';
                break;
            case JAMSFLD_PID:
                tag = "PID";
                enc = 'K';
                break;
            case JAMSFLD_TZUTCINFO:
                tag = "TZUTC";
                enc = 'K';
                break;
            case JAMSFLD_TRACE:
                tag = "Via";
                enc = 'K';
                break;
            case JAMSFLD_SEENBY2D:
                tag = "SEEN-BY:";
                enc = 'S';
                break;
            case JAMSFLD_PATH2D:
                tag = "PATH:";
                enc = 'S';
                break;
            case JAMSFLD_FTSKLUDGE:
                tag = "";
                enc = 'R';
                break;
            default:
                break;
            }

            if (enc == ' ')
                continue;

            vlen = (sf->DatLen < VBUF_CAP - 1) ? (int)sf->DatLen : (int)(VBUF_CAP - 1);

            memcpy(vbuf, sf->Buffer, (size_t)vlen);
            vbuf[vlen] = '\0';

            /* Choose target buffer (top vs bottom) */
            if (enc == 'S')
            {
                target = bbuf;
                tgt_len = &blen;
            }
            else
            {
                target = kbuf;
                tgt_len = &klen;
            }

            if (*tgt_len >= KBUF_CAP - VBUF_CAP - 32)
                continue;

            if (enc == 'R')
            {
                /* FTSKLUDGE stored WITHOUT ^A; prepend if missing */
                int has_ctrla = (vlen > 0 && (unsigned char)vbuf[0] == 0x01);
                wrote = vlen + (has_ctrla ? 0 : 1);

                if (*tgt_len + wrote + 2 < KBUF_CAP)
                {
                    if (!has_ctrla)
                        target[(*tgt_len)++] = (char)0x01;

                    memcpy(target + *tgt_len, vbuf, (size_t)vlen);

                    *tgt_len += vlen;
                    target[(*tgt_len)++] = '\r';
                }
            }
            else if (enc == 'K')
            {
                wrote = snprintf(target + *tgt_len, (size_t)(KBUF_CAP - *tgt_len), "\x01%s: %s\r", tag, vbuf);

                if (wrote > 0)
                    *tgt_len += wrote;
            }
            else /* 'S' = plain-text SEEN-BY/PATH (no ^A) */
            {
                wrote = snprintf(target + *tgt_len, (size_t)(KBUF_CAP - *tgt_len), "%s %s\r", tag, vbuf);

                if (wrote > 0)
                    *tgt_len += wrote;
            }
        }
    }

    if (klen == 0 && blen == 0)
    {
        /* No subfields recovered -- return body as-is */
        result = (char *)malloc(tl + 1);

        if (result)
        {
            if (tl > 0)
                memcpy(result, txt, tl);

            result[tl] = '\0';

            if (out_len)
                *out_len = tl;
        }

        free(kbuf);
        free(bbuf);
        free(vbuf);

        return result;
    }

    result = (char *)malloc((size_t)klen + tl + (size_t)blen + 1);

    if (result)
    {
        char *p = result;

        if (klen > 0)
        {
            memcpy(p, kbuf, (size_t)klen);
            p += klen;
        }

        if (tl > 0)
        {
            memcpy(p, txt, tl);
            p += tl;
        }

        if (blen > 0)
        {
            memcpy(p, bbuf, (size_t)blen);
            p += blen;
        }

        *p = '\0';

        if (out_len)
            *out_len = (uint32_t)(klen + tl + blen);
    }

    free(kbuf);
    free(bbuf);
    free(vbuf);

    return result;
}

char *jam_read_body(JamArea *a, uint32_t msgnum, uint32_t *out_len)
{
    s_JamMsgHeader h;
    s_JamSubPacket *sp = NULL;
    uint32_t slot, tl;
    char *txt, *result;

    if (out_len)
        *out_len = 0;

    if (!a || !a->is_open)
        return NULL;

    slot = find_slot(a, msgnum);

    if (slot == (uint32_t)-1)
        return NULL;

    sp = JAM_NewSubPacket();

    if (!sp)
        return NULL;

    memset(&h, 0, sizeof(h));

    if (JAM_ReadMsgHeader(BASE(a), slot, &h, &sp) != 0)
    {
        JAM_DelSubPacket(sp);
        return NULL;
    }

    if (h.MsgNum != msgnum || (h.Attribute & MSG_DELETED))
    {
        JAM_DelSubPacket(sp);
        return NULL;
    }

    tl = h.TxtLen;

    if (tl == 0)
    {
        result = build_body_with_kludges(sp, "", 0, out_len);
        JAM_DelSubPacket(sp);

        return result;
    }

    txt = (char *)malloc(tl + 1);

    if (!txt)
    {
        JAM_DelSubPacket(sp);
        return NULL;
    }

    if (JAM_ReadMsgText(BASE(a), h.TxtOffset, tl, txt) != 0)
    {
        free(txt);
        JAM_DelSubPacket(sp);

        return NULL;
    }

    txt[tl] = '\0';

    result = build_body_with_kludges(sp, txt, tl, out_len);

    free(txt);

    JAM_DelSubPacket(sp);

    return result;
}

uint32_t jam_read_lastread(JamArea *a, uint32_t ucrc)
{
    s_JamLastRead lr;

    if (!a || !a->is_open)
        return 0;

    if (JAM_ReadLastRead(BASE(a), ucrc, &lr) != 0)
        return 0;

    return lr.LastReadMsg;
}

/* Read LastReadMsg (reader's unread) and HighReadMsg (msglist's seen/asterisk) */
int jam_read_lastread_pair(JamArea *a, uint32_t ucrc, uint32_t *out_last, uint32_t *out_high)
{
    s_JamLastRead lr;

    if (out_last)
        *out_last = 0;

    if (out_high)
        *out_high = 0;

    if (!a || !a->is_open)
        return -1;

    if (JAM_ReadLastRead(BASE(a), ucrc, &lr) != 0)
        return -1;

    if (out_last)
        *out_last = lr.LastReadMsg;

    if (out_high)
        *out_high = lr.HighReadMsg;

    return 0;
}

/* Write */
int jam_write_lastread(JamArea *a, uint32_t ucrc, uint32_t last, uint32_t high)
{
    s_JamLastRead lr;

    if (!a || !a->is_open || !a->is_locked)
        return -1;

    if (JAM_ReadLastRead(BASE(a), ucrc, &lr) != 0)
        memset(&lr, 0, sizeof(lr));

    /* Use CRC as UserID since we only have one local user */
    lr.UserCRC = ucrc;
    lr.UserID = ucrc;
    lr.LastReadMsg = last;
    lr.HighReadMsg = high;

    return (JAM_WriteLastRead(BASE(a), ucrc, &lr) == 0) ? 0 : -1;
}

uint32_t jam_write_msg(JamArea *a, const char *from, const char *to, const char *subject, const char *body, uint32_t bodylen, uint32_t attr, uint32_t reply_to, uint32_t date_written, const char *oaddr, const char *daddr)
{
    s_JamMsgHeader h;
    s_JamSubPacket *sp = NULL;
    s_JamSubfield sf;
    s_JamMsgHeader tmp;
    uint32_t newmsg;
    time_t now;
    uint32_t utc;
    uint32_t tot;

    if (!a || !a->is_open || !a->is_locked)
        return 0;

    if (body && bodylen == 0)
        bodylen = (uint32_t)strlen(body);

    sp = JAM_NewSubPacket();

    if (!sp)
        return 0;

#define ADD_SF(id, str)                        \
    do                                         \
    {                                          \
        if (str && str[0])                     \
        {                                      \
            sf.LoID = (id);                    \
            sf.HiID = 0;                       \
            sf.DatLen = (uint32_t)strlen(str); \
            sf.Buffer = (char *)(str);         \
            JAM_PutSubfield(sp, &sf);          \
        }                                      \
    } while (0)

    ADD_SF(JAMSFLD_OADDRESS, oaddr);
    ADD_SF(JAMSFLD_DADDRESS, daddr);
    ADD_SF(JAMSFLD_SENDERNAME, from);
    ADD_SF(JAMSFLD_RECVRNAME, to);
    ADD_SF(JAMSFLD_SUBJECT, subject);

#undef ADD_SF

    JAM_ClearMsgHeader(&h);

    now = time(NULL);
    utc = (uint32_t)now;
    h.DateWritten = date_written ? date_written : utc;
    h.DateProcessed = utc;
    h.Attribute = attr;
    h.ReplyTo = reply_to;

    if (JAM_AddMessage(BASE(a), &h, sp, (char *)body, bodylen) != 0)
    {
        JAM_DelSubPacket(sp);
        return 0;
    }

    JAM_DelSubPacket(sp);

    /* Read back assigned MsgNum from last slot */
    newmsg = 0;

    if (JAM_GetMBSize(BASE(a), &tot) == 0 && tot > 0)
    {
        memset(&tmp, 0, sizeof(tmp));

        if (JAM_ReadMsgHeader(BASE(a), tot - 1, &tmp, NULL) == 0)
            newmsg = tmp.MsgNum;
    }

    a->msg_count++;
    return newmsg;
}

int jam_delete_msg(JamArea *a, uint32_t msgnum)
{
    uint32_t slot;

    if (!a || !a->is_open || !a->is_locked)
        return -1;

    slot = find_slot(a, msgnum);

    if (slot == (uint32_t)-1)
        return -1;

    if (JAM_DeleteMessage(BASE(a), slot) != 0)
        return -1;

    if (a->msg_count > 0)
        a->msg_count--;

    return 0;
}

int jam_mark_sent(JamArea *a, uint32_t msgnum)
{
    uint32_t slot;
    s_JamMsgHeader h;

    if (!a || !a->is_open || !a->is_locked)
        return -1;

    slot = find_slot(a, msgnum);

    if (slot == (uint32_t)-1)
        return -1;

    memset(&h, 0, sizeof(h));

    if (JAM_ReadMsgHeader(BASE(a), slot, &h, NULL) != 0)
        return -1;

    if (h.MsgNum != msgnum)
        return -1;

    h.Attribute |= MSG_SENT;

    return (JAM_ChangeMsgHeader(BASE(a), slot, &h) == 0) ? 0 : -1;
}

/* Bulk header load (single pass) */
JamMsgInfo *jam_load_headers(JamArea *a, int *out_count, uint32_t filter_mask, uint32_t max_msgs)
{
    uint32_t total, n, cnt = 0, start, window;
    JamMsgInfo *msgs = NULL;
    s_JamMsgHeader h;
    s_JamSubPacket *sp = NULL;

    if (out_count)
        *out_count = 0;

    if (!a || !a->is_open || !out_count)
        return NULL;

    if (JAM_ReadMBHeader(BASE(a), BHDR(a)) != 0)
        return NULL;

    if (JAM_GetMBSize(BASE(a), &total) != 0 || total == 0)
        return NULL;

    if (total > JAM_MAX_MSGS)
        return NULL;

    if (max_msgs > 0 && total > max_msgs)
    {
        start = total - max_msgs;
        window = max_msgs;
    }
    else
    {
        start = 0;
        window = total;
    }

    msgs = (JamMsgInfo *)malloc(window * sizeof(JamMsgInfo));

    if (!msgs)
        return NULL;

    for (n = start; n < total && cnt < window; n++)
    {
        /* Single pass: read header + subfields together */
        sp = JAM_NewSubPacket();

        if (!sp)
            continue;

        memset(&h, 0, sizeof(h));

        if (JAM_ReadMsgHeader(BASE(a), n, &h, &sp) != 0)
        {
            JAM_DelSubPacket(sp);
            continue;
        }

        /* Skip deleted + caller's filter bits */
        if ((h.Attribute & MSG_DELETED) || (filter_mask && (h.Attribute & filter_mask)))
        {
            JAM_DelSubPacket(sp);
            continue;
        }

        memset(&msgs[cnt], 0, sizeof(msgs[cnt]));

        msgs[cnt].msgnum = h.MsgNum;
        msgs[cnt].attr = h.Attribute;
        msgs[cnt].date_written = h.DateWritten;
        msgs[cnt].txt_offset = h.TxtOffset;
        msgs[cnt].txt_len = h.TxtLen;
        msgs[cnt].reply_to = h.ReplyTo;
        msgs[cnt].reply_crc = h.ReplyCRC;
        msgs[cnt].msgid_crc = h.MsgIdCRC;

        extract_sf(sp, &msgs[cnt]);

        JAM_DelSubPacket(sp);

        cnt++;
    }

    if (cnt == 0)
    {
        free(msgs);
        return NULL;
    }

    a->msg_count = cnt;
    *out_count = (int)cnt;

    return msgs;
}

/* Lightweight counting: skips subfield I/O for arealist refresh */
int jam_count_msgs(JamArea *a, uint32_t lastread, uint32_t lastseen, int *out_total, int *out_unread, int *out_new)
{
    uint32_t total, n;
    int t = 0, u = 0, nu = 0;
    s_JamMsgHeader h;

    if (out_total)
        *out_total = 0;

    if (out_unread)
        *out_unread = 0;

    if (out_new)
        *out_new = 0;

    if (!a || !a->is_open)
        return -1;

    if (JAM_ReadMBHeader(BASE(a), BHDR(a)) != 0)
        return -1;

    if (JAM_GetMBSize(BASE(a), &total) != 0)
        return -1;

    if (total == 0)
        return 0;

    if (total > JAM_MAX_MSGS)
        return -1;

    for (n = 0; n < total; n++)
    {
        memset(&h, 0, sizeof(h));

        if (JAM_ReadMsgHeader(BASE(a), n, &h, NULL) != 0)
            continue;

        if (h.Attribute & MSG_DELETED)
            continue;

        t++;

        if (h.MsgNum > lastread)
            u++;

        /* Count messages since last seen (drives "*" in arealist) */
        if (h.MsgNum > lastseen)
            nu++;
    }

    if (out_total)
        *out_total = t;

    if (out_unread)
        *out_unread = u;

    if (out_new)
        *out_new = nu;

    return 0;
}

/* Utilities */
uint32_t jam_username_crc(const char *name)
{
    char lo[80];
    int i;

    if (!name || !name[0])
        return 0;

    for (i = 0; name[i] && i < 79; i++)
        lo[i] = (char)tolower((unsigned char)name[i]);

    lo[i] = '\0';

    return crc32_str(lo, i);
}

int jam_find_by_msgnum(const JamMsgInfo *msgs, int count, uint32_t msgnum)
{
    int i;

    if (!msgs || count <= 0)
        return -1;

    for (i = 0; i < count; i++)
    {
        if (msgs[i].msgnum == msgnum)
            return i;
    }

    return -1;
}
