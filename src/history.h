/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* history.h */

#ifndef HISTORY_H
#define HISTORY_H

/* Macros for history_function() */
#define NO_HIST_TIME 0
#define HIST_TIME    1

/* Macros for clear_logs() and print_logs() */
#define MSG_LOGS 1
#define CMD_LOGS 0

__BEGIN_DECLS

void add_to_cmdhist(char *cmd);
void add_to_dirhist(const char *dir_path);
int  clear_logs(const int flag);
int  get_history(void);
int  history_function(char **args);
int  log_cmd(void);
void log_msg(char *msg_str, const int print_prompt, const int logme,
	const int add_to_msgs_list);
int  print_logs(const int flag);
int  record_cmd(const char *input);

__END_DECLS

#endif /* HISTORY_H */
