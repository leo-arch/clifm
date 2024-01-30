/* strings.c -- misc string manipulation functions */

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

/* The xstrsncpy function is taken from https://github.com/jarun/nnn/blob/master/src/nnn.c,
 * licensed under BSD-2-clause.
 * All changes are licensed under GPL-2.0-or-later. */

/* The xstrverscmp is taken from https://elixir.bootlin.com/uclibc-ng/latest/source/libc/string/strverscmp.c
 * (licensed under GPL2.1+).
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#if defined(__HAIKU__)
# include <stdint.h>
#endif /* __HAIKU__ */
#include <stdio.h>
#include <string.h>
#ifndef HAVE_ARC4RANDOM
# include <time.h> /* time(2) */
#endif /* !HAVE_ARC4RANDOM */
#include <wchar.h>
#if !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__ANDROID__)
# define HAVE_WORDEXP
# include <wordexp.h>
#endif /* !__HAIKU__ && !__OpenBSD && !__ANDROID */
#include <errno.h>

#if defined(__OpenBSD__)
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include "aux.h"
#include "checks.h"
#include "exec.h"  /* exec_chained_cmds */
#ifndef _NO_MAGIC
# include "mime.h" /* MIME-type filter expansion */
#endif /* _NO_MAGIC */
#include "misc.h"
#include "navigation.h"
#include "readline.h"
#include "sort.h"
#include "tags.h"

/* Macros for xstrverscmp() */
/* states: S_N: normal, S_I: comparing integral part, S_F: comparing
           fractionnal parts, S_Z: idem but with leading Zeroes only */
#define S_N 0x0
#define S_I 0x3
#define S_F 0x6
#define S_Z 0x9

/* result_type: VCMP: return diff; VLEN: compare using len_diff/diff */
#define VCMP 2
#define VLEN 3

#define INT_ARRAY_MAX 256

#ifdef SOLARIS_DOORS
# define IS_FILE_TYPE_FILTER(x) ((x) == 'b' || (x) == 'c' || (x) == 'C' \
|| (x) == 'd' || (x) == 'f' || (x) == 'g' || (x) == 'h' || (x) == 'l' \
|| (x) == 'o' || (x) == 'p' || (x) == 's' || (x) == 't' || (x) == 'u' \
|| (x) == 'x' || (x) == 'D' || (x) == 'P')
#else
# define IS_FILE_TYPE_FILTER(x) ((x) == 'b' || (x) == 'c' || (x) == 'C' \
|| (x) == 'd' || (x) == 'f' || (x) == 'g' || (x) == 'h' || (x) == 'l' \
|| (x) == 'o' || (x) == 'p' || (x) == 's' || (x) == 't' || (x) == 'u' \
|| (x) == 'x')
#endif /* SOLARIS_DOORS */

#define IS_GLOB(x, y) (((x) == '*' || (x) == '?' || (x) == '{' ) && (y) != ' ')

#define IS_WORD(x, y) (((x) == '$' && ((y) == '(' || (y) == '{')) \
|| ((x) == '`' && (y) != ' ') || (x) == '~' || (x) == '$')

/* QUOTED_WORDS stores indices of words quoted in the command line so that we
 * can keep track of them and prevent expanding them when spliting the
 * input string (in parse_input_str()). */
static int quoted_words[INT_ARRAY_MAX];
#define QWORDS_ARRAY_LEN (sizeof(quoted_words) / sizeof(int))

/* Quote the string STR according to conf.quoting_style, that is, using either
 * single or double quotes. */
char *
quote_str(const char *str)
{
	if (!str || !*str || conf.quoting_style == QUOTING_STYLE_BACKSLASH)
		return (char *)NULL;

	const size_t len = strlen(str) + 3;
	char *p = xnmalloc(len, sizeof(char));
	char quote_char =
		conf.quoting_style == QUOTING_STYLE_DOUBLE_QUOTES ? '"' : '\'';

	snprintf(p, len, "%c%s%c", quote_char, str, quote_char);

	return p;
}

/* Get the last occurrence of the (non-escaped) character C in STR (whose
 * length is LEN). Return a pointer to it if found or NULL if not. */
char *
get_last_chr(char *str, const char c, const int len)
{
	if (!str || !*str)
		return (char *)NULL;

	int i = len;
	while (--i >= 0) {
		if ((i > 0 && str[i] == c && str[i - 1] != '\\')
		|| (i == 0 && str[i] == c))
			return str + i;
	}

	return (char *)NULL;
}

/* Replace all slashes in STR by the character C. */
char *
replace_slashes(char *str, const char c)
{
	if (!str || !*str)
		return (char *)NULL;

	if (*str == '/')
		str++;

	char *p = savestring(str, strlen(str));
	char *q = p;

	while (q && *q) {
		if (*q == '/' && (q == p || *(q - 1) != '\\'))
			*q = c;
		q++;
	}

	return p;
}

/* Find the character C in the string S ignoring case.
 * Returns a pointer to the matching char in S if C was found, or NULL
 * otherwise. */
char *
xstrcasechr(char *s, char c)
{
	if (!s || !*s)
		return (char *)NULL;

	const char uc = TOUPPER(c);
	while (*s) {
		if (TOUPPER(*s) != uc) {
			s++;
			continue;
		}
		return s;
	}

	return (char *)NULL;
}

/* A reverse strpbrk(3): returns a pointer to the LAST char in S matching
 * a char in ACCEPT, or NULL if no match is found. */
char *
xstrrpbrk(char *s, const char *accept)
{
	if (!s || !*s || !accept || !*accept)
		return (char *)NULL;

	const size_t l = strlen(s);

	int i = (int)l, j;
	while (--i >= 0) {
		for (j = 0; accept[j]; j++) {
			if (s[i] == accept[j])
				return s + i;
		}
	}

	return (char *)NULL;
}

#ifdef _BE_POSIX
/* strcasestr(3) is a not POSIX: let's use this as replacement.
 * Find the first occurrence of the string B in the string A, ignoring case. */
char *
x_strcasestr(char *a, char *b)
{
	if (!a || !b)
		return (char *)NULL;

	size_t f = 0;
	char *p = (char *)NULL, *bb = b;
	while (*a && *b) {
		if (TOUPPER(*a) != TOUPPER(*b)) {
			if (f == 1) {
				b = bb;
				f = 0;
			} else {
				++a;
			}

			continue;
		}

		if (f == 0)
			p = a;
		f = 1;
		++a;
		++b;
	}

	return (!*b && f == 1) ? p : (char *)NULL;
}
#endif /* _BE_POSIX */

/* Modified version of strlcpy(3) using memccpy(3), as suggested here:
 * https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2349.htm
 *
 * Copy at most (N - 1) bytes of SRC into DST and return the number of bytes
 * copied (excluding the terminating null byte), all at once.
 * Note that N must be the total size of the buffer DST, including the
 * terminating NUL byte.
 * Unlike strncpy(3), it always NULL terminates the destination string (DST),
 * even if no NUL char is found in the first N characters of SRC.
 * Returns the number of bytes copied, including the terminating NUL byte. */
size_t
xstrsncpy(char *restrict dst, const char *restrict src, size_t n)
{
	char *end = memccpy(dst, src, '\0', n);
	if (!end) {
		dst[n - 1] = '\0';
		end = dst + n;
	}

	return (size_t)(end - dst);
}

/* A safe strcat(3). Append the string SRC to the buffer DST, always null
 * terminating DST. */
char *
xstrncat(char *restrict dst, const size_t dst_len, const char *restrict src,
	const size_t dst_size)
{
	xstrsncpy(dst + dst_len, src, dst_size - dst_len);
	return dst;
}

/* strverscmp() is a GNU extension, and as such not available on some systems
 * This function is a modified version of the GLIBC and uClibc strverscmp()
 * taken from here:
 * https://elixir.bootlin.com/uclibc-ng/latest/source/libc/string/strverscmp.c
 */

/* Compare S1 and S2 as strings holding indices/version numbers,
 * returning less than, equal to or greater than zero if S1 is less than,
 * equal to or greater than S2 (for more info, see the texinfo doc). */
int
xstrverscmp(const char *s1, const char *s2)
{
	if ((*s1 & 0xc0) == 0xc0 || (*s2 & 0xc0) == 0xc0)
		return strcoll(s1, s2);

	const unsigned char *p1 = (const unsigned char *)s1;
	const unsigned char *p2 = (const unsigned char *)s2;

	/* Jump to first alphanumeric character in both strings */
	while (*p1) {
		if (!IS_DIGIT(*p1) && !IS_ALPHA(*p1) && (*p1 < 'A' || *p1 > 'Z')) {
			++p1;
			continue;
		}
		break;
	}

	if (!*p1)
		p1 = (const unsigned char *)s1;

	while (*p2) {
		if (!IS_DIGIT(*p2) && !IS_ALPHA(*p2) && (*p2 < 'A' || *p2 > 'Z')) {
			++p2;
			continue;
		}
		break;
	}

	if (!*p2)
		p2 = (const unsigned char *)s2;

	/* Symbol(s)    0       [1-9]   others
	 Transition   (10) 0  (01) d  (00) x   */
	static const uint8_t next_state[] = {
	/* state    x    d    0  */
	/* S_N */  S_N, S_I, S_Z,
	/* S_I */  S_N, S_I, S_I,
	/* S_F */  S_N, S_F, S_F,
	/* S_Z */  S_N, S_F, S_Z
	};

	static const int8_t result_type[] __attribute__ ((aligned)) = {
		/* state   x/x  x/d  x/0  d/x  d/d  d/0  0/x  0/d  0/0  */
		/* S_N */  VCMP, VCMP, VCMP, VCMP, VLEN, VCMP, VCMP, VCMP, VCMP,
		/* S_I */  VCMP,   -1,   -1,    1, VLEN, VLEN,    1, VLEN, VLEN,
		/* S_F */  VCMP, VCMP, VCMP, VCMP, VCMP, VCMP, VCMP, VCMP, VCMP,
		/* S_Z */  VCMP,    1,    1,   -1, VCMP, VCMP,   -1, VCMP, VCMP
	};

	unsigned char c1, c2;
	int state, diff;

	if (p1 == p2)
		return 0;

	if (!conf.case_sens_list) {
		c1 = TOUPPER(*p1);
		++p1;
		c2 = TOUPPER(*p2);
		++p2;
	} else {
		c1 = *p1;
		c2 = *p2;
		++p1;
		++p2;
	}

	/* Hint: '0' is a digit too */
	state = S_N + ((c1 == '0') + (IS_DIGIT(c1) != 0));

	while ((diff = c1 - c2) == 0) {
		if (c1 == '\0')
			return diff;

		state = next_state[state];
		if (!conf.case_sens_list) {
			c1 = TOUPPER(*p1);
			++p1;
			c2 = TOUPPER(*p2);
			++p2;
		} else {
			c1 = *p1;
			c2 = *p2;
			++p1;
			++p2;
		}
		state += (c1 == '0') + (IS_DIGIT(c1) != 0);
	}

	state = result_type[state * 3 + (((c2 == '0') + (IS_DIGIT(c2) != 0)))];

	switch (state) {
	case VCMP: return diff;
	case VLEN:
		while (*p1 && IS_DIGIT(*p1)) {
			++p1;
			if (!*p2 || !IS_DIGIT(*p2))
				return 1;
			++p2;
		}
		return (*p2 && IS_DIGIT(*p2)) ? -1 : diff;

	default: return state;
	}
}

/* A strlen implementation able to handle wide chars.
 * Returns the number of columns needed to print the string STR (instead
 * of the number of bytes needed to store STR). */
