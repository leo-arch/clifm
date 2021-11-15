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

#pragma once
#include <sys/stat.h>

/* checks.c */
void file_cmd_check(void);
void check_file_size(char *log_file, int max);
char **check_for_alias(char **args);
int check_regex(char *str);
int check_immutable_bit(char *file);
int is_bin_cmd(const char *str);
int digit_found(const char *str);
int is_internal(const char *cmd);
int is_internal_c(const char *restrict cmd);
int is_number(const char *restrict str);
int is_acl(char *file);
char *get_sudo_path(void);
void check_term(void);
int check_file_access(const struct stat file);
void check_third_party_cmds(void);
