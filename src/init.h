/* init.h */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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

#ifndef INIT_H
#define INIT_H

void check_env_filter(void);
void get_prompt_cmds(void);
void get_aliases(void);
int get_last_path(void);
void check_options(void);
int load_dirhist(void);
size_t get_path_env(void);
size_t get_cdpath(void);
void get_prompt_cmds(void);
int get_sel_files(void);
void init_shell(void);
void load_jumpdb(void);
int load_bookmarks(void);
int load_actions(void);
int load_pinned_dir(void);
int load_prompts(void);
int load_remotes(void);
void load_tags(void);
void get_path_programs(void);
void unset_xargs(void);
void external_arguments(int argc, char **argv);
char *get_date(void);
void get_data_dir(void);
int set_start_path(void);
int init_history(void);
int init_workspaces(void);
int init_gettext(void);
int get_home(void);
int backup_argv(int argc, char **argv);
int get_sys_shell(void);
void set_prop_fields(char *line);
/*int xsecure_env(const int mode); */

struct user_t get_user(void);

#endif /* INIT_H */
