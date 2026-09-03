/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* xbrace.c */

#include "helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aux.h"    /* xnrealloc, xnmalloc, xitoa, xatoi */
#include "checks.h" /* is_number */

typedef struct {
	char **items;
	size_t count;
	size_t capacity;
} brace_t;

static void
list_add(brace_t *list, const char *text)
{
	if (list->count == list->capacity) {
		size_t new_capacity = list->capacity ? list->capacity * 2 : 8;
		list->items =
			xnrealloc(list->items, new_capacity + 1, sizeof(*list->items));
		list->capacity = new_capacity;
	}

	list->items[list->count++] = strdup(text);
	list->items[list->count] = NULL;
}

/* Return a malloc'd string containing the three strings, A, B, and C
 * concatenated. */
static char *
join3(const char *a, const char *b, const char *c)
{
	if (!a || !b || !c)
		return NULL;

	const size_t alen = strlen(a);
	const size_t blen = strlen(b);
	const size_t clen = strlen(c);

	const size_t len = alen + blen + clen + 1;
	char *buf = xnmalloc(len, sizeof(char));

	memcpy(buf, a, alen + 1);
	memcpy(buf + alen, b, blen + 1);
	memcpy(buf + alen + blen, c, clen + 1);

	return buf;
}

/* Finds the first unescaped '{' and its matching '}'.
 * Returns 1 if found, otherwise 0. */
static int
find_braces(const char *s, const char **open, const char **close)
{
	const char *p = NULL;
	int depth = 0;

	for (p = s; *p; p++) {
		if (*p == '{') {
			if (depth == 0)
				*open = p;
			depth++;
		} else {
			if (*p != '}' || depth == 0)
				continue;

			if (--depth == 0) {
				*close = p;
				return 1;
			}
		}
	}

	return 0;
}

static const char *
build_range_value(const int value, const int is_num, int *val_len)
{
	static char str[MAX_INT_STR + 1];
	const char *val = NULL;

	if (!is_num) {
		str[0] = (char)value;
		str[1] = '\0';
		val = (const char *)str;
		if (val_len) *val_len = 1;
	} else if (value >= 0) {
		val = xitoa(value);
		if (val_len)
			*val_len = !val[0] ? 0 : (!val[1] ? 1 : (int)strlen(val));
	} else {
		int bytes = snprintf(str, sizeof(str), "%d", value);
		if (val_len && bytes >= 0) *val_len = bytes;
		val = (const char *)str;
	}

	return val;
}

/* Expand numeric/alphabetic ranges in the string RANGE.
 * Each expanded string is prefixed with PREFIX and suffixed with SUFFIX.
 * The list of expanded strings is returned, or NULL if no expansion is made. */
static char **
expand_brace_ranges(const char *prefix, const char *range, const char *suffix)
{
	char *buf = strdup(range);

	char *first_sep = buf ? strstr(buf, "..") : NULL;
	if (!first_sep) {
		free(buf);
		return NULL;
	}

	*first_sep = '\0';
	char *start_text = buf;
	char *end_text = first_sep + 2; 

	char *step_text = NULL;
	char *second_sep = strstr(end_text, "..");
	if (second_sep) {
		*second_sep = '\0';
		step_text = second_sep + 2;
		if (*step_text && !is_number(step_text)) {
			free(buf);
			return NULL;
		}
	}

	char *st = start_text + (*start_text == '-');
	char *et = end_text + (*end_text == '-');
	const int is_num_start = (IS_DIGIT(*st) && is_number(st));
	const int is_num_end = (IS_DIGIT(*et) && is_number(et));

	/* Both fields in the range must be either numeric or alphabetic. */
	if (is_num_start != is_num_end) {
		free(buf);
		return NULL;
	}

	/* If alphabetic, only a single character is allowed. */
	if ((!is_num_start && start_text[1] != '\0')
	|| (!is_num_end && end_text[1] != '\0')) {
		free(buf);
		return NULL;
	}

	const int first = is_num_start ? xatoi(start_text) : (int)*start_text;
	const int second = is_num_end ? xatoi(end_text) : (int)*end_text;
	int step = step_text ? xatoi(step_text) : -1;
	if (step <= 0)
		step = 1;

	const int max = 10000;

	if (first == INT_MIN || second == INT_MIN || step == INT_MIN
	|| first > max || second > max || step == INT_MIN) {
		free(buf);
		return NULL;
	}

	char **ranges = NULL;
	size_t n = 0;

	if (first > second) {
		ranges = xnmalloc((size_t)(first - second + 2), sizeof(char *));
		for (int i = first; i >= second; i -= step) {
			const char *val = build_range_value(i, is_num_start, NULL);
			ranges[n++] = join3(prefix, val, suffix);
		}
	} else if (first < second) {
		ranges = xnmalloc((size_t)(second - first + 2), sizeof(char *));
		for (int i = first; i <= second; i += step) {
			const char *val = build_range_value(i, is_num_start, NULL);
			ranges[n++] = join3(prefix, val, suffix);
		}
	} else {
		ranges = xnmalloc(2, sizeof(char *));
		const char *val = build_range_value(first, is_num_start, NULL);
		ranges[n++] = join3(prefix, val, suffix);
	}

	ranges[n] = NULL;
	free(buf);
	return ranges;
}

