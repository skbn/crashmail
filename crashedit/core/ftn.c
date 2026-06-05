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

/* ftn.c -- FTN message utilities */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <stdint.h> /* uintptr_t in the tagline seed mixer */
#include "ftn.h"
#include "charset.h"
#include "../wrapper.h" /* for WRAPPER_PID - used as default Origin */
#include <time.h>
#if !defined(PLATFORM_WIN32) && !defined(PLATFORM_AMIGA)
#include <unistd.h> /* getpid() for the tagline seed mixer */
#endif

/* Quoting helpers */
/* Dynamic output buffer that grows on demand */
typedef struct
{
    char *buf;
    size_t len; /* bytes used, not counting trailing NUL */
    size_t cap; /* total allocated */
} QBuf;

const char *ftn_sort_presets[FTN_SORT_PRESETS] = {"O", "E", "uE", "-UE", "-TE"};
const char *ftn_sort_labels[FTN_SORT_PRESETS] = {"orig", "name", "new+name", "unread", "total"};

static const FtnAreaInfo *g_ai;
static const char *g_spec;

/* Quote detection: only '>' marks quote depth */
static int is_qchar(char c)
{
    return c == '>';
}

/* FTN kludge keyword table (add new keywords here for auto-detection) */
const char *const ftn_kludge_names[] =
    {
        "MSGID", "REPLY", "INTL", "FMPT", "TOPT",
        "PID", "TID", "TZUTC", "CHRS", "CHARSET",
        "FLAGS", "DOMAIN", "AREA", "ZONE", "ORIGIN",
        "PATH", "Via", "Recd", "RFC-", "GATE",
        NULL};

/* True if line starts with ^A-or-@ followed by known kludge keyword */
int ftn_is_kludge_line(const char *line, int len, char *out_kw, int out_max)
{
    int skip = 0;
    int i;
    int kwlen;
    int j;

    if (!line || len <= 0)
        return 0;

    /* ^A prefix = kludge by FTS-0001 definition; strip all, not just known keywords */
    if ((unsigned char)line[0] == 0x01)
    {
        if (out_kw && out_max > 0)
        {
            int k = 1;
            int n = 0;

            while (k < len && n < out_max - 1 && (isalpha((unsigned char)line[k]) || isdigit((unsigned char)line[k]) || line[k] == '-'))
                out_kw[n++] = line[k++];

            out_kw[n] = '\0';
        }

        return 1;
    }

    if (line[0] == '@')
        skip = 1;
    else
        skip = 0; /* try bare-keyword match (SEEN-BY:, PATH:, Via, Recd) */

    /* Read the keyword */
    i = skip;

    if (i >= len)
        return 0;

    kwlen = 0;

    while (i + kwlen < len && (isalpha((unsigned char)line[i + kwlen]) || isdigit((unsigned char)line[i + kwlen]) || line[i + kwlen] == '-'))
        kwlen++;

    if (kwlen == 0)
        return 0;

    if (i + kwlen < len)
    {
        char c = line[i + kwlen];

        if (c != ':' && c != ' ' && c != '\r' && c != '\n' && c != '\t')
            return 0;
    }

    for (j = 0; ftn_kludge_names[j]; j++)
    {
        const char *kw = ftn_kludge_names[j];
        int klen = (int)strlen(kw);

        if (klen == kwlen && strncasecmp(line + i, kw, (size_t)klen) == 0)
        {
            /* Without prefix: only PATH/Via/Recd match; others need ^A/@  */
            if (skip == 0 && strcasecmp(kw, "PATH") != 0 && strcasecmp(kw, "Via") != 0 && strcasecmp(kw, "Recd") != 0)
                return 0;

            if (out_kw && out_max > 0)
            {
                int copy = (klen < out_max - 1) ? klen : out_max - 1;
                memcpy(out_kw, line + i, (size_t)copy);
                out_kw[copy] = '\0';
            }

            return 1;
        }
    }

    /* SEEN-BY is special: hyphenated keyword */
    if (i + 7 <= len && strncasecmp(line + i, "SEEN-BY", 7) == 0)
    {
        if (i + 7 == len || line[i + 7] == ':' || line[i + 7] == ' ')
        {
            if (out_kw && out_max > 0)
            {
                strncpy(out_kw, "SEEN-BY", (size_t)(out_max - 1));
                out_kw[out_max - 1] = '\0';
            }

            return 1;
        }
    }

    return 0;
}

int ftn_quote_depth(const char *line, int len)
{
    int depth = 0, i = 0;

    if (!line || len <= 0)
        return 0;

    while (i < len && line[i] == ' ')
        i++;

    while (i < len)
    {
        int j = i;

        while (j < len && j - i < 4 && isalpha((unsigned char)line[j]))
            j++;

        /* Also allow single letter pattern like "M>" */
        if (j == i && j < len && isalpha((unsigned char)line[j]))
            j++;

        if (j < len && is_qchar(line[j]))
        {
            depth++;
            i = j + 1;
        }
        else
        {
            break;
        }
    }

    return depth;
}

/* Extract quote string from line */
int ftn_get_quotestr(const char *line, int len, char *qbuf, int qbufsz)
{
    int i = 0;
    int last = -1; /* index just past the final quote char of the prefix */

    if (!line || len <= 0 || !qbuf || qbufsz <= 0)
        return 0;

    /* Skip leading whitespace (kept verbatim in the copy below) */
    while (i < len && (line[i] == ' ' || line[i] == '\t'))
        i++;

    while (i < len)
    {
        char c = line[i];

        if (is_qchar(c))
        {
            i++;
            last = i; /* prefix provisionally ends here */
        }
        else if (isalpha((unsigned char)c) || c == ' ' || c == '\t')
        {
            /* Could be initials ("RW") or the space between tokens
             * Keep scanning; only commit when we actually see a '>' */
            i++;
        }
        else
        {
            break; /* real content begins */
        }
    }

    /* No quote char found at all: not a quoted line */
    if (last < 0)
    {
        qbuf[0] = '\0';
        return 0;
    }

    /* Copy [0, last) verbatim, then include one trailing space if any */
    {
        int qlen = 0;
        int k;

        for (k = 0; k < last && qlen < qbufsz - 1; k++)
            if (line[k] != '\n')
                qbuf[qlen++] = line[k];

        if (last < len && line[last] == ' ' && qlen < qbufsz - 1)
            qbuf[qlen++] = ' ';

        qbuf[qlen] = '\0';

        return qlen;
    }
}

