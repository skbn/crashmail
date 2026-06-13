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

/* ui_editor_prep.c -- Editor data side: prepare message buffer */
#include "../core/ftn.h"
#include "../../src/jamlib/jam.h"
#include "ui_aka.h"
#include "ui_editor_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Small helpers */
const char *editor_daddr_for_intl(UiApp *app, const char *daddr)
{
    AreaEntry *ae;

    if (daddr && daddr[0])
        return daddr;

    if (app->cfg->forceintl == 2)
        return NULL;

    ae = &app->areas->entries[app->sess.area_idx];

    return (ae->aka && ae->aka[0]) ? ae->aka : app->cfg->aka[0];
}

void editor_reset_state(UiApp *app)
{
    if (app->saved_kludges)
    {
        free(app->saved_kludges);
        app->saved_kludges = NULL;
    }

    if (app->edit_search.rows)
    {
        free(app->edit_search.rows);
        app->edit_search.rows = NULL;
    }

    if (app->edit_search.cols)
    {
        free(app->edit_search.cols);
        app->edit_search.cols = NULL;
    }

    app->edit_search.match_count = 0;
    app->edit_search.is_mode = 0;
    app->edit_search.only_mode = 0;
    app->edit_search.current_match = 0;
    app->edit_search.match_current = 0;

    app->edit_is_new = 0;
    app->edit_is_reply = 0;
    app->edit_reply_to_msgnum = 0;
    app->edit_attr = MSG_LOCAL;
    app->edit_active_field = EF_TO;

    ed_load(app->editor, "");
    attach_clear(app->attach_list);

    /* Clear undo/redo stacks to free memory when exiting editor mode */
    ed_clear_undo_redo(app->editor);
}

/* Extract first name (first whitespace-delimited word) for greeting/signature */
static void first_name(const char *name, char *out, int outsz)
{
    int i = 0;

    if (!out || outsz < 1)
        return;

    out[0] = '\0';

    if (!name)
        return;

    while (name[i] && name[i] != ' ' && name[i] != '\t' && i < outsz - 1)
    {
        out[i] = name[i];
        i++;
    }
    out[i] = '\0';
}

/* Expand template tokens: %t=to, %f=from, %n=orig_from, %o=orig_to, %d=orig_date, %%=% */
static void expand_tokens(const char *tmpl, const char *to_first, const char *from_first, const char *orig_from, const char *orig_to, const char *orig_date, char *out, int outsz)
{
    int o = 0;
    const char *p = tmpl;

    if (!out || outsz < 1)
        return;

    out[0] = '\0';

    if (!tmpl)
        return;

    while (*p && o < outsz - 1)
    {
        if (*p == '%' && p[1])
        {
            const char *ins = NULL;
            char c = p[1];

            if (c == 't')
                ins = to_first;
            else if (c == 'f')
                ins = from_first;
            else if (c == 'n')
                ins = orig_from;
            else if (c == 'o')
                ins = orig_to;
            else if (c == 'd')
                ins = orig_date;
            else if (c == '%')
            {
                out[o++] = '%';
                p += 2;

                continue;
            }

            if (ins)
            {
                while (*ins && o < outsz - 1)
                    out[o++] = *ins++;

                p += 2;

                continue;
            }
        }

        out[o++] = *p++;
    }

    out[o] = '\0';
}

/* Build greeting + attribution header (malloc'd, caller frees) */
static char *build_header_frame(const CrashEditCfg *cfg, const char *to_name, const char *from_name, const char *orig_from, const char *orig_to, const char *orig_date, int is_reply, int orig_was_to_me)
{
    char tof[96], frf[96], onf[96];
    char line[CFG_STR_MAX * 2];
    size_t cap = 1, len = 0;
    char *out;
    size_t ll;

    first_name(to_name, tof, sizeof(tof));
    first_name(from_name, frf, sizeof(frf));
    first_name(orig_from, onf, sizeof(onf));

    out = (char *)malloc(64);

    if (!out)
        return NULL;

    out[0] = '\0';
    cap = 64;

    if (cfg->greeting && cfg->greeting_text[0])
    {
        expand_tokens(cfg->greeting_text, tof, frf, onf, orig_to, orig_date, line, sizeof(line));

        ll = strlen(line);

        if (len + ll + 3 > cap)
        {
            char *np = (char *)realloc(out, len + ll + 3);

            if (!np)
            {
                return out;
            }

            out = np;
            cap = len + ll + 3;
        }

        memcpy(out + len, line, ll);

        len += ll;
        out[len++] = '\n';
        out[len++] = '\n'; /* blank line after greeting */
        out[len] = '\0';
    }

    if (is_reply && cfg->attribution)
    {
        const char *tmpl = orig_was_to_me ? cfg->attrib_self : cfg->attrib_other;

        if (tmpl && tmpl[0])
        {
            expand_tokens(tmpl, tof, frf, onf, orig_to, orig_date, line, sizeof(line));
            ll = strlen(line);

            if (len + ll + 3 > cap)
            {
                char *np = (char *)realloc(out, len + ll + 3);

                if (!np)
                    return out;

                out = np;
                cap = len + ll + 3;
            }

            memcpy(out + len, line, ll);

            len += ll;

            out[len++] = '\n';
            out[len++] = '\n'; /* blank line after attribution */
            out[len] = '\0';
        }
    }

    return out;
}

