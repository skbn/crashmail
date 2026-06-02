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

/* msghdr.c -- FTN message header display and field editing */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include "msghdr.h"
#include "utf8.h"

/* Field storage */

typedef struct
{
    wchar_t *wcs;
    int len, cap;
    int row, col, width; /* Screen position for editing */
} HField;

struct MsgHdr
{
    HField fields[HDR_COUNT];
    int msg_num, msg_total;
    uint32_t attr, date_written, jam_msgnum, reply_to;
    int edit_field; /* -1 = not editing */
    int edit_col;   /* cursor in editing field */
    int is_netmail; /* 1 for netmail areas: DADDR participates in tab cycle */
};

static const int order_nm[4] = {HDR_FROM, HDR_TO, HDR_DADDR, HDR_SUBJECT};
static const int order_ec[3] = {HDR_FROM, HDR_TO, HDR_SUBJECT};

static void field_free(HField *f)
{
    free(f->wcs);

    f->wcs = NULL;
    f->len = f->cap = 0;
}

static int field_set_wcs(HField *f, const wchar_t *wcs, int len)
{
    wchar_t *t;
    int nc = len + 16;

    t = (wchar_t *)malloc((size_t)nc * sizeof(wchar_t));

    if (!t)
        return -1;

    if (len > 0)
        wmemcpy(t, wcs, (size_t)len);

    t[len] = L'\0';

    free(f->wcs);

    f->wcs = t;
    f->len = len;
    f->cap = nc;

    return 0;
}

static int field_set_utf8(HField *f, const char *utf8)
{
    wchar_t *wcs;
    int wlen, rc;

    if (!utf8 || !utf8[0])
        return field_set_wcs(f, L"", 0);

    wcs = utf8_to_wcs(utf8, &wlen);

    if (!wcs)
        return -1;

    rc = field_set_wcs(f, wcs, wlen);

    free(wcs);

    return rc;
}

static int field_grow(HField *f, int need)
{
    int nc;
    wchar_t *t;

    if (f->cap > need + 1)
        return 0;

    nc = need + 32;

    t = (wchar_t *)realloc(f->wcs, (size_t)nc * sizeof(wchar_t));

    if (!t)
        return -1;

    f->wcs = t;
    f->cap = nc;

    return 0;
}

static int field_insert(HField *f, int pos, wchar_t ch)
{
    if (pos < 0)
        pos = 0;

    if (pos > f->len)
        pos = f->len;

    if (f->len >= JAM_FIELD_MAX)
        return -1;

    if (field_grow(f, f->len + 1) != 0)
        return -1;

    wmemmove(&f->wcs[pos + 1], &f->wcs[pos], (size_t)(f->len - pos + 1));

    f->wcs[pos] = ch;
    f->len++;

    return 0;
}

static int field_delete(HField *f, int pos)
{
    if (pos < 0 || pos >= f->len)
        return -1;

    wmemmove(&f->wcs[pos], &f->wcs[pos + 1], (size_t)(f->len - pos));

    f->len--;

    return 0;
}

MsgHdr *msghdr_new()
{
    MsgHdr *h = (MsgHdr *)calloc(1, sizeof(MsgHdr));

    if (!h)
        return NULL;

    h->edit_field = -1;

    return h;
}

void msghdr_free(MsgHdr *h)
{
    int i;

    if (!h)
        return;

    for (i = 0; i < HDR_COUNT; i++)
        field_free(&h->fields[i]);

    free(h);
}

/* Field position setup */
void msghdr_setup_positions(MsgHdr *h, int is_netmail, int cols)
{
    if (!h)
        return;

    h->is_netmail = is_netmail ? 1 : 0;

    /* FROM field */
    h->fields[HDR_FROM].row = 2;
    h->fields[HDR_FROM].col = 7;
    h->fields[HDR_FROM].width = 25;

    /* TO field */
    h->fields[HDR_TO].row = 3;
    h->fields[HDR_TO].col = 7;
    h->fields[HDR_TO].width = 25;

    /* DADDR field (netmail only) */
    h->fields[HDR_DADDR].row = 4;
    h->fields[HDR_DADDR].col = 7;
    h->fields[HDR_DADDR].width = 25;

    /* SUBJECT field: row 4 for echomail, row 5 for netmail */
    h->fields[HDR_SUBJECT].row = is_netmail ? 5 : 4;
    h->fields[HDR_SUBJECT].col = 7;
    h->fields[HDR_SUBJECT].width = 72; /* Will be adjusted based on COLS */

    /* Adjust SUBJECT width based on current COLS */
    if (cols > 0)
    {
        int max_width = cols - 7; /* col 7 is start of field */

        if (max_width < 20)
            max_width = 20; /* Minimum width */

        if (h->fields[HDR_SUBJECT].width > max_width)
            h->fields[HDR_SUBJECT].width = max_width;
    }
}

