/* strings.c -- misc string manipulation function */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
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

#include "helpers.h"

#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <wchar.h>
#if !defined(__HAIKU__) && !defined(__OpenBSD__)
#include <wordexp.h>
#endif
#include "aux.h"
#include "checks.h"
#include "exec.h"
#include "navigation.h"
#include "readline.h"

#ifndef _BE_POSIX
#define CMD_LEN_MAX (PATH_MAX + ((NAME_MAX + 1) << 1))
char len_buf[CMD_LEN_MAX] __attribute__((aligned));
#endif

/* Taken from NNN's source code: very clever */
size_t
xstrsncpy(char *restrict dst, const char *restrict src, size_t n)
{
	char *end = memccpy(dst, src, '\0', n);
	if (!end) {
		dst[n - 1] = '\0';
		end = dst + n;
	}

	return end - dst;
}

size_t
wc_xstrlen(const char *restrict str)
{
	size_t len, _len;
/*#ifndef _BE_POSIX */
	wchar_t *const wbuf = (wchar_t *)len_buf;

	/* Convert multi-byte to wide char */
	_len = mbstowcs(wbuf, str, NAME_MAX);
	int p = wcswidth(wbuf, _len);
	if (p != -1)
		len = p;
	else
		len = 0;
/*#else
	len = u8_xstrlen(str);
#endif */

	return len;
}

/* Truncate an UTF-8 string at length N. Returns zero if truncated and
 * one if not */
int
u8truncstr(char *restrict str, size_t n)
{
	size_t len = 0;

	while (*(str++)) {
		/* Do not count continuation bytes (used by multibyte, that is,
		 * wide or non-ASCII characters) */
		if ((*str & 0xc0) != 0x80) {
			len++;
			if (len == n) {
				*str = '\0';
				return EXIT_SUCCESS;
			}
		}
	}

	return EXIT_FAILURE;
}

/* An strlen implementation able to handle unicode characters. Taken from:
* https://stackoverflow.com/questions/5117393/number-of-character-cells-used-by-string
* Explanation: strlen() counts bytes, not chars. Now, since ASCII chars
* take each 1 byte, the amount of bytes equals the amount of chars.
* However, non-ASCII or wide chars are multibyte chars, that is, one char
* takes more than 1 byte, and this is why strlen() does not work as
* expected for this kind of chars: a 6 chars string might take 12 or
* more bytes */
size_t
u8_xstrlen(const char *restrict str)
{
	size_t len = 0;

	while (*(str++)) {
		if ((*str & 0xc0) != 0x80)
			len++;
	}

	return len;
}

/* Returns the index of the first appearance of c in str, if any, and
 * -1 if c was not found or if no str. NOTE: Same thing as strchr(),
 * except that returns an index, not a pointer */
int
strcntchr(const char *str, const char c)
{
	if (!str)
		return -1;

	register int i = 0;

	while (*str) {
		if (*str == c)
			return i;
		i++;
		str++;
	}

	return -1;
}

/* Returns the index of the last appearance of c in str, if any, and
 * -1 if c was not found or if no str */
int
strcntchrlst(const char *str, const char c)
{
	if (!str)
		return -1;

	register int i = 0;

	int p = -1;
	while (*str) {
		if (*str == c)
			p = i;
		i++;
		str++;
	}

	return p;
}

/* Returns the string after the first appearance of a given char, or
 * returns NULL if C is not found in STR or C is the last char in STR. */
char *
straft(char *str, const char c)
{
	if (!str || !*str || !c)
		return (char *)NULL;

	char *p = str, *q = (char *)NULL;

	while (*p) {
		if (*p == c) {
			q = p;
			break;
		}
		p++;
	}

	/* If C was not found or there is nothing after C */
	if (!q || !*(q + 1))
		return (char *)NULL;

	char *buf = (char *)malloc(strlen(q + 1) + 1);

	if (!buf)
		return (char *)NULL;

	strcpy(buf, q + 1);
	return buf;
}

/* Returns the string after the last appearance of a given char, or
 * NULL if no match */
char *
straftlst(char *str, const char c)
{
	if (!str || !*str || !c)
		return (char *)NULL;

	char *p = str, *q = (char *)NULL;

	while (*p) {
		if (*p == c)
			q = p;
		p++;
	}

	if (!q || !*(q + 1))
		return (char *)NULL;

	char *buf = (char *)malloc(strlen(q + 1) + 1);

	if (!buf)
		return (char *)NULL;

	strcpy(buf, q + 1);
	return buf;
}

/* Returns the substring in str before the first appearance of c. If
 * not found, or C is the first char in STR, returns NULL */
char *
strbfr(char *str, const char c)
{
	if (!str || !*str || !c)
		return (char *)NULL;

	char *p = str, *q = (char *)NULL;
	while (*p) {
		if (*p == c) {
			q = p; /* q is now a pointer to C in STR */
			break;
		}
		p++;
	}

	/* C was not found or it was the first char in STR */
	if (!q || q == str)
		return (char *)NULL;

	*q = '\0';
	/* Now C (because q points to C) is the null byte and STR ends in
	 * C, which is what we want */

	char *buf = (char *)malloc((size_t)(q - str + 1));

	if (!buf) { /* Memory allocation error */
		/* Give back to C its original value, so that STR is not
		 * modified in the process */
		*q = c;
		return (char *)NULL;
	}

	strcpy(buf, str);
	*q = c;
	return buf;
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

	char *buf = (char *)malloc((size_t)(q - str + 1));
	if (!buf) {
		*q = c;
		return (char *)NULL;
	}

	strcpy(buf, str);
	*q = c;
	return buf;
}

/* Returns the string between first ocurrence of A and the first
 * ocurrence of B in STR, or NULL if: there is nothing between A and
 * B, or A and/or B are not found */
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

	char *buf = (char *)malloc((size_t)(pb - pa));

	if (!buf) {
		*pb = b;
		return (char *)NULL;
	}

	strcpy(buf, pa + 1);
	*pb = b;
	return buf;
}

/* Replace the first occurrence of NEEDLE in HAYSTACK by REP */
char *
replace_substr(char *haystack, char *needle, char *rep)
{
	if (!haystack || !*haystack || !needle || !*needle || !rep)
		return (char *)NULL;

	char *ret = strstr(haystack, needle);
	if (!ret)
		return (char *)NULL;

	char *needle_end = ret + strlen(needle);
	*ret = '\0';

	if (*needle_end) {
		size_t rem_len = strlen(needle_end);
		char *rem = (char *)xnmalloc(rem_len + 1, sizeof(char));
		strcpy(rem, needle_end);

		char *new_str = (char *)xnmalloc(strlen(haystack) + strlen(rep)
						+ rem_len + 1, sizeof(char));
		strcpy(new_str, haystack);
		strcat(new_str, rep);
		strcat(new_str, rem);
		free(rem);
		return new_str;
	}

	char *new_str = (char *)xnmalloc(strlen(haystack) + strlen(rep)
					+ 1, sizeof(char));
	strcpy(new_str, haystack);
	strcat(new_str, rep);
	return new_str;
}

