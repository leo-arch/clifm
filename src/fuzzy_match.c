/* fuzzy_match.c */

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

/* This file contains two fuzzy matchers:
 * (1) fuzzy_match_fzy: slower than (2), but more accurate
 * (2) fuzzy_match: faster than (1), but less accurate in some cases */

/* The algorithm used by fuzzy_match_fzy() is taken from
 * https://github.com/jhawthorn/fzy, licensed MIT
 * Modifications are licensed GPL2+ */

#include "helpers.h"

#include <string.h>
/*
#include <ctype.h>
#include <string.h>
//#include <strings.h>
#include <stdio.h>
#include <float.h>
//#include <math.h>
#include <stdlib.h> */

#include "fuzzy_match.h"
#include "strings.h"

/*
#define max(a, b) (((a) > (b)) ? (a) : (b))

struct match_struct {
	int needle_len;
	int haystack_len;

	char lower_needle[MATCH_MAX_LEN];
	char lower_haystack[MATCH_MAX_LEN];

	score_t match_bonus[MATCH_MAX_LEN];
};

static void
precompute_bonus(const char *haystack, score_t *match_bonus)
{
	// Which positions are beginning of words
	char last_ch = '/';
	for (int i = 0; haystack[i]; i++) {
		char ch = haystack[i];
		match_bonus[i] = COMPUTE_BONUS(last_ch, ch);
		last_ch = ch;
	}
}

static void
setup_match_struct(struct match_struct *match, const char *needle, const char *haystack,
	const size_t nlen)
{
//	match->needle_len = (int)strlen(needle);
	match->needle_len = (int)nlen;
//	match->haystack_len = (int)hlen;
	match->haystack_len = (int)strlen(haystack);

	if (match->haystack_len > MATCH_MAX_LEN || match->needle_len > match->haystack_len)
		return;

	for (int i = 0; i < match->needle_len; i++)
		match->lower_needle[i] = (char)tolower(needle[i]);

	for (int i = 0; i < match->haystack_len; i++)
		match->lower_haystack[i] = (char)tolower(haystack[i]);

	precompute_bonus(haystack, match->match_bonus);
}

static inline void
match_row(const struct match_struct *match, int row, score_t *curr_D, score_t *curr_M,
const score_t *last_D, const score_t *last_M)
{
	int n = match->needle_len;
	int m = match->haystack_len;
	int i = row;

	const char *lower_needle = match->lower_needle;
	const char *lower_haystack = match->lower_haystack;
	const score_t *match_bonus = match->match_bonus;

	score_t prev_score = SCORE_MIN;
	score_t gap_score = i == n - 1 ? SCORE_GAP_TRAILING : SCORE_GAP_INNER;

	for (int j = 0; j < m; j++) {
		if (lower_needle[i] == lower_haystack[j]) {
			score_t score = SCORE_MIN;
			if (!i) {
				score = (j * SCORE_GAP_LEADING) + match_bonus[j];
			} else if (j) { // i > 0 && j > 0
				score = max(
						last_M[j - 1] + match_bonus[j],

						// Consecutive match, doesn't stack with match_bonus
						last_D[j - 1] + SCORE_MATCH_CONSECUTIVE);
			}
			curr_D[j] = score;
			curr_M[j] = prev_score = max(score, prev_score + gap_score);
		} else {
			curr_D[j] = SCORE_MIN;
			curr_M[j] = prev_score = prev_score + gap_score;
		}
	}
}

score_t
fuzzy_match_fzy(const char *needle, const char *haystack, size_t *positions,
	const size_t nlen)
{
	if (!*needle)
		return SCORE_MIN;

	struct match_struct match;
	setup_match_struct(&match, needle, haystack, nlen);

	int n = match.needle_len;
	int m = match.haystack_len;

	if (m > MATCH_MAX_LEN || n > m) {
		// Unreasonably large candidate: return no score
		// If it is a valid match it will still be returned, it will
		// just be ranked below any reasonably sized candidates
		return SCORE_MIN;
	} else if (n == m) {
		// Since this method can only be called with a haystack which
		// matches needle. If the lengths of the strings are equal the
		// strings themselves must also be equal (ignoring case).
		if (positions)
			for (int i = 0; i < n; i++)
				positions[i] = (size_t)i;
		return SCORE_MAX;
	}

	// D[][] Stores the best score for this position ending with a match.
	// M[][] Stores the best possible score at this position.
	score_t (*D)[MATCH_MAX_LEN], (*M)[MATCH_MAX_LEN];
	M = malloc(sizeof(score_t) * MATCH_MAX_LEN * (unsigned long)n);
	D = malloc(sizeof(score_t) * MATCH_MAX_LEN * (unsigned long)n);

	score_t *last_D, *last_M;
	score_t *curr_D, *curr_M;

	for (int i = 0; i < n; i++) {
		curr_D = &D[i][0];
		curr_M = &M[i][0];

		match_row(&match, i, curr_D, curr_M, last_D, last_M);

		last_D = curr_D;
		last_M = curr_M;
	}

	// Backtrace to find the positions of optimal matching
	if (positions) {
		int match_required = 0;
		for (int i = n - 1, j = m - 1; i >= 0; i--) {
			for (; j >= 0; j--) {
				// There may be multiple paths which result in
				// the optimal weight.
				//
				// For simplicity, we will pick the first one
				// we encounter, the latest in the candidate
				// string.
				if (D[i][j] != SCORE_MIN &&
				    (match_required || D[i][j] == M[i][j])) {
					// If this score was determined using
					// SCORE_MATCH_CONSECUTIVE, the
					// previous character MUST be a match
					match_required =
					    i && j &&
					    M[i][j] == D[i - 1][j - 1] + SCORE_MATCH_CONSECUTIVE;
					positions[i] = (size_t)j--;
					break;
				}
			}
		}
	}

	score_t result = M[n - 1][m - 1];

	free(M);
	free(D);

	return result;
} */

