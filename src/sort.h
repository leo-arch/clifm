/* sort.h */

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

#include <dirent.h>

/* sort.c */
int alphasort_insensitive(const struct dirent **a, const struct dirent **b);
int xalphasort(const struct dirent **a, const struct dirent **b);
int sort_function(char **arg);
int entrycmp(const void *a, const void *b);
int namecmp(const char *s1, const char *s2);
int skip_nonexec(const struct dirent *ent);
int skip_files(const struct dirent *ent);
