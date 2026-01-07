/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
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