int ftn_is_control(const char *p, int len)
{
    if (!p || len <= 0)
        return 0;

    if (ftn_is_kludge_line(p, len, NULL, 0))
        return 1;

    if (len >= 3 && p[0] == '-' && p[1] == '-' && p[2] == '-' && (len == 3 || p[3] == ' ' || p[3] == '\r' || p[3] == '\n'))
        return 1;

    if (len >= 11 && memcmp(p, " * Origin:", 10) == 0)
        return 1;

    return 0;
}

int ftn_classify_line(const char *line, int len)
{
    int d;

    if (!line || len <= 0)
        return FTN_LT_NORMAL;

    if ((unsigned char)line[0] == 0x01)
        return FTN_LT_KLUDGE;

    if (len >= 8 && memcmp(line, "SEEN-BY:", 8) == 0)
        return FTN_LT_SEENBY;

    if (len >= 5 && memcmp(line, "PATH:", 5) == 0)
        return FTN_LT_PATH;

    if (len >= 3 && line[0] == '.' && line[1] == '.' && line[2] == '.' && (len == 3 || line[3] == ' ' || line[3] == '\r' || line[3] == '\n'))
        return FTN_LT_TAGLINE;

    if (len >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-' && (len == 3 || line[3] == ' ' || line[3] == '\r' || line[3] == '\n'))
        return FTN_LT_TEAR;

    if (len >= 11 && memcmp(line, " * Origin:", 10) == 0)
        return FTN_LT_ORIGIN;

    if (len >= 4 && memcmp(line, "Via ", 4) == 0)
        return FTN_LT_VIA;

    if (len >= 5 && memcmp(line, "Recd ", 5) == 0)
        return FTN_LT_VIA;

    d = ftn_quote_depth(line, len);

    if (d >= 4)
        return FTN_LT_QUOTE4;

    if (d == 3)
        return FTN_LT_QUOTE3;

    if (d == 2)
        return FTN_LT_QUOTE2;

    if (d == 1)
        return FTN_LT_QUOTE1;

    return FTN_LT_NORMAL;
}

/* Kludge handling */
static int is_chrs_kludge(const char *p)
{
    if ((unsigned char)p[0] != 0x01)
        return 0;

    if (strncasecmp(p + 1, "CHRS:", 5) == 0)
        return 1;

    if (strncasecmp(p + 1, "CHRS ", 5) == 0)
        return 1;

    if (strncasecmp(p + 1, "CHARSET:", 8) == 0)
        return 1;

    return 0;
}

/* Find end of line (handles CR/LF/CRLF) */
const char *ftn_next_line(const char *p, int *line_len)
{
    const char *s = p;

    while (*s && *s != '\r' && *s != '\n')
        s++;

    *line_len = (int)(s - p);

    if (*s == '\r')
        s++;

    if (*s == '\n')
        s++;

    return s;
}

void ftn_extract_kludges(const char *body, char **ko, char **co)
{
    const char *p;
    char *kb, *cb;
    int ki = 0, ci = 0;
    size_t blen;

    if (ko)
        *ko = NULL;

    if (co)
        *co = NULL;

    if (!body || !body[0] || !ko || !co)
        return;

    blen = strlen(body);
    kb = (char *)malloc(blen + 1);
    cb = (char *)malloc(blen + 1);

    if (!kb || !cb)
    {
        free(kb);
        free(cb);
        return;
    }

    p = body;

    while (*p)
    {
        int llen;
        const char *np = ftn_next_line(p, &llen);

        if (ftn_is_control(p, llen))
        {
            if (!is_chrs_kludge(p))
            {
                memcpy(&kb[ki], p, (size_t)llen);
                ki += llen;
                kb[ki++] = '\n';
            }
        }
        else
        {
            memcpy(&cb[ci], p, (size_t)llen);
            ci += llen;
            cb[ci++] = '\n';
        }

        p = np;
    }

    kb[ki] = '\0';
    cb[ci] = '\0';

    if (ki > 0)
        *ko = kb;
    else
    {
        *ko = NULL;
        free(kb);
    }

    *co = cb;
}

char *ftn_inject_kludges(const char *saved, const char *cs, const char *body)
{
    char chrs[64];
    char *full, *top, *bot, *cc_kludges, *body_filtered;
    const char *p;
    size_t sz;
    int ti = 0, bi = 0;

    charset_build_kludge(cs, chrs, sizeof(chrs));
    sz = strlen(chrs) + 16;

    if (saved)
        sz += strlen(saved);

    if (body)
        sz += strlen(body);

    /* Process CC kludges from body and filter them out */
    cc_kludges = NULL;
    body_filtered = NULL;

    if (body)
    {
        const char *bp = body;
        char cc_buf[256];
        int cc_count = 0;
        int body_len = 0;

        /* Count CC lines and calculate filtered body length */
        while (*bp)
        {
            int llen;
            const char *np = ftn_next_line(bp, &llen);

            if (strncasecmp(bp, "CC:", 3) == 0)
                cc_count++;
            else
                body_len += llen + 1; /* +1 for newline */

            bp = np;
        }

        if (cc_count > 0)
        {
            /* Allocate buffer for CC kludges and filtered body */
            size_t cc_buf_size = cc_count * 256;
            cc_kludges = (char *)malloc(cc_buf_size);
            body_filtered = (char *)malloc(body_len + 1);

            /* NUL-terminate early so partial alloc leaves empty buffers */
            if (cc_kludges)
                cc_kludges[0] = '\0';

            if (body_filtered)
                body_filtered[0] = '\0';

            if (cc_kludges && body_filtered)
            {
                int bfi = 0;
                bp = body;

                while (*bp)
                {
                    int llen;
                    const char *np = ftn_next_line(bp, &llen);

                    if (strncasecmp(bp, "CC:", 3) == 0)
                    {
                        /* Extract CC destination */
                        const char *cc_start = bp + 3;

                        while (*cc_start == ' ' || *cc_start == '\t')
                            cc_start++;

                        /* Copy CC destination (skip trailing \r\n) */
                        int cc_len = llen - 3;

                        while (cc_len > 0 && (cc_start[cc_len - 1] == '\r' || cc_start[cc_len - 1] == '\n'))
                            cc_len--;

                        if (cc_len > 0)
                        {
                            snprintf(cc_buf, sizeof(cc_buf), "\001CC: %.*s\r\n", cc_len, cc_start);
                            strcat(cc_kludges, cc_buf);
                        }
                    }
                    else
                    {
                        /* Copy non-CC line to filtered body */
                        memcpy(&body_filtered[bfi], bp, (size_t)llen);
                        bfi += llen;
                        body_filtered[bfi++] = '\n';
                    }

                    bp = np;
                }

                body_filtered[bfi] = '\0';
            }
        }
    }

    if (cc_kludges)
        sz += strlen(cc_kludges);

    full = (char *)malloc(sz);
    top = saved ? (char *)malloc(strlen(saved) + 1) : NULL;
    bot = saved ? (char *)malloc(strlen(saved) + 1) : NULL;

    if (!full)
    {
        free(top);
        free(bot);
        free(cc_kludges);
        free(body_filtered);
        return NULL;
    }

    if (saved && (!top || !bot))
    {
        free(full);
        free(top);
        free(bot);
        free(cc_kludges);
        free(body_filtered);

        return NULL;
    }

    /* Split saved: ^A at top, SEEN-BY/tear/origin at bottom */
    if (saved && top && bot)
    {
        p = saved;

        while (*p)
        {
            int llen;
            const char *np = ftn_next_line(p, &llen);

            if ((unsigned char)p[0] == 0x01)
            {
                memcpy(&top[ti], p, (size_t)llen);
                ti += llen;
                top[ti++] = '\n';
            }
            else
            {
                memcpy(&bot[bi], p, (size_t)llen);
                bi += llen;
                bot[bi++] = '\n';
            }
            p = np;
        }

        top[ti] = '\0';
        bot[bi] = '\0';
    }

    full[0] = '\0';

    if (top && ti > 0)
        strcat(full, top);

    strcat(full, chrs);

    if (cc_kludges && cc_kludges[0])
        strcat(full, cc_kludges);

    if (body_filtered && body_filtered[0])
        strcat(full, body_filtered);
    else if (body)
        strcat(full, body);

    if (bot && bi > 0)
        strcat(full, bot);

    free(top);
    free(bot);
    free(cc_kludges);
    free(body_filtered);

    return full;
}

/* Scan body for ^A<kludge>: line; returns pointer to value or NULL */
static const char *find_kludge_value(const char *body, const char *kludge)
{
    const char *p;
    size_t klen;

    if (!body || !kludge)
        return NULL;

    klen = strlen(kludge);

    for (p = body; *p;)
    {
        if ((unsigned char)*p == 0x01 && strncasecmp(p + 1, kludge, klen) == 0 && p[1 + klen] == ':')
        {
            const char *v = p + 2 + klen;

            while (*v == ' ' || *v == '\t')
                v++;

            return v;
        }

        while (*p && *p != '\r' && *p != '\n')
            p++;

        if (*p == '\r')
            p++;

        if (*p == '\n')
            p++;
    }

    return NULL;
}

const char *ftn_find_msgid(const char *body)
{
    return find_kludge_value(body, "MSGID");
}

int ftn_get_kludge_value(const char *body, const char *kludge, char *out, int outsz)
{
    const char *v;
    int i = 0;

    if (!out || outsz <= 0)
        return -1;

    out[0] = '\0';

    v = find_kludge_value(body, kludge);

    if (!v)
        return -1;

    while (*v && *v != '\r' && *v != '\n' && i < outsz - 1)
        out[i++] = *v++;

    out[i] = '\0';

    return 0;
}

/* Quoting helpers */
static int qbuf_reserve(QBuf *q, size_t need)
{
    size_t want;
    char *nb;

    if (q->len + need + 1 <= q->cap)
        return 0;

    want = q->cap ? q->cap * 2 : 256;

    while (want < q->len + need + 1)
        want *= 2;

    nb = (char *)realloc(q->buf, want);

    if (!nb)
        return -1;

    q->buf = nb;
    q->cap = want;

    return 0;
}

static int qbuf_append(QBuf *q, const char *s, size_t n)
{
    if (qbuf_reserve(q, n) != 0)
        return -1;

    memcpy(q->buf + q->len, s, n);

    q->len += n;
    q->buf[q->len] = '\0';

    return 0;
}

static int qbuf_putc(QBuf *q, char c)
{
    return qbuf_append(q, &c, 1);
}

/* Append line with prefix, word-wrapping at margin columns */
static int qbuf_append_wrapped(QBuf *q, const char *prefix, const char *line, int line_len, int margin)
{
    size_t plen = strlen(prefix);
    int pos = 0;

    /* Empty line: emit only newline */
    if (line_len == 0)
        return qbuf_putc(q, '\n');

    if (margin <= 0 || (int)plen + line_len <= margin)
    {
        /* Fits in one line, or wrap disabled: emit verbatim */
        if (qbuf_append(q, prefix, plen) != 0)
            return -1;

        if (qbuf_append(q, line, (size_t)line_len) != 0)
            return -1;

        return qbuf_putc(q, '\n');
    }

    /* Word-wrap: break at last space before margin, or hard-break */
    while (pos < line_len)
    {
        int avail = margin - (int)plen;
        int chunk;
        int brk;

        if (avail < 1)
            avail = 1;

        if (line_len - pos <= avail)
        {
            chunk = line_len - pos;
        }
        else
        {
            /* Look for last space in window */
            chunk = avail;

            for (brk = chunk; brk > 0; brk--)
            {
                if (line[pos + brk] == ' ' || line[pos + brk] == '\t')
                {
                    chunk = brk;
                    break;
                }
            }

            if (brk == 0)
            {
                /* No space: hard break */
                chunk = avail;
            }
        }

        if (qbuf_append(q, prefix, plen) != 0)
            return -1;

        if (qbuf_append(q, line + pos, (size_t)chunk) != 0)
            return -1;

        if (qbuf_putc(q, '\n') != 0)
            return -1;

        pos += chunk;

        /* Skip leading space on next line */
        while (pos < line_len && (line[pos] == ' ' || line[pos] == '\t'))
            pos++;
    }

    return 0;
}

/* Quote body */
char *ftn_quote_body(const char *body)
{
    const char *p;
    char *qb;
    size_t qmax;
    int qi = 0;

    if (!body || !body[0])
        return NULL;

    qmax = strlen(body) * 3 + 1;
    qb = (char *)malloc(qmax);

    if (!qb)
        return NULL;

    p = body;

    while (*p)
    {
        int llen, n;
        const char *np = ftn_next_line(p, &llen);

        if (ftn_is_control(p, llen))
        {
            p = np;
            continue;
        }

        n = snprintf(&qb[qi], qmax - (size_t)qi, "> %.*s\n", llen, p);

        if (n > 0)
        {
            if ((size_t)n >= qmax - (size_t)qi)
                n = (int)(qmax - (size_t)qi) - 1;

            if (n > 0)
                qi += n;
        }

        if ((size_t)qi >= qmax - 4)
            break;

        p = np;
    }

    qb[qi] = '\0';

    return qb;
}

/* Compute initials (up to 2 chars) from UTF-8 name */
static void compute_initials(const char *name, char out[3])
{
    const char *p;
    int n = 0;
    out[0] = ' ';
    out[1] = ' ';
    out[2] = '\0';

    if (!name)
        return;

    p = name;

    while (*p && (unsigned char)*p <= ' ')
        p++;

    if (*p && (unsigned char)*p < 0x80 && isalpha((unsigned char)*p))
    {
        out[n++] = (char)toupper((unsigned char)*p);
    }
    else if (*p)
    {
        out[n++] = *p;
    }

    while (*p && (unsigned char)*p > ' ')
        p++;

    while (*p && (unsigned char)*p <= ' ')
        p++;

    if (*p && n < 2)
    {
        if ((unsigned char)*p < 0x80 && isalpha((unsigned char)*p))
            out[n++] = (char)toupper((unsigned char)*p);
        else
            out[n++] = *p;
    }

    if (n == 1 && name)
    {
        const char *q = name;

        while (*q && (unsigned char)*q <= ' ')
            q++;

        if (*q)
            q++;

        if (*q && (unsigned char)*q > ' ')
        {
            if ((unsigned char)*q < 0x80 && isalpha((unsigned char)*q))
                out[n++] = (char)tolower((unsigned char)*q);
            else
                out[n++] = *q;
        }
    }

    while (n < 2)
        out[n++] = ' ';

    out[2] = '\0';
}

/* Core quoter */
static char *quote_body_named_core(const char *body, const char *from_name, int margin, int keep_kludges)
{
    QBuf q;
    const char *p;
    char ini[3];
    char prefix[8];
    char deepbuf[64];
    char tmp_kludge[1024];

    if (!body || !body[0])
        return NULL;

    q.buf = NULL;
    q.len = 0;
    q.cap = 0;

    if (qbuf_reserve(&q, 256) != 0)
        return NULL;

    q.buf[0] = '\0';

    compute_initials(from_name, ini);

    /* GoldED+ format: " XX> " (leading space + initials + '>' + space) */
    prefix[0] = ' ';
    prefix[1] = ini[0];
    prefix[2] = ini[1];
    prefix[3] = '>';
    prefix[4] = ' ';
    prefix[5] = '\0';

    p = body;

    while (*p)
    {
        int llen;
        const char *np = ftn_next_line(p, &llen);
        const char *content = p;
        int content_len = llen;
        const char *use_prefix = prefix;
        int depth;

        /* ^A kludge: drop or convert-to-@ depending on keep_kludges */
        if (llen > 0 && (unsigned char)p[0] == 0x01)
        {
            int copy_len;

            if (!keep_kludges)
            {
                p = np;
                continue;
            }

            /* Build "@<rest>" in tmp_kludge, then quote that as text */
            if (llen >= (int)sizeof(tmp_kludge))
                copy_len = (int)sizeof(tmp_kludge) - 2;
            else
                copy_len = llen - 1;

            tmp_kludge[0] = '@';

            if (copy_len > 0)
                memcpy(tmp_kludge + 1, p + 1, (size_t)copy_len);

            content = tmp_kludge;
            content_len = copy_len + 1;
        }
        /* PATH kludge (convert to @PATH: when keep_kludges active) */
        else if (keep_kludges && llen >= 5 && memcmp(p, "PATH:", 5) == 0)
        {
            int copy_len = llen;

            if (copy_len >= (int)sizeof(tmp_kludge) - 1)
                copy_len = (int)sizeof(tmp_kludge) - 2;

            tmp_kludge[0] = '@';

            if (copy_len > 0)
                memcpy(tmp_kludge + 1, p, (size_t)copy_len);

            content = tmp_kludge;
            content_len = copy_len + 1;
        }

        /* Already quoted? Extract prefix and deepen */
        depth = ftn_quote_depth(content, content_len);

        if (depth > 0)
        {
            int qlen = ftn_get_quotestr(content, content_len, deepbuf, sizeof(deepbuf));

            if (qlen > 0 && qlen < (int)sizeof(deepbuf) - 2)
            {
                /* Find the first '>' (or quote char) in deepbuf and
                 * insert an extra '>' right after it */
                int fpos = 0;

                while (fpos < qlen && !is_qchar(deepbuf[fpos]))
                    fpos++;

                if (fpos < qlen)
                {
                    /* Insert '>' at position fpos+1 */
                    memmove(deepbuf + fpos + 2, deepbuf + fpos + 1, (size_t)(qlen - fpos - 1 + 1)); /* +1 for NUL */
                    deepbuf[fpos + 1] = '>';
                }
                else
                {
                    /* No quote char located (shouldn't happen): append */
                    deepbuf[qlen] = '>';
                    deepbuf[qlen + 1] = '\0';
                }

                use_prefix = deepbuf;
                content += qlen;
                content_len -= qlen;
            }
        }

        if (qbuf_append_wrapped(&q, use_prefix, content, content_len, margin) != 0)
        {
            free(q.buf);
            return NULL;
        }

        p = np;
    }

    return q.buf;
}

char *ftn_quote_body_named_wrap(const char *body, const char *from_name, int margin)
{
    return quote_body_named_core(body, from_name, margin, 0);
}

char *ftn_quote_body_named(const char *body, const char *from_name)
{
    return quote_body_named_core(body, from_name, 75, 0);
}

char *ftn_quote_body_named_full_wrap(const char *body, const char *from_name, int margin)
{
    return quote_body_named_core(body, from_name, margin, 1);
}

char *ftn_quote_body_named_full(const char *body, const char *from_name)
{
    return quote_body_named_core(body, from_name, 75, 1);
}

/* FTN address helpers */
int ftn_addr_zone(const char *addr)
{
    int z = 0;

    if (!addr)
        return 0;

    while (*addr == ' ' || *addr == '\t')
        addr++;

    while (*addr >= '0' && *addr <= '9')
    {
        z = z * 10 + (*addr - '0');
        addr++;
    }

    if (*addr == ':')
        return z;

    return 0;
}

int ftn_addr_net(const char *addr)
{
    int n = 0;

    if (!addr)
        return 0;

    while (*addr == ' ' || *addr == '\t')
        addr++;

    /* Skip zone */
    while (*addr >= '0' && *addr <= '9')
        addr++;

    if (*addr != ':')
        return 0;

    addr++; /* Skip ':' */

    while (*addr >= '0' && *addr <= '9')
    {
        n = n * 10 + (*addr - '0');
        addr++;
    }

    if (*addr == '/' || *addr == '\0')
        return n;

    return 0;
}

int ftn_addr_node(const char *addr)
{
    int n = 0;

    if (!addr)
        return 0;

    while (*addr == ' ' || *addr == '\t')
        addr++;

    /* Skip zone */
    while (*addr >= '0' && *addr <= '9')
        addr++;

    if (*addr != ':')
        return 0;

    addr++; /* Skip ':' */

    while (*addr >= '0' && *addr <= '9')
        addr++;

    if (*addr != '/')
        return 0;

    addr++;

    while (*addr >= '0' && *addr <= '9')
    {
        n = n * 10 + (*addr - '0');
        addr++;
    }

    if (*addr == '.' || *addr == '\0')
        return n;

    return 0;
}

int ftn_addr_point(const char *addr)
{
    int p = 0;

    if (!addr)
        return 0;

    while (*addr == ' ' || *addr == '\t')
        addr++;

    /* Skip zone */
    while (*addr >= '0' && *addr <= '9')
        addr++;

    if (*addr != ':')
        return 0;

    addr++;

    /* Skip net */
    while (*addr >= '0' && *addr <= '9')
        addr++;

    if (*addr != '/')
        return 0;

    addr++;

    /* Skip node */
    while (*addr >= '0' && *addr <= '9')
        addr++;

    if (*addr != '.')
        return 0;

    addr++;

    while (*addr >= '0' && *addr <= '9')
    {
        p = p * 10 + (*addr - '0');
        addr++;
    }

    return p;
}

int ftn_aka_match_zone(const char *akas_base, int aka_count, size_t stride, const char *dest_addr)
{
    int dz, i;

    if (!akas_base || aka_count <= 0 || stride == 0 || !dest_addr || !dest_addr[0])
        return -1;

    dz = ftn_addr_zone(dest_addr);

    if (dz <= 0)
        return -1;

    for (i = 0; i < aka_count; i++)
    {
        const char *p = akas_base + (size_t)i * stride;

        if (p[0] && ftn_addr_zone(p) == dz)
            return i;
    }

    return -1;
}

/* Tagline & signature */
char *ftn_random_tagline(const char *path)
{
    FILE *f;
    char buf[256];
    char **lines = NULL;
    int count = 0, cap = 0;
    char *out = NULL;
    int i;

    if (!path || !path[0])
        return NULL;

    f = fopen(path, "r");

    if (!f)
        return NULL;

    while (fgets(buf, sizeof(buf), f))
    {
        int l;
        char *p = buf;

        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '#' || *p == ';' || *p == '\0' || *p == '\r' || *p == '\n')
            continue;

        l = (int)strlen(p);

        while (l > 0 && (p[l - 1] == '\r' || p[l - 1] == '\n' || p[l - 1] == ' ' || p[l - 1] == '\t'))
            p[--l] = '\0';

        if (l == 0)
            continue;

        if (count >= cap)
        {
            int nc = cap ? cap * 2 : 16;
            char **nl = (char **)realloc(lines, (size_t)nc * sizeof(char *));

            if (!nl)
                break;

            lines = nl;
            cap = nc;
        }

        lines[count] = (char *)malloc((size_t)l + 1);

        if (!lines[count])
            break;

        memcpy(lines[count], p, (size_t)l + 1);
        count++;
    }

    fclose(f);

    if (count > 0)
    {
        /* Pseudo-random pick with mixed entropy to avoid collisions */
        unsigned long seed = (unsigned long)time(NULL);
        seed ^= (unsigned long)(uintptr_t)&seed;
        seed ^= (unsigned long)count * 2654435761UL; /* Knuth multiplier */

        if (path)
        {
            const unsigned char *u = (const unsigned char *)path;
            while (*u)
            {
                seed = seed * 31u + *u++;
            }
        }

#if !defined(PLATFORM_WIN32) && !defined(PLATFORM_AMIGA)
        seed ^= (unsigned long)getpid() * 16807UL;
#endif

        int idx = (int)(seed % (unsigned long)count);
        out = lines[idx];
        lines[idx] = NULL;
    }

    for (i = 0; i < count; i++)
        free(lines[i]);

    free(lines);

    return out;
}

