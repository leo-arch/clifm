/* checks.h */

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

#ifndef CHECKS_H
#define CHECKS_H

__BEGIN_DECLS

#ifndef _NO_FZF
void check_completion_mode(void);
#endif

//void check_file_size(char *, int);
int  check_file_access(const mode_t, const uid_t, const gid_t);
char **check_for_alias(char **);
int  check_glob_char(const char *, const int);
int  check_immutable_bit(char *);
int  check_regex(char *);
void check_term(void);
void check_third_party_cmds(void);
void file_cmd_check(void);
char *get_sudo_path(void);
int  is_action_name(const char *);
int  is_bin_cmd(char *);
int  is_file_in_cwd(char *);
int  is_force_param(const char *);
int  is_internal(const char *);
int  is_internal_c(const char *restrict);
int  is_internal_f(const char *restrict);
int  is_number(const char *restrict);
int  is_acl(char *);
int  is_url(char *);
void truncate_file(char *, int);

__END_DECLS

#endif /* CHECKS_H */
