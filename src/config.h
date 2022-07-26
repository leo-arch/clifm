/* config.h */

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

#ifndef CONFIG_H
#define CONFIG_H

int create_bm_file(void);
int create_config(char *file);
int create_kbinds_file(void);
int create_mime_file(char *file, int new_prof);
void create_tmp_files(void);
int edit_function(char **comm);
void init_config(void);
int reload_config(void);
void set_div_line(const char *line);
void set_env(void);
void set_finder_paths(void);
void set_sel_file(void);

#endif /* CONFIG_H */