/* Build the signature line that goes BELOW the body (before the tearline), returns malloc'd string ("" if disabled) */
static char *build_signature(const CrashEditCfg *cfg, const char *from_name)
{
    char frf[96];
    char line[CFG_STR_MAX * 2];
    char *out;
    size_t ll;

    if (!cfg->signature || !cfg->signature_text[0])
    {
        out = (char *)malloc(1);

        if (out)
            out[0] = '\0';

        return out;
    }

    first_name(from_name, frf, sizeof(frf));
    expand_tokens(cfg->signature_text, "", frf, "", "", "", line, sizeof(line));

    ll = strlen(line);
    out = (char *)malloc(ll + 4);

    if (!out)
        return NULL;

    /* blank line, signature, newline */
    out[0] = '\n';

    memcpy(out + 1, line, ll);

    out[1 + ll] = '\n';
    out[2 + ll] = '\0';

    return out;
}

/* Concatenate standard FTN kludges (ftn_build_* and wrapper_build_pid produce ^A<NAME>:...\r lines) */
char *editor_build_kludge_block(const CrashEditCfg *cfg, const char *oaddr, const char *daddr, const char *raw_daddr, const char *reply_msgid_value, int is_netmail)
{
    char msgid[160];
    char pid[80];
    char reply[200];
    char intl[100];
    char fmpt[32];
    char topt[32];
    char tzutc[24];
    size_t l1, l2, l4, l5, l6, l7, l8;
    char *out, *p;

    /* TZUTC stored only as JAM subfield (via jam_write_msg), not here */
    ftn_build_msgid(oaddr, msgid, sizeof(msgid));
    wrapper_build_pid(pid, sizeof(pid));

    pid[sizeof(pid) - 1] = '\0';

    ftn_build_tzutc(ftn_effective_tz_offset(cfg->timezone_offset, cfg->timezone_is_manual), tzutc, sizeof(tzutc));
    ftn_build_reply(reply_msgid_value, reply, sizeof(reply));

    /* INTL kludge: only when zones differ (or user forces it) */
    intl[0] = '\0';

    if (cfg->forceintl != 2 && daddr && daddr[0] && oaddr && oaddr[0])
    {
        int orig_zone = ftn_addr_zone(oaddr);
        int dest_zone = ftn_addr_zone(daddr);

        if (cfg->forceintl == 1 || (cfg->forceintl == 0 && orig_zone > 0 && dest_zone > 0 && orig_zone != dest_zone))
        {
            int orig_net = ftn_addr_net(oaddr);
            int orig_node = ftn_addr_node(oaddr);
            int dest_net = ftn_addr_net(daddr);
            int dest_node = ftn_addr_node(daddr);

            snprintf(intl, sizeof(intl), "\001INTL %d:%d/%d %d:%d/%d\r", dest_zone > 0 ? dest_zone : 1, dest_net, dest_node, orig_zone > 0 ? orig_zone : 1, orig_net, orig_node);
        }
    }

    /* FMPT/TOPT: only for netmails when orig/dest has a point number */
    fmpt[0] = '\0';
    topt[0] = '\0';

    if (is_netmail)
    {
        if (oaddr && oaddr[0])
        {
            int op = ftn_addr_point(oaddr);

            if (op > 0)
                snprintf(fmpt, sizeof(fmpt), "\001FMPT %d\r", op);
        }

        if (raw_daddr && raw_daddr[0])
        {
            int dp = ftn_addr_point(raw_daddr);

            if (dp > 0)
                snprintf(topt, sizeof(topt), "\001TOPT %d\r", dp);
        }
    }

    l1 = strlen(msgid);
    l2 = strlen(pid);
    l4 = strlen(reply);
    l5 = strlen(intl);
    l6 = strlen(tzutc);
    l7 = strlen(fmpt);
    l8 = strlen(topt);

    out = (char *)malloc(l1 + l2 + l4 + l5 + l6 + l7 + l8 + 1);

    if (!out)
        return NULL;

    p = out;
    memcpy(p, msgid, l1);
    p += l1;
    memcpy(p, pid, l2);
    p += l2;
    memcpy(p, tzutc, l6);
    p += l6;
    memcpy(p, reply, l4);
    p += l4;
    memcpy(p, intl, l5);
    p += l5;
    memcpy(p, fmpt, l7);
    p += l7;
    memcpy(p, topt, l8);
    p += l8;

    *p = '\0';

    return out;
}

