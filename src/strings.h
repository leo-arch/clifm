/* strings.h */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2023, L. Abramovich <johndoe.arch@outlook.com>
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

__BEGIN_DECLS

char *dequote_str(char *, int);
char *escape_str(const char *);
int  *expand_range(char *, int);
//int  fuzzy_match(char *, char *, const int, const int);

//int  fuzzy_match(char *, char *, const int);
//int  fuzzy_match2(char *, char *, const int, int *);

char *gen_rand_str(size_t);
char *get_last_chr(char *, const char, const int);
//char *get_last_space(char *, const int);
char **get_substr(char *, const char);
char *home_tilde(char *, int *);
char **parse_input_str(char *);
char *remove_quotes(char *);
char *replace_slashes(char *, const char);
char *replace_substr(char *, char *, char *);
char *savestring(const char *restrict, size_t);
char **split_str(const char *, const int);
char *straftlst(char *, const char);
char *strbfrlst(char *, const char);
char *strbtw(char *, const char, const char);
int  strcntchr(const char *, const char);
int  strcntchrlst(const char *, const char);
char *truncate_wname(const char *);
int  u8truncstr(char *restrict, size_t);
size_t wc_xstrlen(const char *restrict);
char *xstrrpbrk(char *, const char *);

#if (defined(__linux__) || defined(__CYGWIN__)) && defined(_BE_POSIX)
char *xstrcasestr(char *, char *);
#endif /* (__linux__ || __CYGWIN__) && _BE_POSIX */

char * xstrcasechr(char *, char);
size_t xstrnlen(const char *restrict);
size_t xstrsncpy(char *restrict, const char *restrict, size_t);
int xstrverscmp(const char *, const char *);

__END_DECLS

#endif /* STRINGS_H */
