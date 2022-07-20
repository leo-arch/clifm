/* strings.h */

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

#ifndef STRINGS_H
#define STRINGS_H

/* Macros for the split_str function */
#define UPDATE_ARGS    1
#define NO_UPDATE_ARGS 0

char *dequote_str(char *text, int mt);
char *escape_str(const char *str);
int *expand_range(char *str, int listdir);
int fuzzy_match(char *s1, char *s2, const int case_sens);
char *gen_rand_str(size_t len);
char **get_substr(char *str, const char ifs);
char *home_tilde(char *new_path, int *_free);
char **parse_input_str(char *str);
char *remove_quotes(char *str);
char *replace_slashes(char *str, const char c);
char *replace_substr(char *haystack, char *needle, char *rep);
char *savestring(const char *restrict str, size_t size);
char **split_str(const char *str, const int update_args);
char *straftlst(char *str, const char c);
char *strbfrlst(char *str, const char c);
char *strbtw(char *str, const char a, const char b);
int strcntchr(const char *str, const char c);
int strcntchrlst(const char *str, const char c);
char *truncate_wname(const char *name);
int u8truncstr(char *restrict str, size_t n);
size_t wc_xstrlen(const char *restrict str);
char *xstrrpbrk(char *s, const char *accept);

#if defined(__linux__) && defined(_BE_POSIX)
char *xstrcasestr(char *a, char *b);
#endif /* __linux && _BE_POSIX */

size_t xstrnlen(const char *restrict s);
size_t xstrsncpy(char *restrict dst, const char *restrict src, size_t n);
int xstrverscmp(const char *s1, const char *s2);

#endif /* STRINGS_H */
