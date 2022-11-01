/* sort.h */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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

#ifndef SORT_H
#define SORT_H

#include <dirent.h>

__BEGIN_DECLS

int  alphasort_insensitive(const struct dirent **, const struct dirent **);
int  xalphasort(const struct dirent **, const struct dirent **);
int  sort_function(char **);
int  entrycmp(const void *, const void *);
int  skip_nonexec(const struct dirent *);
int  skip_files(const struct dirent *);
void print_sort_method(void);

__END_DECLS

#endif /* SORT_H */
