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

/* ui_files.h -- Modal file browser */
#ifndef UI_FILES_H
#define UI_FILES_H

/* Browse for file starting at <start_dir> (or CWD if NULL),
 * fills <out_path> (0=ok, -1=cancel, -2=error) */
int ui_files_pick(const char *title, const char *start_dir, char *out_path, int out_path_sz);

/* Same as ui_files_pick but shows an editable filename field for save-as
 * <init_name> pre-fills the name field (may be NULL) */
int ui_files_save(const char *title, const char *start_dir, const char *init_name, char *out_path, int out_path_sz);

#endif /* UI_FILES_H */
