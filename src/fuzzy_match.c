/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* fuzzy_match.c */

/* This file contains two fuzzy matchers:
 * (1) fuzzy_match_fzy: slower than (2), but more accurate
 * (2) fuzzy_match: faster than (1), but less accurate in some cases */

/* The algorithm used by fuzzy_match_fzy() is taken from
 * https://github.com/jhawthorn/fzy, licensed MIT
 * Modifications are licensed GPL2+ */

#include "helpers.h"

#include <string.h>

#include "fuzzy_match.h"
#include "utf8.h"

/* Three functions added to the utf8.h library */
/* Return the number of bytes needed to advance to the character next to S. */
static int
utf8nextcodepoint(const char *s)
{
	if (0xf0 == (0xf8 & *s)) {
		/* 4-byte utf8 code point (began with 0b11110xxx) */
		return 4;
	} else if (0xe0 == (0xf0 & *s)) {
		/* 3-byte utf8 code point (began with 0b1110xxxx) */
		return 3;
	} else if (0xc0 == (0xe0 & *s)) {
		/* 2-byte utf8 code point (began with 0b110xxxxx) */
		return 2;
	} else { /* (0x00 == (0x80 & *s)) { */
		/* 1-byte ascii (began with 0b0xxxxxxx) */
		return 1;
	}
}

/* A Unicode aware version of xstrcasechr (in strings.c)*/
static char *
utf8casechr(char *s, char *c)
{
	if (!s || !*s || !c || !*c)
		return (char *)NULL;

	utf8_int32_t cps = 0, cpc = 0, cp = 0;
	char *ret = (char *)NULL;

	utf8codepoint(c, &cpc);
	cp = utf8uprcodepoint(cpc);

	while (*s) {
		ret = utf8codepoint(s, &cps);
		if (utf8uprcodepoint(cps) != cp) {
			s = ret;
			continue;
		}

		return s;
	}

	return (char *)NULL;
}

/* Check whether the string S contains at least one UTF8 codepoint.
 * Returns 1 if true or 0 if false. */
int
contains_utf8(const char *s)
{
	if (!s || !*s)
		return 0;

	while (*s) {
		if (IS_UTF8_LEAD_BYTE(*s))
			return 1;
		s++;
	}

	return 0;
}

/* Same as fuzzy_match(), but:
 * 1: Not Unicode aware
 * 2: Much faster */
static int
fuzzy_match_v1(char *s1, char *s2, const size_t s1_len)
{
	const int cs = conf.case_sens_path_comp;
	int included = 0;
	char *p = (char *)NULL;

	if (cs == 1 ? (p = strstr(s2, s1)) : (p = xstrcasestr(s2, s1))) {
		if (p == s2) {
			if (!*(s2 + s1_len))
				return EXACT_MATCH_BONUS;
			return TARGET_BEGINNING_BONUS;
		}

		included = 1;
	}

	int word_beginning = 0;
	int consecutive_chars = 0;
	const int first_char = cs == 1 ? (*s1 == *s2)
		: (TOUPPER(*s1) == TOUPPER(*s2));

	size_t l = 0;
	char *hs = s2;

	while (*s1) {
		char *m = cs == 1 ? strchr(hs, *s1) : xstrcasechr(hs, *s1);
		if (!m)
			break;

		if (s1[1] && m[1] && (cs == 1 ? s1[1] == m[1]
		: TOUPPER(s1[1]) == TOUPPER(m[1]) ))
			consecutive_chars++;

		if (l > 0 && (!IS_ALPHA_CASE(*(m - 1)) || IS_CAMEL_CASE(*m, *(m - 1)) ) )
			word_beginning++;

		m++;
		hs = m;
		s1++;
		l++;
	}

	if (*s1)
		return 0;

	int score = 0;
	score += (word_beginning * WORD_BEGINNING_BONUS);
	score += (first_char * FIRST_CHAR_BONUS);
	score += (included * INCLUDED_BONUS);
	score += (consecutive_chars * CONSECUTIVE_CHAR_BONUS);
	score += ((int)l * SINGLE_CHAR_MATCH_BONUS);

	return score;
}

/* A basic fuzzy matcher. It returns a score based on how much the
 * pattern (S1) matches the item (S2), taking into account:
 *
 * Initial character
 * Word beginnings
 * Consecutive characters
 *
 * fuzzy_match_v1() will be used whenever the pattern contains no UTF8 char
 *
 * The caller can decide whether the returned score is enough. If not,
 * a new item must be inspected until we get the desired score. Previous
 * values should be stored in case the desired score is never reached.
 *
 * What this fuzzy matcher lacks:
 * 1. Taking gap (distance) between matched chars into account */
int
fuzzy_match(char *s1, char *s2, const size_t s1_len, const int type)
{
	if (!s1 || !*s1 || !s2 || !*s2)
		return 0;

	if (type == FUZZY_FILES_ASCII || type == FUZZY_FILES_UTF8) {
		if ((*s1 == '.' && s1[1] == '.') || *s1 == '-')
			return 0;
	}

	if (type == FUZZY_FILES_ASCII || conf.fuzzy_match_algo == 1)
		return fuzzy_match_v1(s1, s2, s1_len);

	const int cs = conf.case_sens_path_comp;
	int included = 0;
	char *p = (char *)NULL;

	if (cs == 1 ? (p = utf8str(s2, s1)) : (p = utf8casestr(s2, s1))) {
		if (p == s2) {
			if (!*(s2 + s1_len))
				return EXACT_MATCH_BONUS;
			return TARGET_BEGINNING_BONUS;
		}

		included = 1;
	}

	utf8_int32_t cp1 = 0, cp2 = 0;
	utf8codepoint(s1, &cp1);
	utf8codepoint(s2, &cp2);

	int word_beginning = 0;
	int consecutive_chars = 0;
	const int first_char = (cs == 1) ? (cp1 == cp2)
		: (utf8uprcodepoint(cp1) == utf8uprcodepoint(cp2));

	size_t l = 0;
	char *hs = s2;

	while (*s1) {
		char *m = (char *)NULL;
		if (cs == 1) {
			utf8codepoint(s1, &cp1);
			m = utf8chr(hs, cp1);
		} else {
			m = utf8casechr(hs, s1);
		}
		if (!m)
			break;

		int a = utf8nextcodepoint(s1);
		int b = utf8nextcodepoint(m);

		cp1 = 0; cp2 = 0;
		utf8codepoint(s1 + a, &cp1);
		utf8codepoint(m + b, &cp2);
		if (*(s1 + a) && *(m + b) && (cs == 1 ? (cp1 == cp2)
		: (utf8uprcodepoint(cp1) == utf8uprcodepoint(cp2)) ) )
			consecutive_chars++;

		const char *bc = l > 0 ? utf8rcodepoint(m, &cp1) : (char *)NULL;
		if (bc) {
			if (IS_WORD_SEPARATOR(*bc)) {
				word_beginning++;
			} else {
				utf8codepoint(bc, &cp2);
				if (utf8isupper(cp2) != 1 && utf8isupper(cp1) == 1)
					word_beginning++;
			}
		}

		hs = m + b;
		s1 += a;
		l++;
	}

	if (*s1)
		return 0;

	int score = 0;
	score += (word_beginning * WORD_BEGINNING_BONUS);
	score += (first_char * FIRST_CHAR_BONUS);
	score += (included * INCLUDED_BONUS);
	score += (consecutive_chars * CONSECUTIVE_CHAR_BONUS);
	score += ((int)l * SINGLE_CHAR_MATCH_BONUS);

	return score;
}