/* Generate a random string of LEN bytes using characters from CHARSET */
char *
gen_rand_str(size_t len)
{
	char charset[] = "0123456789#%-_"
			 "abcdefghijklmnopqrstuvwxyz"
			 "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	srand((unsigned int)time(NULL));

	char *str = (char *)malloc((len + 1) * sizeof(char));
	char *p = str;

	if (!p) {
		fprintf(stderr, "Error allocating %zu bytes\n", len);
		return (char *)NULL;
	}

	int i;
	while (len--) {
		i = rand() % (sizeof(charset) - 1);
		*p++ = charset[i];
	}

	*p = '\0';
	return str;
}

/* Removes end of line char and quotes (single and double) from STR.
 * Returns a pointer to the modified STR if the result is non-blank
 * or NULL */
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

	if (!blank)
		return p;
	return (char *)NULL;
}

/* This function takes a string as argument and split it into substrings
 * taking tab, new line char, and space as word delimiters, except when
 * they are preceded by a quote char (single or double quotes) or in
 * case of command substitution ($(cmd) or `cmd`), in which case
 * eveything after the corresponding closing char is taken as one single
 * string. It also escapes spaecial chars. It returns an array of
 * splitted strings (without leading and terminating spaces) or NULL if
 * str is NULL or if no substring was found, i.e., if str contains
 * only spaces. */
