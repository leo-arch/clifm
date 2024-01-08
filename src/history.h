/* history.h */

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
void log_msg(char *_msg, const int print_prompt, const int logme,
	const int add_to_msgs_list);
int  print_logs(const int flag);
int  record_cmd(char *input);

__END_DECLS

#endif /* HISTORY_H */
