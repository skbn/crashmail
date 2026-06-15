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

#ifdef PLATFORM_AMIGA
const char __attribute__((used)) binkd_stack_size[] = "$STACK:65536";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "wrapper.h"
#include "ui/ui.h"

#if !defined(PLATFORM_WIN32) && !defined(PLATFORM_AMIGA)
#include <unistd.h>
#endif

#ifdef PLATFORM_WIN32
#include <windows.h> /* para GetCurrentProcessId */
#endif

#ifdef PLATFORM_AMIGA
#include <proto/exec.h>
extern struct Library *IFFParseBase;
#endif

int main(int argc, char *argv[])
{
    CrashEditCfg cfg;
    AreaList areas;
    UiApp *app;
    const char *cfgfile;
    unsigned int seed;

#ifdef PLATFORM_AMIGA
    IFFParseBase = OpenLibrary("iffparse.library", 37L);
#endif

    /* Initialize random seed for tagline selection */
    seed = (unsigned int)time(NULL);
#if !defined(PLATFORM_WIN32) && !defined(PLATFORM_AMIGA)
    seed ^= (unsigned int)getpid();
#elif defined(PLATFORM_WIN32)
    seed ^= (unsigned int)GetCurrentProcessId();
#endif
    srand(seed);

    if (argc < 2)
    {
        /* No config path given: use default "crashedit.conf" */
        cfgfile = "crashedit.conf";
    }
    else
    {
        cfgfile = argv[1];
    }

    /* Restart-in-place loop: reload config without exiting (ui_run returns 1 to reload, 0 to quit) */
    for (;;)
    {
        int reload;
        int need_setup = 0;

        /* Load main config (SYSOP, AKAs, AREAFILE); creates default if missing */
        if (cfg_load(&cfg, cfgfile) != 0)
        {
            fprintf(stderr, "Error: cannot load or create config file: %s\n       (does the directory exist?)\n", cfgfile);
            return 1;
        }

        /* Load areas.golded; if missing/empty, force setup screen */
        if (areafile_load(&areas, cfg.areafile) <= 0)
            need_setup = 1;
        else
            areafile_calculate_counts(&areas, cfg.sysop);

        /* Initialize UI (re-parses cfg for VIEWHIDDEN, etc.) */
        app = ui_init(&cfg, &areas);

        if (!app)
        {
            fprintf(stderr, "Error: Failed to initialize UI\n");
            areafile_free(&areas);
            return 1;
        }

        /* Set config path for setup screen */
        ui_set_cfg_path(app, cfgfile);

        /* No usable areas: go to setup */
        if (need_setup)
            ui_force_setup(app);

        /* Run main loop (returns 1 to reload, 0 to quit) */
        reload = ui_run(app);

        /* Cleanup */
        ui_cleanup(app);
        areafile_free(&areas);

        if (!reload)
            break;
    }

#ifdef PLATFORM_AMIGA
    if (IFFParseBase)
        CloseLibrary(IFFParseBase);
#endif

    return 0;
}
