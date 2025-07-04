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

/* config.h */

#ifndef CONFIG_H
#define CONFIG_H

#define DUMP_CONFIG_STR  0
#define DUMP_CONFIG_INT  1
#define DUMP_CONFIG_BOOL 2
#define DUMP_CONFIG_CHR  3

__BEGIN_DECLS

int  config_edit(char **args);
int  create_bm_file(void);
int  create_kbinds_file(void);
int  create_main_config_file(char *file);
int  create_mime_file(char *file, const int new_prof);
void create_tmp_files(void);
void init_config(void);
int  reload_config(void);
int  config_reload(const char *arg);
void set_div_line(char *line);
void set_env(const int reload);
#ifndef _NO_FZF
int  get_fzf_border_type(const char *line);
int  get_fzf_height(char *line);
#endif /* !_NO_FZF */
void set_sel_file(void);
void set_time_style(char *line, char **str, const int ptime);

__END_DECLS

#endif /* CONFIG_H */
