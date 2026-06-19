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

/* wrapper_io.h -- High-level read/write pipeline */
#ifndef WRAPPER_IO_H
#define WRAPPER_IO_H

#include "msgbase.h"
#include "charset.h"

/* Read message body from JAM as UTF-8 (detects charset from CHRS kludge, returns malloc'd UTF-8) */
char *wrapper_read_utf8(MsgBase *a, uint32_t msgnum, char *enc_out);

/* Read body as UTF-8 with optional charset override/fallback/detection */
char *wrapper_read_utf8_ex(MsgBase *a, uint32_t msgnum, const char *override_enc, const char *fallback_enc, char *detected_out, int detected_sz);

/* Prepare body for writing to JAM (injects kludges, converts to charset, LF->CR, returns malloc'd string) */
char *wrapper_prepare_body(const char *utf8_body, const char *saved_kludges, const char *out_charset, int *out_len);

/* Full write: prepare + jam_write_msg in one call (area must be locked, returns new msgnum or 0) */
uint32_t wrapper_write_msg(MsgBase *a, const char *from, const char *to, const char *subject, const char *utf8_body, const char *saved_kludges, const char *out_charset, uint32_t attr, uint32_t reply_to, uint32_t date_written, const char *oaddr, const char *daddr);

#endif