char *ftn_apply_signature(const char *origin, const char *tearline, const char *tagline_file, const char *body, const char *oaddr, int is_echo)
{
    int has_origin;
    size_t bodylen, need;
    char *out;
    char *tag;
    int taglen;
    const char *tear;
    const char *org;

    if (!body)
        return NULL;

    /* Tear line + Origin are echomail-only conventions */
    if (!is_echo)
    {
        out = (char *)malloc(strlen(body) + 1);

        if (!out)
            return NULL;

        strcpy(out, body);
        return out;
    }

    /* Resolve defaults: tearline empty -> WRAPPER_PID, origin empty -> empty */
    tear = (tearline && tearline[0]) ? tearline : WRAPPER_PID;
    org = (origin && origin[0]) ? origin : "";

    /* If body already contains " * Origin:" line, leave it untouched */
    has_origin = (strstr(body, "\n * Origin:") != NULL) || (strncmp(body, " * Origin:", 10) == 0);

    if (has_origin)
    {
        out = (char *)malloc(strlen(body) + 1);

        if (!out)
            return NULL;

        strcpy(out, body);
        return out;
    }

    tag = ftn_random_tagline(tagline_file);
    taglen = tag ? (int)strlen(tag) : 0;

    bodylen = strlen(body);
    need = bodylen + (size_t)taglen + strlen(tear) + strlen(org) + (oaddr ? strlen(oaddr) : 0) + 80;
    out = (char *)malloc(need);

    if (!out)
    {
        free(tag);
        return NULL;
    }

    memcpy(out, body, bodylen);
    out[bodylen] = '\0';

    /* Ensure body ends with exactly one newline */
    if (bodylen == 0 || out[bodylen - 1] != '\n')
        strcat(out, "\n");

    if (tag && tag[0])
    {
        strcat(out, "\n... ");
        strcat(out, tag);
        strcat(out, "\n\n");
    }

    free(tag);

    strcat(out, "--- ");
    strcat(out, tear);
    strcat(out, "\n");
    strcat(out, " * Origin: ");
    strcat(out, org); /* may be empty -> "* Origin:  (aka)" */

    if (oaddr && oaddr[0])
    {
        strcat(out, " (");
        strcat(out, oaddr);
        strcat(out, ")");
    }

    strcat(out, "\n");

    return out;
}

