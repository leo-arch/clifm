/* fuzzy_match.h */

/* Taken from https://github.com/jhawthorn/fzy, licensed MIT
 * Modifications are licensed GPL2+ */

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

#ifndef FUZZY_MATCH_H
#define FUZZY_MATCH_H
/* Macros for our native fuzzy matcher: fuzzy_match() */

#define IS_WORD_SEPARATOR(c) ( (c) == '-' || (c) == '_' || (c) == ' ' || (c) == '.' \
|| (c) ==  ',' || (c) ==  ';' || (c) ==  ':' || (c) ==  '@' || (c) ==  '=' \
|| (c) ==  '+' || (c) ==  '*' || (c) ==  '&')

#define IS_ALPHA_CASE(c) ( ((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') )
#define IS_CAMEL_CASE(c, p) ( (c) >= 'A' && (c) <= 'Z' && (p) >= 'a' && (p) <= 'z' )

#define TARGET_BEGINNING_BONUS  NAME_MAX * 10
#define FIRST_CHAR_BONUS        10
#define INCLUDED_BONUS          8
#define WORD_BEGINNING_BONUS    5
#define CONSECUTIVE_CHAR_BONUS  4
#define SINGLE_CHAR_MATCH_BONUS 2
/* When suggesting file names, an exact match doesn't provide anything
 * else for suggesting, so that it isn't useful */
#define EXACT_MATCH_BONUS       1

__BEGIN_DECLS

int fuzzy_match(char *s1, char *s2, const size_t s1_len, const int type);
int contains_utf8(const char *s);

__END_DECLS

#endif /* FUZZY_MATCH_H */
