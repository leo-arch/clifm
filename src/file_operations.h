/* file_operations.h */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
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

#include "bulk_rename.h"
#include "bulk_remove.h"

/* Macros and array used to print unsafe names description messages.
 * Used by validate_filename(). */
#define UNSAFE_DASH      0
#define UNSAFE_MIME      1
#define UNSAFE_ELN       2
#define UNSAFE_FASTBACK  3
#define UNSAFE_BTS_CONST 4
#define UNSAFE_SYS_KEY   5
#define UNSAFE_CONTROL   6
#define UNSAFE_META      7
#define UNSAFE_TOO_LONG  8
#ifdef _BE_POSIX
# define UNSAFE_NOT_PORTABLE 9
#endif

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

int  batch_link(char **args);
void clear_selbox(void);
int  cp_mv_file(char **args, const int copy_and_rename, const int force);
int  create_files(char **args, const int is_md);
int  create_dirs(char **args);
int  cwd_has_sel_files(void);
int  dup_file(char **cmd);
int  edit_link(char *link);
char *export_files(char **filenames, const int open);
int  open_file(char *file);
int  open_function(char **cmd);
int  remove_files(char **args);
int  symlink_file(char **args);
int  toggle_exec(const char *file, mode_t mode);
int  umask_function(char *arg);
int  xchmod(const char *file, const char *mode_str, const int flag);

__END_DECLS

#endif /* FILE_OPERATIONS_H */
