/* suggestions.h */

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

void clear_suggestion(void);
int rl_suggestions(const char c);
void free_suggestion(void);
/*
int enable_raw_mode(int fd);
int disable_raw_mode(int fd);
int get_cursor_position(int ifd, int ofd);
void print_suggestion(const char *str, size_t offset, const char *color);
int check_completions(const char *str, const size_t len, const char c);
int check_filenames(const char *str, const size_t len, const char c, const int first_word);
int check_history(const char *str, const size_t len);
int check_cmds(const char *str, const size_t len);
int check_jumpdb(const char *str, const size_t len);
int check_int_params(const char *str, const size_t len); */
