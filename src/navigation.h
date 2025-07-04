/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* navigation.h */

#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "workspaces.h"

__BEGIN_DECLS

int  back_function(char **args);
int  backdir(char *str);
int  cd_function(char *new_path, const int cd_flag);
char *fastback(const char *str);
int  forth_function(char **args);
char **get_bd_matches(const char *str, int *n, const int mode);
void print_dirhist(char *query);
int  pwd_function(const char *arg);
int  xchdir(char *dir, const int cd_flag);

__END_DECLS

#endif /* NAVIGATION_H */
