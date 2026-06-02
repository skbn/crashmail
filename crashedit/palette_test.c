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

/* palette_test.c - Program to display Workbench color palette on Amiga */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Library *GadToolsBase = NULL;

#define BOX_SIZE 20
#define BOX_MARGIN 15
#define COLORS_PER_ROW 8
#define TEXT_HEIGHT 18
#define INSTRUCTION_HEIGHT 25

void cleanup(struct Window *window)
{
    if (window)
        CloseWindow(window);

    if (GadToolsBase)
        CloseLibrary(GadToolsBase);

    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);

    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
}

int main()
{
    struct Window *window = NULL;
    struct RastPort *rp;
    int i, x, y;
    char text[64];

    /* Fixed window size with scroll */
    int window_width = 320;
    int window_height = 200;

    /* Open libraries */
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37L);

    if (!GfxBase)
    {
        printf("Error: Cannot open graphics.library\n");
        return 20;
    }

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37L);

    if (!IntuitionBase)
    {
        printf("Error: Cannot open intuition.library\n");
        cleanup(NULL);
        return 20;
    }

    GadToolsBase = OpenLibrary("gadtools.library", 37L);

    if (!GadToolsBase)
    {
        printf("Error: Cannot open gadtools.library\n");
        cleanup(NULL);
        return 20;
    }

    /* Create window */
    window = OpenWindowTags(NULL,
                            WA_Width, window_width,
                            WA_Height, window_height,
                            WA_Title, (ULONG) "Workbench Palette - Colors for config.txt",
                            WA_DragBar, TRUE,
                            WA_DepthGadget, TRUE,
                            WA_CloseGadget, TRUE,
                            WA_SizeGadget, FALSE, /* No resize - fixed size */
                            WA_Activate, TRUE,
                            WA_SimpleRefresh, TRUE,
                            WA_NoCareRefresh, TRUE,
                            WA_NewLookMenus, TRUE,
                            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
                            TAG_DONE);

    if (!window)
    {
        printf("Error: Cannot create window\n");
        cleanup(NULL);
        return 20;
    }

    rp = window->RPort;

    /* Draw all colors */
    for (i = 0; i < 16; i++)
    {
        x = 10 + (i % COLORS_PER_ROW) * (BOX_SIZE + BOX_MARGIN);
        y = 30 + (i / COLORS_PER_ROW) * (BOX_SIZE + BOX_MARGIN + 20);

        /* Draw color box */
        SetAPen(rp, i);
        RectFill(rp, x, y, x + BOX_SIZE - 1, y + BOX_SIZE - 1);

        /* Draw border */
        SetAPen(rp, 1);
        Move(rp, x, y);
        Draw(rp, x + BOX_SIZE - 1, y);
        Draw(rp, x + BOX_SIZE - 1, y + BOX_SIZE - 1);
        Draw(rp, x, y + BOX_SIZE - 1);
        Draw(rp, x, y);

        /* Draw number below box */
        SetAPen(rp, 1);
        snprintf(text, sizeof(text), "%d", i);
        Move(rp, x + 5, y + BOX_SIZE + 12);
        Text(rp, text, strlen(text));
    }

    /* Instructions */
    SetAPen(rp, 1);
    Move(rp, 10, window_height - 20);
    Text(rp, "Use numbers 0-15 in config.txt", 30);

    printf("Window opened. Close it to exit.\n");
    printf("Available colors for config.txt:\n");

    for (i = 0; i < 16; i++)
        printf("  %d\n", i);

    /* Handle window events */
    while (1)
    {
        struct IntuiMessage *msg = (struct IntuiMessage *)GetMsg(window->UserPort);

        if (!msg)
        {
            WaitPort(window->UserPort);
            continue;
        }

        if (msg->Class == IDCMP_CLOSEWINDOW)
        {
            ReplyMsg((struct Message *)msg);
            break;
        }
        else if (msg->Class == IDCMP_REFRESHWINDOW)
        {
            /* Redraw window */
            BeginRefresh(window);

            /* Redraw all colors */
            for (i = 0; i < 16; i++)
            {
                x = 10 + (i % COLORS_PER_ROW) * (BOX_SIZE + BOX_MARGIN);
                y = 30 + (i / COLORS_PER_ROW) * (BOX_SIZE + BOX_MARGIN + 20);

                /* Set pen color */
                SetAPen(rp, i);

                /* Draw color box */
                RectFill(rp, x, y, x + BOX_SIZE - 1, y + BOX_SIZE - 1);

                /* Draw border */
                SetAPen(rp, 1); /* White for border */
                Move(rp, x, y);
                Draw(rp, x + BOX_SIZE - 1, y);
                Draw(rp, x + BOX_SIZE - 1, y + BOX_SIZE - 1);
                Draw(rp, x, y + BOX_SIZE - 1);
                Draw(rp, x, y);

                /* Draw number below box */
                SetAPen(rp, 1);
                snprintf(text, sizeof(text), "%d", i);
                Move(rp, x + 5, y + BOX_SIZE + 12);
                Text(rp, text, strlen(text));
            }

            /* Instructions */
            SetAPen(rp, 1);
            Move(rp, 10, window->Height - 25);
            Text(rp, "Use numbers (0-15) in config.txt color field", 40);

            EndRefresh(window, TRUE);
        }
        else if (msg->Class == IDCMP_NEWSIZE)
        {
            /* Window resized - trigger refresh */
            RefreshWindowFrame(window);
        }

        ReplyMsg((struct Message *)msg);
    }

    cleanup(window);
    return 0;
}