/* Adjust field widths based on current COLS (called on resize) */
void msghdr_resize(MsgHdr *h, int cols)
{
    if (!h)
        return;

    if (cols > 0)
    {
        int max_width = cols - 7; /* col 7 is start of field */

        if (max_width < 20)
            max_width = 20; /* Minimum width */

        /* Adjust SUBJECT width */
        if (h->fields[HDR_SUBJECT].width > max_width)
            h->fields[HDR_SUBJECT].width = max_width;
    }
}

/* Get field position helpers */
int msghdr_field_row(const MsgHdr *h, int field)
{
    if (!h || field < 0 || field >= HDR_COUNT)
        return 0;

    return h->fields[field].row;
}

int msghdr_field_col(const MsgHdr *h, int field)
{
    if (!h || field < 0 || field >= HDR_COUNT)
        return 0;

    return h->fields[field].col;
}

int msghdr_field_width(const MsgHdr *h, int field)
{
    if (!h || field < 0 || field >= HDR_COUNT)
        return 0;

    return h->fields[field].width;
}

/* Fill from JAM */
/*static void format_date(uint32_t epoch, int reader_offset, int tzutc_offset, char *buf, int bufsz)
{
    time_t t = (time_t)epoch;

    /* epoch is already UTC, just convert to reader's local time */
/*if (reader_offset != 0)
    t += (time_t)reader_offset * 60; /* Add reader's timezone offset to UTC */

/*struct tm *tm = gmtime(&t);

if (tm)
    snprintf(buf, (size_t)bufsz, "%02d %s %02d %02d:%02d", tm->tm_mday, "Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec" + (tm->tm_mon * 4), tm->tm_year % 100, tm->tm_hour, tm->tm_min);
else
    snprintf(buf, (size_t)bufsz, "(unknown)");
}*/

static void format_date(uint32_t epoch, int reader_offset, int tzutc_offset, char *buf, int bufsz)
{
    time_t t = (time_t)epoch;

    /* Same approach as the reply attribution line: add the effective
     * timezone offset to the UTC epoch, then use gmtime() to render it
     * localtime() cannot be used because the system TZ may not be set */
    if (tzutc_offset != -1)
        t += (time_t)tzutc_offset * 60;
    else if (reader_offset != 0)
        t += (time_t)reader_offset * 60;

    struct tm *tm = gmtime(&t);

    if (tm)
        snprintf(buf, (size_t)bufsz, "%02d %s %02d %02d:%02d", tm->tm_mday, "Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec" + (tm->tm_mon * 4), tm->tm_year % 100, tm->tm_hour, tm->tm_min);
    else
        snprintf(buf, (size_t)bufsz, "(unknown)");
}

void msghdr_load(MsgHdr *h, const JamMsgInfo *info, const char *area_tag, int msg_num, int msg_total, int reader_offset)
{
    char datebuf[32];

    if (!h || !info)
        return;

    field_set_utf8(&h->fields[HDR_FROM], info->from);
    field_set_utf8(&h->fields[HDR_TO], info->to);
    field_set_utf8(&h->fields[HDR_SUBJECT], info->subject);
    field_set_utf8(&h->fields[HDR_AREA], area_tag ? area_tag : "");
    field_set_utf8(&h->fields[HDR_OADDR], info->oaddress);
    field_set_utf8(&h->fields[HDR_DADDR], info->daddress);

    format_date(info->date_written, reader_offset, info->tzutc_offset, datebuf, sizeof(datebuf));
    field_set_utf8(&h->fields[HDR_DATE], datebuf);

    h->msg_num = msg_num;
    h->msg_total = msg_total;
    h->attr = info->attr;
    h->date_written = info->date_written;
    h->jam_msgnum = info->msgnum;
    h->reply_to = info->reply_to;
    h->edit_field = -1;
}