void ftn_lf_to_cr(char *body)
{
    char *p;

    if (!body)
        return;

    for (p = body; *p; p++)
    {
        if (*p == '\n')
            *p = '\r';
    }
}

int ftn_find_original(const JamMsgInfo *msgs, int count, int cur)
{
    uint32_t rt;
    int i;

    if (!msgs || cur < 0 || cur >= count)
        return -1;

    rt = msgs[cur].reply_to;

    if (rt != 0)
        return jam_find_by_msgnum(msgs, count, rt);

    /* Fallback: search by CRC if reply_to is 0 but we have reply_crc */
    if (msgs[cur].reply_crc != 0 && msgs[cur].reply_crc != (uint32_t)-1)
    {
        for (i = 0; i < count; i++)
        {
            if (msgs[i].msgid_crc == msgs[cur].reply_crc)
                return i;
        }
    }

    return -1;
}

int ftn_find_original_by_msgid(const JamMsgInfo *msgs, int count, const char *reply_msgid)
{
    int i;

    if (!msgs || !reply_msgid || !reply_msgid[0])
        return -1;

    for (i = 0; i < count; i++)
    {
        if (msgs[i].msgid[0] && strcmp(msgs[i].msgid, reply_msgid) == 0)
            return i;
    }

    return -1;
}

