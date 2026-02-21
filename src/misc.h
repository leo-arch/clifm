/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* misc.h */

#ifndef MISC_H
#define MISC_H

#include "help.h"

__BEGIN_DECLS

int  err(const int msg_type, const int prompt_flag, const char *format, ...);
int  alias_import(char *file);
void bonus_function(void);
int  confirm_sudo_cmd(const char **cmd);
int  create_usr_var(char *str);
int  expand_prompt_name(char *name);
int  filter_function(char *arg);
void free_autocmds(const int keep_temp);
void free_prompts(void);
void free_stuff(void);
int  free_remotes(const int exit);
void free_tags(void);
void free_workspaces_filters(void);
char *gen_diff_str(const int diff);
char *get_newname(const char *_prompt, char *old_name, int *quoted);
void get_term_size(void);
int  handle_stdin(void);
int  is_blank_name(const char *s);
int  list_mountpoints(void);
int  new_instance(char *, int);
int  print_reload_msg(const char *ptr, const char *color, const char *msg, ...);
int  pin_directory(char *dir);
void save_last_path(char *last_path_tmp);
void set_eln_color(void);
void set_filter_type(const char c);
void splash(void);
int  unpin_dir(void);
void version_function(const int full);

__END_DECLS

#endif /* MISC_H */
