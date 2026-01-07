/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* checks.h */

#ifndef CHECKS_H
#define CHECKS_H

__BEGIN_DECLS

#ifndef _NO_FZF
void check_completion_mode(void);
#endif /* !_NO_FZF */

int  check_file_access(const mode_t mode, const uid_t uid, const gid_t gid);
char **check_for_alias(char **args);
int  check_glob_char(const char *str, const int gflag);
int  check_regex(const char *str);
void check_third_party_cmds(void);
void file_cmd_check(void);
char *get_sudo_path(void);
int  is_action_name(const char *s);
int  is_bin_cmd(char *str);
int  is_exec_cmd(const char *cmd);
int  is_file_in_cwd(char *name);
int  is_force_param(const char *s);
int  is_internal_cmd(char *cmd, const int flag, const int check_hist,
	const int check_search);
int  is_number(const char *restrict str);
int  is_url(const char *url);
void truncate_file(const char *file, const int max, const int check_dups);

__END_DECLS

#endif /* CHECKS_H */
