/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* file_operations.h */

#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "bulk_rename.h"
#include "bulk_remove.h"

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
int  umask_function(const char *arg);
int  xchmod(const char *file, const char *mode_str, const int flag);

__END_DECLS

#endif /* FILE_OPERATIONS_H */
