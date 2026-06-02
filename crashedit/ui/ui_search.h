/*
 * crashedit - Message area editor for AmigaOS
 *
 * This file is part of the crashedit project.
 *
 * Copyright (C) 2026 Tanausú M.
 *
 * Released under the GNU GPL v2 or later.
 */

/* ui_search.h -- selective-search popup and result browser */

#ifndef CRASHEDIT_UI_SEARCH_H
#define CRASHEDIT_UI_SEARCH_H

#include "ui_internal.h"

/* scope_all_areas: 1 = scan every area in app->areas, 0 = scan only
 * the area currently selected in arealist (or open in msglist) */
UiView ui_search_run(UiApp *app, int scope_all_areas);

#endif
