/* checks.h */

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

#ifndef CHECKS_H
#define CHECKS_H

#include <sys/stat.h>

#ifndef _NO_FZF
void check_completion_mode(void);
#endif
void check_file_size(char *log_file, int max);
int check_file_access(const struct stat *file);
char **check_for_alias(char **cmd);
int check_glob_char(const char *str, const int only_glob);
int check_immutable_bit(char *file);
int check_regex(char *str);
void check_term(void);
void check_third_party_cmds(void);
void file_cmd_check(void);
char *get_sudo_path(void);
int is_bin_cmd(char *str);
int is_file_in_cwd(char *name);
int is_internal(const char *cmd);
int is_internal_c(const char *restrict cmd);
int is_internal_f(const char *restrict cmd);
int is_number(const char *restrict str);
int is_acl(char *file);
int is_url(char *url);

#endif /* CHECKS_H */
