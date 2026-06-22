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

#ifndef UI_MOUSE_H
#define UI_MOUSE_H

#include "ui_internal.h"

/* Mouse event types the platform layer sends us */
typedef enum
{
    UI_MOUSE_PRESS_LEFT,   /* left button pressed */
    UI_MOUSE_RELEASE_LEFT, /* left button released */
    UI_MOUSE_DRAG_LEFT,    /* move while left button held */
    UI_MOUSE_WHEEL_UP,     /* scroll wheel up */
    UI_MOUSE_WHEEL_DOWN,   /* scroll wheel down */
    UI_MOUSE_MOVE          /* hover, no buttons; usually ignored */
} UiMouseEventType;

/* Dispatch mouse event to editor. Returns 1 if consumed, 0 if ignored. y/x must be in character cells relative to editor body */
int ui_mouse_dispatch(UiApp *app, UiMouseEventType type, int y, int x, int body_rows);

/* Convert screen position to logical buffer position (line, col) */
int ui_mouse_screen_to_logical(UiApp *app, int screen_y, int screen_x, int *out_line, int *out_col);

/* Reset internal mouse state (click time, drag flag, etc) Call on focus change, popup close, file switch */
void ui_mouse_reset(void);

/* Set event timestamp in ms. Call before ui_mouse_dispatch() if platform has reliable timestamp */
void ui_mouse_set_event_time_ms(unsigned long ms);

#endif