int ftn_find_reply(const JamMsgInfo *msgs, int count, int cur)
{
    uint32_t mn, mcrc;
    int i;

    if (!msgs || cur < 0 || cur >= count)
        return -1;

    mn = msgs[cur].msgnum;

    for (i = cur + 1; i < count; i++)
    {
        if (msgs[i].reply_to == mn)
            return i;
    }

    for (i = 0; i < cur; i++)
    {
        if (msgs[i].reply_to == mn)
            return i;
    }

    /* Fallback: search by CRC if no reply_to match found */
    mcrc = msgs[cur].msgid_crc;

    if (mcrc != 0 && mcrc != (uint32_t)-1)
    {
        for (i = cur + 1; i < count; i++)
        {
            if (msgs[i].reply_crc == mcrc)
                return i;
        }

        for (i = 0; i < cur; i++)
        {
            if (msgs[i].reply_crc == mcrc)
                return i;
        }
    }

    return -1;
}

int ftn_find_all_replies(const JamMsgInfo *msgs, int count, int cur, int *out, int out_max)
{
    uint32_t mn, mcrc;
    int i, n = 0;

    if (!msgs || !out || out_max <= 0 || cur < 0 || cur >= count)
        return 0;

    mn = msgs[cur].msgnum;
    mcrc = msgs[cur].msgid_crc;

    if (mn == 0 && (mcrc == 0 || mcrc == (uint32_t)-1))
        return 0;

    for (i = 0; i < count && n < out_max; i++)
    {
        if (msgs[i].reply_to == mn)
            out[n++] = i;
        else if (mcrc != 0 && mcrc != (uint32_t)-1 && msgs[i].reply_crc == mcrc)
            out[n++] = i;
    }

    return n;
}