/* A basic fuzzy matcher. It returns a score based on how much the
 * pattern (S1) matches the item (S2) taking into account:
 * 
 * Initial character
 * Word beginnings
 * Consecutive characters
 * 
 * The caller can decide whether the returned score is enough. If not,
 * a new item must be inspected until we get the desired score. Previous
 * values should be stored in case the desired score is never found.
 *
 * What this fuzzy matcher lacks: taking gap (distance) between matched chars
 * into account */
int
fuzzy_match(char *s1, char *s2, const size_t s1_len, const int type)
{
	if (!s1 || !*s1 || !s2 || !*s2)
		return 0;

	if (type == FUZZY_FILES) {
		if ((*s1 == '.' && *(s1 + 1) == '.') || *s1 == '-')
			return 0;
	}

	int cs = conf.case_sens_path_comp;
	int included = 0;
	char *p = (char *)NULL;

	if (cs == 1 ? (p = strstr(s2, s1)) : (p = strcasestr(s2, s1))) {
		if (p == s2) {
			if (!*(s2 + s1_len))
				return EXACT_MATCH_BONUS;
			return TARGET_BEGINNING_BONUS;
		}

		included = 1;
	}

	int word_beginning = 0;
	int consecutive_chars = 0;
	int first_char = 0;
	if (cs == 1)
		first_char = *s1 == *s2 ? 1 : 0;
	else
		first_char = TOUPPER(*s1) == TOUPPER(*s2) ? 1 : 0;

	size_t l = 0;
	char *hs = s2;

	while (*s1) {
		char *m = cs == 1 ? strchr(hs, *s1) : xstrcasechr(hs, *s1);
		if (!m)
			break;

		if (*(s1 + 1) && *(m + 1) && (cs == 1 ? *(s1 + 1) == *(m + 1)
		: TOUPPER(*(s1 + 1)) == TOUPPER(*(m + 1)) ))
			consecutive_chars++;

		if (l > 0 && (!IS_ALPHA_CASE(*(m - 1)) || IS_CAMEL_CASE(*m, *(m - 1)) ) )
			word_beginning++;

		hs = ++m;
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

	return score;
}

/*
#include "levenshtein.h"
static int
calculate_lv_distance(char *s1, char *s2)
{
	levenshtein* lp = NULL;
	lp = lev_init(s1, s2);

	if (!lp)
		return (-1);

	lev_calc(lp);
	int distance = lp->grid[lp->source_length][lp->target_length];
	lev_free(lp);

	return distance;
} */

/*
int
fuzzy_match2(char *s1, char *s2, const size_t s1_len, const int type)
{
	if (!s1 || !*s1 || !s2 || !*s2)
		return 0;

	if (type == FUZZY_FILES) {
		if ((*s1 == '.' && *(s1 + 1) == '.') || *s1 == '-')
			return 0;
	}

	int included = 0;
	char *p = (char *)NULL;
	if (conf.case_sens_path_comp == 1 ? (p = strstr(s2, s1)) : (p = strcasestr(s2, s1))) {
		if (p == s2) {
			if (!*(s2 + s1_len))
				return EXACT_MATCH_BONUS;
			return TARGET_BEGINNING_BONUS;
//			if (*(s2 + len)) { // S2 is longer than S1
//				return FZ_PART_MATCH + (len * FZ_CONS_CHARS);
//			} else { // Exact match
//				return FZ_FULL_MATCH + (len * FZ_CONS_CHARS);
//			}
		}
		included = 1;
	}

	int word_beginning = 0;
	int consecutive_chars = 0;
	int first_char = 0;
	if (conf.case_sens_path_comp == 1)
		first_char = *s1 == *s2 ? 1 : 0;
	else
		first_char = TOUPPER(*s1) == TOUPPER(*s2) ? 1 : 0;

	size_t l = 0;
	char *hs = s2;

//	char *s1b = s1;

	while (*s1) {
		char *m = conf.case_sens_path_comp == 1 ? strchr(hs, *s1) : xstrcasechr(hs, *s1);
		if (!m)
			break;

		if (*(s1 + 1) && *(m + 1) && *(s1 + 1) == *(m + 1))
			consecutive_chars++;

		if (l > 0 && (!IS_ALPHA_CASE(*(m - 1)) || IS_CAMEL_CASE(*m, *(m - 1)) ) )
			word_beginning++;

		hs = ++m;
		s1++;
		l++;
	}

	if (!*s1) {
//		int distance = calculate_lv_distance(s1, s2);
		int score = 0;
		score += (word_beginning * WORD_BEGINNING_BONUS);
		score += (first_char * FIRST_CHAR_BONUS);
		score += (included * INCLUDED_BONUS);
		score += (consecutive_chars * CONSECUTIVE_CHAR_BONUS);
//		printf("%s (%d)\n", s2, score);

		return score;
//		int r = 0;
//		if (first_char == 1 && word_beggining > 0 && included == 0)
//			r = 4;
//		else if (first_char == 1 && cons_chars > 0 && included == 0)
//			r = 3;
//		else if (included == 1)
//			r = 3;
//		else if (first_char == 1 || cons_chars > 0)
//			r = 2;
//		else
//			r = 1;

//		printf("N: %s, len: %zu, FC: %d, WB: %d, CC: %d, D:%d\n", s2, l, first_char,
//			word_beggining, cons_chars, distance);

//		UNUSED(s1b);
//		if (strcmp(s1b, "shotts") == 0)
//			printf("S:'%s:%d'\n", s2, r);

//		return r;
//		if (first_char == 1 && extra_first_chars > 0 && included == 0)
//			return 4;
//		if (first_char == 1 && cons_chars > 0 && included == 0)
//			return 3;
//		if (included == 1)
//			return 3;
//		if (first_char == 1 || cons_chars > 0)
//			return 2;
//		return 1;
//		printf("len: %d, FC: %d, CC: %d, %cM\n", l, first_char,
//			cons_chars, *(s2 + l) ? 'P' : 'F');
	}

	return 0;
} */
