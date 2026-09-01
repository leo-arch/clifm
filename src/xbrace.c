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
} brace_list_t;

static void
list_add(brace_list_t *list, const char *text)
{
	if (list->count == list->capacity) {
		size_t new_capacity = list->capacity ? list->capacity * 2 : 8;
		list->items =
			xnrealloc(list->items, new_capacity, sizeof(*list->items));
		list->capacity = new_capacity;
	}

	list->items[list->count++] = strdup(text);
}

/* Return a malloc'd string containing the three strings, A, B, and C
 * concatenated. */
static char *
join3(const char *a, const char *b, const char *c)
{
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
		} else if (*p == '}') {
			if (depth > 0) {
				depth--;

				if (depth == 0) {
					*close = p;
					return 1;
				}
			}
		}
	}

	return 0;
}

static char *
join3_range(const int value, const char *prefix,
	const char *suffix, const int is_num)
{
	const size_t prefix_len = prefix ? strlen(prefix) : 0;
	const size_t suffix_len = suffix ? strlen(suffix) : 0;
	char str[MAX_INT_STR + 1];
	const char *val = NULL;
	int val_len = 0;

	if (!is_num) {
		str[0] = (char)value;
		str[1] = '\0';
		val = (const char *)str;
		val_len = 1;
	} else if (value >= 0) {
		val = xitoa(value);
		val_len = !val[0] ? 0 : (!val[1] ? 1 : (int)strlen(val));
	} else {
		val_len = snprintf(str, sizeof(str), "%d", value);
		val = (const char *)str;
	}

	const size_t len = prefix_len + (size_t)val_len + suffix_len + 1;
	char *buf = xnmalloc(len, sizeof(char));

	memcpy(buf, prefix, prefix_len + 1);
	memcpy(buf + prefix_len, val, (size_t)val_len + 1);
	memcpy(buf + prefix_len + (size_t)val_len, suffix, suffix_len + 1);

	return buf;
}

/* Expand numeric/alphabetic ranges in the string RANGE.
 * Each expanded string prefixed with PREFIX and suffixed with SUFFIX.
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

	/* Both fields in the range must be either numberic or alphabetic. */
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
			ranges[n++] = join3_range(i, prefix, suffix, is_num_start);
			ranges[n] = NULL;
		}
	} else if (first < second) {
		ranges = xnmalloc((size_t)(second - first + 2), sizeof(char *));
		for (int i = first; i <= second; i += step) {
			ranges[n++] = join3_range(i, prefix, suffix, is_num_start);
			ranges[n] = NULL;
		}
	} else {
		ranges = xnmalloc(2, sizeof(char *));
		ranges[n++] = join3_range(first, prefix, suffix, is_num_start);
		ranges[n] = NULL;
	}

	free(buf);
	return ranges;
}

/* Recusively expand the brace expression EXPRESSION and store the expanded
 * strings into RESULT. */
static void
expand_recursive(const char *expression, brace_list_t *result)
{
	const char *open = NULL;
	const char *close = NULL;

	if (!find_braces(expression, &open, &close)) {
		list_add(result, expression);
		return;
	}

	size_t prefix_length = (size_t)(open - expression);
	size_t inside_length = (size_t)(close - open - 1);
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

		if (c == '{')
			depth++;
		else if (c == '}')
			depth--;

		if ((c == ',' && depth == 0) || c == '\0') {
			size_t part_length = i - part_start;
			char *part = xnmalloc(part_length + 1, sizeof(char));

			memcpy(part, inside + part_start, part_length);
			part[part_length] = '\0';

			/* Expand the selected alternative first, then append the
			 * original suffix. This also handles braces in the suffix. */
			brace_list_t alternative = {0};
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
	brace_list_t result = {0};
	expand_recursive(expression, &result);

	/* Add a NULL terminator so the result can also be traversed like
	 * a conventional argv-style array. */
	result.items = xnrealloc(result.items,
		(result.count + 1), sizeof(*result.items));

	result.items[result.count] = NULL;
	if (count) *count = result.count;

	return result.items;
}
