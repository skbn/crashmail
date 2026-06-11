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

/* wrapper.h -- FTN message library for CrashMail/CrashEdit */
#ifndef WRAPPER_H
#define WRAPPER_H

#include <stdio.h> /* snprintf used by wrapper_build_pid */

#include "core/jam_wrap.h"
#include "core/utf8.h"
#include "core/charset.h"
#include "core/attach.h"
#include "core/ftn.h"
#include "core/keys.h"
#include "components/config.h"
#include "components/areafile.h"
#include "components/editor.h"
#include "core/wrapper_io.h"
#include "components/reader.h"
#include "core/msghdr.h"

/* FTN Message Identification */

/* OS detection for PID kludge (compile-time only, no runtime cost) */
#if defined(PLATFORM_AMIGA) || defined(__AMIGA__) || defined(AMIGA)
#define WRAPPER_OS_NAME "AmigaOS"
#elif defined(__APPLE__) && defined(__MACH__)
#define WRAPPER_OS_NAME "MacOS"
#elif defined(__linux__)
#define WRAPPER_OS_NAME "Linux"
#elif defined(__FreeBSD__)
#define WRAPPER_OS_NAME "FreeBSD"
#elif defined(__OpenBSD__)
#define WRAPPER_OS_NAME "OpenBSD"
#elif defined(__NetBSD__)
#define WRAPPER_OS_NAME "NetBSD"
#elif defined(__DragonFly__)
#define WRAPPER_OS_NAME "DragonFlyBSD"
#elif defined(__CYGWIN__)
#define WRAPPER_OS_NAME "Cygwin"
#elif defined(_WIN32) || defined(_WIN64)
#define WRAPPER_OS_NAME "Windows"
#elif defined(__unix__) || defined(__unix)
#define WRAPPER_OS_NAME "Unix"
#else
#define WRAPPER_OS_NAME "unknown"
#endif

#define WRAPPER_PID "CrashEdit 0.5.6.1 " WRAPPER_OS_NAME

static inline void wrapper_build_pid(char *buf, size_t size)
{
    buf[0] = 1;
    snprintf(buf + 1, size - 1, "PID: %s\r", WRAPPER_PID);
}

#endif
