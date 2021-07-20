/* readline.h */

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

int initialize_readline(void);
char *my_rl_path_completion(const char *text, int state);
char **my_rl_completion(const char *text, int start, int end);
char *my_rl_quote(char *text, int mt, char *qp);
char *rl_no_hist(const char *prompt);
int quote_detector(char *line, int index);
/* char *bin_cmd_generator(const char *text, int state);
char *cschemes_generator(const char *text, int state);
char *filenames_gen_eln(const char *text, int state);
char *filenames_gen_text(const char *text, int state);
char *profiles_generator(const char *text, int state);
char *hist_generator(const char *text, int state);
char *jump_generator(const char *text, int state);
char *jump_entries_generator(const char *text, int state);
char *bookmarks_generator(const char *text, int state); */
int is_quote_char(const char c);
int my_rl_getc(FILE *stream);
int rl_suggestions(char c);
