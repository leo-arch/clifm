/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* fuzzy_match.h */

/* Taken from https://github.com/jhawthorn/fzy, licensed MIT
 * Modifications are licensed GPL2+ */

#ifndef FUZZY_MATCH_H
#define FUZZY_MATCH_H
/* Macros for our native fuzzy matcher: fuzzy_match() */

#define IS_WORD_SEPARATOR(c) ((c) == '-' || (c) == '_' || (c) == ' '      \
|| (c) == '.' || (c) ==  ',' || (c) ==  ';' || (c) ==  ':' || (c) ==  '@' \
|| (c) ==  '=' || (c) ==  '+' || (c) ==  '*' || (c) ==  '&')

#define IS_ALPHA_CASE(c)    (IS_ALPHA_LOW((c)) || IS_ALPHA_UP((c)))
#define IS_CAMEL_CASE(c, p) (IS_ALPHA_UP((c)) && IS_ALPHA_LOW((p)))

#define TARGET_BEGINNING_BONUS  (NAME_MAX * 10)
#define FIRST_CHAR_BONUS        10
#define INCLUDED_BONUS          8
#define WORD_BEGINNING_BONUS    5
#define CONSECUTIVE_CHAR_BONUS  4
#define SINGLE_CHAR_MATCH_BONUS 2
/* When suggesting filenames, an exact match doesn't provide anything
 * else for suggesting, so that it isn't useful */
#define EXACT_MATCH_BONUS       1

__BEGIN_DECLS

int fuzzy_match(const char *s1, const char *s2, const size_t s1_len, const int type);
int contains_utf8(const char *s);

__END_DECLS

#endif /* FUZZY_MATCH_H */
