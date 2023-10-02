/* fsinfo.h */

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

#ifndef FSINFO_H
#define FSINFO_H

__BEGIN_DECLS

#ifdef __linux__
char *get_fs_type_name(const char *, int *);
char *get_remote_fs_name(const char *);
char *get_dev_name(const dev_t);
#elif defined(HAVE_STATFS)
void get_dev_info(const char *, char **, char **);
#endif

__END_DECLS

#endif /* FS_INFO */