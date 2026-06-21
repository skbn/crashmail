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

/* wrapper_io.c -- High-level read/write pipeline. UTF-8 internal */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wrapper_io.h"
#include "utf8.h"
#include "charset.h"
#include "ftn.h"

/* TZUTC stored as kludge in body (not JAM subfield) */

/* Check if body contains CHRS/CHARSET kludge */
static int body_has_chrs_kludge(const char *utf8)
{
    const char *p;

    if (!utf8)
        return 0;

    for (p = utf8; *p; p++)
    {
        if ((unsigned char)*p != 0x01)
            continue;

        if (strncasecmp(p + 1, "CHRS:", 5) == 0 || strncasecmp(p + 1, "CHRS ", 5) == 0 || strncasecmp(p + 1, "CHARSET:", 8) == 0)
            return 1;
    }

    return 0;
}

char *wrapper_read_utf8_ex(MsgBase *a, uint32_t msgnum, const char *override_enc, const char *fallback_enc, char *detected_out, int detected_sz)
{
    char *raw, *utf8;
    uint32_t rawlen;
    char detected[CHARSET_NAME_MAX];
    const char *use_enc;
    int srclen, dstmax;
    int has_chrs;

    if (detected_out && detected_sz > 0)
        detected_out[0] = '\0';

    if (!a)
        return NULL;

    raw = mb_read_body(a, msgnum, &rawlen);

    if (!raw)
        return NULL;

    /* charset_detect fills output (defaults if no kludge), check for actual kludge presence */
    charset_detect(raw, detected, sizeof(detected));
    has_chrs = body_has_chrs_kludge(raw);

    /* Decide which charset to actually decode with */
    if (override_enc && override_enc[0])
        use_enc = override_enc; /* user-forced view charset */
    else if (has_chrs)
        use_enc = detected; /* CHRS kludge says so */
    else if (fallback_enc && fallback_enc[0])
        use_enc = fallback_enc; /* config default */
    else
        use_enc = CHARSET_READ_DEFAULT; /* hard-coded last resort */

    /* Report charset actually used for decoding to caller */
    if (detected_out && detected_sz > 0)
    {
        strncpy(detected_out, use_enc, (size_t)(detected_sz - 1));
        detected_out[detected_sz - 1] = '\0';
    }

    /* Already UTF-8? Return as-is (no conversion needed) */
    if (strcasecmp(use_enc, "UTF-8") == 0)
        return raw;

    /* Convert to UTF-8 (worst case: each byte -> 3 UTF-8 bytes) */
    srclen = (int)rawlen;
    dstmax = srclen * 3 + 256;
    utf8 = (char *)malloc((size_t)dstmax);

    if (!utf8)
    {
        free(raw);
        return NULL;
    }

    charset_body_to_utf8(use_enc, raw, srclen, utf8, dstmax);
    free(raw);

    return utf8;
}

char *wrapper_read_utf8(MsgBase *a, uint32_t msgnum, char *enc_out)
{
    /* Backwards-compatible wrapper, use _ex to distinguish "no CHRS" from "CHRS UTF-8" */
    char detected[CHARSET_NAME_MAX];
    char *body = NULL;

    detected[0] = '\0';
    body = wrapper_read_utf8_ex(a, msgnum, NULL, NULL, detected, sizeof(detected));

    if (enc_out)
    {
        if (detected[0])
        {
            strncpy(enc_out, detected, CHARSET_NAME_MAX - 1);
            enc_out[CHARSET_NAME_MAX - 1] = '\0';
        }
        else
        {
            strncpy(enc_out, CHARSET_READ_DEFAULT, CHARSET_NAME_MAX - 1);
            enc_out[CHARSET_NAME_MAX - 1] = '\0';
        }
    }

    return body;
}

char *wrapper_prepare_body(const char *utf8_body, const char *saved_kludges, const char *out_charset, int *out_len)
{
    char *injected, *converted;
    int ilen, cmax, clen;
    const char *cs;
    const char *body_or_empty;
    int i;

    if (out_len)
        *out_len = 0;

    cs = (out_charset && out_charset[0]) ? out_charset : CHARSET_WRITE_DEFAULT;

    /* TZUTC stored as kludge in body via ftn_build_tzutc */
    body_or_empty = utf8_body ? utf8_body : "";
    injected = ftn_inject_kludges(saved_kludges, cs, body_or_empty);

    if (!injected)
        return NULL;

    ilen = (int)strlen(injected);

    /* Convert from UTF-8 to output charset */
    if (strcasecmp(cs, "UTF-8") == 0)
    {
        converted = injected; /* no conversion needed */
        clen = ilen;
    }
    else
    {
        cmax = ilen + 256; /* charset bytes <= UTF-8 bytes */
        converted = (char *)malloc((size_t)cmax);

        if (!converted)
        {
            free(injected);
            return NULL;
        }

        clen = charset_body_from_utf8(cs, injected, ilen, converted, cmax);

        free(injected);
    }

    /* LF -> CR (FTN standard); iterate by clen to avoid NUL traps */
    for (i = 0; i < clen; i++)
    {
        if (converted[i] == '\n')
            converted[i] = '\r';
    }

    if (out_len)
        *out_len = clen;

    return converted;
}

/* Convert UTF-8 header field to output charset, returns malloc'd buffer (caller frees) */
static char *header_to_charset(const char *utf8, const char *cs)
{
    int slen, dstmax, n;
    char *dst = NULL;

    if (!utf8)
        return NULL;

    if (!cs || !cs[0] || strcasecmp(cs, "UTF-8") == 0)
    {
        dst = (char *)malloc(strlen(utf8) + 1);

        if (dst)
            strcpy(dst, utf8);

        return dst;
    }

    slen = (int)strlen(utf8);
    dstmax = slen + 32; /* charset bytes <= UTF-8 bytes for these CPs */
    dst = (char *)malloc((size_t)dstmax);

    if (!dst)
        return NULL;

    n = charset_body_from_utf8(cs, utf8, slen, dst, dstmax);
    (void)n;

    return dst;
}

uint32_t wrapper_write_msg(MsgBase *a, const char *from, const char *to, const char *subject, const char *utf8_body, const char *saved_kludges, const char *out_charset, uint32_t attr, uint32_t reply_to, uint32_t date_written, const char *oaddr, const char *daddr)
{
    char *body = NULL;
    char *from_cs = NULL, *to_cs = NULL, *subj_cs = NULL;
    int bodylen;
    uint32_t result;
    const char *cs = NULL;

    if (!a || !a->is_open)
        return 0;

    body = wrapper_prepare_body(utf8_body, saved_kludges, out_charset, &bodylen);

    if (!body)
        return 0;

    /* Convert headers to body charset to avoid mojibake */
    cs = (out_charset && out_charset[0]) ? out_charset : CHARSET_WRITE_DEFAULT;
    from_cs = header_to_charset(from, cs);
    to_cs = header_to_charset(to, cs);
    subj_cs = header_to_charset(subject, cs);

    result = mb_write_msg(a, from_cs ? from_cs : from, to_cs ? to_cs : to, subj_cs ? subj_cs : subject, body, (uint32_t)bodylen, attr, reply_to, date_written, oaddr, daddr);

    free(body);
    free(from_cs);
    free(to_cs);
    free(subj_cs);

    return result;
}
