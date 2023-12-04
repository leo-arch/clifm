/* file_operations.h */

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

#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

/* Macros for open_function */
#define OPEN_BLK     0
#define OPEN_CHR     1
#define OPEN_SOCK    2
#define OPEN_FIFO    3
#define OPEN_UNKNOWN 4
#ifdef __sun
# define OPEN_DOOR   5
#endif /*  */

__BEGIN_DECLS

int  batch_link(char **);
int  bulk_rename(char **);
int  bulk_remove(char *, char *);
void clear_selbox(void);
int  cp_mv_file(char **, const int, const int);
int  create_files(char **, const int);
int  create_dirs(char **);
int  dup_file(char **);
int  edit_link(char *);
char *export_files(char **, const int);
int  symlink_file(char **);
int  open_file(char *);
int  open_function(char **);
int  remove_file(char **);
int  xchmod(const char *, const char *, const int);
int  toggle_exec(const char *, mode_t);
int  umask_function(char *);

__END_DECLS

#endif /* FILE_OPERATIONS_H */
