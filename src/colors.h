/* colors.h */

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

#ifndef COLORS_H
#define COLORS_H

#define RL_PRINTABLE    1
#define RL_NO_PRINTABLE 0 /* Add non-printing flags (\001 and \002)*/

/* Tell split_color_line wether we're dealing with interface or file
 * type colors. */
#define SPLIT_INTERFACE_COLORS 0
#define SPLIT_FILETYPE_COLORS  1

/* Max amount of custom color variables in the color scheme file. */
#define MAX_DEFS 128

/* Macros for the set_shades function. */
#define DATE_SHADES 0
#define SIZE_SHADES 1

/* Special colors (#RRGGBB and @NUM) need to be expanded into codes the
 * terminal emulator can understand. For example, "#FF0000" will be expanded
 * to "\x1b[38;2;255;0;0m", and "@160" to "\x1b[38;5;160m". */
#define RGB_COLOR_PREFIX '#'
#define COLOR256_PREFIX  '@'
#define IS_COLOR_PREFIX(c) ((c) == RGB_COLOR_PREFIX || (c) == COLOR256_PREFIX)

__BEGIN_DECLS

void color_codes(void);
void colors_list(char *ent, const int eln, const int pad, const int new_line);
int  cschemes_function(char **args);
#ifndef CLIFM_SUCKLESS
size_t get_colorschemes(void);
#endif /* CLIFM_SUCKLESS */
char *get_dir_color(const char *filename, const mode_t mode,
	const nlink_t links, const filesn_t count);
char *get_ext_color(const char *ext, size_t *val_len);
char *get_file_color(const char *filename, const struct stat *attr);
char *get_regfile_color(const char *filename, const struct stat *attr,
	size_t *is_ext);
int  import_color_scheme(const char *name);
void update_warning_prompt_text_color(void);
void remove_bold_attr(char *str);
char *remove_trash_ext(char **ent);
void reset_filetype_colors(void);
void reset_iface_colors(void);
int  set_colors(const char *colorscheme, const int check_env);
void set_default_colors(void);

__END_DECLS

#endif /* COLORS_H */
