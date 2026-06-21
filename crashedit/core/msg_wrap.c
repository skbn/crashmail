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

/* msg_wrap.c -- FTS-1 *.MSG backend. Bit-compatible with CrashMail */

#if !defined(PLATFORM_WIN32) && !defined(PLATFORM_AMIGA)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif

#include "msg_wrap.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* StoredMsg field offsets */
#define OFF_FROM 0
#define OFF_TO 36
#define OFF_SUBJECT 72
#define OFF_DATETIME 144
#define OFF_TIMESREAD 164
#define OFF_DESTNODE 166
#define OFF_ORIGNODE 168
#define OFF_COST 170
#define OFF_ORIGNET 172
#define OFF_DESTNET 174
#define OFF_DESTZONE 176
#define OFF_ORIGZONE 178
#define OFF_DESTPOINT 180
#define OFF_ORIGPOINT 182
#define OFF_REPLYTO 184
#define OFF_ATTR 186
#define OFF_NEXTREPLY 188

/* FTS Attr bits (on-disk 16-bit field; see fidonet.h in CrashMail) */
#define FTS_PRIVATE 0x0001
#define FTS_CRASH 0x0002
#define FTS_RECD 0x0004
#define FTS_SENT 0x0008
#define FTS_FILEATTACH 0x0010
#define FTS_INTRANSIT 0x0020
#define FTS_ORPHAN 0x0040
#define FTS_KILLSENT 0x0080
#define FTS_LOCAL 0x0100
#define FTS_HOLD 0x0200
#define FTS_FILEREQ 0x0800
#define FTS_RREQ 0x1000
#define FTS_IRRR 0x2000
#define FTS_AUDIT 0x4000
#define FTS_UPDATEREQ 0x8000

/* JAM Attr bits (32-bit; from jamlib jam.h) */
#define JAMA_LOCAL 0x00000001UL
#define JAMA_INTRANSIT 0x00000002UL
#define JAMA_PRIVATE 0x00000004UL
#define JAMA_SENT 0x00000010UL
#define JAMA_KILLSENT 0x00000020UL
#define JAMA_HOLD 0x00000080UL
#define JAMA_CRASH 0x00000100UL
#define JAMA_FILEREQ 0x00001000UL
#define JAMA_FILEATTACH 0x00002000UL
#define JAMA_RECEIPTREQ 0x00010000UL
#define JAMA_CONFIRMREQ 0x00020000UL
#define JAMA_ORPHAN 0x00040000UL

#define JAM_NO_CRC 0xFFFFFFFFUL

#if defined(PLATFORM_AMIGA)
#define PATH_SEP '/'
#elif defined(PLATFORM_WIN32)
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

static uint32_t s_crc_tab[256];
static int s_crc_ready = 0;

/* Lastread file (crashedit.lr); forced little-endian since format is private to crashedit */
typedef struct
{
    uint32_t ucrc;
    uint32_t lastread;
    uint32_t lastseen;
} LrRec;

/* Native-endian u16 access on a byte buffer */
static uint16_t hdr_get_u16(const unsigned char *p)
{
    uint16_t v;

    memcpy(&v, p, sizeof(v));

    return v;
}

static void hdr_set_u16(unsigned char *p, uint16_t v)
{
    memcpy(p, &v, sizeof(v));
}

