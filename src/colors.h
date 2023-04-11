/* colors.h */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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

__BEGIN_DECLS

void color_codes(void);
void colors_list(char *, const int, const int, const int);
int  cschemes_function(char **);
#ifndef CLIFM_SUCKLESS
size_t get_colorschemes(void);
#endif /* CLIFM_SUCKLESS */
char *get_dir_color(const char *, const mode_t, const nlink_t, const int);
char *get_ext_color(char *);
char *get_file_color(const char *, const struct stat *);
char *get_regfile_color(const char *, const struct stat *);
int  import_color_scheme(const char *);
void remove_bold_attr(char **);
char *remove_trash_ext(char **);
void reset_filetype_colors(void);
void reset_iface_colors(void);
int  set_colors(const char *, const int);
void set_default_colors(void);

__END_DECLS

#endif /* COLORS_H */
