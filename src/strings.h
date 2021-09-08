/* strings.h */

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

char **parse_input_str(char *str);
char *savestring(const char *restrict str, size_t size);
char **get_substr(char *str, const char ifs);
char *dequote_str(char *text, int mt);
char *escape_str(const char *str);
int *expand_range(char *str, int listdir);
char *home_tilde(const char *new_path);
int strcntchr(const char *str, const char c);
int strcntchrlst(const char *str, const char c);
char *straft(char *str, const char c);
char *straftlst(char *str, const char c);
char *strbfr(char *str, const char c);
char *strbfrlst(char *str, const char c);
char *strbtw(char *str, const char a, const char b);
char *gen_rand_str(size_t len);
/*char *xstrcpy(char *buf, const char *restrict str);
size_t xstrlen(const char *restrict s);
int xstrncmp(const char *s1, const char *s2, size_t n);
char *xstrncpy(char *buf, const char *restrict str, size_t n); */
size_t xstrsncpy(char *restrict dst, const char *restrict src, size_t n);
size_t wc_xstrlen(const char *restrict str);
int u8truncstr(char *restrict str, size_t n);
size_t u8_xstrlen(const char *restrict str);
char *remove_quotes(char *str);
char *replace_substr(char *haystack, char *needle, char *rep);
size_t xstrnlen(const char *restrict s);
