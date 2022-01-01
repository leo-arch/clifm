/* misc.h */

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

#ifndef MISC_H
#define MISC_H

int _err(int, int, const char *, ...);
int alias_import(char *file);
void bonus_function(void);
int create_usr_var(char *str);
int filter_function(const char *arg);
void free_software(void);
void free_stuff(void);
void free_remotes(int exit);
void handle_stdin(void);
void help_function(void);
int quick_help(void);
int hidden_function(char **comm);
int list_commands(void);
int list_mountpoints(void);
int new_instance(char *dir, int sudo);
/* char *parse_usrvar_value(const char *str, const char c); */
int pin_directory(char *dir);
void print_tips(int all);
void save_last_path(void);
/* void save_pinned_dir(void); */
/*int set_shell(char *str); */
void set_signals_to_ignore(void);
void set_term_title(const char *str);
void splash(void);
int unpin_dir(void);
void version_function(void);
#ifdef LINUX_INOTIFY
void read_inotify(void);
void reset_inotify(void);
#elif defined(BSD_KQUEUE)
void read_kqueue(void);
#endif
int sanitize_cmd(char *cmd, int type);

#endif /* MISC_H */