char **
split_str(const char *str)
{
	if (!str)
		return (char **)NULL;

	size_t buf_len = 0, words = 0, str_len = 0;
	char *buf = (char *)NULL;
	buf = (char *)xnmalloc(1, sizeof(char));
	int quote = 0, close = 0;
	char **substr = (char **)NULL;

	while (*str) {
		switch (*str) {
		/* Command substitution */
		case '$': /* fallthrough */
		case '`':
			/* Define the closing char: If "$(" then ')', else '`' */
			if (*str == '$') {
				/* If escaped, it has no special meaning */
				if ((str_len && *(str - 1) == '\\') || *(str + 1) != '(') {
					buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
					buf[buf_len++] = *str;
					break;
				} else {
					close = ')';
				}
			} else {
				/* If escaped, it has no special meaning */
				if (str_len && *(str - 1) == '\\') {
					buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
					buf[buf_len++] = *str;
					break;
				} else {
					/* If '`' advance one char. Otherwise the while
					 * below will stop at first char, which is not
					 * what we want */
					close = *str;
					str++;
					buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
					buf[buf_len++] = '`';
				}
			}

			/* Copy everything until null byte or closing char */
			while (*str && *str != close) {
				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *(str++);
			}

			/* If the while loop stopped with a null byte, there was
			 * no ending close (either ')' or '`')*/
			if (!*str) {
				fprintf(stderr, _("%s: Missing '%c'\n"), PROGRAM_NAME,
				    close);

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
			buf = (char *)xrealloc(buf, (buf_len + 2) * sizeof(char *));
			buf[buf_len++] = *str;
			buf[buf_len] = ' ';

			break;

		case '\'': /* fallthrough */
		case '"':
			/* If the quote is escaped, it has no special meaning */
			if (str_len && *(str - 1) == '\\') {
				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *str;
				break;
			}

			/* If not escaped, move on to the next char */
			quote = *str;
			str++;

			/* Copy into the buffer whatever is after the first quote
			 * up to the last quote or NULL */
			while (*str && *str != quote) {
				/* If char has special meaning, escape it */
				if (is_quote_char(*str)) {
					buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
					buf[buf_len++] = '\\';
				}

				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *(str++);
			}

			/* The above while breaks with NULL or quote, so that if
			 * *str is a null byte there was not ending quote */
			if (!*str) {
				fprintf(stderr, _("%s: Missing '%c'\n"), PROGRAM_NAME, quote);
				/* Free the current buffer and whatever was already
				 * allocated */
				free(buf);
				buf = (char *)NULL;
				int i = (int)words;

				while (--i >= 0)
					free(substr[i]);
				free(substr);
				return (char **)NULL;
			}
			break;

		/* TAB, new line char, and space are taken as word breaking
		 * characters */
		case '\t':
		case '\n':
		case ' ':
			/* If escaped, just copy it into the buffer */
			if (str_len && *(str - 1) == '\\') {
				buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
				buf[buf_len++] = *str;
			} else {
				/* If not escaped, break the string */
				/* Add a terminating null byte to the buffer, and, if
				 * not empty, dump the buffer into the substrings
				 * array */
				buf[buf_len] = '\0';

				if (buf_len > 0) {
					substr = (char **)xrealloc(substr, (words + 1) * sizeof(char *));
					substr[words] = savestring(buf, buf_len);
					words++;
				}

				/* Clear te buffer to get a new string */
				memset(buf, '\0', buf_len);
				buf_len = 0;
			}
			break;

		/* If neither a quote nor a breaking word char nor command
		 * substitution, just dump it into the buffer */
		default:
			buf = (char *)xrealloc(buf, (buf_len + 1) * sizeof(char *));
			buf[buf_len++] = *str;
			break;
		}

		str++;
		str_len++;
	}

	/* The while loop stops when the null byte is reached, so that the
	 * last substring is not printed, but still stored in the buffer.
	 * Therefore, we need to add it, if not empty, to our subtrings
	 * array */
	buf[buf_len] = '\0';

	if (buf_len > 0) {
		if (!words)
			substr = (char **)xcalloc(words + 1, sizeof(char *));
		else
			substr = (char **)xrealloc(substr, (words + 1) * sizeof(char *));

		substr[words] = savestring(buf, buf_len);
		words++;
	}

	free(buf);
	buf = (char *)NULL;

	if (words) {
		/* Add a final null string to the array */
		substr = (char **)xrealloc(substr, (words + 1) * sizeof(char *));
		substr[words] = (char *)NULL;

		args_n = words - 1;
		return substr;
	} else {
		args_n = 0; /* Just in case, but I think it's not needed */
		return (char **)NULL;
	}
}

char *
split_fusedcmd(char *str)
{
	if (!str || !*str || *str == ';' || *str == ':' || *str == '\\')
		return (char *)NULL;

	char *space = strchr(str, ' ');
	char *slash = strchr(str, '/');

	if (!space && slash) /* If "/some/path/" */
		return (char *)NULL;

	if (space && slash && slash < space) /* If "/some/string something" */
		return (char *)NULL;

	/* The buffer size is the double of STR, just in case each subtr
	 * needs to be splitted */
	char *buf = (char *)xnmalloc(((strlen(str) * 2) + 2), sizeof(char));

	char *p = str, *pp = str;
	char *q = buf;
	char *s = (char *)NULL;
	size_t c = 0;

	while (*p) {
		if (*p == ' ')
			s = p; /* Pointer to last space */

		/* Transform "cmdeln" into "cmd eln" */
		if (*p >= '0' && *p <= '9' && c && *(p - 1) >= 'a' && *(p - 1) <= 'z') {
			/* If a number, move from last to next space/nul looking for
			 * a slash. If found, do nothing */
			if (s) {
				int _cont = 0;
				char *ss = s + 1;
				while (*ss && *ss != ' ') {
					if (*ss == '/') {
						_cont = 1;
						break;
					}
					ss++;
				}
				if (_cont) {
					*(q++) = *(p++);
					continue;
				}
			}

			char tmp = *p;
			*p = '\0';

			if (!is_internal_c(pp)) {
				*p = tmp;
				*(q++) = *(p++);
				continue;
			}

			*p = tmp;
			*(q++) = ' ';
			*(q++) = *(p++);
		}

		else {
			if (*p == ' ' && *(p + 1))
				pp = p + 1;
			*(q++) = *(p++);
		}

		c++;
	}

	*q = '\0';

	/* Readjust the buffer size */
	size_t len = strlen(buf);
	buf = (char *)xrealloc(buf, (len + 1) * sizeof(char));
	return buf;
}

/*
 * This function is one of the keys of CliFM. It will perform a series of
 * actions:
 * 1) Take the string stored by readline and get its substrings without
 * spaces.
 * 2) In case of user defined variable (var=value), it will pass the
 * whole string to exec_cmd(), which will take care of storing the
 * variable;
 * 3) If the input string begins with ';' or ':' the whole string is
 * send to exec_cmd(), where it will be directly executed by the system
 * shell (via launch_execle()) to prevent all of the expansions made
 * here.
 * 4) The following expansions (especific to CLiFM) are performed here:
 * ELN's, "sel" keyword, ranges of numbers (ELN's), pinned dir and
 * bookmark names, and, for internal commands only, tilde, braces,
 * wildcards, command and paramenter substitution, and regex expansion
 * are performed here as well.
 * These expansions are the most import part of this function.
 */

/* NOTE: Though file names could consist of everything except of slash
 * and null characters, POSIX.1 recommends restricting file names to
 * consist of the following characters: letters (a-z, A-Z), numbers
 * (0-9), period (.), dash (-), and underscore ( _ ).

 * NOTE 2: There is no any need to pass anything to this function, since
 * the input string I need here is already in the readline buffer. So,
 * instead of taking the buffer from a function parameter (str) I could
 * simply use rl_line_buffer. However, since I use this function to
 * parse other strings, like history lines, I need to keep the str
 * argument */
char **
parse_input_str(char *str)
{
	register size_t i = 0;
	int fusedcmd_ok = 0;

	/** ###################### */
	/* Before splitting 'CMDNUM' into 'CMD NUM', make sure CMDNUM is not
	 * a cmd in PATH (for example, md5sum) */
	if (digit_found(str) && !is_bin_cmd(str)) {
		char *p = split_fusedcmd(str);
		if (p) {
			fusedcmd_ok = 1;
			str = p;
			p = (char *)NULL;
		}
	}
	/** ###################### */

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
	if (*str == ';' || *str == ':')
		send_shell = 1;

	if (!send_shell) {
		for (i = 0; str[i]; i++) {

				/* ##################################
				 * #   0.b) CONDITIONAL EXECUTION   #
				 * ##################################*/

			/* Check for chained commands (cmd1;cmd2) */
			if (!chaining && str[i] == ';' && i > 0 && str[i - 1] != '\\')
				chaining = 1;

			/* Check for conditional execution (cmd1 && cmd 2)*/
			if (!cond_cmd && str[i] == '&' && i > 0 && str[i - 1] != '\\'
			&& str[i + 1] && str[i + 1] == '&')
				cond_cmd = 1;

				/* ##################################
				 * #   0.c) USER DEFINED VARIABLE   #
				 * ##################################*/

			/* If user defined variable send the whole string to
			 * exec_cmd(), which will take care of storing the
			 * variable. */
			if (!(flags & IS_USRVAR_DEF) && str[i] == '=' && i > 0
			&& str[i - 1] != '\\' && str[0] != '=') {
				/* Remove leading spaces. This: '   a="test"' should be
				 * taken as a valid variable declaration */
				char *p = str;
				while (*p == ' ' || *p == '\t')
					p++;

				/* If first non-space is a number, it's not a variable
				 * name */
				if (!_ISDIGIT(*p)) {
					int space_found = 0;
					/* If there are no spaces before '=', take it as a
					 * variable. This check is done in order to avoid
					 * taking as a variable things like:
					 * 'ls -color=auto' */
					while (*p != '=') {
						if (*(p++) == ' ')
							space_found = 1;
					}

					if (!space_found)
						flags |= IS_USRVAR_DEF;
				}

				p = (char *)NULL;
			}
		}
	}

	/* If chained commands, check each of them. If at least one of them
	 * is internal, take care of the job (the system shell does not know
	 * our internal commands and therefore cannot execute them); else,
	 * if no internal command is found, let it to the system shell */
	if (chaining || cond_cmd) {
		/* User defined variables are always internal, so that there is
		 * no need to check whatever else is in the command string */
		if (flags & IS_USRVAR_DEF) {
			exec_chained_cmds(str);
			if (fusedcmd_ok)
				free(str);
			return (char **)NULL;
		}

		register size_t j = 0;
		size_t str_len = strlen(str), len = 0, internal_ok = 0;
		char *buf = (char *)NULL;

		/* Get each word (cmd) in STR */
		buf = (char *)xcalloc(str_len + 1, sizeof(char));
		for (j = 0; j < str_len; j++) {
			while (str[j] && str[j] != ' ' && str[j] != ';' && str[j] != '&') {
				buf[len++] = str[j++];
			}

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
		buf = (char *)NULL;

		if (internal_ok) {
			exec_chained_cmds(str);
			if (fusedcmd_ok)
				free(str);
			return (char **)NULL;
		}
	}

	if (flags & IS_USRVAR_DEF || send_shell) {
		/* Remove leading spaces, again */
		char *p = str;
		while (*p == ' ' || *p == '\t')
			p++;

		args_n = 0;

		char **cmd = (char **)NULL;
		cmd = (char **)xnmalloc(2, sizeof(char *));
		cmd[0] = savestring(p, strlen(p));
		cmd[1] = (char *)NULL;

		p = (char *)NULL;

		if (fusedcmd_ok)
			free(str);

		return cmd;
		/* If ";cmd" or ":cmd" the whole input line will be send to
		 * exec_cmd() and will be executed by the system shell via
		 * execle(). Since we don't run split_str() here, dequoting
		 * and deescaping is performed directly by the system shell */
	}

		/* ################################################
		 * #     1) SPLIT INPUT STRING INTO SUBSTRINGS    #
		 * ################################################ */

	/* split_str() returns an array of strings without leading,
	 * terminating and double spaces. */
	char **substr = split_str(str);

	/** ###################### */
	if (fusedcmd_ok) /* Just in case split_fusedcmd returned NULL */
		free(str);
	/** ###################### */

	/* NOTE: isspace() not only checks for space, but also for new line,
	 * carriage return, vertical and horizontal TAB. Be careful when
	 * replacing this function. */

	if (!substr)
		return (char **)NULL;

	/* Handle background/foreground process */
	bg_proc = 0;

	if (*substr[args_n] == '&' && !*(substr[args_n] + 1)) {
		bg_proc = 1;
		free(substr[args_n]);
		substr[args_n--] = (char *)NULL;
	} else {
		size_t len = strlen(substr[args_n]);
		if (substr[args_n][len - 1] == '&' && !substr[args_n][len]) {
			substr[args_n][len - 1] = '\0';
			bg_proc = 1;
		}
	}

					/* ######################
					 * #     TRASH AS RM    #
					 * ###################### */
#ifndef _NOTRASH
	if (tr_as_rm && substr[0] && *substr[0] == 'r' && !substr[0][1]) {
		substr[0] = (char *)xrealloc(substr[0], 3 * sizeof(char));
		*substr[0] = 't';
		substr[0][1] = 'r';
		substr[0][2] = '\0';
	}
#endif
				/* ##############################
				 * #   2) BUILTIN EXPANSIONS    #
				 * ##############################

	 * Ranges, sel, ELN, pinned dirs, bookmarks, and internal variables.
	 * These expansions are specific to CliFM. To be able to use them
	 * even with external commands, they must be expanded here, before
	 * sending the input string, in case the command is external, to
	 * the system shell */

	is_sel = 0, sel_is_last = 0;

	size_t int_array_max = 10, ranges_ok = 0;
	int *range_array = (int *)xnmalloc(int_array_max, sizeof(int));

	for (i = 0; i <= args_n; i++) {
		if (!substr[i])
			continue;

		register size_t j = 0;
		/* Replace . and .. by absolute paths */
		if (*substr[i] == '.' && (!substr[i][1] || (substr[i][1] == '.'
		&& !substr[i][2]))) {
			char *tmp = (char *)NULL;
			tmp = realpath(substr[i], NULL);
			substr[i] = (char *)xrealloc(substr[i], (strlen(tmp) + 1)
											* sizeof(char));
			strcpy(substr[i], tmp);
			free(tmp);
		}

			/* ######################################
			 * #     2.a) FASTBACK EXPANSION        #
			 * ###################################### */

		if (*substr[i] == '.' && substr[i][1] == '.' && substr[i][2] == '.') {
			char *tmp = fastback(substr[i]);
			if (tmp) {
				substr[i] = (char *)xrealloc(substr[i], (strlen(tmp) + 1)
														* sizeof(char));
				strcpy(substr[i], tmp);
				free(tmp);
			}
		}

			/* ######################################
			 * #     2.b) PINNED DIR EXPANSION      #
			 * ###################################### */

		if (*substr[i] == ',' && !substr[i][1] && pinned_dir) {
			substr[i] = (char *)xrealloc(substr[i], (strlen(pinned_dir) + 1)
													* sizeof(char));
			strcpy(substr[i], pinned_dir);
		}

			/* ######################################
			 * #      2.c) BOOKMARKS EXPANSION      #
			 * ###################################### */

		/* Expand bookmark names into paths */
		if (expand_bookmarks) {
			int bm_exp = 0;

			for (j = 0; j < bm_n; j++) {
				if (bookmarks[j].name && *substr[i] == *bookmarks[j].name
				&& strcmp(substr[i], bookmarks[j].name) == 0) {

					/* Do not expand bookmark names that conflicts
					 * with a file name in CWD */
					int conflict = 0, k = (int)files;
					while (--k >= 0) {
						if (*bookmarks[j].name == *file_info[k].name
						&& strcmp(bookmarks[j].name, file_info[k].name) == 0) {
							conflict = 1;
							break;
						}
					}

					if (!conflict && bookmarks[j].path) {
						substr[i] = (char *)xrealloc(substr[i],
						    (strlen(bookmarks[j].path) + 1) * sizeof(char));
						strcpy(substr[i], bookmarks[j].path);

						bm_exp = 1;

						break;
					}
				}
			}

			/* Do not perform further checks on the expanded bookmark */
			if (bm_exp)
				continue;
		}

		/* ############################################# */

		size_t substr_len = strlen(substr[i]);

		/* Check for ranges */
		for (j = 0; substr[i][j]; j++) {
			/* If some alphabetic char, besides '-', is found in the
			 * string, we have no range */
			if (substr[i][j] != '-' && !_ISDIGIT(substr[i][j]))
				break;

			/* If a range is found, store its index */
			if (j > 0 && j < substr_len && substr[i][j] == '-' &&
			    _ISDIGIT(substr[i][j - 1]) && _ISDIGIT(substr[i][j + 1]))
				if (ranges_ok < int_array_max)
					range_array[ranges_ok++] = (int)i;
		}

		/* Expand 'sel' only as an argument, not as command */
		if (i > 0 && *substr[i] == 's' && strcmp(substr[i], "sel") == 0)
			is_sel = (short)i;
	}

			/* ####################################
			 * #       2.d) RANGES EXPANSION      #
			 * ####################################*/

	/* Expand expressions like "1-3" to "1 2 3" if all the numbers in
	  * the range correspond to an ELN */

	if (ranges_ok) {
		size_t old_ranges_n = 0;
		register size_t r = 0;

		for (r = 0; r < ranges_ok; r++) {
			size_t ranges_n = 0;
			int *ranges = expand_range(substr[range_array[r] +
							  (int)old_ranges_n], 1);
			if (ranges) {
				register size_t j = 0;

				for (ranges_n = 0; ranges[ranges_n]; ranges_n++);

				char **ranges_cmd = (char **)NULL;
				ranges_cmd = (char **)xcalloc(args_n + ranges_n + 2,
				    sizeof(char *));

				for (i = 0; i < (size_t)range_array[r] + old_ranges_n; i++)
					ranges_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				for (i = 0; i < ranges_n; i++) {
					ranges_cmd[j] = (char *)xcalloc((size_t)DIGINUM(ranges[i])
													+ 1, sizeof(int));
					sprintf(ranges_cmd[j++], "%d", ranges[i]);
				}

				for (i = (size_t)range_array[r] + old_ranges_n + 1;
				     i <= args_n; i++) {
					ranges_cmd[j++] = savestring(substr[i],
					    strlen(substr[i]));
				}

				ranges_cmd[j] = NULL;
				free(ranges);

				for (i = 0; i <= args_n; i++)
					free(substr[i]);

				substr = (char **)xrealloc(substr, (args_n + ranges_n + 2)
													* sizeof(char *));

				for (i = 0; i < j; i++) {
					substr[i] = savestring(ranges_cmd[i], strlen(ranges_cmd[i]));
					free(ranges_cmd[i]);
				}

				free(ranges_cmd);
				args_n = j - 1;
			}

			old_ranges_n += (ranges_n - 1);
		}
	}

	free(range_array);

				/* ##########################
				 * #   2.e) SEL EXPANSION   #
				 * ##########################*/

	/*  if (is_sel && *substr[0] != '/') { */
	if (is_sel) {
		if ((size_t)is_sel == args_n)
			sel_is_last = 1;

		if (sel_n) {
			register size_t j = 0;
			char **sel_array = (char **)NULL;
			sel_array = (char **)xnmalloc(args_n + sel_n + 2, sizeof(char *));

			for (i = 0; i < (size_t)is_sel; i++) {
				if (!substr[i])
					continue;
				sel_array[j++] = savestring(substr[i], strlen(substr[i]));
			}

			for (i = 0; i < sel_n; i++) {
				/* Escape selected file names and copy them into tmp
				 * array */
				char *esc_str = escape_str(sel_elements[i]);
				if (esc_str) {
					sel_array[j++] = savestring(esc_str, strlen(esc_str));
					free(esc_str);
					esc_str = (char *)NULL;
				} else {
					fprintf(stderr, _("%s: %s: Error quoting file name\n"),
					    PROGRAM_NAME, sel_elements[j]);
					/* Free elements selected thus far and and all the
					 * input substrings */
					register size_t k = 0;
					for (k = 0; k < j; k++)
						free(sel_array[k]);
					free(sel_array);

					for (k = 0; k <= args_n; k++)
						free(substr[k]);
					free(substr);

					return (char **)NULL;
				}
			}

			for (i = (size_t)is_sel + 1; i <= args_n; i++)
				sel_array[j++] = savestring(substr[i], strlen(substr[i]));

			for (i = 0; i <= args_n; i++)
				free(substr[i]);

			substr = (char **)xrealloc(substr, (args_n + sel_n + 2)
										* sizeof(char *));

			for (i = 0; i < j; i++) {
				substr[i] = savestring(sel_array[i], strlen(sel_array[i]));
				free(sel_array[i]);
			}

			free(sel_array);
			substr[i] = (char *)NULL;
			args_n = j - 1;
		}

		else {
			/* 'sel' is an argument, but there are no selected files. */
			fprintf(stderr, _("%c%s: There are no selected files%c"),
			    kb_shortcut ? '\n' : '\0', PROGRAM_NAME,
			    kb_shortcut ? '\0' : '\n');

			register size_t j = 0;
			for (j = 0; j <= args_n; j++)
				free(substr[j]);
			free(substr);

			return (char **)NULL;
		}
	}

	int stdin_dir_ok = 0;
	if (STDIN_TMP_DIR && strcmp(ws[cur_ws].path, STDIN_TMP_DIR) == 0)
		stdin_dir_ok = 1;

	for (i = 0; i <= args_n; i++) {
		if (!substr[i])
			continue;

				/* ##########################
				 * #   2.f) ELN EXPANSION   #
				 * ##########################*/

		/* If autocd is set to false, i must be bigger than zero because
		 * the first string in comm_array, the command name, should NOT
		 * be expanded, but only arguments. Otherwise, if the expanded
		 * ELN happens to be a program name as well, this program will
		 * be executed, and this, for sure, is to be avoided */

		/* The 'sort', 'mf', 'ws', and 'jo' commands take digits as
		 * arguments. So, do not expand ELN's in these cases */
		if (substr[0] && strcmp(substr[0], "mf") != 0
		&& strcmp(substr[0], "st") != 0 && strcmp(substr[0], "ws") != 0
		&& strcmp(substr[0], "sort") != 0 && strcmp(substr[0], "jo") != 0) {

			if (is_number(substr[i])) {
				/* Expand first word only if autocd is set to true */
				if ((i == 0 && !autocd && !auto_open) || !substr[i])
					continue;

				int num = atoi(substr[i]);
				/* Expand numbers only if there is a corresponding ELN */

				/* Do not expand ELN if there is a file named as the
				 * ELN */
				if (eln_as_file_n) {
					int conflict = 0;
					if (eln_as_file_n > 1) {
						size_t j;

						for (j = 0; j < eln_as_file_n; j++) {
							if (atoi(file_info[eln_as_file[j]].name) == num) {
								conflict = num;
								/* One conflicting file name is enough */
								break;
							}
						}
					} else {
						if (atoi(file_info[eln_as_file[0]].name) == num)
							conflict = num;
					}

					if (conflict) {
						size_t j;

						for (j = 0; j <= args_n; j++)
							free(substr[j]);
						free(substr);

						fprintf(stderr, _("%s: %d: ELN-filename "
							"conflict. Bypass internal expansions "
							"to fix this issue: ';CMD "
							"FILENAME'\n"), PROGRAM_NAME, conflict);
						return (char **)NULL;
					}
				}

				if (num > 0 && num <= (int)files) {
					/* Replace the ELN by the corresponding escaped
					 * file name */
					int j = num - 1;
					char *esc_str = escape_str(file_info[j].name);

					if (esc_str) {
						if (file_info[j].dir &&
						    file_info[j].name[file_info[j].len - 1] != '/') {
							substr[i] = (char *)xrealloc(substr[i],
							    (strlen(esc_str) + 2) * sizeof(char));
							sprintf(substr[i], "%s/", esc_str);
						} else {
							substr[i] = (char *)xrealloc(substr[i],
							    (strlen(esc_str) + 1) * sizeof(char));
							strcpy(substr[i], esc_str);
						}

						free(esc_str);
						esc_str = (char *)NULL;
					} else {
						fprintf(stderr, _("%s: %s: Error quoting "
								"file name\n"),
								PROGRAM_NAME, file_info[num - 1].name);
						/* Free whatever was allocated thus far */

						for (j = 0; j <= (int)args_n; j++)
							free(substr[j]);
						free(substr);
						return (char **)NULL;
					}
				}
			}
		}

		/* #############################################
		 * #   2.g) USER DEFINED VARIABLES EXPANSION   #
		 * #############################################*/

		if (substr[i][0] == '$' && substr[i][1] != '(' && substr[i][1] != '{') {
			char *var_name = strchr(substr[i], '$');
			if (var_name && *(++var_name)) {
				int j = (int)usrvar_n;
				while (--j >= 0) {
					if (*var_name == *usr_var[j].name
					&& strcmp(var_name, usr_var[j].name) == 0) {
						substr[i] = (char *)xrealloc(substr[i],
						    (strlen(usr_var[j].value) + 1) * sizeof(char));
						strcpy(substr[i], usr_var[j].value);
						break;
					}
				}
			} else {
				fprintf(stderr, _("%s: %s: Error getting variable name\n"),
						PROGRAM_NAME, substr[i]);
				size_t j;
				for (j = 0; j <= args_n; j++)
					free(substr[j]);
				free(substr);
				return (char **)NULL;
			}
		}

		/* We are in STDIN_TMP_DIR: Expand symlinks to target */
		if (stdin_dir_ok) {
			char *real_path = realpath(substr[i], NULL);
			if (real_path) {
				substr[i] = (char *)xrealloc(substr[i],
				    (strlen(real_path) + 1) * sizeof(char));
				strcpy(substr[i], real_path);
				free(real_path);
			}
		}
	}

	/* #### 3) NULL TERMINATE THE INPUT STRING ARRAY #### */
	substr = (char **)xrealloc(substr, sizeof(char *) * (args_n + 2));
	substr[args_n + 1] = (char *)NULL;

	if (!is_internal(substr[0]))
		return substr;

	/* #############################################################
	 * #               ONLY FOR INTERNAL COMMANDS                  #
	 * #############################################################*/

	/* Some functions of CliFM are purely internal, that is, they are not
	 * wrappers of a shell command and do not call the system shell at all.
	 * For this reason, some expansions normally made by the system shell
	 * must be made here (in the lobby [got it?]) in order to be able to
	 * understand these expansions at all. */

		/* ###############################################
		 * #   3) WILDCARD, BRACE, AND TILDE EXPANSION   #
		 * ############################################### */

	int *glob_array = (int *)xnmalloc(int_array_max, sizeof(int));
#if !defined(__HAIKU__) && !defined(__OpenBSD__)
	int *word_array = (int *)xnmalloc(int_array_max, sizeof(int));
#endif
	size_t glob_n = 0, word_n = 0;

	for (i = 0; substr[i]; i++) {

		/* Do not perform any of the expansions below for selected
		 * elements: they are full path file names that, as such, do not
		 * need any expansion */
		if (is_sel) { /* is_sel is true only for the current input and if
			there was some "sel" keyword in it */
			/* Strings between is_sel and sel_n are selected file names */
			if (i >= (size_t)is_sel && i <= sel_n)
				continue;
		}

		/* Ignore the first string of the search function: it will be
		 * expanded by the search function itself */
		if (substr[0][0] == '/' && i == 0)
			continue;

		/* Tilde expansion is made by glob() */
		if (*substr[i] == '~') {
			if (glob_n < int_array_max)
				glob_array[glob_n++] = (int)i;
		}

		register size_t j = 0;
		for (j = 0; substr[i][j]; j++) {
			/* Brace and wildcard expansion is made by glob()
			 * as well */
			if ((substr[i][j] == '*' || substr[i][j] == '?'
			|| substr[i][j] == '[' || substr[i][j] == '{')
			&& substr[i][j + 1] != ' ') {
				/* Strings containing these characters are taken as
			 * wildacard patterns and are expanded by the glob
			 * function. See man (7) glob */
				if (glob_n < int_array_max)
					glob_array[glob_n++] = (int)i;
			}

#if !defined(__HAIKU__) && !defined(__OpenBSD__)
			/* Command substitution is made by wordexp() */
			if (substr[i][j] == '$' && (substr[i][j + 1] == '('
			|| substr[i][j + 1] == '{')) {
				if (word_n < int_array_max)
					word_array[word_n++] = (int)i;
			}

			if (substr[i][j] == '`' && substr[i][j + 1] != ' ') {
				if (word_n < int_array_max)
					word_array[word_n++] = (int)i;
			}
#endif /* __HAIKU__ */
		}
	}

	/* Do not expand if command is deselect, sel or untrash, just to
	 * allow the use of "*" for desel and untrash ("ds *" and "u *")
	 * and to let the sel function handle patterns itself */
	if (glob_n && strcmp(substr[0], "s") != 0 && strcmp(substr[0], "sel") != 0
	&& strcmp(substr[0], "ds") != 0 && strcmp(substr[0], "desel") != 0
	&& strcmp(substr[0], "u") != 0 && strcmp(substr[0], "undel") != 0
	&& strcmp(substr[0], "untrash") != 0) {
		/* 1) Expand glob
		2) Create a new array, say comm_array_glob, large enough to store
		   the expanded glob and the remaining (non-glob) arguments
		   (args_n+gl_pathc)
		3) Copy into this array everything before the glob
		   (i=0;i<glob_char;i++)
		4) Copy the expanded elements (if none, copy the original element,
		   comm_array[glob_char])
		5) Copy the remaining elements (i=glob_char+1;i<=args_n;i++)
		6) Free the old comm_array and fill it with comm_array_glob
	  */
		size_t old_pathc = 0;
		/* glob_array stores the index of the globbed strings. However,
		 * once the first expansion is done, the index of the next globbed
		 * string has changed. To recover the next globbed string, and
		 * more precisely, its index, we only need to add the amount of
		 * files matched by the previous instances of glob(). Example:
		 * if original indexes were 2 and 4, once 2 is expanded 4 stores
		 * now some of the files expanded in 2. But if we add to 4 the
		 * amount of files expanded in 2 (gl_pathc), we get now the
		 * original globbed string pointed by 4.
		*/
		register size_t g = 0;
		for (g = 0; g < (size_t)glob_n; g++) {
			glob_t globbuf;

			if (glob(substr[glob_array[g] + (int)old_pathc],
				GLOB_BRACE | GLOB_TILDE, NULL, &globbuf) != EXIT_SUCCESS) {
				globfree(&globbuf);
				continue;
			}

			if (globbuf.gl_pathc) {
				register size_t j = 0;
				char **glob_cmd = (char **)NULL;
				glob_cmd = (char **)xcalloc(args_n + globbuf.gl_pathc + 1,
											sizeof(char *));

				for (i = 0; i < ((size_t)glob_array[g] + old_pathc); i++)
					glob_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				for (i = 0; i < globbuf.gl_pathc; i++) {
					/* Do not match "." or ".." */
					if (strcmp(globbuf.gl_pathv[i], ".") == 0
					|| strcmp(globbuf.gl_pathv[i], "..") == 0)
						continue;

					/* Escape the globbed file name and copy it */
					char *esc_str = escape_str(globbuf.gl_pathv[i]);
					if (esc_str) {
						glob_cmd[j++] = savestring(esc_str, strlen(esc_str));
						free(esc_str);
					} else {
						fprintf(stderr, _("%s: %s: Error quoting "
							"file name\n"), PROGRAM_NAME, globbuf.gl_pathv[i]);
						register size_t k = 0;
						for (k = 0; k < j; k++)
							free(glob_cmd[k]);
						free(glob_cmd);
						glob_cmd = (char **)NULL;

						for (k = 0; k <= args_n; k++)
							free(substr[k]);
						free(substr);
						globfree(&globbuf);
						return (char **)NULL;
					}
				}

				for (i = (size_t)glob_array[g] + old_pathc + 1;
				i <= args_n; i++)
					glob_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				glob_cmd[j] = (char *)NULL;

				for (i = 0; i <= args_n; i++)
					free(substr[i]);
				free(substr);

				substr = glob_cmd;
				glob_cmd = (char **)NULL;
				args_n = j - 1;
			}

			old_pathc += (globbuf.gl_pathc - 1);
			globfree(&globbuf);
		}
	}

	free(glob_array);

		/* #############################################
		 * #    4) COMMAND & PARAMETER SUBSTITUTION    #
		 * ############################################# */
#if !defined(__HAIKU__) && !defined(__OpenBSD__)
	if (word_n) {
		size_t old_pathc = 0;
		register size_t w = 0;
		for (w = 0; w < (size_t)word_n; w++) {
			wordexp_t wordbuf;
			if (wordexp(substr[word_array[w] + (int)old_pathc],
				&wordbuf, 0) != EXIT_SUCCESS) {
				wordfree(&wordbuf);
				continue;
			}

			if (wordbuf.we_wordc) {
				register size_t j = 0;
				char **word_cmd = (char **)NULL;

				word_cmd = (char **)xcalloc(args_n + wordbuf.we_wordc + 1,
											sizeof(char *));

				for (i = 0; i < ((size_t)word_array[w] + old_pathc); i++)
					word_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				for (i = 0; i < wordbuf.we_wordc; i++) {
					/* Escape the globbed file name and copy it*/
					char *esc_str = escape_str(wordbuf.we_wordv[i]);
					if (esc_str) {
						word_cmd[j++] = savestring(esc_str, strlen(esc_str));
						free(esc_str);
					} else {
						fprintf(stderr, _("%s: %s: Error quoting "
							"file name\n"), PROGRAM_NAME, wordbuf.we_wordv[i]);

						register size_t k = 0;
						for (k = 0; k < j; k++)
							free(word_cmd[k]);
						free(word_cmd);

						word_cmd = (char **)NULL;

						for (k = 0; k <= args_n; k++)
							free(substr[k]);
						free(substr);
						return (char **)NULL;
					}
				}

				for (i = (size_t)word_array[w] + old_pathc + 1;
				i <= args_n; i++)
					word_cmd[j++] = savestring(substr[i], strlen(substr[i]));

				word_cmd[j] = (char *)NULL;

				for (i = 0; i <= args_n; i++)
					free(substr[i]);
				free(substr);
				substr = word_cmd;
				word_cmd = (char **)NULL;
				args_n = j - 1;
			}

			old_pathc += (wordbuf.we_wordc - 1);
			wordfree(&wordbuf);
		}
	}

	free(word_array);
#endif /* __HAIKU__ */

	if (substr[0] && (*substr[0] == 'd' || *substr[0] == 'u')
	&& (strcmp(substr[0], "desel") == 0 || strcmp(substr[0], "undel") == 0
	|| strcmp(substr[0], "untrash") == 0)) {
		/* Null terminate the input string array (again) */
		substr = (char **)xrealloc(substr, (args_n + 2) * sizeof(char *));
		substr[args_n + 1] = (char *)NULL;
		return substr;
	}

		/* #############################################
		 * #             5) REGEX EXPANSION            #
		 * ############################################# */

	if (*substr[0] == 's' && (!substr[0][1] || strcmp(substr[0], "sel") == 0))
		return substr;

	char **regex_files = (char **)xnmalloc(files + args_n + 2, sizeof(char *));
	size_t j, r_files = 0;

	for (i = 0; substr[i]; i++) {
		if (r_files > (files + args_n))
			break;

		/* Ignore the first string of the search function: it will be
		 * expanded by the search function itself */
		if (*substr[0] == '/') {
			regex_files[r_files++] = substr[i];
			continue;
		}

		if (check_regex(substr[i]) != EXIT_SUCCESS) {
			regex_files[r_files++] = substr[i];
			continue;
		}

		regex_t regex;
		if (regcomp(&regex, substr[i], REG_NOSUB | REG_EXTENDED) != EXIT_SUCCESS) {
			/*          fprintf(stderr, "%s: %s: Invalid regular expression",
					PROGRAM_NAME, substr[i]); */
			regfree(&regex);
			regex_files[r_files++] = substr[i];
			continue;
		}

		int reg_found = 0;

		for (j = 0; j < files; j++) {
			if (regexec(&regex, file_info[j].name, 0, NULL, 0) == EXIT_SUCCESS) {
				regex_files[r_files++] = file_info[j].name;
				reg_found = 1;
			}
		}

		if (!reg_found)
			regex_files[r_files++] = substr[i];

		regfree(&regex);
	}

	if (r_files) {
		regex_files[r_files] = (char *)NULL;
		char **tmp_files = (char **)xnmalloc(r_files + 2, sizeof(char *));
		size_t k = 0;
		for (j = 0; regex_files[j]; j++)
			tmp_files[k++] = savestring(regex_files[j], strlen(regex_files[j]));
		tmp_files[k] = (char *)NULL;

		for (j = 0; j <= args_n; j++)
			free(substr[j]);
		free(substr);

		substr = tmp_files;
		tmp_files = (char **)NULL;
		args_n = k - 1;
		free(tmp_files);
	}

	free(regex_files);
	substr = (char **)xrealloc(substr, (args_n + 2) * sizeof(char *));
	substr[args_n + 1] = (char *)NULL;
	return substr;
}

/* Reduce "$HOME" to tilde ("~"). The new_path variable is always either
 * "$HOME" or "$HOME/file", that's why there's no need to check for
 * "/file" */
char *
home_tilde(const char *new_path)
{
	if (!home_ok || !new_path || !*new_path)
		return (char *)NULL;

	char *path_tilde = (char *)NULL;

	/* If path == HOME */
	if (new_path[1] == user.home[1] && strcmp(new_path, user.home) == 0) {
		path_tilde = (char *)xnmalloc(2, sizeof(char));
		path_tilde[0] = '~';
		path_tilde[1] = '\0';
	} else if (new_path[1] == user.home[1]
	&& strncmp(new_path, user.home, user.home_len) == 0) {
		/* If path == HOME/file */
		path_tilde = (char *)xnmalloc(strlen(new_path + user.home_len + 1) + 3,
										sizeof(char));
		sprintf(path_tilde, "~/%s", new_path + user.home_len + 1);
	} else {
		path_tilde = (char *)xnmalloc(strlen(new_path) + 1, sizeof(char));
		strcpy(path_tilde, new_path);
	}

	return path_tilde;
}

/* Expand a range of numbers given by str. It will expand the range
 * provided that both extremes are numbers, bigger than zero, equal or
 * smaller than the amount of files currently listed on the screen, and
 * the second (right) extreme is bigger than the first (left). Returns
 * an array of int's with the expanded range or NULL if one of the
 * above conditions is not met */
int *
expand_range(char *str, int listdir)
{
	if (strcntchr(str, '-') == -1)
		return (int *)NULL;

	char *first = (char *)NULL;
	first = strbfr(str, '-');

	if (!first)
		return (int *)NULL;

	if (!is_number(first)) {
		free(first);
		return (int *)NULL;
	}

	char *second = (char *)NULL;
	second = straft(str, '-');

	if (!second) {
		free(first);
		return (int *)NULL;
	}

	if (!is_number(second)) {
		free(first);
		free(second);
		return (int *)NULL;
	}

	int afirst = atoi(first), asecond = atoi(second);
	free(first);
	free(second);

	if (listdir) {
		if (afirst <= 0 || afirst > (int)files || asecond <= 0
		|| asecond > (int)files || afirst >= asecond)
			return (int *)NULL;
	} else {
		if (afirst >= asecond)
			return (int *)NULL;
	}

	int *buf = (int *)NULL;
	buf = (int *)xcalloc((size_t)(asecond - afirst) + 2, sizeof(int));

	size_t i, j = 0;
	for (i = (size_t)afirst; i <= (size_t)asecond; i++)
		buf[j++] = (int)i;

	return buf;
}

/* used a lot.
 * creates a copy of a string */
char *
savestring(const char *restrict str, size_t size)
{
	if (!str)
		return (char *)NULL;

	char *ptr = (char *)NULL;
	ptr = (char *)malloc((size + 1) * sizeof(char));

	if (!ptr)
		return (char *)NULL;
	strcpy(ptr, str);

	return ptr;
}

/* Take a string and returns the same string escaped. If nothing to be
 * escaped, the original string is returned */
char *
escape_str(const char *str)
{
	if (!str)
		return (char *)NULL;

	size_t len = 0;
	char *buf = (char *)NULL;

	buf = (char *)xnmalloc(strlen(str) * 2 + 1, sizeof(char));

	while (*str) {
		if (is_quote_char(*str))
			buf[len++] = '\\';
		buf[len++] = *(str++);
	}

	buf[len] = '\0';
	return buf;
}

/* Get all substrings from STR using IFS as substring separator, and,
 * if there is a range, expand it. Returns an array containing all
 * substrings in STR plus expandes ranges, or NULL if: STR is NULL or
 * empty, STR contains only IFS(s), or in case of memory allocation
 * error */
char **
get_substr(char *str, const char ifs)
{
	if (!str || *str == '\0')
		return (char **)NULL;

	/* ############## SPLIT THE STRING #######################*/

	char **substr = (char **)NULL;
	void *p = (char *)NULL;
	size_t str_len = strlen(str);
	size_t length = 0, substr_n = 0;
	char *buf = (char *)xnmalloc(str_len + 1, sizeof(char));

	while (*str) {
		while (*str != ifs && *str != '\0' && length < (str_len + 1))
			buf[length++] = *(str++);
		if (length) {
			buf[length] = '\0';
			p = (char *)realloc(substr, (substr_n + 1) * sizeof(char *));
			if (!p) {
				/* Free whatever was allocated so far */
				size_t i;
				for (i = 0; i < substr_n; i++)
					free(substr[i]);
				free(substr);
				return (char **)NULL;
			}
			substr = (char **)p;
			p = (char *)calloc(length + 1, sizeof(char));

			if (!p) {
				size_t i;
				for (i = 0; i < substr_n; i++)
					free(substr[i]);
				free(substr);
				return (char **)NULL;
			}

			substr[substr_n] = p;
			p = (char *)NULL;
			strncpy(substr[substr_n++], buf, length);
			length = 0;
		} else {
			str++;
		}
	}

	free(buf);

	if (!substr_n)
		return (char **)NULL;

	size_t i = 0, j = 0;
	p = (char *)realloc(substr, (substr_n + 1) * sizeof(char *));

	if (!p) {
		for (i = 0; i < substr_n; i++)
			free(substr[i]);
		free(substr);
		substr = (char **)NULL;
		return (char **)NULL;
	}

	substr = (char **)p;
	p = (char *)NULL;
	substr[substr_n] = (char *)NULL;

	/* ################### EXPAND RANGES ######################*/

	int afirst = 0, asecond = 0;

	for (i = 0; substr[i]; i++) {
		/* Check if substr is a valid range */
		int ranges_ok = 0;
		/* If range, get both extremes of it */
		for (j = 1; substr[i][j]; j++) {
			if (substr[i][j] == '-') {
				/* Get strings before and after the dash */
				char *first = strbfr(substr[i], '-');
				if (!first)
					break;

				char *second = straft(substr[i], '-');
				if (!second) {
					free(first);
					break;
				}

				/* Make sure it is a valid range */
				if (is_number(first) && is_number(second)) {
					afirst = atoi(first), asecond = atoi(second);
					if (asecond <= afirst) {
						free(first);
						free(second);
						break;
					}

					/* We have a valid range */
					ranges_ok = 1;
					free(first);
					free(second);
				} else {
					free(first);
					free(second);
					break;
				}
			}
		}

		if (!ranges_ok)
			continue;

		/* If a valid range */
		size_t k = 0, next = 0;
		char **rbuf = (char **)NULL;
		rbuf = (char **)xcalloc((substr_n + (size_t)(asecond - afirst) + 1),
								sizeof(char *));

		/* Copy everything before the range expression
		 * into the buffer */
		for (j = 0; j < i; j++)
			rbuf[k++] = savestring(substr[j], strlen(substr[j]));

		/* Copy the expanded range into the buffer */
		for (j = (size_t)afirst; j <= (size_t)asecond; j++) {
			rbuf[k] = (char *)xcalloc((size_t)DIGINUM((int)j) + 1, sizeof(char));
			sprintf(rbuf[k++], "%zu", j);
		}

		/* Copy everything after the range expression into
		 * the buffer, if anything */
		if (substr[i + 1]) {
			next = k;
			for (j = (i + 1); substr[j]; j++) {
				rbuf[k++] = savestring(substr[j], strlen(substr[j]));
			}
		} else { /* If there's nothing after last range, there's no next
		either */
			next = 0;
		}

		/* Repopulate the original array with the expanded range and
		 * remaining strings */
		substr_n = k;
		for (j = 0; substr[j]; j++)
			free(substr[j]);

		substr = (char **)xrealloc(substr, (substr_n + 1) * sizeof(char *));

		for (j = 0; j < substr_n; j++) {
			substr[j] = savestring(rbuf[j], strlen(rbuf[j]));
			free(rbuf[j]);
		}

		free(rbuf);
		substr[j] = (char *)NULL;

		/* Proceede only if there's something after the last range */
		if (next)
			i = next;
		else
			break;
	}

	/* ############## REMOVE DUPLICATES ###############*/

	char **dstr = (char **)NULL;
	size_t len = 0, d;

	for (i = 0; i < substr_n; i++) {
		int duplicate = 0;
		for (d = (i + 1); d < substr_n; d++) {
			if (*substr[i] == *substr[d] && strcmp(substr[i], substr[d]) == 0) {
				duplicate = 1;
				break;
			}
		}

		if (duplicate) {
			free(substr[i]);
			continue;
		}

		dstr = (char **)xrealloc(dstr, (len + 1) * sizeof(char *));
		dstr[len++] = savestring(substr[i], strlen(substr[i]));
		free(substr[i]);
	}

	free(substr);
	dstr = (char **)xrealloc(dstr, (len + 1) * sizeof(char *));
	dstr[len] = (char *)NULL;
	return dstr;
}

/* This function simply deescapes whatever escaped chars it founds in
 * TEXT, so that readline can compare it to system file names when
 * completing paths. Returns a string containing text without escape
 * sequences */
char *
dequote_str(char *text, int mt)
{
	if (!text || !*text)
		return (char *)NULL;

	/* At most, we need as many bytes as text (in case no escape sequence
	 * is found)*/
	char *buf = (char *)NULL;
	buf = (char *)xnmalloc(strlen(text) + 1, sizeof(char));
	size_t len = 0;

	while (*text) {
		switch (*text) {
		case '\\':
			buf[len++] = *(++text);
			break;
		default:
			buf[len++] = *text;
			break;
		}
		if (!*text)
			break;
		text++;
	}

	buf[len] = '\0';
	return buf;
}