int ftn_next_unread(const JamMsgInfo *msgs, int count, int cur, uint32_t lastread)
{
    int i;

    if (!msgs)
        return -1;

    for (i = cur + 1; i < count; i++)
    {
        if (msgs[i].msgnum > lastread)
            return i;
    }

    return -1;
}

int ftn_prev_unread(const JamMsgInfo *msgs, int count, int cur, uint32_t lastread)
{
    int i;

    if (!msgs)
        return -1;

    for (i = cur - 1; i >= 0; i--)
    {
        if (msgs[i].msgnum > lastread)
            return i;
    }

    return -1;
}

static int istristr(const char *h, const char *n)
{
    size_t nl, hl;

    if (!n || !n[0])
        return 1; /* empty needle matches everything */

    if (!h)
        return 0; /* NULL haystack matches nothing */

    nl = strlen(n);
    hl = strlen(h);

    if (nl > hl)
        return 0; /* needle longer than haystack */

    while (*h)
    {
        size_t i;

        if (hl < nl)
            return 0; /* remaining haystack too short */

        for (i = 0; i < nl; i++)
        {
            if (tolower((unsigned char)h[i]) != tolower((unsigned char)n[i]))
                break;
        }

        if (i == nl)
            return 1;

        h++;
        hl--;
    }

    return 0;
}