/* Load <path> as UTF-8 text up to <max> bytes, call ed_load() on success (TEMPLATE feature) */
static void load_template_if_any(Ed *editor, const char *path)
{
    FILE *f;
    long sz;
    char *tmpl;
    size_t r;

    if (!path || !path[0])
        return;

    f = fopen(path, "rb");

    if (!f)
        return;

    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz > 0 && sz < 64 * 1024)
    {
        tmpl = (char *)malloc((size_t)sz + 1);

        if (tmpl)
        {
            r = fread(tmpl, 1, (size_t)sz, f);

            tmpl[r] = '\0';

            ed_load(editor, tmpl);

            free(tmpl);
        }
    }

    fclose(f);
}

/* Extract MSGID kludge value from body into out[] (strips \r/\n, 0=ok, -1=missing) */
static int extract_msgid(const char *body_utf8, char *out, int outsz)
{
    const char *mid;
    int i;

    if (!body_utf8 || !out || outsz < 2)
        return -1;

    out[0] = '\0';
    mid = ftn_find_msgid(body_utf8);

    if (!mid)
        return -1;

    for (i = 0; mid[i] && mid[i] != '\r' && mid[i] != '\n' && i < outsz - 1; i++)
        out[i] = mid[i];

    out[i] = '\0';

    return out[0] ? 0 : -1;
}

/* Prep for NEW message */
void ui_editor_prep_new(UiApp *app)
{
    AreaEntry *ae;
    const char *oaddr;
    const char *area_tag;
    const char *daddr_new;
    char *hdr;
    char *sig;
    size_t hl;
    size_t sl;

    if (!app)
        return;

    editor_reset_state(app);

    ae = &app->areas->entries[app->sess.area_idx];
    area_tag = ae->name ? ae->name : "";

    /* Pick origin AKA: echo/local locks to area's AKA, netmail uses selected AKA */
    if (ae->type == AREATYPE_NETMAIL)
    {
        const char *picked = ui_aka_at(app->areas, app->cfg, app->edit_aka_idx);

        if (!picked)
        {
            picked = ui_aka_at(app->areas, app->cfg, 0);
            app->edit_aka_idx = 0;
        }

        oaddr = picked ? picked : app->cfg->aka[0];
    }
    else
    {
        oaddr = (ae->aka && ae->aka[0]) ? ae->aka : app->cfg->aka[0];
    }

    /* GoldED+ defaults: netmail = Loc+Pvt, anything else = Loc */
    app->edit_attr = (ae->type == AREATYPE_NETMAIL) ? (MSG_LOCAL | MSG_PRIVATE) : MSG_LOCAL;

    msghdr_new_msg(app->edit_hdr, area_tag, app->cfg->sysop, oaddr, ae->type == AREATYPE_NETMAIL ? "" : "All", NULL, ftn_effective_tz_offset(app->cfg->timezone_offset, app->cfg->timezone_is_manual));
    msghdr_setup_positions(app->edit_hdr, ae->type == AREATYPE_NETMAIL, COLS);

    /* Initialize charset tracking for new messages */
    strncpy(app->edit_charset_saved, app->edit_charset, sizeof(app->edit_charset_saved) - 1);

    app->edit_charset_saved[sizeof(app->edit_charset_saved) - 1] = '\0';
    app->edit_charset_manually_changed = 0;

    /* Preserve user-forced edit_charset override, else cfg default */
    if (!app->edit_charset[0])
    {
        strncpy(app->edit_charset, app->cfg->charset, sizeof(app->edit_charset) - 1);
        app->edit_charset[sizeof(app->edit_charset) - 1] = '\0';
    }

    daddr_new = editor_daddr_for_intl(app, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR));

    app->saved_kludges = editor_build_kludge_block(app->cfg, oaddr, daddr_new, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR), NULL, ae->type == AREATYPE_NETMAIL);
    app->edit_is_new = 1;
    app->edit_active_field = EF_TO;

    /* GoldED+ framing for new message: greeting + signature (empty if disabled) */
    hdr = build_header_frame(app->cfg, msghdr_get_utf8_tmp(app->edit_hdr, HDR_TO), msghdr_get_utf8_tmp(app->edit_hdr, HDR_FROM), NULL, NULL, NULL, 0, 0);
    sig = build_signature(app->cfg, msghdr_get_utf8_tmp(app->edit_hdr, HDR_FROM));
    hl = hdr ? strlen(hdr) : 0;
    sl = sig ? strlen(sig) : 0;

    if (hl || sl)
    {
        char *buf = (char *)malloc(hl + sl + 2);

        if (buf)
        {
            size_t n = 0;

            if (hl)
            {
                memcpy(buf + n, hdr, hl);
                n += hl;
            }

            /* leave a line for the user to type between greeting and signature */
            buf[n++] = '\n';

            if (sl)
            {
                memcpy(buf + n, sig, sl);
                n += sl;
            }

            buf[n] = '\0';
            ed_load(app->editor, buf);

            free(buf);
        }
        else
        {
            ed_load(app->editor, "");
        }
    }
    else
    {
        ed_load(app->editor, "");
    }

    free(hdr);
    free(sig);

    load_template_if_any(app->editor, app->cfg->template_file);
}

