/* fuzzy_match.h */

/* Taken from https://github.com/jhawthorn/fzy, licensed MIT
 * Modifications are licensed GPL2+ */

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

/* Macros for fuzzy_match_fzy() */
/*
#include <math.h>

typedef double score_t;
#define SCORE_MAX INFINITY
#define SCORE_MIN -INFINITY

#define MATCH_MAX_LEN 1024

#define SCORE_GAP_LEADING -0.005
#define SCORE_GAP_TRAILING -0.005
#define SCORE_GAP_INNER -0.01
#define SCORE_MATCH_CONSECUTIVE 1.0
#define SCORE_MATCH_SLASH 0.9
#define SCORE_MATCH_WORD 0.8
#define SCORE_MATCH_CAPITAL 0.7
#define SCORE_MATCH_DOT 0.6

#define ASSIGN_LOWER(v) \
	['a'] = (v), \
	['b'] = (v), \
	['c'] = (v), \
	['d'] = (v), \
	['e'] = (v), \
	['f'] = (v), \
	['g'] = (v), \
	['h'] = (v), \
	['i'] = (v), \
	['j'] = (v), \
	['k'] = (v), \
	['l'] = (v), \
	['m'] = (v), \
	['n'] = (v), \
	['o'] = (v), \
	['p'] = (v), \
	['q'] = (v), \
	['r'] = (v), \
	['s'] = (v), \
	['t'] = (v), \
	['u'] = (v), \
	['v'] = (v), \
	['w'] = (v), \
	['x'] = (v), \
	['y'] = (v), \
	['z'] = (v)

#define ASSIGN_UPPER(v) \
	['A'] = (v), \
	['B'] = (v), \
	['C'] = (v), \
	['D'] = (v), \
	['E'] = (v), \
	['F'] = (v), \
	['G'] = (v), \
	['H'] = (v), \
	['I'] = (v), \
	['J'] = (v), \
	['K'] = (v), \
	['L'] = (v), \
	['M'] = (v), \
	['N'] = (v), \
	['O'] = (v), \
	['P'] = (v), \
	['Q'] = (v), \
	['R'] = (v), \
	['S'] = (v), \
	['T'] = (v), \
	['U'] = (v), \
	['V'] = (v), \
	['W'] = (v), \
	['X'] = (v), \
	['Y'] = (v), \
	['Z'] = (v)

#define ASSIGN_DIGIT(v) \
	['0'] = (v), \
	['1'] = (v), \
	['2'] = (v), \
	['3'] = (v), \
	['4'] = (v), \
	['5'] = (v), \
	['6'] = (v), \
	['7'] = (v), \
	['8'] = (v), \
	['9'] = (v)

static const score_t bonus_states[3][256] = {
	{ 0 },
	{
		['/'] = SCORE_MATCH_SLASH,
		['-'] = SCORE_MATCH_WORD,
		['_'] = SCORE_MATCH_WORD,
		[' '] = SCORE_MATCH_WORD,
		['.'] = SCORE_MATCH_DOT,
	},
	{
		['/'] = SCORE_MATCH_SLASH,
		['-'] = SCORE_MATCH_WORD,
		['_'] = SCORE_MATCH_WORD,
		[' '] = SCORE_MATCH_WORD,
		['.'] = SCORE_MATCH_DOT,

		// ['a' ... 'z'] = SCORE_MATCH_CAPITAL,
		ASSIGN_LOWER(SCORE_MATCH_CAPITAL)
	}
};

static const size_t bonus_index[256] = {
	ASSIGN_UPPER(2), // ['A' ... 'Z'] = 2
	ASSIGN_LOWER(1), // ['a' ... 'z'] = 1
	ASSIGN_DIGIT(1)  // ['0' ... '9'] = 1
};

#define COMPUTE_BONUS(last_ch, ch) (bonus_states[bonus_index[(unsigned char)(ch)]][(unsigned char)(last_ch)])

score_t fuzzy_match_fzy(const char *, const char *, size_t *, const size_t);
*/
int fuzzy_match(char *, char *, const size_t, const int);
int contains_utf8(const char *);

#endif /* FUZZY_MATCH_H */
