/* keybinds.h */

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

#pragma once

/* keybinds.c */
void readline_kbinds(void);
int kbinds_function(char **args);
char *find_key(char *function);
int keybind_exec_cmd(char *str);
int load_keybinds(void);
int rl_create_file(int count, int key);
int rl_refresh(int count, int key);
int rl_parent_dir(int count, int key);
int rl_root_dir(int count, int key);
int rl_home_dir(int count, int key);
int rl_next_dir(int count, int key);
int rl_first_dir(int count, int key);
int rl_last_dir(int count, int key);
int rl_previous_dir(int count, int key);
int rl_long(int count, int key);
int rl_folders_first(int count, int key);
int rl_light(int count, int key);
int rl_hidden(int count, int key);
int rl_open_config(int count, int key);
int rl_open_keybinds(int count, int key);
int rl_open_cscheme(int count, int key);
int rl_open_bm_file(int count, int key);
int rl_open_jump_db(int count, int key);
int rl_open_mime(int count, int key);
int rl_mountpoints(int count, int key);
int rl_select_all(int count, int key);
int rl_deselect_all(int count, int key);
int rl_bookmarks(int count, int key);
int rl_selbox(int count, int key);
int rl_clear_line(int count, int key);
int rl_sort_next(int count, int key);
int rl_sort_previous(int count, int key);
int rl_lock(int count, int key);
int rl_remove_sel(int count, int key);
int rl_export_sel(int count, int key);
int rl_move_sel(int count, int key);
int rl_rename_sel(int count, int key);
int rl_paste_sel(int count, int key);
int rl_quit(int count, int key);
int rl_previous_profile(int count, int key);
int rl_next_profile(int count, int key);
int rl_dirhist(int count, int key);
int rl_archive_sel(int count, int key);
int rl_new_instance(int count, int key);
int rl_clear_msgs(int count, int key);
int rl_trash_sel(int count, int key);
int rl_untrash_all(int count, int key);
int rl_open_sel(int count, int key);
int rl_bm_sel(int count, int key);
int rl_kbinds_help(int count, int key);
int rl_cmds_help(int count, int key);
int rl_manpage(int count, int key);
int rl_pinned_dir(int count, int key);
int rl_ws1(int count, int key);
int rl_ws2(int count, int key);
int rl_ws3(int count, int key);
int rl_ws4(int count, int key);
int rl_plugin1(int count, int key);
int rl_plugin2(int count, int key);
int rl_plugin3(int count, int key);
int rl_plugin4(int count, int key);
