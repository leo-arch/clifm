/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* readline.h */

#ifndef READLINE_H
#define READLINE_H

__BEGIN_DECLS

int  initialize_readline(void);
int  is_quote_char(const char c);
char **my_rl_completion(const char *text, int start, int end);
char *my_rl_path_completion(const char *text, int state);
int  rl_get_y_or_n(const char *msg_str, char default_answer);
char *rl_no_hist(const char *prompt_str, const int tabcomp);
char *secondary_prompt(const char *prompt_str, const char *line);

__END_DECLS

#endif /* READLINE_H */
