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

/* ui.h -- FTN reader/editor UI */
#ifndef UI_UI_H
#define UI_UI_H

#include "../wrapper.h"

typedef struct UiApp UiApp;

/* Initialize ncurses, colors, all views, load extended config (cfg/areas must outlive app, NULL on failure) */
UiApp *ui_init(CrashEditCfg *cfg, AreaList *areas);

/* Run main event loop (returns when user quits) */
int ui_run(UiApp *app); /* returns 1 if config reload requested, else 0 */

/* Tell UI where config file lives (for setup screen save), UiApp is opaque so this is a setter */
void ui_set_cfg_path(UiApp *app, const char *path);

/* Force UI to start in setup screen (first run/no usable config/areas), user must save or quit */
void ui_force_setup(UiApp *app);

/* Tear down ncurses and free all resources */
void ui_cleanup(UiApp *app);

#endif
