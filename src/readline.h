/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* readline.h */

#ifndef READLINE_H
#define READLINE_H

/* Macros for rl_get_y* functions */
#define RL_ANSWER_NO   0
#define RL_ANSWER_YES  1
#define RL_ANSWER_ALL  2
#define RL_ANSWER_QUIT 3

__BEGIN_DECLS

int  initialize_readline(void);
int  is_quote_char(const char c);
char **my_rl_completion(const char *text, int start, int end);
char *my_rl_path_completion(const char *text, int state);
int  rl_get_y_or_n(const char *msg_str, char default_answer);
int  rl_get_y_n_all(const char *msg_str, char default_answer);
char *rl_no_hist(const char *prompt_str, const int tabcomp);
char *secondary_prompt(const char *prompt_str, const char *line);

__END_DECLS

#endif /* READLINE_H */
