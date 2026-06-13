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

/* msghdr.h -- FTN message header display and field editing */
#ifndef WRAPPER_MSGHDR_H
#define WRAPPER_MSGHDR_H

#include <stdint.h>
#include <wchar.h>
#include "jam_wrap.h"

typedef struct MsgHdr MsgHdr;

/* Field identifiers */
#define HDR_FROM 0
#define HDR_TO 1
#define HDR_SUBJECT 2
#define HDR_AREA 3  /* read-only */
#define HDR_DATE 4  /* read-only */
#define HDR_OADDR 5 /* origin address */
#define HDR_DADDR 6 /* destination address */
#define HDR_COUNT 7

/* Special keys for msghdr_edit_key */
#define HDR_KEY_BS 0x08
#define HDR_KEY_DEL 0x7F
#define HDR_KEY_LEFT 0x100
#define HDR_KEY_RIGHT 0x101
#define HDR_KEY_HOME 0x102
#define HDR_KEY_END 0x103

MsgHdr *msghdr_new();
void msghdr_free(MsgHdr *h);

/* Field position setup */
void msghdr_setup_positions(MsgHdr *h, int is_netmail, int cols);

/* Adjust field widths based on current COLS (called on resize) */
void msghdr_resize(MsgHdr *h, int cols);

/* Get field position (row, col, width) */
int msghdr_field_row(const MsgHdr *h, int field);
int msghdr_field_col(const MsgHdr *h, int field);
int msghdr_field_width(const MsgHdr *h, int field);

/* Fill from JAM header info */
void msghdr_load(MsgHdr *h, const JamMsgInfo *info, const char *area_tag, int msg_num, int msg_total, int reader_offset);

/* Fill for new message/reply */
void msghdr_new_msg(MsgHdr *h, const char *area_tag, const char *from_utf8, const char *oaddr, const char *to_utf8, const char *daddr, int tz_offset);

/* Set/get individual fields */
void msghdr_set(MsgHdr *h, int field, const wchar_t *value);
void msghdr_set_utf8(MsgHdr *h, int field, const char *utf8);
const wchar_t *msghdr_get(const MsgHdr *h, int field);
int msghdr_get_len(const MsgHdr *h, int field);

/* Export field as UTF-8 (caller frees) */
char *msghdr_get_utf8(const MsgHdr *h, int field);

/* Export field as UTF-8 into rotating static buffer (8 slots), don't free, returns "" on NULL */
const char *msghdr_get_utf8_tmp(const MsgHdr *h, int field);

/* Formatted info (read-only) */
int msghdr_msgnum(const MsgHdr *h);
int msghdr_msgtotal(const MsgHdr *h);
uint32_t msghdr_attr(const MsgHdr *h);
uint32_t msghdr_date_written(const MsgHdr *h);
uint32_t msghdr_jam_msgnum(const MsgHdr *h);
uint32_t msghdr_reply_to(const MsgHdr *h);

/* Header field editing */
void msghdr_edit_start(MsgHdr *h, int field);
int msghdr_edit_field(const MsgHdr *h);
int msghdr_edit_col(const MsgHdr *h);
int msghdr_edit_key(MsgHdr *h, int key);

/* TAB: move to next editable field (FROM->TO->SUBJECT->DADDR->FROM) */
void msghdr_edit_tab(MsgHdr *h);

/* Shift+TAB: move to previous editable field */
void msghdr_edit_stab(MsgHdr *h);

/* Stop editing (deselect field) */
void msghdr_edit_stop(MsgHdr *h);

#endif