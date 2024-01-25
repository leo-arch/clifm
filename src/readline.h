/* readline.h */

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

#ifndef READLINE_H
#define READLINE_H

__BEGIN_DECLS

int  initialize_readline(void);
int  is_quote_char(const char c);
char **my_rl_completion(const char *text, int start, int end);
char *my_rl_path_completion(const char *text, int state);
size_t rl_count_words(char *line, const char ifs);
int  rl_get_y_or_n(const char *_msg);
char *rl_no_hist(const char *prompt);
char *secondary_prompt(const char *_prompt, const char *line);

__END_DECLS

#endif /* READLINE_H */
