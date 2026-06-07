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

/* main_ui.c -- Entry point for new UI */

#ifdef PLATFORM_AMIGA
const char __attribute__((used)) binkd_stack_size[] = "$STACK:65536";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wrapper.h"
#include "ui/ui.h"

int main(int argc, char *argv[])
{
    CrashEditCfg cfg;
    AreaList areas;
    UiApp *app;
    const char *cfgfile;

    if (argc < 2)
    {
        /* No config path given: default to "crashedit.conf" in the
         * current working directory -- i.e. wherever crashedit was
         * launched from. cfg_load creates it (with defaults) if it
         * isn't there, and we drop the user straight into setup
         * below, so a bare first run "just works". */
        cfgfile = "crashedit.conf";
    }
    else
    {
        cfgfile = argv[1];
    }

    /* Restart-in-place loop: the setup screen saves the config and asks
     * for a reload, which tears everything down and rebuilds from the
     * (now-updated) file — same end state as quitting and relaunching,
     * but without leaving the program. ui_run() returns 1 to request
     * this, 0 to quit for real */
    for (;;)
    {
        int reload;
        int need_setup = 0;

        /* Load main config (SYSOP, AKAs, AREAFILE). If the file is
         * missing, cfg_load creates a default one in place; a failure
         * here means it couldn't even be created -- almost always
         * because the parent directory doesn't exist. */
        if (cfg_load(&cfg, cfgfile) != 0)
        {
            fprintf(stderr, "Error: cannot load or create config file: %s\n       (does the directory exist?)\n", cfgfile);
            return 1;
        }

        /* Load areas.golded. If it's missing or empty we do NOT bail
         * out: areafile_load leaves the list in a valid empty state,
         * and we force the setup screen so the user can point AREAFILE
         * at a real file (or fix the path) and save. After saving, the
         * reload re-runs this loop and re-checks. The user stays in
         * setup until areas load or they quit. */
        if (areafile_load(&areas, cfg.areafile) <= 0)
            need_setup = 1;
        else
            areafile_calculate_counts(&areas, cfg.sysop);

        /* Initialize UI (re-parses cfg file for VIEWHIDDEN, etc.) */
        app = ui_init(&cfg, &areas);

        if (!app)
        {
            fprintf(stderr, "Error: Failed to initialize UI\n");
            areafile_free(&areas);
            return 1;
        }

        /* Let the setup screen know where to save. */
        ui_set_cfg_path(app, cfgfile);

        /* No usable areas yet -> straight into setup. */
        if (need_setup)
            ui_force_setup(app);

        /* Run main loop; non-zero return = reload requested */
        reload = ui_run(app);

        /* Cleanup */
        ui_cleanup(app);
        areafile_free(&areas);

        if (!reload)
            break;
    }

    return 0;
}