/* Build quote source: keep only visible line categories, returns malloc'd UTF-8 (caller frees) */
static char *build_quote_source(const char *body_utf8, int want_klg, int want_via, int want_hid)
{
    const char *p = body_utf8;
    size_t cap, len;
    char *out;

    if (!body_utf8)
        return NULL;

    cap = strlen(body_utf8) + 1;
    out = (char *)malloc(cap);

    if (!out)
        return NULL;

    len = 0;

    while (*p)
    {
        const char *start = p;
        int llen, type, keep;
        size_t need;

        while (*p && *p != '\r' && *p != '\n')
            p++;

        llen = (int)(p - start);

        /* skip the terminator(s) */
        while (*p == '\r' || *p == '\n')
            p++;

        type = ftn_classify_line(start, llen);

        if (type == FTN_LT_KLUDGE)
            keep = want_klg;
        else if (type == FTN_LT_SEENBY || type == FTN_LT_VIA || type == FTN_LT_PATH)
            keep = want_via;
        else
            keep = 1;

        if (!keep)
            continue;

        need = len + (size_t)llen + 2;

        if (need > cap)
        {
            char *np = (char *)realloc(out, need * 2);

            if (!np)
                break;

            out = np;
            cap = need * 2;
        }

        memcpy(out + len, start, (size_t)llen);

        len += (size_t)llen;
        out[len++] = '\r'; /* FTN line terminator */
        out[len] = '\0';
    }

    return out;
}

