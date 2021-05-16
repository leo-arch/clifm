/* config.h */

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

int regen_config(void);
int edit_function(char **comm);
void define_config_file_names(void);
int create_config(const char *file);
void create_config_files(void);
void create_def_cscheme(void);
int create_kbinds_file(void);
void init_config(void);
void read_config(void);
int create_bm_file(void);
int reload_config(void);
void copy_plugins(void);
void edit_xresources(void);
void create_tmp_files(void);
void set_sel_file(void);
void set_env(void);