static int area_cmp(const void *va, const void *vb)
{
    int ia = *(const int *)va, ib = *(const int *)vb;
    const FtnAreaInfo *a = &g_ai[ia], *b = &g_ai[ib];
    const FtnAreaInfo *A, *B;
    const char *p = g_spec;
    int rev = 0, cmp = 0;

    while (*p && cmp == 0)
    {
        A = rev ? b : a;
        B = rev ? a : b;

        switch (*p)
        {
        case '-':
            rev = 1;
            break;
        case '+':
            rev = 0;
            break;
        case 'E':
        case 'e':
            cmp = strcasecmp(A->name ? A->name : "", B->name ? B->name : "");
            rev = 0;
            break;
        case 'U':
        {
            int ua = A->unread > 0 ? A->unread : 0;
            int ub = B->unread > 0 ? B->unread : 0;

            if (!rev)
            {
                if (!ua)
                    ua = 0x7FFFFFFF;
                if (!ub)
                    ub = 0x7FFFFFFF;
            }

            cmp = (ua > ub) - (ua < ub);
            rev = 0;
            break;
        }
        case 'u':
            cmp = (B->unread > 0 ? 1 : 0) - (A->unread > 0 ? 1 : 0);
            rev = 0;
            break;
        case 'T':
        case 't':
            cmp = (A->total > B->total) - (A->total < B->total);
            rev = 0;
            break;
        case 'O':
        case 'o':
            cmp = (ia > ib) - (ia < ib);
            rev = 0;
            break;
        default:
            break;
        }
        p++;
    }

    return cmp;
}

int ftn_sort_areas(const FtnAreaInfo *areas, int count, const char *spec, const char *filter, int *order_out)
{
    int i, vis = 0;

    if (!areas || !order_out || count <= 0)
        return 0;

    for (i = 0; i < count; i++)
    {
        if (filter && filter[0])
        {
            if (!istristr(areas[i].name, filter) && !istristr(areas[i].desc, filter))
                continue;
        }

        order_out[vis++] = i;
    }

    if (vis > 1 && spec && spec[0])
    {
        g_ai = areas;
        g_spec = spec;
        qsort(order_out, (size_t)vis, sizeof(int), area_cmp);
    }

    return vis;
}

/* Detect system timezone offset in minutes (+east/-west of UTC) */
int ftn_detect_timezone_offset()
{
    time_t t = time(NULL);
    struct tm utc_tm, local_tm;
    time_t t_utc, t_local;
    int dt;

    memset(&utc_tm, 0, sizeof(utc_tm));
    memset(&local_tm, 0, sizeof(local_tm));

    /* Get UTC time */
#ifdef PLATFORM_WIN32
    if (gmtime_s(&utc_tm, &t) != 0)
        return 0;
#else
    if (!gmtime_r(&t, &utc_tm))
        return 0;
#endif

    utc_tm.tm_isdst = -1;

    /* Get local time */
#ifdef PLATFORM_WIN32
    if (localtime_s(&local_tm, &t) != 0)
        return 0;
#else
    if (!localtime_r(&t, &local_tm))
        return 0;
#endif

    local_tm.tm_isdst = -1;

    /* Convert both to timestamps using mktime (which treats as local) */
    /* This is the same trick GoldED+ uses */
    t_utc = mktime(&utc_tm);
    t_local = mktime(&local_tm);

    /* The difference is the timezone offset */
#ifdef PLATFORM_AMIGA
    dt = (int)(t_local - t_utc);
#else
    dt = (int)difftime(t_local, t_utc);
#endif

    return dt / 60; /* Convert to minutes */
}

/* TZ offset in minutes (+east). Auto-detected unless manually set;
 * on AmigaOS the config value is always authoritative */
int ftn_effective_tz_offset(int cfg_offset, int cfg_is_manual)
{
#ifdef PLATFORM_AMIGA
    return cfg_offset;
#else
    if (cfg_is_manual)
        return cfg_offset;

    return ftn_detect_timezone_offset();
#endif
}