/* Prep for REPLY */
void ui_editor_prep_reply(UiApp *app, uint32_t orig_msgnum)
{
    UiSession *s;
    AreaEntry *ae;
    int idx;
    char *body_utf8;
    char *quoted;
    const char *oaddr;
    char re_subj[128];
    char reply_to_msgid[200];
    const JamMsgInfo *m;
    const char *daddr_reply;
    char detected_orig[32];
    int has_chrs = 0;
    detected_orig[0] = '\0';

    if (!app)
        return;

    editor_reset_state(app);

    s = &app->sess;
    ae = &app->areas->entries[s->area_idx];

    idx = jam_find_by_msgnum(s->msgs, s->msg_count, orig_msgnum);

    if (idx < 0)
    {
        ui_editor_prep_new(app);
        return;
    }

    m = &s->msgs[idx];
    reply_to_msgid[0] = '\0';

    /* Origin AKA: netmail auto-matches the destination zone */
    if (ae->type == AREATYPE_NETMAIL)
    {
        int zmatch = ftn_aka_match_zone(&app->cfg->aka[0][0], app->cfg->aka_count, sizeof(app->cfg->aka[0]), m->oaddress);

        if (zmatch >= 0)
            app->edit_aka_idx = zmatch;
        else if (app->edit_aka_idx < 0 || app->edit_aka_idx >= app->cfg->aka_count)
            app->edit_aka_idx = app->cfg->aka_selected;

        oaddr = app->cfg->aka[app->edit_aka_idx];
    }
    else
    {
        oaddr = (ae->aka && ae->aka[0]) ? ae->aka : app->cfg->aka[0];
    }

    /* Subject: prepend "Re: " unless already present */
    if (m->subject[0])
    {
        if (strncasecmp(m->subject, "Re:", 3) == 0)
            snprintf(re_subj, sizeof(re_subj), "%s", m->subject);
        else
            snprintf(re_subj, sizeof(re_subj), "Re: %s", m->subject);
    }
    else
    {
        snprintf(re_subj, sizeof(re_subj), "Re:");
    }

    msghdr_new_msg(app->edit_hdr, ae->name ? ae->name : "", app->cfg->sysop, oaddr, m->from[0] ? m->from : "All", ae->type == AREATYPE_NETMAIL ? m->oaddress : NULL, ftn_effective_tz_offset(app->cfg->timezone_offset, app->cfg->timezone_is_manual));
    msghdr_set_utf8(app->edit_hdr, HDR_SUBJECT, re_subj);
    msghdr_setup_positions(app->edit_hdr, ae->type == AREATYPE_NETMAIL, COLS);

    /* Read body, detect charset, strip kludges, quote */
    body_utf8 = wrapper_read_utf8_ex(&s->jam, orig_msgnum, app->view_charset[0] ? app->view_charset : NULL, NULL, detected_orig, sizeof(detected_orig));

    if (body_utf8)
    {
        const char *p = body_utf8;

        while (*p)
        {
            if ((unsigned char)*p == 0x01)
            {
                if (strncasecmp(p + 1, "CHRS:", 5) == 0 || strncasecmp(p + 1, "CHRS ", 5) == 0 || strncasecmp(p + 1, "CHARSET:", 8) == 0)
                {
                    has_chrs = 1;
                    break;
                }
            }

            p++;
        }
    }

    /* Save original edit_charset before auto-detection */
    strncpy(app->edit_charset_saved, app->edit_charset, sizeof(app->edit_charset_saved) - 1);

    app->edit_charset_saved[sizeof(app->edit_charset_saved) - 1] = '\0';
    app->edit_charset_manually_changed = 0;

    if (!app->edit_charset[0])
    {
        const char *use;

        /* Use detected charset if available (from CHRS kludge or auto-detection) */
        if (detected_orig[0])
            use = detected_orig;
        else
            use = NULL; /* Keep Auto mode if detection failed */

        if (use)
        {
            strncpy(app->edit_charset, use, sizeof(app->edit_charset) - 1);
            app->edit_charset[sizeof(app->edit_charset) - 1] = '\0';
        }
    }

    if (body_utf8)
    {
        char *orig_kludges = NULL;
        char *clean_body = NULL;
        char *vis_body = NULL;
        const char *to_quote;
        int qmargin;
        int want_klg, want_via, want_hid;

        /* SOFT-WRAP (default): quote without word-wrapping (margin 0), HARD-WRAP: keep configured GoldED+ QUOTEMARGIN */
        qmargin = (app->cfg && app->cfg->hard_wrap) ? app->cfg->quotemargin : 0;

        ftn_extract_kludges(body_utf8, &orig_kludges, &clean_body);
        extract_msgid(body_utf8, reply_to_msgid, sizeof(reply_to_msgid));

        /* Quote visibility: match reader's live toggles if active, else config defaults */
        if (app->reader && app->cur_msgnum == orig_msgnum)
        {
            want_klg = rd_kludges_visible(app->reader);
            want_via = rd_vias_visible(app->reader);
            want_hid = rd_hidden_visible(app->reader);
        }
        else
        {
            want_klg = app->cfg ? app->cfg->viewkludge : 0;
            want_via = 0;
            want_hid = app->cfg ? app->cfg->viewhidden : 0;
        }

        if (!want_klg && !want_via && !want_hid)
        {
            to_quote = clean_body ? clean_body : body_utf8;
            quoted = ftn_quote_body_named_wrap(to_quote, m->from, qmargin);
        }
        else
        {
            vis_body = build_quote_source(body_utf8, want_klg, want_via, want_hid);

            if (vis_body)
            {
                quoted = ftn_quote_body_named_full_wrap(vis_body, m->from, qmargin);
            }
            else
            {
                to_quote = clean_body ? clean_body : body_utf8;
                quoted = ftn_quote_body_named_wrap(to_quote, m->from, qmargin);
            }
        }

        if (quoted)
        {
            /* GoldED+ framing for reply: [greeting] [attribution] <quote> <blank> [sig] */
            char odate[64];
            char *frame;
            char *sig;
            size_t fl, ql, sl, n;
            char *buf;
            int to_me = 0;
            const char *eh_from;
            const char *eh_to;
            int off;

            /* Use current time in LOCAL timezone for reply attribution */
            time_t t = time(NULL);

#ifndef PLATFORM_AMIGA
            /* On non-Amiga: time() returns UTC, add offset to get local time */

            off = ftn_effective_tz_offset(app->cfg->timezone_offset, app->cfg->timezone_is_manual);
            t += (time_t)off * 60;
#endif
            /* On Amiga: time() already returns local time, no adjustment needed */

            struct tm *tm = gmtime(&t);

            if (tm)
                snprintf(odate, sizeof(odate), "%02d %s %02d %02d:%02d", tm->tm_mday, "Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec" + (tm->tm_mon * 4), tm->tm_year % 100, tm->tm_hour, tm->tm_min);
            else
                odate[0] = '\0';

            eh_from = msghdr_get_utf8_tmp(app->edit_hdr, HDR_FROM);
            eh_to = msghdr_get_utf8_tmp(app->edit_hdr, HDR_TO);

            if (m->to[0] && app->cfg->sysop[0] && strcasecmp(m->to, app->cfg->sysop) == 0)
                to_me = 1;

            frame = build_header_frame(app->cfg, eh_to, /* %t */
                                       eh_from,         /* %f */
                                       eh_from,         /* %n = HDR_FROM of the editor header */
                                       /*eh_to,*/       /* %o = HDR_TO   of the editor header */
                                       m->from,
                                       odate, 1, to_me);

            sig = build_signature(app->cfg, msghdr_get_utf8_tmp(app->edit_hdr, HDR_FROM));
            fl = frame ? strlen(frame) : 0;
            ql = strlen(quoted);
            sl = sig ? strlen(sig) : 0;

            buf = (char *)malloc(fl + ql + sl + 3);

            if (buf)
            {
                n = 0;

                if (fl)
                {
                    memcpy(buf + n, frame, fl);
                    n += fl;
                }

                memcpy(buf + n, quoted, ql);

                n += ql;
                /*buf[n++] = '\n';*/
                buf[n++] = '\n'; /* blank line to type the reply */

                if (sl)
                {
                    memcpy(buf + n, sig, sl);
                    n += sl;
                }

                buf[n] = '\0';
                ed_load(app->editor, buf);

                free(buf);
            }
            else
            {
                ed_load(app->editor, quoted);
            }

            free(frame);
            free(sig);
            free(quoted);

            ed_move_bottom(app->editor);
            ed_move_end(app->editor);
        }
        else
        {
            ed_load(app->editor, "");
        }

        free(vis_body);
        free(clean_body);
        free(orig_kludges);
        free(body_utf8);
    }
    else
    {
        ed_load(app->editor, "");
    }

    daddr_reply = editor_daddr_for_intl(app, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR));

    app->saved_kludges = editor_build_kludge_block(app->cfg, oaddr, daddr_reply, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR), reply_to_msgid[0] ? reply_to_msgid : NULL, ae->type == AREATYPE_NETMAIL);
    app->edit_is_new = 1;
    app->edit_is_reply = 1;
    app->edit_reply_to_msgnum = orig_msgnum;
    app->edit_active_field = EF_BODY;
}

