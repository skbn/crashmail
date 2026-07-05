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

#ifndef UI_TTS_H
#define UI_TTS_H

#include "ui_internal.h"

#ifdef HAVE_TTS

/* Load/reload TTS from config, idempotent, called at startup and after Setup */
void ui_tts_load_from_config(UiApp *app);

/* Free handle, safe if NULL */
void ui_tts_unload(UiApp *app);

/* Speak selection or paragraph from editor or reader */
void ui_tts_speak_action(UiApp *app);

/* Speak from cursor to end or whole message from reader */
void ui_tts_speak_all_action(UiApp *app);

/* Dictate whole message from top, uses overflow buffer for large messages */
void ui_tts_speak_doc_action(UiApp *app);

/* Toggle pause/resume */
void ui_tts_pause_toggle(UiApp *app);

/* Stop and clear queue */
void ui_tts_stop(UiApp *app);

/* Modal popup for voice/rate/pitch/volume, applies live and optionally persists */
void ui_tts_popup(UiApp *app);

/* Pump TTS state machine, returns 1 if status bar needs redraw */
int ui_tts_is_active(void *vapp);

/* Pump TTS state machine, return 1 if status bar needs refresh */
int ui_tts_tick(UiApp *app);

/* 1 while speech is actively playing: main loops switch to short-poll input so tts_poll() can feed chunks without waiting for a keypress */
int ui_tts_is_busy(UiApp *app);

#endif /* HAVE_TTS */

#endif /* UI_TTS_H */
