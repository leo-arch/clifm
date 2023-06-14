/* navigation.h */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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

#ifndef NAVIGATION_H
#define NAVIGATION_H

__BEGIN_DECLS

int  back_function(char **);
int  backdir(char *);
int  cd_function(char *, const int);
char *fastback(char *);
int  forth_function(char **);
char **get_bd_matches(const char *, int *, const int);
int  handle_workspaces(char **);
void print_dirhist(char *);
int  pwd_function(const char *);
int  xchdir(char *, const int);

__END_DECLS

#endif /* NAVIGATION_H */
