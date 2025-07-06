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

/* strings.h */

#ifndef STRINGS_H
#define STRINGS_H

/* Macros for the split_str function */
#define UPDATE_ARGS    1
#define NO_UPDATE_ARGS 0

__BEGIN_DECLS

int  detect_space(char *s);
size_t count_chars(const char *s, const char c);
size_t count_words(size_t *start_word, size_t *full_word);
char *escape_str(const char *str);
char *gen_rand_str(const size_t len);
char *get_last_chr(char *str, const char c, const int len);
char **get_substr(char *str, const char ifs, const int fproc);
char *home_tilde(char *new_path, int *free_buf);
char **parse_input_str(char *str);
char *quote_str(const char *str);
char *remove_quotes(char *str);
char *replace_invalid_chars(const char *name);
char *replace_slashes(char *str, const char c);
char *replace_substr(const char *haystack, const char *needle, char *rep);
char *savestring(const char *restrict str, const size_t size);
char **split_str(char *str, const int update_args);
char *strbfrlst(char *str, const char c);
char *strbtw(char *str, const char a, const char b);
int  strcntchr(const char *str, const char c);
int  u8truncstr(char *restrict str, size_t max);
char *unescape_str(char *text, int mt);
size_t wc_xstrlen(const char *restrict str);
char *xstrrpbrk(char *s, const char *accept);

#if defined(_BE_POSIX)
char *x_strcasestr(char *a, char *b);
#endif /* _BE_POSIX */

char * xstrcasechr(char *s, char c);
size_t xstrsncpy(char *restrict dst, const char *restrict src, size_t n);
char * xstrncat(char *restrict dst, const size_t dst_len,
	const char *restrict src, const size_t dst_size);
int  xstrverscmp(const char *s1, const char *s2);

__END_DECLS

#endif /* STRINGS_H */