/* Prep for EDIT existing */
void ui_editor_prep_edit(UiApp *app, uint32_t msgnum)
{
    UiSession *s;
    AreaEntry *ae;
    int idx;
    char *body_utf8;
    const char *oaddr;
    const char *daddr_edit;
    char detected[CHARSET_NAME_MAX];
    int has_chrs = 0;

    if (!app)
        return;

    editor_reset_state(app);

    s = &app->sess;
    ae = &app->areas->entries[s->area_idx];

    idx = jam_find_by_msgnum(s->msgs, s->msg_count, msgnum);

    if (idx < 0)
    {
        ui_editor_prep_new(app);
        return;
    }

    msghdr_load(app->edit_hdr, &s->msgs[idx], ae->name ? ae->name : "", idx + 1, s->msg_count, ftn_effective_tz_offset(app->cfg->timezone_offset, app->cfg->timezone_is_manual));
    msghdr_setup_positions(app->edit_hdr, ae->type == AREATYPE_NETMAIL, COLS);

    /* Origin AKA: same rules as prep_new */
    if (ae->type == AREATYPE_NETMAIL)
    {
        const char *picked = ui_aka_at(app->areas, app->cfg, app->edit_aka_idx);

        if (!picked)
        {
            picked = ui_aka_at(app->areas, app->cfg, 0);
            app->edit_aka_idx = 0;
        }

        oaddr = picked ? picked : app->cfg->aka[0];
    }
    else
    {
        oaddr = (ae->aka && ae->aka[0]) ? ae->aka : app->cfg->aka[0];
    }

    /* Use detected charset if no user override */
    detected[0] = '\0';
    /*body_utf8 = wrapper_read_utf8(&s->jam, msgnum, detected);*/
    body_utf8 = wrapper_read_utf8_ex(&s->jam, msgnum, app->view_charset[0] ? app->view_charset : NULL, NULL, detected, sizeof(detected));

    /* Check if message has CHRS kludge */
    if (body_utf8)
    {
        const char *p = body_utf8;

        while (*p)
        {
            if ((unsigned char)*p == 0x01)
            {
                if (strncasecmp(p + 1, "CHRS:", 5) == 0 || strncasecmp(p + 1, "CHRS ", 5) == 0 || strncasecmp(p + 1, "CHARSET:", 8) == 0)
                {
                    has_chrs = 1;
                    break;
                }
            }

            p++;
        }
    }

    /* Save original edit_charset before auto-detection */
    strncpy(app->edit_charset_saved, app->edit_charset, sizeof(app->edit_charset_saved) - 1);
    app->edit_charset_saved[sizeof(app->edit_charset_saved) - 1] = '\0';
    app->edit_charset_manually_changed = 0;

    if (!app->edit_charset[0])
    {
        const char *use;

        /* Use detected charset if available (from CHRS kludge or auto-detection) */
        if (detected[0])
            use = detected;
        else
            use = NULL; /* Keep Auto mode if detection failed */

        if (use)
        {
            strncpy(app->edit_charset, use, sizeof(app->edit_charset) - 1);
            app->edit_charset[sizeof(app->edit_charset) - 1] = '\0';
        }
    }

    if (body_utf8)
    {
        const char *content;
        size_t cl;
        char *with_nl;
        char *kludges_out = NULL;
        char *clean_out = NULL;

        /* Strip kludges and bare control lines (SEEN-BY, PATH, tear, origin) before editing */
        ftn_extract_kludges(body_utf8, &kludges_out, &clean_out);

        content = clean_out ? clean_out : body_utf8;
        cl = strlen(content);
        with_nl = (char *)malloc(cl + 3);

        if (with_nl)
        {
            memcpy(with_nl, content, cl);

            with_nl[cl] = '\n';
            with_nl[cl + 1] = '\n';
            with_nl[cl + 2] = '\0';

            ed_load(app->editor, with_nl);

            free(with_nl);
        }
        else
        {
            ed_load(app->editor, content);
        }

        free(clean_out);
        free(kludges_out); /* old kludges discarded, regenerated on save */
        free(body_utf8);

        daddr_edit = editor_daddr_for_intl(app, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR));
        app->saved_kludges = editor_build_kludge_block(app->cfg, oaddr, daddr_edit, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR), NULL, ae->type == AREATYPE_NETMAIL);
        ed_move_bottom(app->editor);
        ed_move_end(app->editor);
    }
    else
    {
        ed_load(app->editor, "");
        daddr_edit = editor_daddr_for_intl(app, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR));

        app->saved_kludges = editor_build_kludge_block(app->cfg, oaddr, daddr_edit, msghdr_get_utf8_tmp(app->edit_hdr, HDR_DADDR), NULL, ae->type == AREATYPE_NETMAIL);
    }

    app->edit_is_new = 0;
    app->edit_reply_to_msgnum = s->msgs[idx].reply_to;

    /* Keep original attrs, clear SENT/KILLSENT, force LOCAL */
    app->edit_attr = s->msgs[idx].attr;
    app->edit_attr |= MSG_LOCAL;
    app->edit_attr &= ~((uint32_t)(MSG_SENT | MSG_KILLSENT));

    app->edit_active_field = EF_BODY;

    /* Used by ui_editor_save() to delete the old copy */
    app->cur_msgnum = msgnum;
}