void msghdr_new_msg(MsgHdr *h, const char *area_tag, const char *from_utf8, const char *oaddr, const char *to_utf8, const char *daddr, int tz_offset)
{
    time_t now;
    char datebuf[32];

    if (!h)
        return;

    field_set_utf8(&h->fields[HDR_FROM], from_utf8 ? from_utf8 : "");
    field_set_utf8(&h->fields[HDR_TO], to_utf8 ? to_utf8 : "All");
    field_set_utf8(&h->fields[HDR_SUBJECT], "");
    field_set_utf8(&h->fields[HDR_AREA], area_tag ? area_tag : "");
    field_set_utf8(&h->fields[HDR_OADDR], oaddr ? oaddr : "");
    field_set_utf8(&h->fields[HDR_DADDR], daddr ? daddr : "");

    now = time(NULL);
    format_date((uint32_t)now, tz_offset, -1, datebuf, sizeof(datebuf));
    field_set_utf8(&h->fields[HDR_DATE], datebuf);

    h->msg_num = 0;
    h->msg_total = 0;
    h->attr = 0;
    h->date_written = (uint32_t)now;
    h->jam_msgnum = 0;
    h->reply_to = 0;
    h->edit_field = -1;
}

/* Set / Get */
void msghdr_set(MsgHdr *h, int f, const wchar_t *v)
{
    if (!h || f < 0 || f >= HDR_COUNT)
        return;

    field_set_wcs(&h->fields[f], v ? v : L"", v ? (int)wcslen(v) : 0);
}

void msghdr_set_utf8(MsgHdr *h, int f, const char *utf8)
{
    if (!h || f < 0 || f >= HDR_COUNT)
        return;

    field_set_utf8(&h->fields[f], utf8 ? utf8 : "");
}

const wchar_t *msghdr_get(const MsgHdr *h, int f)
{
    if (!h || f < 0 || f >= HDR_COUNT || !h->fields[f].wcs)
        return L"";

    return h->fields[f].wcs;
}

int msghdr_get_len(const MsgHdr *h, int f)
{
    if (!h || f < 0 || f >= HDR_COUNT)
        return 0;

    return h->fields[f].len;
}

char *msghdr_get_utf8(const MsgHdr *h, int f)
{
    if (!h || f < 0 || f >= HDR_COUNT || !h->fields[f].wcs)
        return NULL;

    return wcs_to_utf8(h->fields[f].wcs, h->fields[f].len);
}

/* Rotating-buffer variant: copy field's UTF-8 form into one of N static
 * slots and return the pointer. Same convention as ui_wcs2u8. Used by
 * callsites that previously did msghdr_get_utf8(...) inline as a
 * function arg and leaked the malloc'd result */
const char *msghdr_get_utf8_tmp(const MsgHdr *h, int f)
{
    static char pool[8][JAM_FIELD_MAX * 4 + 4];
    static int slot = 0;
    char *out;
    char *tmp;
    int n;

    out = pool[slot];
    slot = (slot + 1) & 7;
    out[0] = '\0';

    if (!h || f < 0 || f >= HDR_COUNT || !h->fields[f].wcs || h->fields[f].len <= 0)
        return out;

    /* Reuse the malloc version for the conversion, then copy into the
     * static slot and free. A direct in-place conversion would avoid
     * the malloc but the code duplication isn't worth it for header
     * fields (at most JAM_FIELD_MAX wide chars) */
    tmp = wcs_to_utf8(h->fields[f].wcs, h->fields[f].len);

    if (!tmp)
        return out;

    n = (int)strlen(tmp);

    if (n > (int)sizeof(pool[0]) - 1)
        n = (int)sizeof(pool[0]) - 1;

    memcpy(out, tmp, (size_t)n);
    out[n] = '\0';

    free(tmp);

    return out;
}

int msghdr_msgnum(const MsgHdr *h) { return h ? h->msg_num : 0; }

int msghdr_msgtotal(const MsgHdr *h) { return h ? h->msg_total : 0; }