/* Recusively expand the brace expression EXPRESSION and store the expanded
 * strings into RESULT. */
static void
expand_recursive(const char *expression, brace_t *result)
{
	const char *open = NULL;
	const char *close = NULL;

	/* Write into RESULT once there are no more expandable braces. */
	if (!find_braces(expression, &open, &close)) {
		list_add(result, expression);
		return;
	}

	const size_t prefix_length = (size_t)(open - expression);
	const size_t inside_length = (size_t)(close - open - 1);
	const char *suffix = close + 1;

	char *prefix = xnmalloc(prefix_length + 1, sizeof(char));
	char *inside = xnmalloc(inside_length + 1, sizeof(char));

	memcpy(prefix, expression, prefix_length);
	prefix[prefix_length] = '\0';

	memcpy(inside, open + 1, inside_length);
	inside[inside_length] = '\0';

	/* Split the brace contents on commas at depth zero.
	 * For example: "1,{2,3},4" becomes: "1" "{2,3}" "4" */
	size_t part_start = 0;
	int depth = 0;

	for (size_t i = 0; i <= inside_length; i++) {
		char c = inside[i];

		if (c == '{') {
			depth++;
		} else {
			if (c == '}')
				depth--;
		}

		if ((c == ',' && depth == 0) || c == '\0') {
			const size_t part_len = i - part_start;
			char *part = xnmalloc(part_len + 1, sizeof(char));

			memcpy(part, inside + part_start, part_len);
			part[part_len] = '\0';

			/* Expand the selected alternative first, then append the
			 * original suffix. This also handles braces in the suffix. */
			brace_t alternative = {0};
			expand_recursive(part, &alternative);
			free(part);

			for (size_t j = 0; j < alternative.count; j++) {
				char *alt = alternative.items[j];

				/* Expand numeric/alphabetic ranges as well. */
				char **ranges = NULL;
				if (strstr(alternative.items[j], "..")
				&& (ranges = expand_brace_ranges(prefix, alt, suffix))) {
					for (size_t k = 0; ranges[k]; k++) {
						expand_recursive(ranges[k], result);
						free(ranges[k]);
					}
					free(ranges);
				} else {
					char *combined = join3(prefix, alt, suffix);
					expand_recursive(combined, result);
					free(combined);
				}

				free(alternative.items[j]);
			}

			free(alternative.items);

			part_start = i + 1;
		}
	}

	free(prefix);
	free(inside);
}

/* Return a NULL-terminated array containing the expanded strings
 * corresponding to the brace expression EXPRESSION.
 * The caller must free the resulting array. */
char **
brace_expand(const char *expression, size_t *count)
{
	brace_t result = {0};
	expand_recursive(expression, &result);

	if (count) *count = result.count;

	return result.items;
}