void ftn_build_tzutc(int offset_mins, char *out, int outsz)
{
    int sign, hours, mins;

    if (!out || outsz < 20)
    {
        if (out && outsz > 0)
            out[0] = '\0';

        return;
    }

    if (offset_mins < 0)
    {
        sign = 1;
        offset_mins = -offset_mins;
    }
    else
        sign = 0;

    hours = offset_mins / 60;
    mins = offset_mins % 60;

    /* Build TZUTC kludge (FTS-4008: positive without sign, negative with -) */
    out[0] = 1; /* SOH */

    if (sign)
        snprintf(out + 1, (size_t)outsz - 1, "TZUTC: -%02d%02d\r", hours, mins);
    else
        snprintf(out + 1, (size_t)outsz - 1, "TZUTC: %02d%02d\r", hours, mins);
}

void ftn_build_msgid(const char *addr, char *out, int outsz)
{
    static unsigned long serial = 0;

    if (!out || outsz < 2)
        return;

    serial++;
    snprintf(out, (size_t)outsz, "\x01MSGID: %s %08lx\r", (addr && addr[0]) ? addr : "0:0/0", (unsigned long)(time(NULL) ^ (serial * 2654435761UL)));
}

void ftn_build_reply(const char *msgid_value, char *out, int outsz)
{
    if (!out || outsz < 2)
        return;

    if (!msgid_value || !msgid_value[0])
    {
        out[0] = '\0';
        return;
    }

    snprintf(out, (size_t)outsz, "\x01REPLY: %s\r", msgid_value);
}

/* Find unquoted " * Origin:" line (FTS-0004); NULL if absent */
static const char *find_unquoted_origin_line(const char *body)
{
    const char *p;

    if (!body)
        return NULL;

    /* Match at start of body or after any '\n' or '\r' */
    if (strncmp(body, " * Origin:", 10) == 0)
        return body;

    for (p = body; *p; p++)
    {
        if ((*p == '\n' || *p == '\r') && strncmp(p + 1, " * Origin:", 10) == 0)
            return p + 1;
    }

    return NULL;
}

int ftn_find_origin_address(const char *body, char *out, int outsz)
{
    const char *line, *end, *open, *close, *q;
    int len;
    int i, has_digit = 0, has_colon = 0, has_slash = 0;

    if (out && outsz > 0)
        out[0] = '\0';

    if (!body || !out || outsz <= 0)
        return -1;

    line = find_unquoted_origin_line(body);

    if (!line)
        return -1;

    /* Find end of line */
    for (end = line; *end && *end != '\r' && *end != '\n'; end++)
        ;

    /* Last '(' before end-of-line */
    open = NULL;

    for (q = line; q < end; q++)
    {
        if (*q == '(')
            open = q;
    }

    if (!open)
        return -1;

    close = NULL;

    for (q = open + 1; q < end; q++)
    {
        if (*q == ')')
        {
            close = q;
            break;
        }
    }

    if (!close)
        return -1;

    len = (int)(close - open - 1);

    if (len <= 0 || len >= outsz)
        return -1;

    /* Sanity: must look like an FTN address (digit, ':', '/' or '.') */
    for (i = 0; i < len; i++)
    {
        char c = open[1 + i];

        if (c >= '0' && c <= '9')
            has_digit = 1;
        else if (c == ':')
            has_colon = 1;
        else if (c == '/')
            has_slash = 1;
        else if (c != '.')
            return -1;
    }

    if (!has_digit || !has_colon || !has_slash)
        return -1;

    memcpy(out, open + 1, (size_t)len);
    out[len] = '\0';

    return 0;
}

static int is_qchar_w(wchar_t c)
{
    /*return c == L'>' || c == L'|' || c == L']' || c == L')';*/
    return c == L'>';
}

int ftn_quote_depth_wcs(const wchar_t *line, int len)
{
    int depth = 0, i = 0;

    if (!line || len <= 0)
        return 0;

    while (i < len && line[i] == L' ')
        i++;

    while (i < len)
    {
        int j = i;

        while (j < len && j - i < 4 && iswalpha(line[j]))
            j++;

        if (j < len && is_qchar_w(line[j]))
        {
            depth++;
            i = j + 1;

            while (i < len && line[i] == L' ')
                i++;
        }
        else
            break;
    }

    return depth;
}

int ftn_classify_wcs(const wchar_t *line, int len)
{
    int depth;

    if (!line || len <= 0)
        return FTN_LT_NORMAL;

    /* Kludge: ^A = 0x01 */
    if (line[0] == 0x01)
        return FTN_LT_KLUDGE;

    /* SEEN-BY: */
    if (len >= 8 && wcsncmp(line, L"SEEN-BY:", 8) == 0)
        return FTN_LT_SEENBY;

    /* PATH: (FTS-5005) */
    if (len >= 5 && wcsncmp(line, L"PATH:", 5) == 0)
        return FTN_LT_PATH;

    /* Tagline: "... " (3 dots + space/eol) */
    if (len >= 3 && line[0] == L'.' && line[1] == L'.' && line[2] == L'.' && (len == 3 || line[3] == L' ' || line[3] == L'\r' || line[3] == L'\n'))
        return FTN_LT_TAGLINE;

    /* Tear line: "---" or "--- text" */
    if (len >= 3 && line[0] == L'-' && line[1] == L'-' && line[2] == L'-' && (len == 3 || line[3] == L' ' || line[3] == L'\r' || line[3] == L'\n'))
        return FTN_LT_TEAR;

    /* Origin: " * Origin:" */
    if (len >= 11 && wcsncmp(line, L" * Origin:", 10) == 0)
        return FTN_LT_ORIGIN;

    /* Via: ^AVia */
    if (len >= 4 && line[0] == 0x01 && wcsncmp(line + 1, L"Via", 3) == 0)
        return FTN_LT_VIA;

    /* Quote levels */
    depth = ftn_quote_depth_wcs(line, len);

    if (depth >= 4)
        return FTN_LT_QUOTE4;

    if (depth == 3)
        return FTN_LT_QUOTE3;

    if (depth == 2)
        return FTN_LT_QUOTE2;

    if (depth == 1)
        return FTN_LT_QUOTE1;

    return FTN_LT_NORMAL;
}