uint32_t msghdr_attr(const MsgHdr *h) { return h ? h->attr : 0; }

uint32_t msghdr_date_written(const MsgHdr *h) { return h ? h->date_written : 0; }

uint32_t msghdr_jam_msgnum(const MsgHdr *h) { return h ? h->jam_msgnum : 0; }

uint32_t msghdr_reply_to(const MsgHdr *h) { return h ? h->reply_to : 0; }

/* Field editing */
void msghdr_edit_start(MsgHdr *h, int f)
{
    if (!h || f < 0 || (f > HDR_SUBJECT && f != HDR_DADDR))
        return;

    h->edit_field = f;
    h->edit_col = h->fields[f].len;
}

int msghdr_edit_field(const MsgHdr *h) { return h ? h->edit_field : -1; }

int msghdr_edit_col(const MsgHdr *h) { return h ? h->edit_col : 0; }

int msghdr_edit_key(MsgHdr *h, int key)
{
    HField *f;

    if (!h || h->edit_field < 0)
        return -1;

    /* Don't allow editing FROM field (sysop) */
    if (h->edit_field == HDR_FROM)
        return -1;

    f = &h->fields[h->edit_field];

    if (key == HDR_KEY_BS)
    {
        if (h->edit_col > 0)
        {
            field_delete(f, h->edit_col - 1);
            h->edit_col--;
        }
    }
    else if (key == HDR_KEY_DEL)
    {
        if (h->edit_col < f->len)
            field_delete(f, h->edit_col);
    }
    else if (key == HDR_KEY_LEFT)
    {
        if (h->edit_col > 0)
            h->edit_col--;
    }
    else if (key == HDR_KEY_RIGHT)
    {
        if (h->edit_col < f->len && h->edit_col < f->width)
            h->edit_col++;
    }
    else if (key == HDR_KEY_HOME)
    {
        h->edit_col = 0;
    }
    else if (key == HDR_KEY_END)
    {
        h->edit_col = (f->len < f->width) ? f->len : f->width;
    }
    else if (key >= 0x20)
    {
        if (h->edit_col < f->width && field_insert(f, h->edit_col, (wchar_t)key) == 0)
            h->edit_col++;
    }

    return 0;
}

/* Tab cycle field navigation */
/* Get field position in tab cycle, returns -1 if not found */
static int cycle_index(const MsgHdr *h, int f)
{
    if (h->is_netmail)
    {
        if (f == HDR_FROM)
            return 0;

        if (f == HDR_TO)
            return 1;

        if (f == HDR_DADDR)
            return 2;

        if (f == HDR_SUBJECT)
            return 3;
    }
    else
    {
        if (f == HDR_FROM)
            return 0;

        if (f == HDR_TO)
            return 1;

        if (f == HDR_SUBJECT)
            return 2;
    }

    return -1;
}

/* Return the field at cycle position `idx` mod cycle length */
static int cycle_field(const MsgHdr *h, int idx)
{
    int len = h->is_netmail ? 4 : 3;
    int m = ((idx % len) + len) % len; /* positive modulo */

    return h->is_netmail ? order_nm[m] : order_ec[m];
}

void msghdr_edit_tab(MsgHdr *h)
{
    int cur, next;

    if (!h || h->edit_field < 0)
        return;

    cur = cycle_index(h, h->edit_field);

    if (cur < 0)
    {
        h->edit_field = HDR_FROM;
    }
    else
    {
        next = cycle_field(h, cur + 1);
        h->edit_field = next;
    }

    if (h->fields[h->edit_field].len > 0)
        h->edit_col = h->fields[h->edit_field].len;
    else
        h->edit_col = 0;
}

void msghdr_edit_stab(MsgHdr *h)
{
    int cur, prev;

    if (!h || h->edit_field < 0)
        return;

    cur = cycle_index(h, h->edit_field);

    if (cur < 0)
    {
        h->edit_field = HDR_FROM;
    }
    else
    {
        prev = cycle_field(h, cur - 1);
        h->edit_field = prev;
    }

    if (h->fields[h->edit_field].len > 0)
        h->edit_col = h->fields[h->edit_field].len;
    else
        h->edit_col = 0;
}

void msghdr_edit_stop(MsgHdr *h)
{
    if (h)
        h->edit_field = -1;
}
