/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* autocmds.h */

#ifndef AUTOCMDS_H
#define AUTOCMDS_H

__BEGIN_DECLS

int  add_autocmd(char **args);
void update_autocmd_opts(const int opt);
int  check_autocmds(void);
void parse_autocmd_line(char *cmd, const size_t buflen);
void print_autocmd_msg(void);
void reset_opts(void);
void revert_autocmd_opts(void);

__END_DECLS

#endif /* AUTOCMDS_H */