size_t
wc_xstrlen(const char *restrict str)
{
	/* Convert multi-byte to wide char */
	/* Most of the time we use this function to get the number of characters
	 * in names: in this case a buffer of NAME_MAX + 1 is enough. However, we
	 * sometimes use this function to get the number of characters in the
	 * current command line (rl_line_buffer), which has no size limit beyond
	 * what an int can hold (INT_MAX), which is huge. ARG_MAX (128k or more),
	 * though much smaller than INT_MAX, should be enough most of the time.
	 * Even PATH_MAX, usually 4096, is still too narrow to hold a complete
	 * command line: a history entry involving several paths might easily be
	 * longer than PATH_MAX. */
	static wchar_t wbuf[ARG_MAX];
	const size_t len = mbstowcs(wbuf, str, ARG_MAX);
	if (len == (size_t)-1) /* Invalid multi-byte sequence found */
		return 0;

	const int width = wcswidth(wbuf, len);
	if (width != -1)
		return (size_t)width;

	/* A non-printable wide char was found */
	return 0;
}

/* Truncate an UTF-8 string at width MAX.
 * Returns the difference beetween MAX and the point at which STR was actually
 * trimmed (this difference should be added to STR as spaces to equate MAX
 * and get a correct length).
 * Since a wide char could take two o more columns to be drawn, and since
 * you might want to trim the name in the middle of a wide char, this
 * function won't store the last wide char to avoid taking more columns
 * than MAX. In this case, the programmer should take care of filling the
 * empty spaces (usually no more than one) theyself. */
int
u8truncstr(char *restrict str, const size_t max)
{
	int len = 0;
	static wchar_t buf[NAME_MAX + 1];
	*buf = L'\0';
	if (mbstowcs(buf, str, NAME_MAX + 1) == (size_t)-1)
		return 0;

	int i, bmax = (int)max;
	if (bmax < 0)
		bmax = conf.max_name_len;

	for (i = 0; buf[i]; i++) {
		int l = wcwidth(buf[i]);
		if (len + l > bmax) {
			buf[i] = L'\0';
			break;
		}
		len += l;
	}

	wcsncpy((wchar_t *)str, buf, (size_t)bmax * sizeof(wchar_t)); /* flawfinder: ignore */

	return bmax - len;
}

/* Replace invalid characters in NAME by INVALID_CHAR ('^').
 * This function is called only if wc_xstrlen() returns zero, in which case
 * we have either a non-printable char or an invalid multi-byte sequence. */