/* Save helpers: free extracted headers */
static void free_save_fields(char *from, char *to, char *subj, char *oa, char *da, char *body)
{
    free(from);
    free(to);
    free(subj);
    free(oa);
    free(da);
    free(body);
}

int ui_editor_save(UiApp *app)
{
    UiSession *s = &app->sess;
    char *from = msghdr_get_utf8(app->edit_hdr, HDR_FROM);
    char *to = msghdr_get_utf8(app->edit_hdr, HDR_TO);
    char *subj = msghdr_get_utf8(app->edit_hdr, HDR_SUBJECT);
    char *oa = msghdr_get_utf8(app->edit_hdr, HDR_OADDR);
    char *da = msghdr_get_utf8(app->edit_hdr, HDR_DADDR);
    char *body = ed_to_string(app->editor);
    AreaEntry *ae = &app->areas->entries[s->area_idx];
    uint32_t mn = 0;
    char **attach_subjects = NULL;
    int attach_subj_count = 0;
    uint32_t attr;
    int is_echo;
    int has_origin;
    int lost;
    const char *daddr_for_intl;
    char reply_msgid[200];
    uint32_t dw;

    if (!from || !to || !subj || !body)
    {
        free_save_fields(from, to, subj, oa, da, body);
        return -1;
    }

    /* Append tear+origin to echomail (idempotent) */
    is_echo = (ae && ae->type != AREATYPE_NETMAIL);
    has_origin = (app->saved_kludges && strstr(app->saved_kludges, " * Origin:") != NULL);

    if (is_echo && !has_origin)
    {
        char *signed_body = ftn_apply_signature(app->cfg->origin, app->cfg->tearline, app->cfg->tagline_file, body, oa, 1);

        if (signed_body)
        {
            free(body);
            body = signed_body;
        }
    }

    /* Warn the user if the chosen charset can't encode some chars */
    lost = charset_count_lossy(body, app->edit_charset);

    if (lost > 0)
    {
        char msg[128];

        snprintf(msg, sizeof(msg), "%d character(s) cannot be represented in %s. Save anyway?", lost, app->edit_charset);

        if (ui_popup_confirm("Charset warning", msg) != 1)
        {
            free_save_fields(from, to, subj, oa, da, body);
            ui_status(app, "Save cancelled.");
            return -1;
        }
    }

    if (jam_lock(&s->jam, JAM_LOCK_RETRIES) != 0)
    {
        ui_status(app, "Cannot lock area to save.");
        free_save_fields(from, to, subj, oa, da, body);
        return -1;
    }

    /* Regenerate kludges now that all header fields are final */
    daddr_for_intl = editor_daddr_for_intl(app, da);

    reply_msgid[0] = '\0';

    if (app->edit_is_reply && app->edit_reply_to_msgnum > 0)
    {
        char detected_charset[CHARSET_NAME_MAX];
        char *body_orig = wrapper_read_utf8_ex(&s->jam, app->edit_reply_to_msgnum, app->view_charset[0] ? app->view_charset : NULL, NULL, detected_charset, sizeof(detected_charset));

        if (body_orig)
        {
            const char *mid = ftn_find_msgid(body_orig);

            if (mid)
            {
                int i = 0;

                while (mid[i] && mid[i] != '\r' && mid[i] != '\n' && i < (int)sizeof(reply_msgid) - 1)
                {
                    reply_msgid[i] = mid[i];
                    i++;
                }

                reply_msgid[i] = '\0';
            }

            free(body_orig);
        }
    }

    free(app->saved_kludges);

    app->saved_kludges = editor_build_kludge_block(app->cfg, oa, daddr_for_intl, da, reply_msgid[0] ? reply_msgid : NULL, ae->type == AREATYPE_NETMAIL);

    /* Write new, delete old on success; attachments may split into
     * multiple messages if subject exceeds ATTACH_SUBJ_LIMIT */
    attr = app->edit_attr;

    if (app->attach_list && app->attach_list->count > 0)
    {
        attach_subjects = attach_build_subjects(app->attach_list, &attach_subj_count);
        attr |= JAMATTR_FILEATTACH;
    }

    /* DateWritten: UTC epoch (Amiga adjusts from local time) */
#ifdef PLATFORM_AMIGA
    /* Amiga time() returns local time; convert to UTC for storage */
    dw = (uint32_t)((time_t)time(NULL) - (time_t)app->cfg->timezone_offset * 60);
#else
    dw = (uint32_t)time(NULL);
#endif

    if (attach_subjects && attach_subj_count > 0)
    {
        /* Split attachments: one msg per subject, reply linkage on first only */
        int k;
        uint32_t this_mn;

        for (k = 0; k < attach_subj_count; k++)
        {
            uint32_t reply_for_this = (k == 0) ? app->edit_reply_to_msgnum : 0;

            this_mn = wrapper_write_msg(&s->jam, from, to, attach_subjects[k], body, app->saved_kludges, app->edit_charset, attr, reply_for_this, dw, oa, da);

            if (this_mn == 0)
            {
                /* Partial failure; keep what was written, stop */
                break;
            }

            if (k == 0)
                mn = this_mn;
        }
    }
    else
    {
        /* Single message: no attachments or splitter OOM fallback */
        mn = wrapper_write_msg(&s->jam, from, to, subj, body, app->saved_kludges, app->edit_charset, attr, app->edit_reply_to_msgnum, dw, oa, da);
    }

    attach_free_subjects(attach_subjects, attach_subj_count);

    if (mn != 0 && !app->edit_is_new)
    {
        /* Edit mode: delete old version now that new is written */
        jam_delete_msg(&s->jam, app->cur_msgnum);
    }

    jam_unlock(&s->jam);
    free_save_fields(from, to, subj, oa, da, body);

    if (mn == 0)
    {
        ui_status(app, "Write failed");
        return -1;
    }

    /* Refresh the visible message list */
    free(s->msgs);

    s->msgs = NULL;
    s->msgs = jam_load_headers(&s->jam, &s->msg_count, 0, (uint32_t)app->cfg->msglistmax);

    ui_session_rebuild_order(app);

    if (attach_subj_count > 1)
        ui_status(app, "Saved as msg #%lu (split into %d messages)", (unsigned long)mn, attach_subj_count);
    else
        ui_status(app, "Saved as msg #%lu", (unsigned long)mn);

    return 0;
}
