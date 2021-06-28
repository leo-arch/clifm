/* selection.h */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
 * All rights reserved.

 * CliFM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CliFM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

#pragma once

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/ioctl.h>
#endif

/* selection.c */
int deselect(char **comm);
int sel_function(char **args);
void show_sel_files(void);
int sel_glob(char *str, const char *sel_path, mode_t filetype);
int sel_regex(char *str, const char *sel_path, mode_t filetype);
/* int select_file(char *file); */
int save_sel(void);