char *
replace_invalid_chars(const char *name)
{
	size_t len = strlen(name);
	char *n = xnmalloc(len + 1, sizeof(char));
	char *p = n;

	mbstate_t mbstate = {0};
	char *q = (char *)name;
	char *qlimit = q + len;

	while (*q) {
		if (*q >= ' ' && *q < 127) { /* Printable ASCII char */
			*n = *q;
			n++; q++;
			continue;
		}

#if defined(CHAR_MIN) && CHAR_MIN < 0
		if (*q >= '\0' && *q < ' ') { /* Control char */
#else
		if (*q < ' ') {
#endif /* CHAR_MIN < 0 */
			*n = INVALID_CHR;
			n++; q++;
			continue;
		}

		do {
			wchar_t wc;
			size_t bytes = mbrtowc(&wc, q, (size_t)(qlimit - q), &mbstate);
			if (bytes == (size_t)-1 || bytes == (size_t)-2) {
				*n = INVALID_CHR; /* Invalid UTF-8 */
				n++; q++;
			} else { /* Valid UTF-8 */
				for (; bytes > 0; --bytes) {
					*n = *q;
					n++; q++;
				}
			}

			if (!*q)
				break;
		} while (mbsinit(&mbstate) == 0);
	}

	*n = '\0';

	return p;
}

/* Returns the index of the first appearance of c in str, if any, and
 * -1 if c was not found or if no str. NOTE: Same thing as strchr(),
 * except that returns an index, not a pointer. */
int
strcntchr(const char *str, const char c)
{
	if (!str)
		return (-1);

	int i = 0;

	while (*str) {
		if (*str == c)
			return i;
		i++;
		str++;
	}

	return (-1);
}

/* Returns the index of the last appearance of C in STR, if any, and
 * -1 if C was not found or STR is NULL. */
int
strcntchrlst(const char *str, const char c)
{
	if (!str)
		return (-1);

	int i = 0;

	int p = -1;
	while (*str) {
		if (*str == c)
			p = i;
		i++;
		str++;
	}

	return p;
}

/* Get substring in STR before the last appearance of C. Returns
 * substring  if C is found and NULL if not (or if C was the first
 * char in STR). */
char *
strbfrlst(char *str, const char c)
{
	if (!str || !*str || !c)
		return (char *)NULL;

	char *p = str, *q = (char *)NULL;
	while (*p) {
		if (*p == c)
			q = p;
		p++;
	}

	if (!q || q == str)
		return (char *)NULL;

	*q = '\0';

	const size_t buf_len = (size_t)(q - str);
	char *buf = malloc(buf_len + 1);
	if (!buf) {
		*q = c;
		return (char *)NULL;
	}

	xstrsncpy(buf, str, buf_len + 1);
	*q = c;
	return buf;
}

/* Returns the string between first ocurrence of A and the first
 * ocurrence of B in STR, or NULL if: there is nothing between A and
 * B, or A and/or B are not found. */
char *
strbtw(char *str, const char a, const char b)
{
	if (!str || !*str || !a || !b)
		return (char *)NULL;

	char *p = str, *pa = (char *)NULL, *pb = (char *)NULL;
	while (*p) {
		if (!pa) {
			if (*p == a)
				pa = p;
		} else if (*p == b) {
			pb = p;
			break;
		}
		p++;
	}

	if (!pb)
		return (char *)NULL;

	*pb = '\0';

	const size_t buf_len = (size_t)(pb - pa);
	char *buf = malloc(buf_len + 1);

	if (!buf) {
		*pb = b;
		return (char *)NULL;
	}

	xstrsncpy(buf, pa + 1, buf_len + 1);
	*pb = b;
	return buf;
}

/* Replace the first occurrence of NEEDLE in HAYSTACK by REP. */
char *
replace_substr(const char *haystack, const char *needle, char *rep)
{
	if (!haystack || !*haystack || !needle || !*needle || !rep)
		return (char *)NULL;

	char *ret = strstr(haystack, needle);
	if (!ret)
		return (char *)NULL;

	char *needle_end = ret + strlen(needle);
	*ret = '\0';

	size_t new_str_len = 0;

	if (*needle_end) {
		const size_t rem_len = strlen(needle_end);
		char *rem = xnmalloc(rem_len + 1, sizeof(char));
		xstrsncpy(rem, needle_end, rem_len + 1);

		new_str_len = strlen(haystack) + strlen(rep) + rem_len + 1;
		char *new_str = xnmalloc(new_str_len, sizeof(char));
		snprintf(new_str, new_str_len, "%s%s%s", haystack, rep, rem);
		free(rem);
		return new_str;
	}

	new_str_len = strlen(haystack) + strlen(rep) + 1;
	char *new_str = xnmalloc(new_str_len, sizeof(char));
	snprintf(new_str, new_str_len, "%s%s", haystack, rep);

	return new_str;
}

/* Generate a random string of LEN bytes using characters from CHARSET. */
char *
gen_rand_str(const size_t len)
{
	static const char charset[] = "0123456789#%-_"
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

#ifndef HAVE_ARC4RANDOM
	srandom((unsigned int)time(NULL));
#endif /* !HAVE_ARC4RANDOM */

	char *p = xnmalloc(len + 1, sizeof(char));
	char *str = p;

	int x = (int)len;
	while (x--) {
#ifndef HAVE_ARC4RANDOM
		const long i = random() % (int)(sizeof(charset) - 1);
#else
		const uint32_t i = arc4random_uniform(sizeof(charset) - 1);
#endif /* !HAVE_ARC4RANDOM */
		*p = charset[i];
		p++;
	}

	*p = '\0';
	return str;
}

/* Removes end of line char and quotes (single and double) from STR.
 * Returns a pointer to the modified STR if the result is non-blank,
 * or NULL. */
char *
remove_quotes(char *str)
{
	if (!str || !*str)
		return (char *)NULL;

	char *p = str;
	size_t len = strlen(p);

	if (len > 0 && p[len - 1] == '\n') {
		p[len - 1] = '\0';
		len--;
	}

	if (len > 0 && (p[len - 1] == '\'' || p[len - 1] == '"'))
		p[len - 1] = '\0';

	if (*p == '\'' || *p == '"')
		p++;

	if (!*p)
		return (char *)NULL;

	char *q = p;
	int blank = 1;

	while (*q) {
		if (*q != ' ' && *q != '\n' && *q != '\t') {
			blank = 0;
			break;
		}
		q++;
	}

	if (blank == 0)
		return p;

	return (char *)NULL;
}

/* Set all slots in the QUOTED_WORDS array to -1 to mark uninitialized slots.
 * QUOTED_WORDS is used to keep track (by array index) of all quoted words
 * in the command line. */
static void
init_quoted_words(void)
{
	size_t i;
	for (i = 0; i < QWORDS_ARRAY_LEN; i++)
		quoted_words[i] = -1;
}

/* After expanding multiple fields from an expandable expression, say 'sel',
 * we lost track of quoted words, because the old indices point now to
 * expanded fields.
 * Ex: 'p sel "12"' -> the quoted word index is originally 2, but after the
 * expansion of the 'sel' keyword, the index 2 points now to a selected file.
 *
 * This function updates quoted word indices taking into account the starting
 * point, i.e. the word after the expandable expression (START), and the amount
 * of actually expanded fields (N). */
static void
update_quoted_words_index(const size_t start, const size_t added_items)
{
	const size_t s = start + 1;
	const size_t n = added_items - (added_items > 0 ? 1 : 0);

	size_t i;
	for (i = 0; i < QWORDS_ARRAY_LEN; i++)
		if (quoted_words[i] > -1 && (size_t)quoted_words[i] >= s)
			quoted_words[i] += (int)n;
}

/* Return 1 if the word at index INDEX is quoted, or zero otherwise. */
static int
is_quoted_word(const size_t index)
{
	size_t i;
	for (i = 0; i < QWORDS_ARRAY_LEN; i++)
		if (index == (size_t)quoted_words[i])
			return 1;

	return 0;
}

/* Some commands need quotes to be preserved (they'll handle quotes themselves
 * later). Return 1 if true or 0 otherwise. */
int
cmd_keeps_quotes(char *str)
{
	if (rl_dispatching == 1)
		return 0;

	if (flags & IN_BOOKMARKS_SCREEN)
		return 0;

	if (*str == '\'' || *str == '"') /* Quoted first word */
		return 0;

	char *p = strchr(str, ' ');
	if (p) {
		*p = '\0';
		const int ret = is_internal_c(str);
		*p = ' ';
		/* Let's keep quotes for external commands */
		if (ret == 0)
			return 1;
	}

	return (strncmp(str, "ft ", 3) == 0 || strncmp(str, "filter ", 7) == 0);
}

/* This function takes a string as argument and splits it into substrings
 * taking tab, new line char, and space as word delimiters, except when
 * they are preceded by a quote char (single or double quotes) or in
 * case of command substitution ($(cmd) or `cmd`), in which case
 * eveything before the corresponding closing char is taken as a single
 * string. It also escapes special chars. It returns an array of
 * split strings (without leading and terminating spaces) or NULL if
 * str is NULL or if no substring was found, i.e., if str contains
 * only spaces. */
char **
split_str(char *str, const int update_args)
{
	if (!str)
		return (char **)NULL;

	init_quoted_words();

	size_t buf_len = 0, words = 0, str_len = 0;
	char *buf = xnmalloc(1, sizeof(char));
	int close = 0;
	char **substr = (char **)NULL;

	int keep_quotes = cmd_keeps_quotes(str);

	while (*str) {
		switch (*str) {
		/* Command substitution */
		case '$': /* fallthrough */
		case '`':
			/* Define the closing char: If "$(" then ')', else '`' */
			if (*str == '$') {
				/* If escaped, it has no special meaning */
				if ((str_len && *(str - 1) == '\\') || *(str + 1) != '(') {
					buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
					buf[buf_len] = *str;
					buf_len++;
					break;
				} else {
					close = ')';
				}
			} else {
				/* If escaped, it has no special meaning */
				if (str_len && *(str - 1) == '\\') {
					buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
					buf[buf_len] = *str;
					buf_len++;
					break;
				} else {
					/* If '`' advance one char. Otherwise the while
					 * below will stop at first char, which is not
					 * what we want */
					close = *str;
					str++;
					buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
					buf[buf_len] = '`';
					buf_len++;
				}
			}

			/* Copy everything until null byte or closing char */
			while (*str && *str != close) {
				buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
				buf[buf_len] = *str;
				buf_len++;
				str++;
			}

			/* If the while loop stopped with a null byte, there was
			 * no ending close (either ')' or '`')*/
			if (!*str) {
				xerror(_("%s: Missing '%c'\n"), PROGRAM_NAME, close);
				free(buf);
				buf = (char *)NULL;
				int i = (int)words;

				while (--i >= 0)
					free(substr[i]);
				free(substr);

				return (char **)NULL;
			}

			/* Copy the closing char and add an space: this function
			 * takes space as word breaking char, so that everything
			 * in the buffer will be copied as one single word */
			buf = xnrealloc(buf, buf_len + 2, sizeof(char *));
			buf[buf_len] = *str;
			buf_len++;
			buf[buf_len] = ' ';

			break;

		case '\'': /* fallthrough */
		case '"': {
			const int is_quoted = (keep_quotes == 1
				|| (str_len > 0 && *(str - 1) == '\\'));
			/* If the quote is escaped, keep it. */
			if (is_quoted == 1) {
				buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
				buf[buf_len] = *str;
				buf_len++;
			}

			const char quote = *str; /* A copy of the opening quote. */
			str++; /* Move on to the next char. */

			/* Copy into the buffer whatever is after the first quote up
			 * to the last quote or NULL. */
			while (*str && *str != quote) {
				/* If char has special meaning, escape it. */
				if (!(flags & IN_BOOKMARKS_SCREEN) && is_quoted == 0
				&& (is_quote_char(*str) || *str == '.')) {
					/* Escape '.' to prevent realpath expansions. */
					buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
					buf[buf_len] = '\\';
					buf_len++;
				}

				buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
				buf[buf_len] = *str;
				buf_len++;
				str++;
			}

			/* The above while breaks with NULL or quote, so that if
			 * *STR is a null byte there was not ending quote. */
			if (!*str) {
				xerror(_("%s: Missing closing quote: '%c'\n"),
					PROGRAM_NAME, quote);
				/* Free stuff and return. */
				free(buf);
				buf = (char *)NULL;
				int i = (int)words;

				while (--i >= 0)
					free(substr[i]);
				free(substr);
				return (char **)NULL;
			}

			if (is_quoted == 1) { /* Add closing quote. */
				buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
				buf[buf_len] = (char)quote;
				buf_len++;
			}

			/* If coming from parse_input_str (main command line), mark
			 * quoted words: no expansion will be made on these words. */
			if (update_args == 1 && words < QWORDS_ARRAY_LEN)
				quoted_words[words] = (int)words;
			}

			break;

		/* TAB, new line char, and space are taken as word breaking characters */
		case '\t': /* fallthrough */
		case '\n': /* fallthrough */
		case ' ':
			/* If escaped, just copy it into the buffer */
			if (str_len && *(str - 1) == '\\') {
				buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
				buf[buf_len] = *str;
				buf_len++;
			} else {
				/* If not escaped, break the string */
				/* Add a terminating null byte to the buffer, and, if not empty,
				 * dump the buffer into the substrings array */
				buf[buf_len] = '\0';

				if (buf_len > 0) {
					substr = xnrealloc(substr, words + 1, sizeof(char *));
					substr[words] = savestring(buf, buf_len);
					words++;
				}

				/* Clear the buffer to get a new string */
				memset(buf, '\0', buf_len);
				buf_len = 0;
			}
			break;

		/* If neither a quote nor a breaking word char nor command substitution,
		 * just dump it into the buffer. */
		default:
			if (*str == '\\' && (flags & IN_BOOKMARKS_SCREEN))
				break;
			buf = xnrealloc(buf, buf_len + 1, sizeof(char *));
			buf[buf_len] = *str;
			buf_len++;
			break;
		}

		str++;
		str_len++;
	}

	/* The while loop stops when the null byte is reached, so that the last
	 * substring is not printed, but still stored in the buffer. Therefore,
	 * we need to add it, if not empty, to our substrings array */
	buf[buf_len] = '\0';

	if (buf_len > 0) {
		if (words == 0)
			substr = xcalloc(words + 1, sizeof(char *));
		else
			substr = xnrealloc(substr, words + 1, sizeof(char *));

		substr[words] = savestring(buf, buf_len);
		words++;
	}

	free(buf);
	buf = (char *)NULL;

	if (words > 0) {
		/* Add a final null string to the array */
		substr = xnrealloc(substr, words + 1, sizeof(char *));
		substr[words] = (char *)NULL;

		if (update_args == 1)
			args_n = words - 1;
		return substr;
	} else {
		if (update_args == 1)
			args_n = 0; /* Just in case, but I think it's not needed */
		return (char **)NULL;
	}
}

/* Return 1 if STR contains only numbers or a range of numbers, or 0 if not. */
static int
check_fused_param(char *str)
{
	char *p = str;
	size_t c = 0, i = 0;
	int ok = 1;

	while (*p) {
		if (i > 0 && *p == '-' && *(p - 1) >= '1' && *(p - 1) <= '9') {
			c++;
		} else if (*p == ' ') {
			break;
		} else if (*p < '0' || *p > '9') {
			ok = 0;
			break;
		}
		p++;
		i++;
	}

	return (ok == 1 && c <= 1);
}

static char *
split_fused_param(char *str)
{
	if (!str || !*str || *str == ';' || *str == ':' || *str == '\\')
		return (char *)NULL;

	const char *space = strchr(str, ' ');
	const char *slash = strchr(str, '/');

	if (!space && slash) /* If "/some/path/" */
		return (char *)NULL;

	if (space && slash && slash < space) /* If "/some/string something" */
		return (char *)NULL;

	/* The buffer size is the double of STR, just in case each subtr
	 * needs to be splitted. */
	char *buf = xnmalloc(((strlen(str) * 2) + 2), sizeof(char));

	size_t c = 0; /* Bytes counter */
	char *p = str, *pp = str; /* Pointers to original string */
	char *b = buf; /* Pointer to our buffer */
	size_t words = 1; /* Number of words in str */

	while (*p) {
		switch (*p) {
		case ' ': /* We only allow splitting the first command word */
			if (c && *(p - 1) != ' ' && *(p - 1) != '|'
			&& *(p - 1) != '&' && *(p - 1) != ';')
				words++;
			if (*(p + 1))
				pp = p + 1;
			break;
		case '&': /* fallthrough */
		case '|': /* fallthrough */
		case ';':
			words = 1;
			if (*(p + 1))
				pp = p + 1;
			break;
		default: break;
		}

		if (words == 1 && c && *p >= '1' && *p <= '9'
		&& (*(p - 1) < '0' || *(p - 1) > '9')
		&& check_fused_param(p)) {
			char t = *p;
			*p = '\0';
			if (is_internal_f(pp)) {
				*b = ' ';
				b++;
			}
			*p = t;
		}

		*b = *p;
		b++; p++;
		c++;
	}

	*b = '\0';

	/* Readjust the buffer size */
	const size_t len = strlen(buf);
	buf = xnrealloc(buf, len + 1, sizeof(char));
	return buf;
}

static int
check_shell_functions(const char *str)
{
	if (!str || !*str)
		return 0;

	if (int_vars == 0) { /* Take assignements as shell functions */
		const char *s = strchr(str, ' ');
		const char *e = strchr(str, '=');
		if (!s && e)
			return 1;
		if (s && e && e < s)
			return 1;
	}

/*	char **b = (char **)NULL;

	switch (shell) {
	case SHELL_NONE: return 0;
	case SHELL_BASH: b = bash_builtins; break;
	case SHELL_DASH: b = dash_builtins; break;
	case SHELL_KSH: b = ksh_builtins; break;
	case SHELL_TCSH: b = tcsh_builtins; break;
	case SHELL_ZSH: b = zsh_builtins; break;
	default: return 0;
	} */

	const char *const funcs[] = {
		"for ", "for(",
		"do ", "do(",
		"while ", "while(",
		"until ", "until(",
		"if ", "if(",
		"[ ", "[[ ", "test ",
		"case ", "case(",
		"echo ", "printf ",
		"declare ",
		"(( ",
		"set ",
		"source ", ". ",
		NULL
	};

	size_t i;
	for (i = 0; funcs[i]; i++) {
		const size_t f_len = strlen(funcs[i]);
		if (*str == *funcs[i] && strncmp(str, funcs[i], f_len) == 0)
			return 1;
	}

	return 0;
}

/* Check whether STR is an internal command with a fused parameter (CMDNUMBER).
 * Returns FUNC_SUCCESS if true or FUNC_FAILURE otherwise. */
static int
is_fused_param(char *str)
{
	if (!str || !*str || !*(str + 1))
		return FUNC_FAILURE;

	char *p = str, *q = (char *)NULL;
	int d = 0;

	while (*p && *p != ' ') {
		if (d == 0 && p != str && IS_DIGIT(*p) && IS_ALPHA(*(p - 1))) {
			q = p;
			d = 1;
		}
		if (d == 1 && IS_ALPHA(*p))
			return FUNC_FAILURE;
		p++;
	}

	if (d == 0)
		return FUNC_FAILURE;
	if (!q)
		return FUNC_FAILURE;

	char c = *q;
	*q = '\0';

	const int ret = is_internal_f(str);
	*q = c;

	if (ret == 0)
		return FUNC_FAILURE;

	return FUNC_SUCCESS;
}

#ifndef _NO_TAGS
/* Expand "t:TAG" into the corresponding tagged files.
 * ARGS is an array with the current input substrings, and TAG_INDEX
 * is the index of the tag expresion (t:) in this array.
 * The expansion is performed in ARGS array itself.
 * Returns the number of files tagged as ARGS[TAG_INDEX] or zero on error. */
static size_t
expand_tag(char ***args, const int tag_index)
{
	char **s = *args;
	if (!s)
		return 0;

	char *tag = (s[tag_index] && *(s[tag_index] + 1) && *(s[tag_index] + 2))
		? s[tag_index] + 2 : (char *)NULL;
	if (!tag || !*tag || !tags_dir || is_tag(tag) == 0)
		return 0;

	char dir[PATH_MAX + 1];
	snprintf(dir, sizeof(dir), "%s/%s", tags_dir, tag);

	struct dirent **t = (struct dirent **)NULL;
	const int n = scandir(dir, &t, NULL, conf.case_sens_list
		? xalphasort : alphasort_insensitive);
	if (n == -1)
		return 0;

	size_t i, j = 0;
	if (n <= 2) { /* Empty dir: only self and parent */
		for (i = 0; i < (size_t)n; i++)
			free(t[i]);
		free(t);
		return 0;
	}

	const size_t len = args_n + 1 + ((size_t)n - 2) + 1;
	char **p = xnmalloc(len, sizeof(char *));

	/* Copy whatever is before the tag expression */
	for (i = 0; i < (size_t)tag_index; i++) {
		p[j] = savestring(s[i], strlen(s[i]));
		j++;
	}
	p[j] = (char *)NULL;

	/* Append the file names pointed to by the tag expression */
	for (i = 0; i < (size_t)n; i++) {
		if (SELFORPARENT(t[i]->d_name))
			continue;

		char filename[PATH_MAX + NAME_MAX + 2];
		snprintf(filename, sizeof(filename), "%s/%s", dir, t[i]->d_name);

		char rpath[PATH_MAX + 1];
		*rpath = '\0';
		char *ret = realpath(filename, rpath);
		if (!ret || !*rpath) {
			/* This tagged file points to a non-existent file. Just copy
			 * the tag path. */
			xstrsncpy(rpath, filename, sizeof(rpath));
		}

		char *esc_str = escape_str(rpath);
		char *q = esc_str ? esc_str : rpath;
		p[j] = savestring(q, strlen(q));
		j++;
		free(esc_str);
	}
	p[j] = (char *)NULL;

	/* Append whatever is after the tag expression */
	for (i = (size_t)tag_index + 1; i <= args_n; i++) {
		if (!s[i])
			continue;
		p[j] = savestring(s[i], strlen(s[i]));
		j++;
	}
	p[j] = (char *)NULL;

	/* Free the dirent struct */
	for (i = 0; i < (size_t)n; i++)
		free(t[i]);
	free(t);

	/* Free the original array (ARGS) and make it point to the new
	 * array with the expanded tag expression */
	for (i = 0; i <= args_n; i++)
		free(s[i]);
	free(s);
	/* ARGS will be later free'd by run_main_loop(). This is why the leak
	 * warning emitted by GCC(12.1) analyzer is a false positive */
	*args = p;

	args_n = (j > 0) ? j - 1 : 0;
	/* Do not count self and parent dirs */
	return (size_t)n - 2;
}

static void
expand_tags(char ***substr)
{
	size_t ntags = 0, i;
	int *tag_index = (int *)NULL;
	struct stat a;

	for (i = 0; (*substr)[i]; i++) {
		if (*(*substr)[i] == 't' && *((*substr)[i] + 1) == ':'
		&& lstat((*substr)[i], &a) == -1) {
			tag_index = xnrealloc(tag_index, ntags + 2, sizeof(int));
			tag_index[ntags] = (int)i;
			ntags++;
			tag_index[ntags] = -1;
		}
	}

	if (ntags == 0)
		return;

	for (i = 0; i < ntags; i++) {
		const size_t tn = expand_tag(substr, tag_index[i]);
		/* TN is the amount of files tagged as SUBSTR[TAG_INDEX[I]]
		 * Let's update the index of the next tag expression using this
		 * value: if the next tag expression was at index 2, and if
		 * the current tag expression was expanded to 3 files,
		 * the new index of the next tag expression is 4 (the space
		 * occupied by the first expanded file is the same used
		 * by the current tag expression, so that it doesn't count) */
		if (tn > 0 && tag_index[i + 1] != -1)
			tag_index[i + 1] += ((int)tn - 1);
	}

	free(tag_index);
}
#endif /* NO_TAGS */

#ifndef _NO_MAGIC
static char **
expand_mime_type_filter(const char *pattern)
{
	if (!pattern || !*pattern)
		return (char **)NULL;

	char **t = xnmalloc((size_t)files + 1, sizeof(char *));
	char buf[PATH_MAX + 1];

	filesn_t i, n = 0;
	for (i = 0; i < files; i++) {
		char *name = file_info[i].name;
		if (virtual_dir == 1) {
			*buf = '\0';
			if (xreadlink(XAT_FDCWD, file_info[i].name, buf, sizeof(buf)) == -1
			|| !*buf)
				continue;
			name = buf;
		}

		char *m = (name && *name) ? xmagic(name, MIME_TYPE) : (char *)NULL;
		if (!m) continue;

		char *p = strstr(m, pattern);
		free(m);

		if (!p) continue;

		t[n] = savestring(name, strlen(name));
		n++;
	}

	t[n] = (char *)NULL;

	if (n == 0)
		{ free(t); return (char **)NULL; }

	t = xnrealloc(t, (size_t)n + 1, sizeof(char *));
	return t;
}
#endif /* !_NO_MAGIC */

static char **
expand_file_type_filter(const char t)
{
	if (files == 0)
		return (char **)NULL;

	filesn_t i = 0, n = 0;

	char **f = xnmalloc((size_t)files + 1, sizeof(char *));
	char buf[PATH_MAX + 1];

	while (i < files) {
		char *name = file_info[i].name;
		if (virtual_dir == 1) {
			*buf = '\0';
			if (xreadlink(XAT_FDCWD, file_info[i].name, buf, sizeof(buf)) == -1
			|| !*buf)
				continue;
			name = buf;
		}

		if (!name || !*name)
			continue;

		switch (t) {
		case 'b':
			if (file_info[i].type == DT_BLK)
				f[n++] = strdup(name);
			break;
		case 'c':
			if (file_info[i].type == DT_CHR)
				f[n++] = strdup(name);
			break;
		case 'C':
			if (file_info[i].color == ca_c)
				f[n++] = strdup(name);
			break;
		case 'd':
			if (file_info[i].dir == 1)
				f[n++] = strdup(name);
			break;
#ifdef SOLARIS_DOORS
		case 'D':
			if (file_info[i].type == DT_DOOR)
				f[n++] = strdup(name);
			break;
		case 'P':
			if (file_info[i].type == DT_PORT)
				f[n++] = strdup(name);
			break;
#endif /* SOLARIS_DOORS */
		case 'f':
			if (file_info[i].type == DT_REG)
				f[n++] = strdup(name);
			break;
		case 'h':
			if (file_info[i].dir == 0 && file_info[i].linkn > 1)
				f[n++] = strdup(name);
			break;
		case 'l':
			if (file_info[i].type == DT_LNK)
				f[n++] = strdup(name);
			break;

		case 'o':
			if (file_info[i].color == tw_c || file_info[i].color == ow_c)
				f[n++] = strdup(name);
			break;
		case 't':
			if (file_info[i].color == tw_c || file_info[i].color == st_c)
				f[n++] = strdup(name);
			break;

		case 'p':
			if (file_info[i].type == DT_FIFO)
				f[n++] = strdup(name);
			break;
		case 's':
			if (file_info[i].type == DT_SOCK)
				f[n++] = strdup(name);
			break;
		case 'x':
			if (file_info[i].exec == 1)
				f[n++] = strdup(name);
			break;
		case 'u':
			if (file_info[i].color == su_c)
				f[n++] = strdup(name);
			break;
		case 'g':
			if (file_info[i].color == sg_c)
				f[n++] = strdup(name);
			break;
		default: break;
		}

		i++;
	}

	if (n == 0) {
		free(f);
		return (char **)NULL;
	}

	f[n] = (char *)NULL;
	f = xnrealloc(f, (size_t)n + 1, sizeof(char *));

	return f;
}

static char **
get_bm_paths(void)
{
	if (bm_n == 0)
		return (char **)NULL;

	char **b = xnmalloc(bm_n + 1, sizeof(char *));

	size_t i;
	for (i = 0; i < bm_n && bookmarks[i].path; i++)
		b[i] = bookmarks[i].path;

	b[i] = (char *)NULL;
	return b;
}

/* Reconstruct the array DST inserting all fields in the array SRC at
 * index I in DST. NUM is updated to the number of inserted fields. */
static char **
insert_fields(char ***dst, char ***src, const size_t i, size_t *num)
{
	if (!*dst || !*src)
		return (char **)NULL;

	char **s = *src;

	/* 1. Get amount of fields in SRC */
	size_t sn;
	for (sn = 0; s[sn]; sn++);

	if (sn == 0)
		return (char **)NULL;

	update_quoted_words_index(i, sn);

	/* 2. Store fields in DST after the field to be expanded (I) */
	char **tail = args_n > i /* Substraction must be bigger than zero */
		? xnmalloc(args_n - i + 1, sizeof(char *)) : (char **)NULL;

	size_t t, n = 0;
	if (tail) {
		for (t = i + 1; (*dst)[t]; t++)
			tail[n++] = strdup((*dst)[t]);
		tail[n] = (char *)NULL;
	}

	/* 3. Append SRC fields, plus TAIL fields, to DST */
	char **d = xnmalloc(args_n + sn + 1, sizeof(char *));

	size_t c;
	for (c = 0; c < i; c++)
		d[c] = strdup((*dst)[c]);

	for (n = 0; s[n]; n++) {
		char *p = escape_str(s[n]);
		d[c++] = strdup(p ? p : s[n]);
		free(p);
	}

	if (tail) {
		for (n = 0; tail[n]; n++) {
			d[c++] = strdup(tail[n]);
			free(tail[n]);
		}
		free(tail);
	}

	*num = sn;
	d[c] = (char *)NULL;
	return d;
}

/* Expand the ELN at SUBSTR[I] into the corresponding file name, which is
 * quoted, if necessary, according to the value of conf.quoting_style. */
static void
eln_expand(char ***substr, const size_t i)
{
	const filesn_t num = xatof((*substr)[i]);
	if (num == -1)
		return;

	/* Because of should_expand_eln(), which is called immediately before this
	 * function, it is guaranteed that NUM won't over/under-flow:
	 * NUM is > 0 and <= the amount of listed files (and this latter is
	 * never bigger than FILESN_MAX). */
	const filesn_t j = num - 1;

	/* If file name starts with a dash, and the command is external,
	 * use the absolute path to the file name, to prevent the command from
	 * taking the file name as a command option. */
	char *abs_path = (char *)NULL;
	if (file_info[j].name && *file_info[j].name == '-'
	&& !is_internal_c((*substr)[0]))
		abs_path = realpath(file_info[j].name, NULL);

	char *esc_str = (char *)NULL;
	if (conf.quoting_style == QUOTING_STYLE_BACKSLASH
	|| is_internal_c((*substr)[0]) || is_number((*substr)[0]))
		esc_str = escape_str(abs_path ? abs_path : file_info[j].name);
	else
		esc_str = quote_str(abs_path ? abs_path : file_info[j].name);

	free(abs_path);

	if (!esc_str)
		return;

	if (i == 0)
		flags |= FIRST_WORD_IS_ELN;

	/* Replace the ELN by the corresponding escaped file name */
	if (file_info[j].type == DT_DIR && file_info[j].name[file_info[j].len > 0
	? file_info[j].len - 1 : 0] != '/') {
		const size_t len = strlen(esc_str) + 2;
		(*substr)[i] = xnrealloc((*substr)[i], len, sizeof(char));
		snprintf((*substr)[i], len, "%s/", esc_str);
		free(esc_str);
	} else {
		free((*substr)[i]);
		(*substr)[i] = esc_str;
	}
}

static void
expand_sel(char ***substr)
{
	size_t i = 0;

	if (sel_n == 0)
		return;

	size_t j = 0;
	char **sel_array = xnmalloc(args_n + sel_n + 2, sizeof(char *));

	/* 1. Copy all words before 'sel' */
	for (i = 0; i < (size_t)is_sel; i++) {
		if (!(*substr)[i])
			continue;
		sel_array[j] = savestring((*substr)[i], strlen((*substr)[i]));
		j++;
	}

	update_quoted_words_index((size_t)is_sel, sel_n);

	/* 2. Add all selected files (in place of 'sel') */
	for (i = 0; i < sel_n; i++) {
		/* Escape selected file names and copy them into tmp array */
		char *esc_str = escape_str(sel_elements[i].name);
		if (!esc_str)
			continue;

		sel_array[j] = esc_str;
		j++;
	}

	/* 3. Add words after 'sel' as well */
	for (i = (size_t)is_sel + 1; i <= args_n; i++) {
		sel_array[j] = savestring((*substr)[i], strlen((*substr)[i]));
		j++;
	}

	sel_array[j] = (char *)NULL;

	/* 4. Free the original input string and replace by the new sel_array */
	for (i = 0; i <= args_n; i++)
		free((*substr)[i]);
	free(*substr);

	(*substr) = sel_array;

	args_n = j - 1;
}

/* Expand the 'sel' keyword (or 's:') in SUBSTR to all selected files */
static void
expand_sel_keyword(char ***substr)
{
	if (!(*substr) || !(*substr)[0])
		return;

	struct stat a;
	size_t i;

	for (i = 1; (*substr)[i]; i++) {
		if (*(*substr)[i] != 's')
			continue;

		if ( ( ((*substr)[i][1] == ':' && !(*substr)[i][2])
		|| strcmp((*substr)[i], "sel") == 0)
		&& lstat((*substr)[i], &a) == -1) {
			is_sel = (int)i;
			if ((size_t)is_sel == args_n)
				sel_is_last = 1;

			expand_sel(substr);
		}
	}

	/* If "sel" is last, and there are selected elements, and the command
	 * is either "cp" or "mv", emulate "c" or "m" respectivelly: later,
	 * and final "." will be added to the command, so that we avoid
	 * overwriting files. */
	if (sel_n == 0 || is_sel == 0 || sel_is_last == 0)
		return;

	if (strcmp((*substr)[0], "cp") == 0 || strcmp((*substr)[0], "mv") == 0)
		(*substr)[0][1] = '\0';
}

/* Expand the bookmark NAME into the corresponding bookmark path.
 * Returns FUNC_SUCCESS if the expansion took place or FUNC_FAILURE otherwise */
static int
expand_bm_name(char **name)
{
	size_t j;
	int bm_exp = FUNC_FAILURE;
	char *p = unescape_str(*name + 2, 0);
	char *n = p ? p : *name + 2;

	for (j = 0; j < bm_n; j++) {
		if (!bookmarks[j].name || *n != *bookmarks[j].name
		|| strcmp(n, bookmarks[j].name) != 0)
			continue;

		char *q = escape_str(bookmarks[j].path);
		char *tmp = q ? q : bookmarks[j].path;
		const size_t tmp_len = strlen(tmp);
		*name = xnrealloc(*name, tmp_len + 1, sizeof(char));
		xstrsncpy(*name, tmp, tmp_len + 1);
		free(q);
		bm_exp = FUNC_SUCCESS;

		break;
	}

	free(p);
	return bm_exp;
}

/* Expand the internal variable NAME into its right value. */
static void
expand_int_var(char **name)
{
	char *var_name = (*name) + 1;

	int j = (int)usrvar_n;
	while (--j >= 0) {
		if (*var_name != *usr_var[j].name
		|| strcmp(var_name, usr_var[j].name) != 0 || !usr_var[j].value)
			continue;

		const size_t val_len = strlen(usr_var[j].value);
		*name = xnrealloc(*name, val_len + 1, sizeof(char));
		xstrsncpy(*name, usr_var[j].value, val_len + 1);
		break;
	}
}

static void
expand_file_type(char ***substr)
{
	size_t i = 0;
	struct stat a;

	int *file_type_array = xnmalloc(INT_ARRAY_MAX, sizeof(int));
	size_t file_type_n = 0;

	for (i = 0; (*substr)[i] && file_type_n < INT_ARRAY_MAX; i++) {
		if (*(*substr)[i] != '=' || !(*substr)[i][1])
			continue;

		if (IS_FILE_TYPE_FILTER((*substr)[i][1])) {
			if (lstat((*substr)[i], &a) == -1) {
				file_type_array[file_type_n] = (int)i;
				file_type_n++;
			}
			continue;
		}

		xerror(_("%s: '%c': Invalid file type filter. Run 'help "
			"file-filters' for more information\n"), PROGRAM_NAME,
			(*substr)[i][1]);
	}

	size_t old_ft = 0;
	for (i = 0; i < file_type_n; i++) {
		int index = file_type_array[i] + (int)old_ft;

		char **p =
			(*substr)[index][1] ? expand_file_type_filter((*substr)[index][1])
			: (char **)NULL;

		size_t c = 0;
		if (p) {
			char **ret = insert_fields(substr, &p, (size_t)index, &c);

			size_t n;
			for (n = 0; p[n]; n++)
				free(p[n]);
			free(p);

			if (ret) {
				for (n = 0; n <= args_n; n++)
					free((*substr)[n]);
				free((*substr));

				(*substr) = ret;
				ret = (char **)NULL;
				args_n += (c > 0 ? c - 1 : 0);
			}
		}

		old_ft += (c > 0 ? c - 1 : 0);
	}

	free(file_type_array);
}

#ifndef _NO_MAGIC
static void
expand_mime_type(char ***substr)
{
	size_t i = 0;
	int *mime_type_array = xnmalloc(INT_ARRAY_MAX, sizeof(int));
	size_t mime_type_n = 0;
	struct stat a;

	for (i = 0; (*substr)[i] && mime_type_n < INT_ARRAY_MAX; i++) {
		if (*(*substr)[i] == '@' && *((*substr)[i] + 1)
		&& lstat((*substr)[i], &a) == -1) {
			mime_type_array[mime_type_n] = (int)i;
			mime_type_n++;
		}
	}

	if (mime_type_n > 0) {
		fputs(_("Querying MIME types... "), stdout);
		fflush(stdout);
	}

	size_t old_mt = 0;
	for (i = 0; i < mime_type_n; i++) {
		int index = mime_type_array[i] + (int)old_mt;

		char **p = *((*substr)[index] + 1)
			? expand_mime_type_filter((*substr)[index] + 1) : (char **)NULL;

		size_t c = 0;
		if (p) {
			char **ret = insert_fields(substr, &p, (size_t)index, &c);

			size_t n;
			for (n = 0; p[n]; n++)
				free(p[n]);
			free(p);

			if (ret) {
				for (n = 0; n <= args_n; n++)
					free((*substr)[n]);
				free((*substr));

				(*substr) = ret;
				ret = (char **)NULL;
				args_n += (c > 0 ? c - 1 : 0);
			}
		}

		old_mt += (c > 0 ? c - 1 : 0);
	}

	if (mime_type_n > 0) {
		putchar('\r');
		ERASE_TO_RIGHT;
		fflush(stdout);
	}

	free(mime_type_array);
}
#endif /* !_NO_MAGIC */

static void
expand_bookmarks(char ***substr)
{
	size_t i = 0;
	struct stat a;

	int *bm_array = xnmalloc(INT_ARRAY_MAX, sizeof(int));
	size_t bn = 0;

	for (i = 0; (*substr)[i] && bn < INT_ARRAY_MAX; i++) {
		if (*(*substr)[i] == 'b' && *((*substr)[i] + 1) == ':'
		&& !*((*substr)[i] + 2) && lstat((*substr)[i], &a) == -1) {
			bm_array[bn] = (int)i;
			bn++;
		}
	}

	size_t old_bm = 0;
	for (i = 0; i < bn; i++) {
		int index = bm_array[i] + (int)old_bm;
		char **p = get_bm_paths();
		size_t c = 0;

		if (p) {
			char **ret = insert_fields(substr, &p, (size_t)index, &c);
			free(p);

			if (ret) {
				size_t n;
				for (n = 0; n <= args_n; n++)
					free((*substr)[n]);
				free((*substr));

				(*substr) = ret;
				ret = (char **)NULL;
				args_n += (c > 0 ? c - 1 : 0);
			}
		}

		old_bm += (c > 0 ? c - 1 : 0);
	}

	free(bm_array);
}

static int
expand_glob(char ***substr, const int *glob_array, const size_t glob_n)
{
	size_t old_pathc = 0;
	size_t i = 0;

	size_t g = 0;
	for (g = 0; g < (size_t)glob_n; g++) {
		glob_t globbuf;

		if (glob((*substr)[glob_array[g] + (int)old_pathc],
			GLOB_BRACE | GLOB_TILDE, NULL, &globbuf) != FUNC_SUCCESS) {
			globfree(&globbuf);
			continue;
		}

		if (globbuf.gl_pathc == 0)
			goto CONT;

		size_t j = 0;
		char **glob_cmd = (char **)NULL;
		glob_cmd = xcalloc(args_n + globbuf.gl_pathc + 1, sizeof(char *));

		for (i = 0; i < ((size_t)glob_array[g] + old_pathc); i++) {
			glob_cmd[j] = savestring((*substr)[i], strlen((*substr)[i]));
			j++;
		}

		for (i = 0; i < globbuf.gl_pathc; i++) {
			if (SELFORPARENT(globbuf.gl_pathv[i]))
				continue;

			char *esc_str = (char *)NULL;
			/* Escape the globbed file name and copy it */
			if (virtual_dir == 1 && is_file_in_cwd(globbuf.gl_pathv[i])) {
				char buf[PATH_MAX + 1]; *buf = '\0';
				if (xreadlink(XAT_FDCWD, globbuf.gl_pathv[i], buf,
				sizeof(buf)) == -1 || !*buf)
					continue;
				esc_str = escape_str(buf);
			} else {
				esc_str = escape_str(globbuf.gl_pathv[i]);
			}

			if (esc_str) {
				glob_cmd[j] = esc_str;
				j++;
			} else {
				xerror(_("%s: '%s': Error quoting file name\n"),
					PROGRAM_NAME, globbuf.gl_pathv[i]);
				continue;
			}
		}

		for (i = (size_t)glob_array[g] + old_pathc + 1; i <= args_n; i++) {
			glob_cmd[j] = savestring((*substr)[i], strlen((*substr)[i]));
			j++;
		}

		glob_cmd[j] = (char *)NULL;

		for (i = 0; i <= args_n; i++)
			free((*substr)[i]);
		free((*substr));

		(*substr) = glob_cmd;
		glob_cmd = (char **)NULL;
		args_n = j - 1;

CONT:
		old_pathc += (globbuf.gl_pathc - 1);
		globfree(&globbuf);
	}

	return 0;
}

#ifdef HAVE_WORDEXP
static int
expand_word(char ***substr, const int *word_array, const size_t word_n)
{
	size_t old_pathc = 0;
	size_t w = 0, i = 0;

	const int is_sel_cmd =
		(strcmp((*substr)[0], "s") == 0 || strcmp((*substr)[0], "sel") == 0);

	for (w = 0; w < word_n; w++) {
		if (is_sel_cmd == 1) {
			/* If the command is 'sel', perform only command substitution
			 * and environment variables expansion. Otherwise, wordexp(3)
			 * modifies the input string and breaks other expansions made
			 * by the sel function, mostly regex expansion. */
			char *p = strchr((*substr)[word_array[w] + (int)old_pathc], '$');
			if (p && *(p + 1) != '(' && (*(p + 1) < 'A' || *(p + 1) > 'Z'))
				continue;
		}

		wordexp_t wordbuf;
		if (wordexp((*substr)[word_array[w] + (int)old_pathc],
			&wordbuf, 0) != FUNC_SUCCESS) {
			wordfree(&wordbuf);
			continue;
		}

		if (wordbuf.we_wordc) {
			size_t j = 0;
			char **word_cmd = xcalloc(args_n + wordbuf.we_wordc + 1,
				sizeof(char *));

			for (i = 0; i < ((size_t)word_array[w] + old_pathc); i++) {
				word_cmd[j] = savestring((*substr)[i], strlen((*substr)[i]));
				j++;
			}

			for (i = 0; i < wordbuf.we_wordc; i++) {
				/* Escape the globbed file name and copy it */
				char *esc_str = escape_str(wordbuf.we_wordv[i]);
				if (esc_str) {
					word_cmd[j] = esc_str;
					j++;
				} else {
					xerror(_("%s: '%s': Error quoting file name\n"),
						PROGRAM_NAME, wordbuf.we_wordv[i]);

					size_t k = 0;
					for (k = 0; k < j; k++)
						free(word_cmd[k]);
					free(word_cmd);

					word_cmd = (char **)NULL;

					for (k = 0; k <= args_n; k++)
						free((*substr)[k]);
					free((*substr));
					return (-1);
				}
			}

			for (i = (size_t)word_array[w] + old_pathc + 1; i <= args_n; i++) {
				word_cmd[j] = savestring((*substr)[i], strlen((*substr)[i]));
				j++;
			}

			word_cmd[j] = (char *)NULL;

			for (i = 0; i <= args_n; i++)
				free((*substr)[i]);
			free((*substr));

			(*substr) = word_cmd;
			word_cmd = (char **)NULL;
			args_n = j - 1;
		}

		old_pathc += (wordbuf.we_wordc - 1);
		wordfree(&wordbuf);
	}

	return 0;
}
#endif /* HAVE_WORDEXP */

static size_t
check_ranges(char ***substr, int **range_array)
{
	size_t i = 0, j = 0, n = 0;
	struct stat a;

	for (i = 0; i <= args_n; i++) {
		if (!(*substr)[i] || is_quoted_word(i) || lstat((*substr)[i], &a) != -1)
			continue;

		const size_t len = strlen((*substr)[i]);

		for (j = 0; (*substr)[i][j]; j++) {
			/* If some alphabetic char, besides '-', is found in the
			 * string, we have no range. */
			if ((*substr)[i][j] != '-' && !IS_DIGIT((*substr)[i][j]))
				break;

			/* If a range is found, store its index. */
			if (j > 0 && j < len && (*substr)[i][j] == '-'
			&& IS_DIGIT((*substr)[i][j - 1])) {
				if (n >= INT_ARRAY_MAX)
					break;
				(*range_array)[n] = (int)i;
				n++;
			}
		}
	}

	return n;
}

/* Expand a range of numbers given by STR. It will expand the range
 * provided that both extremes are numbers, bigger than zero, equal or
 * smaller than the amount of files currently listed on the screen, and
 * the second (right) extreme is bigger than the first (left). Returns
 * an array of int's with the expanded range or NULL if one of the
 * above conditions is not met. */
static filesn_t *
expand_range(char *str, int listdir)
{
	if (!str || !*str)
		return (filesn_t *)NULL;

	struct stat a;
	if (lstat(str, &a) != -1)
		return (filesn_t *)NULL;

	char *p = strchr(str, '-');
	if (!p || p == str || *(p - 1) < '0' || *(p - 1) > '9')
		return (filesn_t *)NULL;

	*p = '\0';
	const int ret = is_number(str);
	*p = '-';
	if (!ret)
		return (filesn_t *)NULL;

	const filesn_t afirst = xatof(str);

	++p;
	filesn_t asecond = 0;
	if (!*p) { /* No second field: assume last listed file */
		asecond = files;
	} else {
		if (!is_number(p))
			return (filesn_t *)NULL;
		asecond = xatof(p);
	}

	if (afirst == -1 || asecond == -1)
		return (filesn_t *)NULL;

	if (listdir) {
		if (afirst <= 0 || afirst > files || asecond <= 0
		|| asecond > files || afirst >= asecond)
			return (filesn_t *)NULL;
	} else {
		if (afirst >= asecond) 
			return (filesn_t *)NULL;
	}

	filesn_t *buf = xcalloc((size_t)(asecond - afirst) + 2, sizeof(filesn_t));

	filesn_t i, j = 0;
	for (i = afirst; i <= asecond; i++) {
		buf[j] = i;
		j++;
	}

	return buf;
}

static void
expand_ranges(char ***substr)
{
	size_t i = 0, j = 0;

	int *range_array = xnmalloc(INT_ARRAY_MAX, sizeof(int));
	const size_t ranges_ok = check_ranges(substr, &range_array);

	if (ranges_ok == 0) {
		free(range_array);
		return;
	}

	size_t old_ranges_n = 0;
	size_t r = 0;

	for (r = 0; r < ranges_ok; r++) {
		size_t ranges_n = 0;
		filesn_t *ranges = expand_range((*substr)[range_array[r] +
			(int)old_ranges_n], 1);

		if (ranges) {
			j = 0;
			for (ranges_n = 0; ranges[ranges_n]; ranges_n++);

			update_quoted_words_index((size_t)range_array[r]
				+ old_ranges_n, ranges_n);

			char **ranges_cmd = (char **)NULL;
			ranges_cmd = xcalloc(args_n + ranges_n + 2, sizeof(char *));

			for (i = 0; i < (size_t)range_array[r] + old_ranges_n; i++) {
				if (!(*substr)[i])
					continue;
				ranges_cmd[j] = savestring((*substr)[i], strlen((*substr)[i]));
				j++;
			}

			for (i = 0; i < ranges_n; i++) {
				const size_t len = (size_t)DIGINUM(ranges[i]) + 1;
				ranges_cmd[j] = xnmalloc(len, sizeof(int));
				snprintf(ranges_cmd[j], len, "%zd", ranges[i]);
				j++;
			}

			for (i = (size_t)range_array[r] + old_ranges_n + 1;
			i <= args_n; i++) {
				if (!(*substr)[i])
					continue;
				ranges_cmd[j] = savestring((*substr)[i], strlen((*substr)[i]));
				j++;
			}

			ranges_cmd[j] = (char *)NULL;
			free(ranges);

			for (i = 0; i <= args_n; i++)
				free((*substr)[i]);
			free((*substr));

			(*substr) = ranges_cmd;
			args_n = j - 1;
		}

		old_ranges_n += (ranges_n - 1);
	}

	free(range_array);
}

static void
expand_regex(char ***substr)
{
	/* Let's store all strings currently in substr plus REGEX expanded
	 * files, if any, in a temporary array. */
	char **tmp = xnmalloc((size_t)files + args_n + 2, sizeof(char *));
	filesn_t i, j;
	size_t n = 0;
	regex_t regex;

/*	int reg_flags = conf.case_sens_list == 1 ? (REG_NOSUB | REG_EXTENDED)
			: (REG_NOSUB | REG_EXTENDED | REG_ICASE); */

	const int reg_flags = (REG_NOSUB | REG_EXTENDED);

	for (i = 0; (*substr)[i]; i++) {
		if (n > ((size_t)files + args_n))
			break;

		/* Ignore the first string of the search function: it will be
		 * expanded by the search function itself. */
		if (*(*substr)[0] == '/') {
			tmp[n] = (*substr)[i];
			n++;
			continue;
		}

		/* At this point, all file names are escaped. But check_regex()
		 * needs unescaped file names. So, let's deescape it. */
		char *p = strchr((*substr)[i], '\\');
		char *dstr = (char *)NULL;
		if (p)
			dstr = unescape_str((*substr)[i], 0);

		char *t = dstr ? dstr : (*substr)[i];

		/* Prepend an initial '^' and append and ending '$' to prevent
		 * accidental file expansions. For example, a file named file.txt
		 * must not be expanded given the pattern "ile.t". In other words,
		 * we force the use of ".*PATTERN.*" instead of just "PATTERN". */
		const size_t l = strlen(t) + 3;
		char *rstr = xnmalloc(l, sizeof(char));
		snprintf(rstr, l, "^%s$", t);
		free(dstr);

		int ret = check_regex(rstr);

		if (ret != FUNC_SUCCESS
		|| regcomp(&regex, rstr, reg_flags) != FUNC_SUCCESS) {
			if (ret == FUNC_SUCCESS)
				regfree(&regex);
			free(rstr);
			tmp[n] = (*substr)[i];
			n++;
			continue;
		}

		free(rstr);
		int reg_found = 0;

		for (j = 0; j < files; j++) {
			if (regexec(&regex, file_info[j].name, 0, NULL, 0) != FUNC_SUCCESS)
				continue;

			/* Make sure the matching file name is not already in the tmp array */
			filesn_t m = (filesn_t)n, found = 0;
			while (--m >= 0) {
				if (*file_info[j].name == *tmp[m]
				&& strcmp(file_info[j].name, tmp[m]) == 0)
					found = 1;
			}

			if (found == 1)
				continue;

			tmp[n] = file_info[j].name;
			n++;
			reg_found = 1;
		}

		if (reg_found == 0) {
			tmp[n] = (*substr)[i];
			n++;
		}

		regfree(&regex);
	}

	if (n > 0) {
		tmp[n] = (char *)NULL;

		char **tmp_files = xnmalloc(n + 2, sizeof(char *));

		size_t k = 0;
		for (j = 0; tmp[j]; j++) {
			struct stat a;
			if (virtual_dir == 1 && lstat(tmp[j], &a) == 0
			&& S_ISLNK(a.st_mode) && is_file_in_cwd(tmp[j])) {
				char buf[PATH_MAX]; *buf = '\0';
				const ssize_t buf_len =
					xreadlink(XAT_FDCWD, tmp[j], buf, sizeof(buf));
				if (buf_len == -1 || !*buf)
					continue;
				tmp_files[k] = savestring(buf, (size_t)buf_len);
			} else {
				tmp_files[k] = savestring(tmp[j], strlen(tmp[j]));
			}

			k++;
		}
		tmp_files[k] = (char *)NULL;

		for (j = 0; (*substr)[j]; j++)
			free((*substr)[j]);
		free((*substr));

		(*substr) = tmp_files;
		args_n = (k > 0 ? k - 1 : k);
	}

	free(tmp);
}

static int
expand_symlink(char **substr)
{
	struct stat a;
	char *name = strchr(*substr, '\\') ? unescape_str(*substr, 0) : *substr;
	if (!name)
		return 0;

	const size_t l = strlen(name);
	if (l > 0 && name[l - 1] == '/')
		name[l - 1] = '\0';

	const int ret = lstat(name, &a);
	const int link_ok = (ret != -1 && S_ISLNK(a.st_mode));

	if (link_ok == 0) {
		if (name != *substr) free(name);
		return 0;
	}

	char target[PATH_MAX + 1]; *target = '\0';
	if (xreadlink(XAT_FDCWD, name, target, sizeof(target)) == -1) {
		xerror("realpath: '%s': %s\n", name, strerror(errno));
		if (name != *substr) free(name);
		return (-1);
	}

	if (name != *substr) free(name);

	char *estr = escape_str(target);
	name = estr ? estr : target;

	const size_t rp_len = strlen(name);
	*substr = xnrealloc(*substr, rp_len + 1, sizeof(char));
	xstrsncpy(*substr, name, rp_len + 1);

	free(estr);

	return 0;
}

/* Return 1 if glob expansion should be performed for the current command,
 * or 0 otherwise. */
static int
glob_expand(char **cmd)
{
	if (!cmd || !cmd[0] || !*cmd[0])
		return 0;

	/* Do not expand if command is deselect, sel or untrash, just to
	 * allow the use of "*" for desel and untrash ("ds *" and "u *"),
	 * and to let the sel function handle glob patterns itself. */

	if (strcmp(cmd[0], "s") != 0 && strcmp(cmd[0], "sel") != 0

	&& strcmp(cmd[0], "ds") != 0 && strcmp(cmd[0], "desel") != 0

	&& strcmp(cmd[0], "u") != 0 && strcmp(cmd[0], "undel") != 0
	&& strcmp(cmd[0], "untrash") != 0

	&& !( *cmd[0] == 't' && (!cmd[0][1] || strcmp(cmd[0], "tr") == 0
	|| strcmp(cmd[0], "trash") == 0) && cmd[1] && *cmd[1] == 'd'
	&& strcmp(cmd[1], "del") == 0 ) )
		return 1;

	return 0;
}

/* Return 0 if CMD should be regex expanded, or 1 otherwise. */
static int
regex_expand(const char *cmd)
{
	if (!cmd || !*cmd)
		return 0;

	if (strcmp(cmd, "ds") == 0 || strcmp(cmd, "desel") == 0
	|| strcmp(cmd, "u") == 0 || strcmp(cmd, "undel") == 0
	|| strcmp(cmd, "untrash") == 0
	|| strcmp(cmd, "s") == 0 || strcmp(cmd, "sel") == 0)
		return 0;

	return 1;
}

static char **
gen_full_line(char **str, const int fusedcmd_ok)
{
	/* Remove leading spaces */
	char *p = *str;
	while (*p == ' ' || *p == '\t')
		p++;

	args_n = 0;

	char **cmd = xnmalloc(2, sizeof(char *));
	cmd[0] = savestring(p, strlen(p));
	cmd[1] = (char *)NULL;

	if (fusedcmd_ok == 1)
		free(*str);

	/* If ";cmd" or ":cmd" the whole input line will be send to
	 * exec_cmd() and will be executed by the system shell via
	 * execl(). Since we don't run split_str() here, dequoting
	 * and unescaping is performed directly by the system shell. */
	return cmd;
}

static int
check_int_var(char *str)
{
	/* Remove leading spaces. This: '   a="test"' should be
	 * taken as a valid variable declaration */
	char *p = str;
	while (*p == ' ' || *p == '\t')
		p++;

	/* If first non-space is a number, it's not a variable name */
	if (IS_DIGIT(*p))
		return 0;

	int space_found = 0;
	/* If there are no spaces before '=', take it as a
	 * variable. This check is done in order to avoid
	 * taking as a variable things "ls --color=auto" */
	while (*p != '=') {
		if (*(p++) == ' ')
			space_found = 1;
	}

	if (space_found == 0)
		return 1;

	return 0;
}

static int
check_chained_cmds(char *str)
{
	/* If the user wants to create a file containing either '|' or ';' in the
	 * name, they should be allowed. The file name will be later validated by
	 * the 'new' function itself. */
	if (str && (strncmp(str, "n ", 2) == 0 || strncmp(str, "new ", 4) == 0))
		return 0;

	/* User defined variables are always internal, so that there is
	 * no need to check whatever else is in the command string */
	if (flags & IS_USRVAR_DEF) {
		exec_chained_cmds(str);
		return 1;
	}

	size_t i = 0;
	const size_t str_len = strlen(str);
	size_t len = 0, internal_ok = 0;
	char *buf = (char *)NULL;

	/* Get each word (cmd) in STR */
	buf = xnmalloc(str_len + 1, sizeof(char));
	for (i = 0; i < str_len; i++) {
		while (str[i] && str[i] != ' ' && str[i] != ';' && str[i] != '&') {
			buf[len] = str[i];
			len++;
			i++;
		}
		buf[len] = '\0';

		if (strcmp(buf, "&&") != 0) {
			if (is_internal_c(buf)) {
				internal_ok = 1;
				break;
			}
		}

		memset(buf, '\0', len);
		len = 0;
	}

	free(buf);

	if (internal_ok == 1) {
		exec_chained_cmds(str);
		return 1;
	}

	return 0;
}

/*
 * This function is one of the keys of CliFM. It will perform a series of
 * actions:
 * 1) Take the string stored by readline and get its substrings without
 * leading and trailing spaces (dequoting/unescaping if necessary).
 * 2) In case of user defined variables (var=value), it will pass the
 * whole string to exec_cmd(), which will take care of storing the
 * variable;
 * 3) If the input string begins with ';' or ':' the whole string is
 * sent to exec_cmd(), where it will be directly executed by the system
 * shell.
 * 4) The following expansions (especific to CLiFM) are performed here:
 * ELN's, "sel" keyword, ranges of numbers (ELN's), tags, pinned dir,
 * bookmark names, environment variables, file types (=x), mime types (@...),
 * path normalization, fastback, and, for internal commands only, tilde,
 * braces, wildcards, command and paramenter substitution, and regex.
 */
char **
parse_input_str(char *str)
{
	if (!str)
		return (char **)NULL;

	size_t i = 0;
	int fusedcmd_ok = 0;

	flags &= ~FIRST_WORD_IS_ELN;
	flags &= ~IS_USRVAR_DEF;

	/* If internal command plus fused parameter, split it */
	if (is_fused_param(str) == FUNC_SUCCESS) {
		char *p = split_fused_param(str);
		if (p) {
			fusedcmd_ok = 1;
			str = p;
		}
	}

			/* ########################################
			* #    0) CHECK FOR SPECIAL FUNCTIONS    #
			* ########################################*/

	int chaining = 0, cond_cmd = 0, send_shell = 0;

				/* ###########################
				 * #  0.a) RUN AS EXTERNAL   #
				 * ###########################*/

	/* If invoking a command via ';' or ':' set the send_shell flag to
	 * true and send the whole string to exec_cmd(), in which case no
	 * expansion is made: the command is send to the system shell as
	 * is. */
	if (*str == ';' || *str == ':' || check_shell_functions(str) == 1)
		send_shell = 1;

	if (send_shell == 0) {
		for (i = 0; str[i]; i++) {

				/* ##################################
				 * #   0.b) CONDITIONAL EXECUTION   #
				 * ##################################*/

			/* Check for chained commands (cmd1;cmd2) */
			if (chaining == 0 && str[i] == ';' && i > 0 && str[i - 1] != '\\')
				chaining = 1;

			/* Check for conditional execution (cmd1 && cmd 2)*/
			if (cond_cmd == 0 && str[i] == '&' && i > 0 && str[i - 1] != '\\'
			&& str[i + 1] == '&')
				cond_cmd = 1;

				/* ##################################
				 * #   0.c) USER DEFINED VARIABLE   #
				 * ##################################*/

			/* If user defined variable send the whole string to
			 * exec_cmd(), which will take care of storing the
			 * variable. */
			if (!(flags & IS_USRVAR_DEF) && int_vars == 1 && str[i] == '='
			&& i > 0 && str[i - 1] != '\\' && str[0] != '=') {
				if (check_int_var(str) == 1)
					flags |= IS_USRVAR_DEF;
			}
		}
	}

	/* If chained commands, check each of them. If at least one of them
	 * is internal, take care of the job (the system shell does not know
	 * our internal commands and therefore cannot execute them); else,
	 * if no internal command is found, let it to the system shell */
	if (chaining == 1 || cond_cmd == 1) {
		if (check_chained_cmds(str) == 1) {
			if (fusedcmd_ok == 1)
				free(str);
			return (char **)NULL;
		}
	}

	if ((flags & IS_USRVAR_DEF) || send_shell == 1)
		return gen_full_line(&str, fusedcmd_ok);

		/* ################################################
		 * #     1) SPLIT INPUT STRING INTO SUBSTRINGS    #
		 * ################################################ */

	/* split_str() returns an array of strings without leading,
	 * terminating and double spaces. */
	char **substr = split_str(str, UPDATE_ARGS);

	if (fusedcmd_ok == 1) /* Just in case split_fusedcmd returned NULL */
		free(str);

	if (!substr)
		return (char **)NULL;

	/* Do not perform expansions for the 'n/new' command */
	if (*substr[0] == 'n' && (!substr[0][1] || strcmp(substr[0], "new") == 0))
		return substr;

	/* Handle background/foreground process */
	bg_proc = 0;

	if (args_n > 0 && *substr[args_n] == '&' && !*(substr[args_n] + 1)) {
		bg_proc = 1;
		free(substr[args_n]);
		substr[args_n] = (char *)NULL;
		args_n--;
	} else {
		const size_t len = strlen(substr[args_n]);
		if (len > 0 && substr[args_n][len - 1] == '&' && !substr[args_n][len]) {
			substr[args_n][len - 1] = '\0';
			bg_proc = 1;
		}
	}

					/* ######################
					 * #     TRASH AS RM    #
					 * ###################### */
#ifndef _NO_TRASH
	if (conf.tr_as_rm && substr[0] && *substr[0] == 'r' && !substr[0][1]) {
		substr[0] = xnrealloc(substr[0], 2, sizeof(char));
		*substr[0] = 't';
		substr[0][1] = '\0';
	}
#endif /* !_NO_TRASH*/

				/* ##############################
				 * #   2) BUILTIN EXPANSIONS    #
				 * ##############################

	 * Ranges, sel, ELN, pinned dirs, bookmarks, and internal variables.
	 * These expansions are specific to CliFM. To be able to use them
	 * even with external commands, they must be expanded here, before
	 * sending the input string, in case the command is external, to
	 * the system shell. */
	is_sel = 0; sel_is_last = 0;

	const int stdin_dir_ok = (stdin_tmp_dir
		&& strcmp(workspaces[cur_ws].path, stdin_tmp_dir) == 0);

	/* Let's expand ranges first: the numbers resulting from the expanded range
	 * will be expanded into the corresponding file names by eln_expand() below. */
	expand_ranges(&substr);

	for (i = 0; i <= args_n; i++) {
		if (!substr[i] || (is_quoted_word(i)
		&& (virtual_dir == 0 || is_file_in_cwd(substr[i]) == 0)))
			continue;

		/* The following expansions expand into a SINGLE field */

				/* ##########################
				 * #   2.1) ELN EXPANSION   #
				 * ########################## */

		/* should_expand_eln() will check rl_line_buffer looking for the
		 * command name. Now, if the command name has a fused ELN plus space
		 * and at least a second parameter (ex: CMD1 2), should_expand_eln()
		 * will fail. Let's redirect rl_line_buffer to the buffered command
		 * name (CMD) so that the check will be properly performed. */
		char *lb_tmp = rl_line_buffer;
		if (rl_dispatching == 0 && fusedcmd_ok == 1)
			rl_line_buffer = substr[0];
		if (should_expand_eln(substr[i]) == 1)
			eln_expand(&substr, i);
		rl_line_buffer = lb_tmp;

				/* ################################
				 * #  2.2) ENVIRONEMNT VARIABLES  #
				 * ###############################*/

		if (*substr[i] == '$') {
			char *p = getenv(substr[i] + 1);
			if (p) {
				const size_t plen = strlen(p) + 1;
				substr[i] = xnrealloc(substr[i], plen, sizeof(char));
				xstrsncpy(substr[i], p, plen);
			}
		}

				/* ################################
				 * #  2.3) TILDE: ~user and home  #
				 * ################################ */

		if (*substr[i] == '~') {
			char *p = tilde_expand(substr[i]);
			if (p) {
				free(substr[i]);
				substr[i] = p;
			}
		}

			/* ##################################
			 * #     2.4) URI file scheme       #
			 * ################################## */
			/* file:///some/file -> /some/file */

		size_t slen = strlen(substr[i]);
		if (slen > FILE_URI_PREFIX_LEN && IS_FILE_URI(substr[i])) {
			char tmp[PATH_MAX + 1];
			xstrsncpy(tmp, substr[i], sizeof(tmp));
			xstrsncpy(substr[i], tmp + FILE_URI_PREFIX_LEN, slen + 1);
		}

			/* ###############################
			 * #     2.5) "." and ".."       #
			 * ############################### */

		if ((*substr[i] == '.' && (!substr[i][1] || (substr[i][1] == '.'
		&& !substr[i][2]))) || strstr(substr[i], "/..")) {
			char *tmp = normalize_path(substr[i], strlen(substr[i]));
			if (tmp) {
				free(substr[i]);
				substr[i] = tmp;
			}
		}

			/* ######################################
			 * #     2.6) FASTBACK EXPANSION        #
			 * ###################################### */

		if (*substr[i] == '.' && substr[i][1] == '.' && substr[i][2] == '.') {
			char *tmp = fastback(substr[i]);
			if (tmp) {
				free(substr[i]);
				substr[i] = tmp;
			}
		}

			/* ######################################
			 * #     2.7) PINNED DIR EXPANSION      #
			 * ###################################### */

		if (*substr[i] == ',' && !substr[i][1] && pinned_dir) {
			const size_t plen = strlen(pinned_dir);
			substr[i] = xnrealloc(substr[i], plen + 1, sizeof(char));
			xstrsncpy(substr[i], pinned_dir, plen + 1);
		}

			/* ######################################
			 * #   2.8) BOOKMARK NAMES EXPANSION    #
			 * ###################################### */

		/* Expand bookmark name (b:NAME) into the corresponding path */
		if (*substr[i] == 'b' && substr[i][1] == ':' && substr[i][2]) {
			if (expand_bm_name(&substr[i]) == FUNC_SUCCESS)
				continue;
		}

			/* ###################################
			 * #   2.9) USER DEFINED VARIABLES   #
			 * ###################################*/

		if (int_vars == 1 && usrvar_n > 0) {
			if (substr[i][0] == '$' && substr[i][1] && substr[i][1] != '('
			&& substr[i][1] != '{')
				expand_int_var(&substr[i]);
		}

			/* ###################################
			 * #  2.10) SYMLINKS IN VIRTUAL DIR  #
			 * ################################### */

		/* We are in STDIN_TMP_DIR: Expand symlinks to target */
		if (stdin_dir_ok == 1 && expand_symlink(&substr[i]) == -1) {
			for (i = 0; i <= args_n; i++)
				free(substr[i]);
			free(substr);
			return (char **)NULL;
		}
	}

	/* The following expansions expand into MULTIPLE fields */

				/* ###########################
				 * #   2.11) SEL EXPANSION   #
				 * ########################### */

	expand_sel_keyword(&substr);

				/* #################################
				 * #     2.12) TAGS EXPANSION      #
				 * ################################# */

#ifndef _NO_TAGS
	expand_tags(&substr);
#endif /* _NO_TAGS */

				/* ################################
				 * #    2.13) FILE TYPE (=CHAR)   #
				 * ################################ */

	expand_file_type(&substr);

				/* ##################################
				 * #   2.14) MIME TYPE (@PATTERN)   #
				 * ################################## */
#ifndef _NO_MAGIC
	expand_mime_type(&substr);
#endif /* !_NO_MAGIC */

				/* ###############################
				 * #    2.15) BOOKMARKS (b:)     #
				 * ############################### */

	expand_bookmarks(&substr);


	/* #### NULL TERMINATE THE INPUT STRING ARRAY #### */
	substr = xnrealloc(substr, args_n + 2, sizeof(char *));
	substr[args_n + 1] = (char *)NULL;

	const int is_action = is_action_name(substr[0]);
	if (is_internal(substr[0]) == 0 && is_action == 0)
		return substr;

		/* ####################################################
		 * #          3) SHELL-LIKE EXPANSIONS                #
		 * #      Only for internal commands and plugins      #
		 * #################################################### */

	/* Most clifm functions are purely internal, that is, they are not
	 * wrappers of a shell command and do not call the system shell at all.
	 * For this reason, some expansions normally made by the system shell
	 * (wildcards, regular expressions, and command substitution) must be
	 * made here (in the lobby [got it?]) in order to be able to understand
	 * these expansions at all. */

	/* Let's first mark substrings containing special expansions made by either
	 * glob(3) and wordexp(3). */

	int *glob_array = xnmalloc(INT_ARRAY_MAX, sizeof(int));
	size_t glob_n = 0;

#ifdef HAVE_WORDEXP
	int *word_array = xnmalloc(INT_ARRAY_MAX, sizeof(int));
	size_t word_n = 0;
#endif /* HAVE_WORDEXP */

	for (i = 0; substr[i]; i++) {
		if ((is_action == 1 && i == 0) || is_quoted_word(i))
			continue;
		/* Do not perform any of the expansions below for selected
		 * elements: they are full path file names that, as such, do not
		 * need any expansion. */
		if (is_sel > 0) { /* is_sel is true only for the current input and if
			there was some "sel" keyword in it. */
			/* Strings between is_sel and sel_n are selected file names.
			 * Skip them. */
			if (i >= (size_t)is_sel && i <= sel_n)
				continue;
		}

		/* Ignore the first word of the search function: it will be
		 * expanded by the search function itself. */
		if (substr[0][0] == '/' && i == 0)
			continue;

#ifdef HAVE_WORDEXP
		/* Let's make wordexp(3) ignore escaped words. */
		int is_escaped = strchr(substr[i], '\\') ? 1 : 0;
#endif /* HAVE_WORDEXP */

		size_t j = 0;
		for (j = 0; substr[i][j]; j++) {
			/* Brace and wildcard expansion is made by glob(3). */
			if (IS_GLOB(substr[i][j], substr[i][j + 1])) {
				/* Strings containing these characters are taken as wildacard
				 * patterns and are expanded by the glob function. See glob(7). */
				if (glob_n < INT_ARRAY_MAX) {
					glob_array[glob_n] = (int)i;
					glob_n++;
				}
			}

#ifdef HAVE_WORDEXP
			/* Command substitution, tilde, and environment variables
			 * expansion is made by wordexp(3) */
			if (is_escaped == 0 && IS_WORD(substr[i][j], substr[i][j + 1])) {
				/* Unlike glob() and tilde_expand(), wordexp() can expand tilde
				 * and env vars even in the middle of a string. Ex: $HOME/Downloads */
				if (word_n < INT_ARRAY_MAX) {
					word_array[word_n] = (int)i;
					word_n++;
				}
			}
#endif /* HAVE_WORDEXP */
		}
	}

			/* ##########################################
			 * #   3.1) WILDCARDS AND BRACE EXPANSION   #
			 * ########################################## */

	if (glob_n > 0 && glob_expand(substr) == 1) {
		if (expand_glob(&substr, glob_array, glob_n) == -1)
			return (char **)NULL;
	}

	free(glob_array);

		/* ###############################################
		 * #    3.2) COMMAND & PARAMETER SUBSTITUTION    #
		 * ############################################### */

#ifdef HAVE_WORDEXP
	if (word_n > 0) {
		if (expand_word(&substr, word_array, word_n) == -1)
			return (char **)NULL;
	}

	free(word_array);
#endif /* HAVE_WORDEXP */

			/* #######################################
			 * #       3.3) REGEX EXPANSION          #
			 * ####################################### */

	if (regex_expand(substr[0]) == 1)
		expand_regex(&substr);

	/* #### NULL TERMINATE THE INPUT STRING ARRAY (again) #### */
	substr = xnrealloc(substr, args_n + 2, sizeof(char *));
	substr[args_n + 1] = (char *)NULL;

	return substr;
}

/* Reduce "$HOME" to tilde ("~").
 * If NEW_PATH isn't in home, NEW_PATH is returned without allocating new
 * memory, in which case FREE_BUF is set to zero.
 * Otherwise, the reduced path is copied into malloc'ed memory. */
char *
home_tilde(char *new_path, int *free_buf)
{
	*free_buf = 0;
	if (home_ok == 0 || !new_path || !*new_path || !user.home)
		return (char *)NULL;

	/* If new_path == HOME */
	if (new_path[1] && user.home[1] && new_path[1] == user.home[1]
	&& strcmp(new_path, user.home) == 0) {
		char *path_tilde = xnmalloc(2, sizeof(char));
		path_tilde[0] = '~';
		path_tilde[1] = '\0';
		*free_buf = 1;
		return path_tilde;
	}

	if (new_path[1] && user.home[1] && new_path[1] == user.home[1]
	&& strncmp(new_path, user.home, user.home_len) == 0
	/* Avoid names like these: "HOMEfile". It should always be rather "HOME/file" */
	&& (user.home[user.home_len - 1] == '/'
	|| *(new_path + user.home_len) == '/') ) {
		/* If new_path == HOME/file */
		const size_t len = strlen(new_path + user.home_len + 1) + 3;
		char *path_tilde = xnmalloc(len, sizeof(char));
		snprintf(path_tilde, len, "~/%s", new_path + user.home_len + 1);

		*free_buf = 1;
		return path_tilde;
	}

	return new_path;
}

/* Returns a pointer to a copy of the string STR, malloc'ed with size SIZE, or
 * NULL on error. */
char *
savestring(const char *restrict str, size_t size)
{
	if (!str)
		return (char *)NULL;

	char *ptr = (char *)NULL;
	ptr = malloc((size + 1) * sizeof(char));

	if (!ptr)
		return (char *)NULL;

	xstrsncpy(ptr, str, size + 1);
	return ptr;
}

/* Take a string and returns the same string escaped.
 * If there is nothing to be escaped, the original string is returned. In
 * either cases, the returned string must be free'd by the caller. */
char *
escape_str(const char *str)
{
	if (!str)
		return (char *)NULL;

	size_t len = 0;
	char *buf = (char *)NULL;

	buf = xnmalloc(strlen(str) * 2 + 1, sizeof(char));

	while (*str) {
		if (is_quote_char(*str)) {
			buf[len] = '\\';
			len++;
		}
		buf[len] = *str;
		len++;
		str++;
	}

	buf[len] = '\0';
	return buf;
}

/* Get all substrings in STR using IFS as substring separator.
 * If FPROC is set to 1, some further processing is performed: ranges
 * are expanded and duplicates removed.
 * Returns an array containing all substrings in STR. */
char **
get_substr(char *str, const char ifs, const int fproc)
{
	if (!str || !*str)
		return (char **)NULL;

	/* a. SPLIT THE STRING */
	char **substr = (char **)NULL;
	size_t len = 0;
	size_t substr_n = 0;
	const size_t str_len = strlen(str);
	char *buf = xnmalloc(str_len + 1, sizeof(char));

	while (*str) {
		while (*str != ifs && *str != '\0' && len < (str_len + 1)) {
			buf[len] = *str;
			len++;
			str++;
		}

		if (len > 0) {
			buf[len] = '\0';
			substr = xnrealloc(substr, substr_n + 2, sizeof(char *));
			substr[substr_n] = savestring(buf, len);
			substr_n++;
			len = 0;
		} else {
			str++;
		}
	}

	free(buf);

	if (substr_n == 0)
		return (char **)NULL;

	substr[substr_n] = (char *)NULL;

	if (fproc == 0)
		return substr;

	/* b. EXPAND RANGES */
	const size_t argsbk = args_n;
	args_n = substr_n;
	expand_ranges(&substr);
	args_n = argsbk;

	/* c. REMOVE DUPLICATES */
	for (substr_n = 0; substr[substr_n]; substr_n++);
	if (substr_n == 0) {
		free(substr);
		return (char **)NULL;
	}

	char **dstr = (char **)NULL;
	size_t i, j;
	size_t n = 0;

	for (i = 0; i < substr_n; i++) {
		int dup = 0;
		for (j = (i + 1); j < substr_n; j++) {
			if (*substr[i] == *substr[j] && strcmp(substr[i], substr[j]) == 0) {
				dup = 1;
				break;
			}
		}

		if (dup == 0) {
			dstr = xnrealloc(dstr, n + 2, sizeof(char *));
			dstr[n] = savestring(substr[i], strlen(substr[i]));
			n++;
		}

		free(substr[i]);
	}

	free(substr);

	dstr[n] = (char *)NULL;
	return dstr;
}

/* This function simply unescapes whatever escaped chars it founds in
 * TEXT. Returns a string containing TEXT without escape sequences. */
char *
unescape_str(char *text, int mt)
{
	UNUSED(mt);
	if (!text || !*text)
		return (char *)NULL;

	/* At most, we need as many bytes as in TEXT (in case no escape
	 * sequence is found). */
	char *buf = xnmalloc(strlen(text) + 1, sizeof(char));
	size_t len = 0;

	while (*text) {
		if (*text == '\\') {
			++text;
			buf[len] = *text;
			len++;
		} else {
			buf[len] = *text;
			len++;
		}

		if (!*text)
			break;
		text++;
	}

	buf[len] = '\0';
	return buf;
}
