/* colors.h */

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

#ifndef COLORS_H
#define COLORS_H

#include <sys/stat.h>

void color_codes(void);
void colors_list(char *ent, const int eln, const int pad, const int new_line);
int cschemes_function(char **args);
#ifndef CLIFM_SUCKLESS
size_t get_colorschemes(void);
#endif /* CLIFM_SUCKLESS */
char *get_dir_color(const char *filename, const mode_t mode, const nlink_t links);
char *get_ext_color(char *ext);
char *get_file_color(const char *filename, const struct stat *attr);
int import_color_scheme(const char *name);
void reset_filetype_colors(void);
void reset_iface_colors(void);
int set_colors(const char *colorscheme, const int env);
void set_default_colors(void);
void unset_suggestions_color(void);

#endif /* COLORS_H */
