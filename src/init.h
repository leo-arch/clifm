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

/* init.h */

#ifndef INIT_H
#define INIT_H

__BEGIN_DECLS

int  backup_argv(const int argc, char **argv);
void check_env_filter(void);
void check_options(void);
void get_aliases(void);
size_t get_cdpath(void);
#ifdef LINUX_FSINFO
void get_ext_mountpoints(void);
#endif /* LINUX_FSINFO */
int  get_home(void);
int  get_last_path(void);
size_t get_path_env(const int check_timestamps);
void get_path_programs(void);
void get_prompt_cmds(void);
int  get_sel_files(void);
int  get_sys_shell(void);
struct user_t get_user_data(void);
void init_conf_struct(void);
int  init_gettext(void);
int  init_history(void);
void init_shell(void);
int  init_workspaces(void);
void init_workspaces_opts(void);
int  load_actions(void);
int  load_bookmarks(void);
int  load_dirhist(void);
void load_file_templates(void);
void load_jumpdb(void);
int  load_pinned_dir(void);
int  load_prompts(void);
int  load_remotes(void);
void load_tags(void);
int  restore_shell(void);
void set_prop_fields(const char *line);
void unset_xargs(void);

__END_DECLS

#endif /* INIT_H */