/* Little-endian u32 -- used ONLY for crashedit.lr file */
static uint32_t rd_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u32le(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

/* CRC32 with ASCII tolower; matches jamlib for MSGID/REPLY reply linking */
static void crc_init(void)
{
    uint32_t i;
    uint32_t j;
    uint32_t c;

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

static uint32_t crc32_lo(const char *s, int len)
{
    uint32_t crc;
    int i;
    unsigned char ch;

    crc_init();
    crc = 0xFFFFFFFFUL;

    for (i = 0; i < len; i++)
    {
        ch = (unsigned char)s[i];

        if (ch >= 'A' && ch <= 'Z')
            ch = (unsigned char)(ch + ('a' - 'A'));

        crc = (crc >> 8) ^ s_crc_tab[(crc ^ ch) & 0xFF];
    }

    return crc;
}

static int join_path(char *out, size_t out_sz, const char *dir, const char *file)
{
    size_t dl;
    size_t fl;
    size_t need;
    char last;

    if (!out || out_sz == 0 || !dir || !file)
        return -1;

    dl = strlen(dir);
    fl = strlen(file);

    if (dl == 0)
    {
        if (fl + 1 > out_sz)
            return -1;

        memcpy(out, file, fl + 1);
        return 0;
    }

    last = dir[dl - 1];

#if defined(PLATFORM_AMIGA)
    /* Amiga: ':' (volume) and '/' both act as separators */
    if (last == '/' || last == ':')
    {
        need = dl + fl + 1;

        if (need > out_sz)
            return -1;

        memcpy(out, dir, dl);
        memcpy(out + dl, file, fl + 1);
        return 0;
    }
#elif defined(PLATFORM_WIN32)
    if (last == '\\' || last == '/')
    {
        need = dl + fl + 1;

        if (need > out_sz)
            return -1;

        memcpy(out, dir, dl);
        memcpy(out + dl, file, fl + 1);
        return 0;
    }
#else
    if (last == '/')
    {
        need = dl + fl + 1;

        if (need > out_sz)
            return -1;

        memcpy(out, dir, dl);
        memcpy(out + dl, file, fl + 1);
        return 0;
    }
#endif

    need = dl + 1 + fl + 1;

    if (need > out_sz)
        return -1;

    memcpy(out, dir, dl);
    out[dl] = PATH_SEP;
    memcpy(out + dl + 1, file, fl + 1);
    return 0;
}

/* Fixed-size text field helpers (NUL-stop on read, zero-pad on write) */
static void hdr_get_str(char *dst, size_t dst_sz, const unsigned char *src, size_t src_sz)
{
    size_t i;
    size_t n;

    if (!dst || dst_sz == 0)
        return;

    n = src_sz;

    if (n > dst_sz - 1)
        n = dst_sz - 1;

    for (i = 0; i < n; i++)
    {
        if (src[i] == 0)
            break;

        dst[i] = (char)src[i];
    }

    dst[i] = '\0';
}

static void hdr_set_str(unsigned char *dst, size_t dst_sz, const char *src)
{
    size_t sl;
    size_t n;

    if (!dst || dst_sz == 0)
        return;

    if (!src)
        src = "";

    sl = strlen(src);
    n = sl;

    /* Reserve one byte for NUL termination; FTS-1 implementations expect NUL-terminated text */
    if (n >= dst_sz)
        n = dst_sz - 1;

    if (n > 0)
        memcpy(dst, src, n);

    memset(dst + n, 0, dst_sz - n);
}

/* FTS attr <-> JAM attr mapping */
static uint32_t ftn_to_jam_attr(uint16_t f)
{
    uint32_t r = 0;

    if (f & FTS_PRIVATE)
        r |= JAMA_PRIVATE;

    if (f & FTS_CRASH)
        r |= JAMA_CRASH;

    if (f & FTS_SENT)
        r |= JAMA_SENT;

    if (f & FTS_FILEATTACH)
        r |= JAMA_FILEATTACH;

    if (f & FTS_INTRANSIT)
        r |= JAMA_INTRANSIT;

    if (f & FTS_ORPHAN)
        r |= JAMA_ORPHAN;

    if (f & FTS_KILLSENT)
        r |= JAMA_KILLSENT;

    if (f & FTS_LOCAL)
        r |= JAMA_LOCAL;

    if (f & FTS_HOLD)
        r |= JAMA_HOLD;

    if (f & FTS_FILEREQ)
        r |= JAMA_FILEREQ;

    if (f & FTS_RREQ)
        r |= JAMA_RECEIPTREQ;

    if (f & FTS_IRRR)
        r |= JAMA_CONFIRMREQ;

    return r;
}

static uint16_t jam_to_ftn_attr(uint32_t j)
{
    uint16_t r = 0;

    if (j & JAMA_PRIVATE)
        r |= FTS_PRIVATE;

    if (j & JAMA_CRASH)
        r |= FTS_CRASH;

    if (j & JAMA_SENT)
        r |= FTS_SENT;

    if (j & JAMA_FILEATTACH)
        r |= FTS_FILEATTACH;

    if (j & JAMA_INTRANSIT)
        r |= FTS_INTRANSIT;

    if (j & JAMA_ORPHAN)
        r |= FTS_ORPHAN;

    if (j & JAMA_KILLSENT)
        r |= FTS_KILLSENT;

    if (j & JAMA_LOCAL)
        r |= FTS_LOCAL;

    if (j & JAMA_HOLD)
        r |= FTS_HOLD;

    if (j & JAMA_FILEREQ)
        r |= FTS_FILEREQ;

    if (j & JAMA_RECEIPTREQ)
        r |= FTS_RREQ;

    if (j & JAMA_CONFIRMREQ)
        r |= FTS_IRRR;

    return r;
}

/* FTN date "DD MMM YY  HH:MM:SS" (19 chars + NUL) */
static const char s_months[12][4] =
    {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/* out >= 20 */
static void make_ftn_date(uint32_t t, char *out)
{
    time_t tt;
    struct tm *tm;
    char tmp[40];
    int mon;
    int year;
    int mday;
    int hh;
    int mm;
    int ss;
    int n;

    if (!out)
        return;

    tt = (time_t)t;
    tm = localtime(&tt);

    if (!tm)
    {
        memcpy(out, "01 Jan 70  00:00:00", 20);
        return;
    }

    mon = tm->tm_mon;

    if (mon < 0 || mon > 11)
        mon = 0;

    year = tm->tm_year % 100;

    if (year < 0)
        year += 100;

    mday = tm->tm_mday;

    if (mday < 1)
        mday = 1;
    else if (mday > 31)
        mday = 31;

    hh = tm->tm_hour;

    if (hh < 0)
        hh = 0;
    else if (hh > 23)
        hh = 23;

    mm = tm->tm_min;

    if (mm < 0)
        mm = 0;
    else if (mm > 59)
        mm = 59;

    ss = tm->tm_sec;

    if (ss < 0)
        ss = 0;
    else if (ss > 59)
        ss = 59;

    /* Write into larger temp buffer to avoid FORTIFY_SOURCE format-length overflow; final form is 19 chars */
    n = snprintf(tmp, sizeof(tmp), "%02d %s %02d  %02d:%02d:%02d", mday, s_months[mon], year, hh, mm, ss);

    if (n < 0)
        n = 0;

    if (n > 19)
        n = 19;

    memcpy(out, tmp, (size_t)n);
    out[n] = '\0';
}

static int month_from_abbrev(const char *s)
{
    int i;
    char a;
    char b;
    char c;
    char ma;
    char mb;
    char mc;

    if (!s)
        return -1;

    for (i = 0; i < 12; i++)
    {
        a = s[0];
        b = s[1];
        c = s[2];

        if (a >= 'A' && a <= 'Z')
            a = (char)(a + ('a' - 'A'));

        if (b >= 'A' && b <= 'Z')
            b = (char)(b + ('a' - 'A'));

        if (c >= 'A' && c <= 'Z')
            c = (char)(c + ('a' - 'A'));

        ma = s_months[i][0];
        mb = s_months[i][1];
        mc = s_months[i][2];

        if (ma >= 'A' && ma <= 'Z')
            ma = (char)(ma + ('a' - 'A'));

        if (mb >= 'A' && mb <= 'Z')
            mb = (char)(mb + ('a' - 'A'));

        if (mc >= 'A' && mc <= 'Z')
            mc = (char)(mc + ('a' - 'A'));

        if (a == ma && b == mb && c == mc)
            return i;
    }

    return -1;
}

static uint32_t parse_ftn_date(const char *s)
{
    struct tm tm;
    char mname[4];
    int day;
    int mon;
    int year;
    int hh;
    int mm;
    int ss;
    int n;
    time_t t;

    if (!s || !s[0])
        return 0;

    memset(&tm, 0, sizeof(tm));
    memset(mname, 0, sizeof(mname));

    n = sscanf(s, "%d %3s %d %d:%d:%d", &day, mname, &year, &hh, &mm, &ss);

    if (n != 6)
        return 0;

    mon = month_from_abbrev(mname);

    if (mon < 0)
        return 0;

    if (year < 70)
        year += 2000;
    else if (year < 100)
        year += 1900;

    if (day < 1 || day > 31 || hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 60)
        return 0;

    tm.tm_mday = day;
    tm.tm_mon = mon;
    tm.tm_year = year - 1900;
    tm.tm_hour = hh;
    tm.tm_min = mm;
    tm.tm_sec = ss;
    tm.tm_isdst = -1;

    t = mktime(&tm);

    if (t == (time_t)-1)
        return 0;

    return (uint32_t)t;
}

/* 4D FTN address: "zone:net/node.point" or "zone:net/node" */
static void build_4d_addr(char *out, size_t sz, uint16_t z, uint16_t net, uint16_t node, uint16_t pt)
{
    if (!out || sz == 0)
        return;

    if (z == 0 && net == 0 && node == 0)
    {
        out[0] = '\0';
        return;
    }

    if (pt)
        snprintf(out, sz, "%u:%u/%u.%u", (unsigned)z, (unsigned)net, (unsigned)node, (unsigned)pt);
    else
        snprintf(out, sz, "%u:%u/%u", (unsigned)z, (unsigned)net, (unsigned)node);
}

static int parse_4d_addr(const char *s, uint16_t *z, uint16_t *net, uint16_t *node, uint16_t *pt)
{
    unsigned uz, un, ud, up;
    int got;

    if (!s || !s[0])
        return -1;

    uz = un = ud = up = 0;

    got = sscanf(s, "%u:%u/%u.%u", &uz, &un, &ud, &up);

    if (got < 3)
    {
        up = 0;
        got = sscanf(s, "%u:%u/%u", &uz, &un, &ud);

        if (got < 3)
            return -1;
    }

    if (z)
        *z = (uint16_t)(uz & 0xFFFF);

    if (net)
        *net = (uint16_t)(un & 0xFFFF);

    if (node)
        *node = (uint16_t)(ud & 0xFFFF);

    if (pt)
        *pt = (uint16_t)(up & 0xFFFF);

    return 0;
}

/* Kludge value extraction: finds "^A<name>:" at line boundary and copies the rest of the line into out */
static int find_kludge_value(const char *buf, size_t len, const char *name, char *out, size_t out_sz)
{
    size_t nlen;
    size_t i;
    size_t v;
    size_t e;
    size_t copy;
    char prev;
    char ch;

    if (!buf || !name || !out || out_sz == 0)
        return 0;

    out[0] = '\0';
    nlen = strlen(name);

    if (nlen == 0 || len < nlen + 2)
        return 0;

    for (i = 0; i + nlen + 2 <= len; i++)
    {
        if (i > 0)
        {
            prev = buf[i - 1];

            if (prev != '\r' && prev != '\n' && prev != '\x01' && prev != '\0')
                continue;
        }

        if ((unsigned char)buf[i] != 0x01)
            continue;

        if (memcmp(buf + i + 1, name, nlen) != 0)
            continue;

        if (buf[i + 1 + nlen] != ':' && buf[i + 1 + nlen] != ' ')
            continue;

        v = i + 1 + nlen + 1;

        while (v < len && (buf[v] == ' ' || buf[v] == '\t'))
            v++;

        e = v;

        while (e < len)
        {
            ch = buf[e];

            if (ch == '\r' || ch == '\n' || ch == '\0' || (unsigned char)ch == 0x01)
                break;

            e++;
        }

        copy = e - v;

        if (copy > out_sz - 1)
            copy = out_sz - 1;

        if (copy > 0)
            memcpy(out, buf + v, copy);

        out[copy] = '\0';

        return 1;
    }

    return 0;
}

/* Parse FTN INTL kludge "<DESTADDR> <ORIGADDR>" (3D addresses); returns 1 on success, 0 otherwise */
static int parse_intl(const char *value, uint16_t *dz, uint16_t *dn, uint16_t *dnode, uint16_t *oz, uint16_t *on, uint16_t *onode)
{
    const char *sp;
    char dest[64];
    char orig[64];
    size_t dl;

    if (!value || !value[0])
        return 0;

    sp = strchr(value, ' ');
    if (!sp)
        return 0;

    dl = (size_t)(sp - value);

    if (dl >= sizeof(dest))
        dl = sizeof(dest) - 1;

    memcpy(dest, value, dl);
    dest[dl] = '\0';

    while (*sp == ' ' || *sp == '\t')
        sp++;

    strncpy(orig, sp, sizeof(orig) - 1);
    orig[sizeof(orig) - 1] = '\0';

    if (parse_4d_addr(dest, dz, dn, dnode, NULL) != 0)
        return 0;

    if (parse_4d_addr(orig, oz, on, onode, NULL) != 0)
        return 0;

    return 1;
}

/* Sorted nums[] cache helpers */
static int cmp_u32(const void *pa, const void *pb)
{
    uint32_t a = *(const uint32_t *)pa;
    uint32_t b = *(const uint32_t *)pb;

    if (a < b)
        return -1;

    if (a > b)
        return 1;

    return 0;
}

static int nums_grow(MsgArea *a, int need)
{
    int new_cap;
    uint32_t *p = NULL;

    if (a->nums_cap >= need)
        return 0;

    new_cap = a->nums_cap > 0 ? a->nums_cap : 64;

    while (new_cap < need)
    {
        if (new_cap > (int)((unsigned)-1 / 2 / sizeof(uint32_t)))
            return -1;

        new_cap *= 2;
    }

    p = (uint32_t *)realloc(a->nums, (size_t)new_cap * sizeof(uint32_t));

    if (!p)
        return -1;

    a->nums = p;
    a->nums_cap = new_cap;

    return 0;
}

/* Parse "<digits>.msg" / ".MSG"; returns msgnum (>= 2), or 0 on mismatch; 1.msg is HighWater Mark */
static uint32_t parse_msg_filename(const char *name)
{
    size_t i;
    size_t len;
    uint32_t n;
    uint32_t prev;

    if (!name || !name[0] || name[0] < '0' || name[0] > '9')
        return 0;

    n = 0;
    i = 0;

    while (name[i] >= '0' && name[i] <= '9')
    {
        prev = n;
        n = n * 10 + (uint32_t)(name[i] - '0');

        if (n < prev)
            return 0;

        i++;

        if (i > 10)
            return 0;
    }

    if (i == 0)
        return 0;

    len = strlen(name);

    if (len - i != 4)
        return 0;

    if (name[i] != '.')
        return 0;

    if ((name[i + 1] != 'm' && name[i + 1] != 'M') ||
        (name[i + 2] != 's' && name[i + 2] != 'S') ||
        (name[i + 3] != 'g' && name[i + 3] != 'G'))
        return 0;

    if (n < 2)
        return 0;

    return n;
}

static int nums_insert_sorted(MsgArea *a, uint32_t n)
{
    int lo;
    int hi;
    int mid;

    if (nums_grow(a, a->nums_count + 1) != 0)
        return -1;

    lo = 0;
    hi = a->nums_count;

    while (lo < hi)
    {
        mid = lo + (hi - lo) / 2;

        if (a->nums[mid] < n)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (lo < a->nums_count)
        memmove(a->nums + lo + 1, a->nums + lo, (size_t)(a->nums_count - lo) * sizeof(uint32_t));

    a->nums[lo] = n;
    a->nums_count++;

    if (a->nums_count == 1 || n < a->low_msg)
        a->low_msg = n;

    if (n > a->high_msg)
        a->high_msg = n;

    return 0;
}

static void nums_remove_value(MsgArea *a, uint32_t n)
{
    int lo;
    int hi;
    int mid;

    lo = 0;
    hi = a->nums_count;

    while (lo < hi)
    {
        mid = lo + (hi - lo) / 2;

        if (a->nums[mid] < n)
            lo = mid + 1;
        else if (a->nums[mid] > n)
            hi = mid;
        else
        {
            if (mid + 1 < a->nums_count)
                memmove(a->nums + mid, a->nums + mid + 1, (size_t)(a->nums_count - mid - 1) * sizeof(uint32_t));

            a->nums_count--;

            if (a->nums_count == 0)
            {
                a->low_msg = 0;
                a->high_msg = 0;
            }
            else
            {
                a->low_msg = a->nums[0];
                a->high_msg = a->nums[a->nums_count - 1];
            }

            return;
        }
    }
}

/* Read HighWater Mark from 1.msg.ReplyTo (if present) */
static void read_hwm_from_1_msg(MsgArea *a)
{
    char full[300];
    FILE *fp = NULL;
    unsigned char hdr[MSG_STORED_HDR_SIZE];

    a->hwm = 0;

    if (join_path(full, sizeof(full), a->path, "1.msg") != 0)
        return;

    fp = fopen(full, "rb");
    if (!fp)
        return;

    if (fread(hdr, 1, MSG_STORED_HDR_SIZE, fp) == MSG_STORED_HDR_SIZE)
        a->hwm = (uint32_t)hdr_get_u16(hdr + OFF_REPLYTO);

    fclose(fp);
}

/* Directory scan -- rebuild a->nums + low/high + hwm */
static int scan_dir(MsgArea *a)
{
    PfDir *d = NULL;
    const char *name;
    uint32_t n;
    int r;
    int w;

    a->nums_count = 0;
    a->low_msg = 0;
    a->high_msg = 0;
    a->hwm = 0;

    if (!a->path[0])
        return -1;

    d = pf_dir_open(a->path);

    if (!d)
        return -1;

    while ((name = pf_dir_next(d)) != NULL)
    {
        n = parse_msg_filename(name);

        if (n == 0)
            continue;

        if (nums_grow(a, a->nums_count + 1) != 0)
        {
            pf_dir_close(d);
            return -1;
        }

        a->nums[a->nums_count++] = n;
    }

    pf_dir_close(d);

    if (a->nums_count > 1)
        qsort(a->nums, (size_t)a->nums_count, sizeof(uint32_t), cmp_u32);

    /* Defensive dedup */
    if (a->nums_count > 1)
    {
        w = 1;

        for (r = 1; r < a->nums_count; r++)
        {
            if (a->nums[r] != a->nums[w - 1])
                a->nums[w++] = a->nums[r];
        }

        a->nums_count = w;
    }

    if (a->nums_count > 0)
    {
        a->low_msg = a->nums[0];
        a->high_msg = a->nums[a->nums_count - 1];
    }

    read_hwm_from_1_msg(a);
    return 0;
}

/* open / close / lock / unlock */
int msg_open(MsgArea *a, const char *path)
{
    if (!a || !path)
        return -1;

    if (strlen(path) >= sizeof(a->path))
        return -1;

    memset(a, 0, sizeof(*a));
    strcpy(a->path, path);

    if (!pf_is_directory(path))
        return -1;

    if (scan_dir(a) != 0)
    {
        free(a->nums);

        a->nums = NULL;
        a->nums_cap = 0;
        a->nums_count = 0;
        a->path[0] = '\0';

        return -1;
    }

    a->is_open = 1;

    return 0;
}

void msg_close(MsgArea *a)
{
    if (!a)
        return;

    if (a->is_locked)
        msg_unlock(a);

    free(a->nums);

    a->nums = NULL;
    a->nums_count = 0;
    a->nums_cap = 0;
    a->low_msg = 0;
    a->high_msg = 0;
    a->hwm = 0;
    a->is_open = 0;
    a->path[0] = '\0';
}

int msg_lock(MsgArea *a, int retries)
{
    char lockpath[300];
    int i;

    if (!a || !a->is_open)
        return -1;

    if (a->is_locked)
        return 0;

    if (join_path(lockpath, sizeof(lockpath), a->path, MSG_LOCK_FILENAME) != 0)
        return -1;

    for (i = 0; i <= retries; i++)
    {
        a->lock = pf_lock_create(lockpath);

        if (a->lock)
        {
            a->is_locked = 1;
            return 0;
        }

        if (i < retries)
            pf_sleep_ms(200);
    }

    return -1;
}

void msg_unlock(MsgArea *a)
{
    if (!a || !a->is_locked)
        return;

    if (a->lock)
    {
        pf_lock_release(a->lock);
        a->lock = NULL;
    }

    a->is_locked = 0;
}

/* Read full message body */
static char *slurp_msg_file(const char *fullpath, size_t *out_total, size_t *out_body_off)
{
    FILE *fp = NULL;
    long sz;
    char *buf = NULL;
    size_t n;

    if (!fullpath)
        return NULL;

    fp = fopen(fullpath, "rb");

    if (!fp)
        return NULL;

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return NULL;
    }

    sz = ftell(fp);

    if (sz < (long)MSG_STORED_HDR_SIZE || sz > 16L * 1024L * 1024L)
    {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        fclose(fp);
        return NULL;
    }

    if ((size_t)sz > SIZE_MAX - 1)
    {
        fclose(fp);
        return NULL;
    }

    buf = (char *)malloc((size_t)sz + 1);

    if (!buf)
    {
        fclose(fp);
        return NULL;
    }

    n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    if (n != (size_t)sz)
    {
        free(buf);
        return NULL;
    }

    buf[n] = '\0';

    if (out_total)
        *out_total = n;

    if (out_body_off)
        *out_body_off = MSG_STORED_HDR_SIZE;

    return buf;
}

char *msg_read_body(MsgArea *a, uint32_t msgnum, uint32_t *out_len)
{
    char name[16];
    char full[300];
    char *raw, *out;
    size_t total;
    size_t body_off;
    size_t body_len;

    if (out_len)
        *out_len = 0;

    if (!a || !a->is_open || msgnum < 2)
        return NULL;

    snprintf(name, sizeof(name), "%u.msg", (unsigned)msgnum);

    if (join_path(full, sizeof(full), a->path, name) != 0)
        return NULL;

    total = 0;
    body_off = 0;
    raw = slurp_msg_file(full, &total, &body_off);

    if (!raw)
        return NULL;

    if (total < body_off)
    {
        free(raw);
        return NULL;
    }

    body_len = total - body_off;

    /* Strip the FTS-1 trailing NUL if present */
    if (body_len > 0 && raw[body_off + body_len - 1] == '\0')
        body_len--;

    out = (char *)malloc(body_len + 1);

    if (!out)
    {
        free(raw);
        return NULL;
    }

    if (body_len > 0)
        memcpy(out, raw + body_off, body_len);

    out[body_len] = '\0';

    free(raw);

    if (out_len)
        *out_len = (uint32_t)body_len;

    return out;
}

/* On success: returns pointer + count (caller frees), or NULL+0 if file absent/empty; on corruption: status = -1 */
static LrRec *lr_read_all(const MsgArea *a, int *out_count, int *out_status)
{
    char full[300];
    FILE *fp = NULL;
    unsigned char hdr[12];
    unsigned char buf[12];
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t i;
    LrRec *recs = NULL;

    *out_count = 0;
    *out_status = 0;

    if (join_path(full, sizeof(full), a->path, MSG_LR_FILENAME) != 0)
    {
        *out_status = -1;
        return NULL;
    }

    fp = fopen(full, "rb");

    if (!fp)
        return NULL;

    if (fread(hdr, 1, 12, fp) != 12)
    {
        fclose(fp);
        return NULL;
    }

    magic = rd_u32le(hdr);
    version = rd_u32le(hdr + 4);
    count = rd_u32le(hdr + 8);

    if (magic != MSG_LR_MAGIC || version != MSG_LR_VERSION)
    {
        fclose(fp);
        *out_status = -1;
        return NULL;
    }

    if (count > 1024UL * 1024UL)
    {
        fclose(fp);
        *out_status = -1;
        return NULL;
    }

    if (count == 0)
    {
        fclose(fp);
        return NULL;
    }

    recs = (LrRec *)malloc((size_t)count * sizeof(LrRec));

    if (!recs)
    {
        fclose(fp);
        *out_status = -1;
        return NULL;
    }

    for (i = 0; i < count; i++)
    {
        if (fread(buf, 1, 12, fp) != 12)
        {
            free(recs);
            fclose(fp);
            *out_status = -1;
            return NULL;
        }

        recs[i].ucrc = rd_u32le(buf);
        recs[i].lastread = rd_u32le(buf + 4);
        recs[i].lastseen = rd_u32le(buf + 8);
    }

    fclose(fp);
    *out_count = (int)count;

    return recs;
}

static int lr_write_all(const MsgArea *a, const LrRec *recs, int count)
{
    char full[300];
    char tmp[316];
    FILE *fp = NULL;
    unsigned char hdr[12];
    unsigned char buf[12];
    int i;

    if (join_path(full, sizeof(full), a->path, MSG_LR_FILENAME) != 0)
        return -1;

    if (count < 0)
        count = 0;

    if ((size_t)snprintf(tmp, sizeof(tmp), "%s.tmp", full) >= sizeof(tmp))
        return -1;

    fp = fopen(tmp, "wb");

    if (!fp)
        return -1;

    wr_u32le(hdr, MSG_LR_MAGIC);
    wr_u32le(hdr + 4, MSG_LR_VERSION);
    wr_u32le(hdr + 8, (uint32_t)count);

    if (fwrite(hdr, 1, 12, fp) != 12)
    {
        fclose(fp);
        pf_remove_file(tmp);
        return -1;
    }

    for (i = 0; i < count; i++)
    {
        wr_u32le(buf, recs[i].ucrc);
        wr_u32le(buf + 4, recs[i].lastread);
        wr_u32le(buf + 8, recs[i].lastseen);

        if (fwrite(buf, 1, 12, fp) != 12)
        {
            fclose(fp);
            pf_remove_file(tmp);
            return -1;
        }
    }

    if (fflush(fp) != 0 || fclose(fp) != 0)
    {
        pf_remove_file(tmp);
        return -1;
    }

    if (pf_atomic_rename(tmp, full) != 0)
    {
        pf_remove_file(tmp);
        return -1;
    }

    return 0;
}

int msg_read_lastread_pair(MsgArea *a, uint32_t ucrc, uint32_t *out_last, uint32_t *out_high)
{
    LrRec *recs = NULL;
    int count;
    int status;
    int i;

    if (out_last)
        *out_last = 0;

    if (out_high)
        *out_high = 0;

    if (!a || !a->is_open)
        return -1;

    count = 0;
    status = 0;
    recs = lr_read_all(a, &count, &status);

    if (status != 0)
        return -1;

    if (!recs || count == 0)
        return 0;

    for (i = 0; i < count; i++)
    {
        if (recs[i].ucrc == ucrc)
        {
            if (out_last)
                *out_last = recs[i].lastread;

            if (out_high)
                *out_high = recs[i].lastseen;

            break;
        }
    }

    free(recs);

    return 0;
}

int msg_write_lastread(MsgArea *a, uint32_t ucrc, uint32_t last, uint32_t high)
{
    LrRec *recs, *grown;
    int count;
    int status;
    int i;
    int found;
    int rc;

    if (!a || !a->is_open || !a->is_locked)
        return -1;

    count = 0;
    status = 0;
    recs = lr_read_all(a, &count, &status);

    if (status != 0)
    {
        free(recs);
        return -1;
    }

    found = 0;

    for (i = 0; i < count; i++)
    {
        if (recs[i].ucrc == ucrc)
        {
            recs[i].lastread = last;
            recs[i].lastseen = high;
            found = 1;
            break;
        }
    }

    if (!found)
    {
        grown = (LrRec *)realloc(recs, (size_t)(count + 1) * sizeof(LrRec));

        if (!grown)
        {
            free(recs);
            return -1;
        }

        recs = grown;
        recs[count].ucrc = ucrc;
        recs[count].lastread = last;
        recs[count].lastseen = high;
        count++;
    }

    rc = lr_write_all(a, recs, count);

    free(recs);

    return rc;
}

/* Header reading helpers */
static int read_msg_prefix(const char *fullpath, unsigned char *hdr_out, char *body_out, size_t scan_max, size_t *body_bytes)
{
    FILE *fp = NULL;
    size_t got;

    if (body_bytes)
        *body_bytes = 0;

    fp = fopen(fullpath, "rb");

    if (!fp)
        return -1;

    got = fread(hdr_out, 1, MSG_STORED_HDR_SIZE, fp);

    if (got != MSG_STORED_HDR_SIZE)
    {
        fclose(fp);
        return -1;
    }

    if (scan_max > 0 && body_out)
    {
        got = fread(body_out, 1, scan_max, fp);

        if (body_bytes)
            *body_bytes = got;
    }

    fclose(fp);
    return 0;
}

/* Populate JamMsgInfo from StoredMsg + body; netmail kludges override binary fields, echomail uses MSGID/ORIGID */
static void fill_info_from_msg(JamMsgInfo *info, uint32_t msgnum, const unsigned char *hdr, const char *body, size_t body_len)
{
    char datebuf[24];
    char msgid_val[160];
    char reply_val[160];
    char tzutc_val[24];
    char origid_val[160];
    char intl_val[128];
    char fmpt_val[16];
    char topt_val[16];
    uint16_t dest_z;
    uint16_t dest_n;
    uint16_t dest_node;
    uint16_t dest_p;
    uint16_t orig_z;
    uint16_t orig_n;
    uint16_t orig_node;
    uint16_t orig_p;
    uint16_t reply_to_u16, attr;
    const char *src, *sp;
    size_t copy_len;
    int hours;
    int mins;
    char sign;
    int intl_ok;

    memset(info, 0, sizeof(*info));
    info->tzutc_offset = -1;
    info->msgnum = msgnum;

    /* Text fields */
    hdr_get_str(info->from, sizeof(info->from), hdr + OFF_FROM, 36);
    hdr_get_str(info->to, sizeof(info->to), hdr + OFF_TO, 36);
    hdr_get_str(info->subject, sizeof(info->subject), hdr + OFF_SUBJECT, 72);
    hdr_get_str(datebuf, sizeof(datebuf), hdr + OFF_DATETIME, 20);

    info->date_written = parse_ftn_date(datebuf);

    /* Numeric fields (native endian via memcpy) */
    dest_node = hdr_get_u16(hdr + OFF_DESTNODE);
    orig_node = hdr_get_u16(hdr + OFF_ORIGNODE);
    orig_n = hdr_get_u16(hdr + OFF_ORIGNET);
    dest_n = hdr_get_u16(hdr + OFF_DESTNET);
    dest_z = hdr_get_u16(hdr + OFF_DESTZONE);
    orig_z = hdr_get_u16(hdr + OFF_ORIGZONE);
    dest_p = hdr_get_u16(hdr + OFF_DESTPOINT);
    orig_p = hdr_get_u16(hdr + OFF_ORIGPOINT);
    reply_to_u16 = hdr_get_u16(hdr + OFF_REPLYTO);
    attr = hdr_get_u16(hdr + OFF_ATTR);

    info->attr = ftn_to_jam_attr(attr);
    info->reply_to = (uint32_t)reply_to_u16;

    /* Scan kludges in the body prefix */
    msgid_val[0] = '\0';
    reply_val[0] = '\0';
    tzutc_val[0] = '\0';
    origid_val[0] = '\0';
    intl_val[0] = '\0';
    fmpt_val[0] = '\0';
    topt_val[0] = '\0';

    if (body && body_len > 0)
    {
        find_kludge_value(body, body_len, "MSGID", msgid_val, sizeof(msgid_val));
        find_kludge_value(body, body_len, "REPLY", reply_val, sizeof(reply_val));
        find_kludge_value(body, body_len, "TZUTC", tzutc_val, sizeof(tzutc_val));
        find_kludge_value(body, body_len, "ORIGID", origid_val, sizeof(origid_val));
        find_kludge_value(body, body_len, "INTL", intl_val, sizeof(intl_val));
        find_kludge_value(body, body_len, "FMPT", fmpt_val, sizeof(fmpt_val));
        find_kludge_value(body, body_len, "TOPT", topt_val, sizeof(topt_val));
    }

    /* INTL kludge takes priority over header zone/net/node (like CrashMail) */
    intl_ok = 0;

    if (intl_val[0])
    {
        uint16_t idz;
        uint16_t idn;
        uint16_t idnode;
        uint16_t ioz;
        uint16_t ion;
        uint16_t ionode;

        if (parse_intl(intl_val, &idz, &idn, &idnode, &ioz, &ion, &ionode))
        {
            dest_z = idz;
            dest_n = idn;
            dest_node = idnode;
            orig_z = ioz;
            orig_n = ion;
            orig_node = ionode;
            intl_ok = 1;
        }
    }

    /* FMPT/TOPT only override points for netmail */
    if (fmpt_val[0])
    {
        int p = atoi(fmpt_val);

        if (p > 0 && p < 65536)
            orig_p = (uint16_t)p;
    }

    if (topt_val[0])
    {
        int p = atoi(topt_val);

        if (p > 0 && p < 65536)
            dest_p = (uint16_t)p;
    }

    /* Compose final 4D addresses */
    build_4d_addr(info->oaddress, sizeof(info->oaddress), orig_z, orig_n, orig_node, orig_p);
    build_4d_addr(info->daddress, sizeof(info->daddress), dest_z, dest_n, dest_node, dest_p);

    /* Echomail fallback: recover origin from ORIGID/MSGID when header zones are 0 and no INTL */
    if (!intl_ok && info->oaddress[0] == '\0')
    {
        src = origid_val[0] ? origid_val : msgid_val;

        if (src[0])
        {
            sp = strchr(src, ' ');
            copy_len = sp ? (size_t)(sp - src) : strlen(src);

            if (copy_len >= sizeof(info->oaddress))
                copy_len = sizeof(info->oaddress) - 1;

            memcpy(info->oaddress, src, copy_len);
            info->oaddress[copy_len] = '\0';
        }
    }

    /* MSGID + CRC */
    if (msgid_val[0])
    {
        copy_len = strlen(msgid_val);

        if (copy_len >= sizeof(info->msgid))
            copy_len = sizeof(info->msgid) - 1;

        memcpy(info->msgid, msgid_val, copy_len);

        info->msgid[copy_len] = '\0';
        info->msgid_crc = crc32_lo(msgid_val, (int)strlen(msgid_val));
    }
    else
    {
        info->msgid_crc = JAM_NO_CRC;
    }

    if (reply_val[0])
        info->reply_crc = crc32_lo(reply_val, (int)strlen(reply_val));
    else
        info->reply_crc = JAM_NO_CRC;

    /* TZUTC -> minutes east of UTC (FTS-4001 +/-HHMM) */
    if (tzutc_val[0])
    {
        sign = tzutc_val[0];

        if ((sign == '+' || sign == '-') && isdigit((unsigned char)tzutc_val[1]) && isdigit((unsigned char)tzutc_val[2]) && isdigit((unsigned char)tzutc_val[3]) && isdigit((unsigned char)tzutc_val[4]))
        {
            hours = (tzutc_val[1] - '0') * 10 + (tzutc_val[2] - '0');
            mins = (tzutc_val[3] - '0') * 10 + (tzutc_val[4] - '0');

            info->tzutc_offset = (hours * 60 + mins) * (sign == '-' ? -1 : 1);
        }
    }
}

JamMsgInfo *msg_load_headers(MsgArea *a, int *out_count, uint32_t filter_mask, uint32_t max_msgs)
{
    int total;
    int start;
    int want;
    int i;
    int kept;
    JamMsgInfo *out = NULL;
    JamMsgInfo info;
    unsigned char hdr[MSG_STORED_HDR_SIZE];
    char *body_buf = NULL;
    uint32_t mn;
    char name[16];
    char full[300];
    size_t bb;

    if (out_count)
        *out_count = 0;

    if (!a || !a->is_open || !out_count)
        return NULL;

    if (scan_dir(a) != 0)
        return NULL;

    total = a->nums_count;

    if (total <= 0)
        return NULL;

    if (max_msgs > 0 && (uint32_t)total > max_msgs)
    {
        start = total - (int)max_msgs;
        want = (int)max_msgs;
    }
    else
    {
        start = 0;
        want = total;
    }

    out = (JamMsgInfo *)malloc((size_t)want * sizeof(JamMsgInfo));

    if (!out)
        return NULL;

    body_buf = (char *)malloc(MSG_KLUDGE_SCAN_BYTES);

    if (!body_buf)
    {
        free(out);
        return NULL;
    }

    kept = 0;

    for (i = start; i < total; i++)
    {
        mn = a->nums[i];
        snprintf(name, sizeof(name), "%u.msg", (unsigned)mn);

        if (join_path(full, sizeof(full), a->path, name) != 0)
            continue;

        bb = 0;

        if (read_msg_prefix(full, hdr, body_buf, MSG_KLUDGE_SCAN_BYTES, &bb) != 0)
            continue;

        fill_info_from_msg(&info, mn, hdr, body_buf, bb);

        if (filter_mask && (info.attr & filter_mask))
            continue;

        out[kept++] = info;
    }

    free(body_buf);

    if (kept == 0)
    {
        free(out);
        return NULL;
    }

    *out_count = kept;
    return out;
}

int msg_count_msgs(MsgArea *a, uint32_t lastread, uint32_t lastseen, int *out_total, int *out_unread, int *out_new)
{
    int i;
    int t;
    int u;
    int nw;

    if (out_total)
        *out_total = 0;

    if (out_unread)
        *out_unread = 0;

    if (out_new)
        *out_new = 0;

    if (!a || !a->is_open)
        return -1;

    if (scan_dir(a) != 0)
        return -1;

    t = a->nums_count;
    u = 0;
    nw = 0;

    for (i = 0; i < a->nums_count; i++)
    {
        if (a->nums[i] > lastread)
            u++;

        if (a->nums[i] > lastseen)
            nw++;
    }

    if (out_total)
        *out_total = t;

    if (out_unread)
        *out_unread = u;

    if (out_new)
        *out_new = nw;

    return 0;
}

/* Write new message; caller body has user kludges, we inject FTN-routing kludges for CrashMail/GoldEd */
uint32_t msg_write_msg(MsgArea *a, const char *from, const char *to, const char *subject, const char *body, uint32_t bodylen, uint32_t attr, uint32_t reply_to, uint32_t date_written, const char *oaddr, const char *daddr)
{
    char datetime[20];
    unsigned char hdr[MSG_STORED_HDR_SIZE];
    uint16_t orig_z;
    uint16_t orig_n;
    uint16_t orig_node;
    uint16_t orig_p;
    uint16_t dest_z;
    uint16_t dest_n;
    uint16_t dest_node;
    uint16_t dest_p;
    uint16_t fts_attr;
    char name[16];
    char full[300];
    char tmp[316];
    char prefix[256];
    int prefix_len;
    int n;
    FILE *fp, *probe;
    uint32_t newnum;
    int attempts;
    int has_dest_addr;
    static const unsigned char nul = 0;

    if (!a || !a->is_open || !a->is_locked)
        return 0;

    if (body && bodylen == 0)
        bodylen = (uint32_t)strlen(body);

    orig_z = orig_n = orig_node = orig_p = 0;
    dest_z = dest_n = dest_node = dest_p = 0;

    if (oaddr && oaddr[0])
        parse_4d_addr(oaddr, &orig_z, &orig_n, &orig_node, &orig_p);

    if (daddr && daddr[0])
        parse_4d_addr(daddr, &dest_z, &dest_n, &dest_node, &dest_p);

    has_dest_addr = (dest_z != 0 || dest_n != 0 || dest_node != 0);

    if (date_written != 0)
        make_ftn_date(date_written, datetime);
    else
        make_ftn_date((uint32_t)time(NULL), datetime);

    fts_attr = jam_to_ftn_attr(attr);

    /* Header */
    memset(hdr, 0, sizeof(hdr));

    hdr_set_str(hdr + OFF_FROM, 36, from ? from : "");
    hdr_set_str(hdr + OFF_TO, 36, to ? to : "");
    hdr_set_str(hdr + OFF_SUBJECT, 72, subject ? subject : "");
    hdr_set_str(hdr + OFF_DATETIME, 20, datetime);

    hdr_set_u16(hdr + OFF_TIMESREAD, 0);
    hdr_set_u16(hdr + OFF_DESTNODE, dest_node);
    hdr_set_u16(hdr + OFF_ORIGNODE, orig_node);
    hdr_set_u16(hdr + OFF_COST, 0);
    hdr_set_u16(hdr + OFF_ORIGNET, orig_n);
    hdr_set_u16(hdr + OFF_DESTNET, dest_n);
    hdr_set_u16(hdr + OFF_DESTZONE, dest_z);
    hdr_set_u16(hdr + OFF_ORIGZONE, orig_z);
    hdr_set_u16(hdr + OFF_DESTPOINT, dest_p);
    hdr_set_u16(hdr + OFF_ORIGPOINT, orig_p);
    hdr_set_u16(hdr + OFF_REPLYTO, (uint16_t)(reply_to & 0xFFFF));
    hdr_set_u16(hdr + OFF_ATTR, fts_attr);
    hdr_set_u16(hdr + OFF_NEXTREPLY, 0);

    /* Kludge prefix (FMPT/TOPT/INTL): only when dest address exists; avoid duplicates if caller already added them */
    prefix_len = 0;
    prefix[0] = '\0';

    if (has_dest_addr)
    {
        int body_has_fmpt = 0;
        int body_has_topt = 0;
        int body_has_intl = 0;
        char dummy[2];

        if (body && bodylen > 0)
        {
            body_has_fmpt = find_kludge_value(body, bodylen, "FMPT", dummy, sizeof(dummy));
            body_has_topt = find_kludge_value(body, bodylen, "TOPT", dummy, sizeof(dummy));
            body_has_intl = find_kludge_value(body, bodylen, "INTL", dummy, sizeof(dummy));
        }

        if (orig_p > 0 && !body_has_fmpt)
        {
            n = snprintf(prefix + prefix_len, sizeof(prefix) - prefix_len, "\x01FMPT %u\r", (unsigned)orig_p);

            if (n > 0 && (size_t)(prefix_len + n) < sizeof(prefix))
                prefix_len += n;
        }

        if (dest_p > 0 && !body_has_topt)
        {
            n = snprintf(prefix + prefix_len, sizeof(prefix) - prefix_len, "\x01TOPT %u\r", (unsigned)dest_p);

            if (n > 0 && (size_t)(prefix_len + n) < sizeof(prefix))
                prefix_len += n;
        }

        if (!body_has_intl)
        {
            /* INTL always uses 3D (no point); points carried by FMPT/TOPT */
            n = snprintf(prefix + prefix_len, sizeof(prefix) - prefix_len, "\x01INTL %u:%u/%u %u:%u/%u\r", (unsigned)dest_z, (unsigned)dest_n, (unsigned)dest_node, (unsigned)orig_z, (unsigned)orig_n, (unsigned)orig_node);

            if (n > 0 && (size_t)(prefix_len + n) < sizeof(prefix))
                prefix_len += n;
        }
    }

    /* Find next free msgnum: above both high_msg and any HWM left by CrashMail */
    newnum = a->high_msg;

    if (a->hwm > newnum)
        newnum = a->hwm;

    newnum++;

    if (newnum < 2)
        newnum = 2;

    attempts = 0;

    while (attempts <= 1000)
    {
        snprintf(name, sizeof(name), "%u.msg", (unsigned)newnum);

        if (join_path(full, sizeof(full), a->path, name) != 0)
            return 0;

        probe = fopen(full, "rb");

        if (!probe)
            break;

        fclose(probe);

        newnum++;
        attempts++;
    }

    if (attempts > 1000)
        return 0;

    /* Write atomically via .tmp */
    if ((size_t)snprintf(tmp, sizeof(tmp), "%s.tmp", full) >= sizeof(tmp))
        return 0;

    fp = fopen(tmp, "wb");

    if (!fp)
        return 0;

    /* Header */
    if (fwrite(hdr, 1, MSG_STORED_HDR_SIZE, fp) != MSG_STORED_HDR_SIZE)
    {
        fclose(fp);
        pf_remove_file(tmp);
        return 0;
    }

    /* FMPT/TOPT/INTL kludges (if any) */
    if (prefix_len > 0)
    {
        if (fwrite(prefix, 1, (size_t)prefix_len, fp) != (size_t)prefix_len)
        {
            fclose(fp);
            pf_remove_file(tmp);
            return 0;
        }
    }

    /* User body (already contains MSGID/REPLY/TZUTC/charset etc) */
    if (bodylen > 0 && body)
    {
        if (fwrite(body, 1, bodylen, fp) != bodylen)
        {
            fclose(fp);
            pf_remove_file(tmp);
            return 0;
        }
    }

    /* FTS-1 trailing NUL */
    if (fwrite(&nul, 1, 1, fp) != 1)
    {
        fclose(fp);
        pf_remove_file(tmp);
        return 0;
    }

    if (fflush(fp) != 0 || fclose(fp) != 0)
    {
        pf_remove_file(tmp);
        return 0;
    }

    if (pf_atomic_rename(tmp, full) != 0)
    {
        pf_remove_file(tmp);
        return 0;
    }

    nums_insert_sorted(a, newnum);

    return newnum;
}

int msg_delete_msg(MsgArea *a, uint32_t msgnum)
{
    char name[16];
    char full[300];
    int rc;

    if (!a || !a->is_open || !a->is_locked || msgnum < 2)
        return -1;

    snprintf(name, sizeof(name), "%u.msg", (unsigned)msgnum);

    if (join_path(full, sizeof(full), a->path, name) != 0)
        return -1;

    rc = pf_remove_file(full);
    nums_remove_value(a, msgnum);

    return rc;
}

int msg_mark_sent(MsgArea *a, uint32_t msgnum)
{
    char name[16];
    char full[300];
    FILE *fp = NULL;
    unsigned char attr_buf[2];
    uint16_t attr;

    if (!a || !a->is_open || !a->is_locked || msgnum < 2)
        return -1;

    snprintf(name, sizeof(name), "%u.msg", (unsigned)msgnum);

    if (join_path(full, sizeof(full), a->path, name) != 0)
        return -1;

    fp = fopen(full, "r+b");

    if (!fp)
        return -1;

    if (fseek(fp, OFF_ATTR, SEEK_SET) != 0)
    {
        fclose(fp);
        return -1;
    }

    if (fread(attr_buf, 1, 2, fp) != 2)
    {
        fclose(fp);
        return -1;
    }

    attr = hdr_get_u16(attr_buf);
    attr |= FTS_SENT;
    hdr_set_u16(attr_buf, attr);

    if (fseek(fp, OFF_ATTR, SEEK_SET) != 0)
    {
        fclose(fp);
        return -1;
    }

    if (fwrite(attr_buf, 1, 2, fp) != 2)
    {
        fclose(fp);
        return -1;
    }

    if (fflush(fp) != 0)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return 0;
}
