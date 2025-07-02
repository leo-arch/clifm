/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* readline.c -- functions to control the behavior of readline,
 * specially completions. It also introduces both the syntax highlighting
 * and the suggestions system (via my_rl_getc) */

/* The following functions are taken from Bash (1.14.7), licensed under
 * GPL-1.0-or-later:
 * my_rl_getc
 * my_rl_path_completion
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#include <string.h>
#include <strings.h> /* str(n)casecmp() */
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h> /* Needed by groups_generator(): getgrent(3) */

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include "misc.h"
#include "aux.h"
#include "checks.h"
#include "fuzzy_match.h"
#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif /* !_NO_HIGHLIGHT */
#include "keybinds.h"
#include "mime.h" /* xmagic() */
#include "navigation.h"
#include "readline.h"
#include "sort.h" /* compare_strings() */
#include "spawn.h"
#ifndef _NO_SUGGESTIONS
# include "suggestions.h"
#endif /* !_NO_SUGGESTIONS */
#include "tabcomp.h"
#include "tags.h"

#define DEL_EMPTY_LINE     1
#define DEL_NON_EMPTY_LINE 2

/* The maximum number of bytes we need to contain any Unicode code point
 * as a C string: 4 bytes plus a trailing nul byte. */
#define UTF8_MAX_LEN 5

#define RL_VI_MODE 0

#define SUGGEST_ONLY             0
#define RL_INSERT_CHAR           1
#ifndef _NO_SUGGESTIONS
# define SKIP_CHAR               2
#endif /* !_NO_SUGGESTIONS */
#define SKIP_CHAR_NO_REDISPLAY   3

#define MAX_EXT_OPTS NAME_MAX
#define MAX_EXT_OPTS_LEN NAME_MAX

static char ext_opts[MAX_EXT_OPTS][MAX_EXT_OPTS_LEN];
#ifndef _NO_TAGS
static struct dirent **tagged_files = (struct dirent **)NULL;
static int tagged_files_n = 0;
#endif /* !_NO_TAGS */
static int cb_running = 0;
static char rl_default_answer = 0;

static const char *
gen_y_n_str(const char def_answer)
{
	switch (def_answer) {
	case 'y': return "[Y/n]";
	case 'n': return "[y/N]";
	case 'u': /* fallthrough */
	default:  return "[y/n]";
	}
}

static char
set_default_answer(const char default_answer)
{
	if (conf.default_answer.default_all != 0)
		return conf.default_answer.default_all;

	if (default_answer == 0)
		return conf.default_answer.default_;

	return default_answer;
}

/* Get user input (y/n, uppercase is allowed) using MSG_STR as prompt message.
 * If DEFAULT_ANSWER isn't zero, it will be used in case the user just
 * presses Enter on an empty line.
 * Returns 1 if 'y' or 0 if 'n'. */
int
rl_get_y_or_n(const char *msg_str, char default_answer)
{
	rl_default_answer = set_default_answer(default_answer);

	const char *yes_no_str = gen_y_n_str(rl_default_answer);
	const size_t msg_len = strlen(msg_str) + strlen(yes_no_str) + 3;
	char *msg = xnmalloc(msg_len, sizeof(char));
	snprintf(msg, msg_len, "%s %s ", msg_str, yes_no_str);

	int ret = 0;

	char *answer = (char *)NULL;
	while (!answer) {
		answer = rl_no_hist(msg, 0);
		if (!answer)
			continue;

		if (!*answer) {
			free(answer);
			answer = (char *)NULL;
			continue;
		}

		switch (*answer) {
		case 'y': /* fallthrough */
		case 'Y':
			if (!answer[1] || strcmp(answer + 1, "es") == 0)
				{ free(answer); ret = 1; break; }
			else
				{ free(answer); answer = (char *)NULL; continue; }
		case 'n': /* fallthrough */
		case 'N':
			if (!answer[1] || (answer[1] == 'o' && !answer[2]))
				{ free(answer); ret = 0; break; }
			else
				{ free(answer); answer = (char *)NULL; continue; }
		default: free(answer); answer = (char *)NULL; continue;
		}
	}

	free(msg);
	return ret;
}

/* Delete key implementation */
static void
xdelete(void)
{
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

	rl_delete(1, 0);
}

/* Backspace implementation */
static void
xbackspace(void)
{
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

	rl_rubout(1, 0);
}

#ifndef _NO_SUGGESTIONS
static void
leftmost_bell(void)
{
	if (conf.bell_style == BELL_VISIBLE) {
		rl_extend_line_buffer(2);
		*rl_line_buffer = ' ';
		*(rl_line_buffer + 1) = '\0';
		rl_end = rl_point = 1;
	}

	rl_ring_bell();

	if (conf.bell_style == BELL_VISIBLE) {
		rl_delete_text(0, rl_end);
		rl_end = rl_point = 0;
	}
}
#endif /* !_NO_SUGGESTIONS */

/* Construct a wide-char (UTF8) byte by byte.
 * This function is called multiple times until we get a full wide-char.
 * Each byte (C), in each subsequent call, is appended to a string (WC_STR),
 * until we have a complete multi-byte char (WC_BYTES were copied into WC_STR),
 * in which case we insert the character into the readline buffer (my_rl_getc
 * will then trigger the suggestions system using the updated input buffer). */
static int
construct_utf8_char(unsigned char c)
{
	static char wc_str[UTF8_MAX_LEN] = "";
	static size_t wc_len = 0;
	static int wc_bytes = 0;

	if (wc_len == 0)
		wc_bytes = utf8_bytes(c);

	/* utf8_bytes returns -1 in case of error, and a zero bytes wide-char
	 * should never happen. */
	if (wc_bytes < 1)
		return SKIP_CHAR_NO_REDISPLAY;

	if (wc_len < (size_t)wc_bytes - 1) {
		wc_str[wc_len] = (char)c;
		wc_len++;
		/* Incomplete wide char: do not trigger suggestions. */
		return SKIP_CHAR_NO_REDISPLAY;
	}

	wc_str[wc_len] = (char)c;
	wc_len++;
	wc_str[wc_len] = '\0';

	if (conf.highlight == 1 && cur_color != tx_c && cur_color != hv_c
	&& cur_color != hc_c && cur_color != hp_c && cur_color != hq_c) {
		cur_color = tx_c;
		fputs(cur_color, stdout);
	}

	rl_insert_text(wc_str);
	rl_redisplay();
	wc_len = 0;
	wc_bytes = 0;
	memset(wc_str, '\0', UTF8_MAX_LEN);

	return SUGGEST_ONLY;
}

/* Handle the input char C and sepcify what to do next based on this char
 * Return values:
 * SUGGEST_ONLY = Do not insert char (already inserted here). Make suggestions.
 * RL_INSERT_CHAR = Let readline insert the char. Do not suggest.
 * SKIP_CHAR = Do not insert char. Do not suggest.
 * SKIP_CHAR_NO_REDISPLAY = Same as SKIP_CHAR, but do not call rl_redisplay(). */
static int
rl_exclude_input(const unsigned char c, const unsigned char prev)
{
	/* Delete or backspace keys. */
	int del_key = 0;
	int space = 0;
	char *ptr = (char *)NULL;

	/* Disable suggestions while in vi mode. */
	if (rl_editing_mode == RL_VI_MODE) {
#ifndef _NO_SUGGESTIONS
		if (suggestion.printed)
			clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
		return RL_INSERT_CHAR;
	}

	/* Skip escape sequences, mostly arrow keys. */
	if (rl_readline_state & RL_STATE_MOREINPUT) {
		if (c == '~') {
			/* This should be the delete key. */
#ifndef _NO_SUGGESTIONS
			if (suggestion.printed)
				clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
		}

		else if (prev == '[' && c == '3' && rl_point < rl_end) {
			xdelete();
			del_key = DEL_NON_EMPTY_LINE;
			goto END;
		}

		/* Handle history events. If a suggestion has been printed and
		 * a history event is triggered (usually via the Up and Down arrow
		 * keys), the suggestion buffer won't be freed. Let's do it here. */
#ifndef _NO_SUGGESTIONS
		else if ((c == 'A' || c == 'B') && suggestion_buf) {
			clear_suggestion(CS_FREEBUF);
		}
#endif /* !_NO_SUGGESTIONS */

		else {
			if (c == 'C' || c == 'D')
				cmdhist_flag = 0;
		}

		return RL_INSERT_CHAR;
	}

	if (c == CTRL('D') && rl_point < rl_end) {
		xdelete();
		del_key = DEL_NON_EMPTY_LINE;
		goto END;
	}

	if (c == CTRL('U')) { /* Kill line */
#ifndef _NO_SUGGESTIONS
		if (wrong_cmd == 1)
			recover_from_wrong_cmd();
#endif /* !_NO_SUGGESTIONS */
		return RL_INSERT_CHAR;
	}

	/* Skip control characters (0 - 31) except backspace (8), tab(9),
	 * enter (13), and escape (27). */
	if (c < ' ' && c != KEY_BACKSPACE && c != KEY_TAB
	&& c != KEY_ENTER && c != KEY_ESC)
		return RL_INSERT_CHAR;

	if (IS_UTF8_CHAR(c))
		return construct_utf8_char(c);

	if (c != KEY_ESC)
		cmdhist_flag = 0;

	/* Skip ESC, del/backspace, Enter, and TAB keys. */
	switch (c) {
		case KEY_DELETE: /* fallthrough */
		case KEY_BACKSPACE:
			del_key = (rl_point == 0 && rl_end == 0)
				? DEL_EMPTY_LINE : DEL_NON_EMPTY_LINE;
			xbackspace();
			if (rl_end == 0 && cur_color != tx_c) {
				cur_color = tx_c;
				fputs(tx_c, stdout);
			}
			goto END;

		case KEY_ENTER:
#ifndef _NO_SUGGESTIONS
			if (suggestion_buf != NULL)
				clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
			cur_color = tx_c;
			fputs(tx_c, stdout);
			return RL_INSERT_CHAR;

		case KEY_ESC:
			return RL_INSERT_CHAR;

		case KEY_TAB:
#ifndef _NO_SUGGESTIONS
			if (suggestion.printed) {
				if (suggestion.nlines >= 2 || suggestion.type == ELN_SUG
				|| suggestion.type == BOOKMARK_SUG
				|| suggestion.type == ALIAS_SUG
				|| suggestion.type == JCMD_SUG)
					clear_suggestion(CS_FREEBUF);
			}
#endif /* !_NO_SUGGESTIONS */
			return RL_INSERT_CHAR;

		default: break;
	}

	char t[2];
	t[0] = (char)c;
	t[1] = '\0';
	rl_insert_text(t);

END:
#ifndef _NO_SUGGESTIONS
	ptr = strrchr(rl_line_buffer, ' ');
	space = ptr ? (int)(ptr - rl_line_buffer) : -1;

	/* Do not take into account ending spaces. */
	if (space >= 0 && !rl_line_buffer[space + 1])
		space = -1;

	if (rl_point != rl_end && c != KEY_ESC) {
		if (rl_point < space) {
			if (suggestion.printed)
				clear_suggestion(CS_FREEBUF);
		}

		if (wrong_cmd == 1) { /* Wrong cmd and we are on the first word. */
			char *fs = strchr(rl_line_buffer, ' ');
			if (fs && rl_line_buffer + rl_point <= fs)
				space = -1;
		}
	}
#else
	UNUSED(space); UNUSED(ptr);
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
	if (wrong_cmd == 0 && conf.highlight == 1)
		recolorize_line();
#endif /* !_NO_HIGHLIGHT */

	if (del_key <= 0)
		return SUGGEST_ONLY;

#ifndef _NO_SUGGESTIONS
	/* Since we have removed a char, let's check if there is
	 * a suggestion available using the modified input line. */
	if (wrong_cmd == 1 && space == -1 && rl_end > 0) {
		/* If a suggestion is found, the normal prompt will be
		 * restored and wrong_cmd will be set to zero. */
		rl_suggestions((unsigned char)rl_line_buffer[rl_end - 1]);
		return SKIP_CHAR;
	}

	if (rl_point == 0 && rl_end == 0) {
		if (wrong_cmd == 1)
			recover_from_wrong_cmd();
		if (del_key == DEL_EMPTY_LINE)
			leftmost_bell();
	}
#endif /* !_NO_SUGGESTIONS */

#ifdef NO_BACKWARD_SUGGEST
	return SKIP_CHAR;
#else
	return SUGGEST_ONLY;
#endif /* NO_BACKWARD_SUGGEST */
}

/* Unicode aware implementation of readline's rl_expand_prompt()
 * Returns the number of terminal columns taken by the last prompt line,
 * 0 if STR is NULL or empty, and FALLBACK_PROMPT_OFFSET in case of error
 * (malformed prompt: either RL_PROMPT_START_IGNORE or RL_PROMPT_END_IGNORE
 * is missing). */
static int
xrl_expand_prompt(char *str)
{
	if (!str || !*str)
		return 0;

	int count = 0;
	while (*str) {
		char *start = strchr(str, RL_PROMPT_START_IGNORE);
		if (!start) {
			char *end = strchr(str, RL_PROMPT_END_IGNORE);
			if (end) {
				err('w', PRINT_PROMPT, "%s: Malformed prompt: "
					"RL_PROMPT_END_IGNORE (\\%d) without "
					"RL_PROMPT_START_IGNORE (\\%d)\n", PROGRAM_NAME,
					RL_PROMPT_END_IGNORE, RL_PROMPT_START_IGNORE);
				return FALLBACK_PROMPT_OFFSET;
			}

			/* No ignore characters found */
			return (int)wc_xstrlen(str);
		}

		if (start != str) {
			char c = *start;
			*start = '\0';
			count += (int)wc_xstrlen(str);
			*start = c;
		}

		char *end = strchr(start, RL_PROMPT_END_IGNORE);
		if (!end) {
			err('w', PRINT_PROMPT, "%s: Malformed prompt: "
				"RL_PROMPT_START_IGNORE (\\%d) without "
				"RL_PROMPT_END_IGNORE (\\%d)\n", PROGRAM_NAME,
				RL_PROMPT_START_IGNORE, RL_PROMPT_END_IGNORE);
			return FALLBACK_PROMPT_OFFSET;
		}

		if (*(++end))
			str = end;
		else
			break;
	}

	return count;
}

/* Get the number of visible chars in the last line of the prompt (STR). */
static int
get_prompt_offset(char *str)
{
	if (!str || !*str)
		return 0;

	char *newline = strrchr(str, '\n');
	return xrl_expand_prompt((newline && *(++newline)) ? newline : str) + 1;
}

/* Whenever we are on a secondary prompt, multi-byte chars are present, and
 * we press Right on such char, rl_point gets the wrong cursor offset.
 * This function corrects this offset.
 * NOTE: This is just a workaround. No correction should be made because
 * nothing should be corrected in the first place. The question is: why is
 * rl_point wrong here, and why does this happen only in secondary prompts? */
static void
fix_rl_point(const unsigned char c)
{
	if (!RL_ISSTATE(RL_STATE_MOREINPUT) || c != 'C')
		return;

	const char point = rl_line_buffer[rl_point];
	/* Continue only in case of leading or continuation multi-byte. */
	if (!IS_UTF8_CHAR(point))
		return;

	const int mlen = mblen(rl_line_buffer + rl_point, MB_CUR_MAX);
	rl_point += mlen > 0 ? mlen - 1 : 0;
}

/* Custom implementation of readline's rl_getc() hacked to introduce
 * suggestions, alternative tab completion, and syntax highlighting.
 * This function is automatically called by readline() to handle input. */
static int
my_rl_getc(FILE *stream)
{
	int result;
	unsigned char c;
	static unsigned char prev = 0;

	if (prompt_offset == UNSET)
		prompt_offset = get_prompt_offset(rl_prompt);

	while (1) {
		result = (int)read(fileno(stream), &c, sizeof(unsigned char)); /* flawfinder: ignore */
		if (result == sizeof(unsigned char)) {
			/* Ctrl+d (empty command line only). Let's check that the previous
			 * char wasn't ESC to prevent Ctrl+Alt+d to be taken as Ctrl+d */
			if (c == CTRL('D') && prev != KEY_ESC && rl_nohist == 0
			&& (!rl_line_buffer || !*rl_line_buffer))
				rl_quit(0, 0);

			if (rl_end == 0) {
				if (conf.highlight == 1)
					rl_redisplay();
			}

			/* Syntax highlighting is made from here */
			const int ret = rl_exclude_input(c, prev);
			prev = c;

			if (ret == RL_INSERT_CHAR) {
				if (rl_nohist == 1 && !(flags & NO_FIX_RL_POINT))
					fix_rl_point(c);
				return c;
			}

#ifndef _NO_SUGGESTIONS
			if (ret == SUGGEST_ONLY && conf.suggestions == 1)
				rl_suggestions(c);
#endif /* !_NO_SUGGESTIONS */

			if (ret != SKIP_CHAR_NO_REDISPLAY)
				rl_redisplay();
			continue;
		}
		/* If zero characters are returned, then the file that we are
		reading from is empty! Return EOF in that case. */
		if (result == 0)
			return (EOF);

		/* read(3) either failed (returned -1) or is > sizeof(unsigned char) */
#if defined(EWOULDBLOCK) && defined(O_NDELAY)
		if (errno == EWOULDBLOCK) {
			int xflags;

			if ((xflags = fcntl(fileno(stream), F_GETFL, 0)) < 0)
				return (EOF);
			if (xflags & O_NDELAY) {
/*				xflags &= ~O_NDELAY; */
				fcntl(fileno(stream), F_SETFL, flags);
				continue;
			}
			continue;
		}
#endif /* EWOULDBLOCK && O_NDELAY */

#if defined(_POSIX_VERSION) && defined(EAGAIN) && defined(O_NONBLOCK)
		if (errno == EAGAIN) {
			int xflags;

			if ((xflags = fcntl(fileno(stream), F_GETFL, 0)) < 0)
				return (EOF);
			if (xflags & O_NONBLOCK) {
/*				xflags &= ~O_NONBLOCK; */
				fcntl(fileno(stream), F_SETFL, flags);
				continue;
			}
		}
#endif /* _POSIX_VERSION && EAGAIN && O_NONBLOCK */

		if (errno != EINTR)
			return (EOF);

		  /* If the error that we received was SIGINT, then try again,
		 this is simply an interrupted system call to read().
		 Otherwise, some error ocurred, also signifying EOF. */
	}
}

/* Alternative taking input function used by alt_rl_prompt(), our readline
 * alternative interface. */
static int
alt_rl_getc(FILE *stream)
{
	int result;
	unsigned char c;
	static unsigned char prev = 0;

	while (1) {
		result = (int)read(fileno(stream), &c, sizeof(unsigned char)); /* flawfinder: ignore */
		if (result == sizeof(unsigned char)) {

			if ((c == CTRL('D') || c == CTRL('X')) && prev != KEY_ESC) {
				MOVE_CURSOR_UP(1);
				return (EOF);
			}

			prev = c;
			fix_rl_point(c);
			return c;
		}

		/* If zero characters are returned, then the file that we are
		reading from is empty! Return EOF in that case. */
		if (result == 0)
			return (EOF);

		/* read(3) either failed (returned -1) or is > sizeof(unsigned char) */
#if defined(EWOULDBLOCK) && defined(O_NDELAY)
		if (errno == EWOULDBLOCK) {
			int xflags;
			if ((xflags = fcntl(fileno(stream), F_GETFL, 0)) < 0)
				return (EOF);
			if (xflags & O_NDELAY) {
/*				xflags &= ~O_NDELAY; */
				fcntl(fileno(stream), F_SETFL, flags);
				continue;
			}
			continue;
		}
#endif /* EWOULDBLOCK && O_NDELAY */

#if defined(_POSIX_VERSION) && defined(EAGAIN) && defined(O_NONBLOCK)
		if (errno == EAGAIN) {
			int xflags;

			if ((xflags = fcntl(fileno(stream), F_GETFL, 0)) < 0)
				return (EOF);
			if (xflags & O_NONBLOCK) {
/*				xflags &= ~O_NONBLOCK; */
				fcntl(fileno(stream), F_SETFL, flags);
				continue;
			}
		}
#endif /* _POSIX_VERSION && EAGAIN && O_NONBLOCK */

		if (errno != EINTR)
			return (EOF);

		  /* If the error that we received was SIGINT, then try again,
		 this is simply an interrupted system call to read().
		 Otherwise, some error occurred, also signifying EOF. */
	}
}

/* Callback function called for each line when accept-line executed, EOF
 * seen, or EOF character read. This sets a flag and returns; it could
 * also call exit(3). */
static void
cb_linehandler(char *line)
{
	/* alt_rl_getc returns EOF in case of Ctrl+d and Ctrl+x, in which case
	 * LINE is NULL. */
	if (line == NULL) {
		if (line == 0)
			putchar('\n');
		free(line);
		rl_callback_handler_remove();
		cb_running = 0;
	} else {
		if (*line) {
			/* Write input into a global variable */
			rl_callback_handler_input = savestring(line, strlen(line));
			rl_callback_handler_remove();
			cb_running = 0;
		} else {
			/* Enter on empty line. If we have a default answer (set via
			 * rl_get_y_or_n()), let's return this answer. */
			if (rl_default_answer != 0) {
				rl_callback_handler_input = xnmalloc(2, sizeof(char));
				*rl_callback_handler_input = rl_default_answer;
				rl_callback_handler_input[1] = '\0';
				rl_callback_handler_remove();
				cb_running = 0;
			}
		}

		free(line);
	}

	rl_default_answer = 0;
}

static int
alt_rl_prompt(const char *prompt_str, const char *line)
{
	cb_running = 1;
	kbind_busy = 1;
	rl_getc_function = alt_rl_getc;
	const int highlight_bk = conf.highlight;
	conf.highlight = 0;

	/* Install the line handler */
	rl_callback_handler_install(prompt_str, cb_linehandler);

	/* Set the initial line content, if any */
	if (line) {
		rl_insert_text(line);
		rl_redisplay();
	}

	/* Take input */
	while (cb_running == 1)
		rl_callback_read_char();

	conf.highlight = highlight_bk;
	kbind_busy = 0;
	rl_getc_function = my_rl_getc;
	return FUNC_SUCCESS;
}

char *
secondary_prompt(const char *prompt_str, const char *line)
{
	alt_rl_prompt(prompt_str, line);

	if (!rl_callback_handler_input)
		return (char *)NULL;

	char *input = savestring(rl_callback_handler_input,
		strlen(rl_callback_handler_input));
	free(rl_callback_handler_input);
	rl_callback_handler_input = (char *)NULL;

	return input;
}

/* Simply check a single chartacter (c) against the quoting characters
 * list defined in the quote_chars global array (which takes its values from
 * rl_filename_quote_characters. */
int
is_quote_char(const char c)
{
	if (c == '\0' || !quote_chars)
		return (-1);

	char *p = quote_chars;

	while (*p) {
		if (c == *p)
			return 1;
		p++;
	}

	return 0;
}

char *
rl_no_hist(const char *prompt_str, const int tabcomp)
{
	rl_notab = (tabcomp == 0);
	rl_nohist = 1;
	char *input = secondary_prompt(prompt_str, NULL);
	rl_nohist = rl_notab = 0;

	if (!input) /* Ctrl+d */
		return savestring("q", 1);

	if (!*input) {
		free(input);
		return (char *)NULL;
	}

	/* Do we have some non-blank char? */
	int blank = 1;
	char *p = input;

	while (*p) {
		if (*p != ' ' && *p != '\n' && *p != '\t') {
			blank = 0;
			break;
		}
		p++;
	}

	if (blank == 1) {
		free(input);
		return (char *)NULL;
	}

	return input;
}

/* Used by readline to check if a char in the string being completed is
 * quoted or not */
static int
quote_detector(char *line, int index)
{
	if (index > 0 && line[index - 1] == '\\' && !quote_detector(line, index - 1))
		return 1;

	return 0;
}

/* Performs bash-style filename quoting for readline (put a backslash
 * before any char listed in rl_filename_quote_characters.
 * Modified version of:
 * https://utcc.utoronto.ca/~cks/space/blog/programming/ReadlineQuotingExample*/
static char *
my_rl_quote(char *text, int mt, char *qp) /* NOLINT */
{
	/* NOTE: mt and qp arguments are not used here, but are required by
	 * rl_filename_quoting_function */
	UNUSED(mt); UNUSED(qp);

	/* How it works: P and R are pointers to the same memory location
	 * initialized (malloced) twice as big as the line that needs to be
	 * quoted (in case all chars in the line need to be quoted); TP is a
	 * pointer to TEXT, which contains the string to be quoted. We move
	 * through TP to find all chars that need to be quoted ("a's" becomes
	 * "a\'s", for example). At this point we cannot return P, since this
	 * pointer is at the end of the string, so that we return R instead,
	 * which is at the beginning of the same string pointed to by P. */
	char *r = (char *)NULL, *p = (char *)NULL, *tp = (char *)NULL;

	size_t text_len = strlen(text);
	/* Worst case: every character of text needs to be escaped. In this
	 * case we need 2x text's bytes plus the NULL byte. */
	p = xnmalloc((text_len * 2) + 1, sizeof(char));
	r = p;

	if (r == NULL)
		return (char *)NULL;

	/* Escape whatever char that needs to be escaped */
	for (tp = text; *tp; tp++) {
		if (is_quote_char(*tp)) {
			*p = '\\';
			p++;
		}

		*p = *tp;
		p++;
	}

	/* Add a final null byte to the string */
	*p = '\0';
	return r;
}

static inline int
filter_cd_cmd(const char *dirname, const char *d_name, char *buf, mode_t type)
{
	if (type == DT_DIR)
		return 1;

	if (type != DT_LNK)
		return 0;

	if (*dirname == '.' && !dirname[1])
		return (get_link_ref(d_name) == S_IFDIR);

	snprintf(buf, PATH_MAX + 1, "%s%s", dirname, d_name);
	return (get_link_ref(buf) == S_IFDIR);
}

static inline int
filter_open_cmd(const char *dirname, const char *d_name, char *buf, mode_t type)
{
	if (type == DT_REG || type == DT_DIR)
		return 1;

	if (type != DT_LNK)
		return 0;

	int ret = -1;
	if (*dirname == '.' && !dirname[1]) {
		ret = get_link_ref(d_name);
	} else {
		snprintf(buf, PATH_MAX + 1, "%s%s", dirname, d_name);
		ret = get_link_ref(buf);
	}

	return (ret == S_IFDIR || ret == S_IFREG);
}

/* Compare the strings S1 to S2. Return 1 if they match or 0 otherwise. */
static inline int
check_match(const char *s1, const char *s2, const size_t s1_len)
{
	if (conf.case_sens_path_comp == 0) {
		return TOUPPER(*s1) != TOUPPER(*s2) ? 0
			: (strncasecmp(s1, s2, s1_len) == 0);
	}

	return *s1 != *s2 ? 0 : (strncmp(s1, s2, s1_len) == 0);
}

static inline int
get_best_fuzzy_match(char *filename, const char *dirname, char *d_name,
	const size_t flen, const int fuzzy_str_type, int *best_fz_score)
{
	const int score = fuzzy_match(filename, d_name, flen, fuzzy_str_type);
	if (score <= *best_fz_score)
		return 0;

	/* Best score so far. Keep a copy the best ranked entry.
	 * FZ_MATCH is global. */
	if (!dirname || (*dirname == '.' && !dirname[1])) {
		xstrsncpy(fz_match, d_name, sizeof(fz_match));
	} else {
		snprintf(fz_match, sizeof(fz_match), "%s%s",
			dirname, d_name);
	}

	/* If the current score isn't the best possible one, let's keep looking
	 * for the best one. */
	if (score != TARGET_BEGINNING_BONUS) {
		*best_fz_score = score;
		return 0;
	}

	return 1;
}

/* This is the filename_completion_function() function of an old Bash
 * release (1.14.7) modified to fit Clifm's needs */
/* state is zero before completion, and 1 ... n after getting
 * possible completions. Example:
 * cd Do[TAB] -> state 0
 * cuments/ -> state 1
 * wnloads/ -> state 2
 * */
char *
my_rl_path_completion(const char *text, int state)
{
	if (!text || !*text || alt_prompt > 1)
		return (char *)NULL;

	static DIR *directory;
	static char *filename = (char *)NULL;
	static char *dirname = (char *)NULL;
	static char *users_dirname = (char *)NULL;
	static size_t filename_len;
	static int match;
	struct dirent *ent = (struct dirent *)NULL;
	static char tmp[PATH_MAX + 1];
	static char *tmp_text = (char *)NULL;

	static int is_cd_cmd = 0;
	static int is_open_cmd = 0;
	static int is_trash_cmd = 0;
	static int line_buffer_has_space = 0;
	static int conf_suggestions = 0;
	static int conf_fuzzy_match = 0;
	static int conf_auto_open = 0;
	static int conf_autocd = 0;

	/* Dequote string to be completed (text), if necessary. */
	if (strchr(text, '\\')) {
		char *p = savestring(text, strlen(text));
		tmp_text = unescape_str(p, 0);
		free(p);
		if (!tmp_text)
			return (char *)NULL;
	}

	/* If we don't have any state, then do some initialization. */
	if (state == 0) {
		if (rl_line_buffer) {
			is_cd_cmd = (*rl_line_buffer == 'c'
				&& rl_line_buffer[1] == 'd' && rl_line_buffer[2] == ' ');
			is_open_cmd = (*rl_line_buffer == 'o'
				&& (strncmp(rl_line_buffer, "o ", 2) == 0
				|| strncmp(rl_line_buffer, "open ", 5) == 0));
			is_trash_cmd = (*rl_line_buffer == 't'
				&& (strncmp(rl_line_buffer, "t ", 2) == 0
				|| strncmp(rl_line_buffer, "trash ", 6) == 0));
			line_buffer_has_space = strchr(rl_line_buffer, ' ') != NULL;
		} else {
			is_cd_cmd = is_open_cmd = is_trash_cmd = line_buffer_has_space = 0;
		}

		conf_suggestions = conf.suggestions;
		conf_fuzzy_match = conf.fuzzy_match;
		conf_auto_open = conf.auto_open;
		conf_autocd = conf.autocd;

		free(dirname);
		free(filename);
		free(users_dirname);

		/* tmp_text is not NULL whenever text was dequoted. */
		const char *p = tmp_text ? tmp_text : text;
		size_t text_len = filename_len = strlen(p);
		if (text_len > 0) {
			filename = savestring(p, text_len);
			dirname = savestring(p, text_len);
		} else {
			filename = savestring("", 1);
			dirname = savestring("", 1);
		}

		/* Get everything after the last slash. */
		char *base_name = strrchr(dirname, '/');
		if (base_name) {
			/* At this point, FILENAME has been allocated with FILENAME_LEN bytes. */
			xstrsncpy(filename, ++base_name, filename_len + 1);
			filename_len -= (size_t)(base_name - dirname);
			*base_name = '\0';
		} else { /* No slash. DIR is the current directory. */
			*dirname = '.';
			dirname[1] = '\0';
		}

		/* Save the version of the directory that the user typed. */
		users_dirname = savestring(dirname, strlen(dirname));

		char *temp_dirname = tilde_expand(dirname);
		if (temp_dirname) {
			free(dirname);
			dirname = temp_dirname;
		}

		char *dir_name = dirname;
		if (text_len > FILE_URI_PREFIX_LEN && IS_FILE_URI(p))
			dir_name = dirname + FILE_URI_PREFIX_LEN;

		/* Resolve special expressions in the resulting directory. */
		char *norm_path = dir_name;
		if ((*dir_name == '.' && dir_name[1] == '.' && dir_name[2] == '/')
		|| strstr(dir_name, "/.."))
			norm_path = normalize_path(dir_name, strlen(dir_name));

		directory = opendir(norm_path);
		if (norm_path != dir_name)
			free(norm_path);

		rl_filename_completion_desired = 1;
	}

	free(tmp_text);
	tmp_text = (char *)NULL;

	/* Now that we have some state, we can read the directory. If we found
	 * a match among files in dir, break the loop and print the match */
	match = 0;

	/* ############### COMPLETION FILTER ################## */
	/* #        This is the heart of the function         #
	 * #################################################### */
	mode_t type;
	const int fuzzy_str_type =
		(conf.fuzzy_match == 1 && contains_utf8(filename) == 1)
		? FUZZY_FILES_UTF8 : FUZZY_FILES_ASCII;
	int best_fz_score = 0;

	while (directory && (ent = readdir(directory))) {
		char *ename = ent->d_name;

#if !defined(_DIRENT_HAVE_D_TYPE)
		struct stat attr;
		if (*dirname == '.' && !dirname[1])
			xstrsncpy(tmp, ename, sizeof(tmp));
		else
			snprintf(tmp, sizeof(tmp), "%s%s", dirname, ename);

		if (lstat(tmp, &attr) == -1)
			continue;
		type = get_dt(attr.st_mode);
#else
		type = ent->d_type;
#endif /* !_DIRENT_HAVE_D_TYPE */

		if (((conf_suggestions == 1 && words_num == 1)
		|| line_buffer_has_space == 0)
		&& ((type == DT_DIR && conf_autocd == 0)
		|| (type != DT_DIR && conf_auto_open == 0)))
			continue;

		/* Only dir names for cd */
		if ((conf_suggestions == 0 || words_num > 1) && conf_fuzzy_match == 1
		&& is_cd_cmd == 1 && type != DT_DIR)
			continue;

		/* If the user entered nothing before TAB (e.g., "cd [TAB]") */
		if (filename_len == 0) {
			/* Exclude "." and ".." as possible completions */
			if (SELFORPARENT(ename))
				continue;

			/* If 'cd', match only dirs or symlinks to dir */
			if (is_cd_cmd == 1)
				match = filter_cd_cmd(dirname, ename, tmp, type);

			/* If 'open', allow only reg files, dirs, and symlinks */
			else if (is_open_cmd == 1)
				match = filter_open_cmd(dirname, ename, tmp, type);

			/* If 'trash', allow only reg files, dirs, symlinks, pipes
			 * and sockets. You should not trash a block or a character
			 * device. */
			else if (is_trash_cmd == 1)
				match = (type != DT_BLK && type != DT_CHR);

			/* No filter for everything else. Just print whatever is there. */
			else
				match = 1;
		}

		/* If there is at least one char to complete (e.g., "cd .[TAB]") */
		else {
			if (rl_point < rl_end || conf_fuzzy_match == 0
			|| (*filename == '.' && filename[1] == '.')
			|| *filename == '-'
			|| (tabmode == STD_TAB && !(flags & STATE_SUGGESTING))) {
				/* Regular matching. */
				if (check_match(filename, ename, filename_len) == 0)
					continue;
			} else {
				/* Fuzzy matching. */
				if (flags & STATE_SUGGESTING) {
					if (get_best_fuzzy_match(filename, dirname, ename,
					filename_len, fuzzy_str_type, &best_fz_score) == 0)
						continue;
				} else { /* This is for tab completion: accept all matches. */
					if (fuzzy_match(filename, ename,
					filename_len, fuzzy_str_type) == 0)
						continue;
				}
			}

			if (is_cd_cmd == 1)
				match = filter_cd_cmd(dirname, ename, tmp, type);
			else if (is_open_cmd == 1)
				match = filter_open_cmd(dirname, ename, tmp, type);
			else if (is_trash_cmd == 1)
				match = (type != DT_BLK && type != DT_CHR);
			else
				match = 1;
		}

		if (match == 1)
			break;
	}

	char *cur_match = (char *)NULL;

	/* readdir() returns NULL on reaching the end of the directory stream.
	 * So that if ENT is not NULL, we have a match. */
	if (ent) { /* Same as match == 1 */
		cur_comp_type = TCMP_PATH;
		if (dirname && (dirname[0] != '.' || dirname[1])) {
			size_t len = strlen(users_dirname) + strlen(ent->d_name) + 1;
			cur_match = xnmalloc(len, sizeof(char));
			snprintf(cur_match, len, "%s%s", users_dirname, ent->d_name);
		} else {
			cur_match = savestring(ent->d_name, strlen(ent->d_name));
		}
	}

	/* Clear state. */
	if ((flags & STATE_SUGGESTING) || !ent) {
		if (directory) {
			closedir(directory);
			directory = (DIR *)NULL;
		}

		free(dirname); dirname = (char *)NULL;
		free(filename); filename = (char *)NULL;
		free(users_dirname); users_dirname = (char *)NULL;
	}

	return cur_match;
}

/* Used by bookmarks completion */
static char *
bookmarks_generator(const char *text, int state)
{
	if (!bookmarks || bm_n == 0)
		return (char *)NULL;

	static int i;
	static size_t len;
	static int prefix;

	if (state == 0) {
		i = 0;
		prefix = (*text == 'b' && text[1] == ':') ? 2 : 0;
		len = strlen(text + prefix);
	}

	/* Look for bookmarks in bookmark names for a match */
	while ((size_t)i < bm_n) {
		const char *name = bookmarks[i++].name;
		if (!name || !*name)
			continue;

		if ((conf.case_sens_list == 1 ? strncmp(name, text + prefix, len)
		: strncasecmp(name, text + prefix, len)) != 0)
			continue;

		if (prefix == 2) {
			char t[NAME_MAX + 3];
			snprintf(t, sizeof(t), "b:%s", name);
			return strdup(t);
		} else {
			return strdup(name);
		}
	}

	return (char *)NULL;
}

/* Generate a list of internal commands and a brief description
 * for cmd<TAB> */
static char *
int_cmds_generator(const char *text, int state)
{
	UNUSED(text);
	static int i;

	if (state == 0)
		i = 0;

	static char *const cmd_desc[] = {
		"/       (search for files)",
		"ac      (archive/compress files)",
		"acd     (toggle autocd)",
		"actions (manage actions-plugins)",
		"ad      (dearchive/decompress files)",
		"alias   (list aliases)",
		"ao      (toggle auto-open)",
		"auto    (set a temporary autocommand)",
		"b       (change to the previously visited directory)",
		"bd      (change to a parent directory)",
		"bl      (create symbolic links in bulk)",
		"bb      (sanitize non-ASCII filenames)",
		"bm      (manage bookmarks)",
		"br      (bulk-rename files)",
		"c       (copy files)",
		"cd      (change directory)",
		"cl      (toggle columns)",
		"cmd     (jump to the COMMANDS section in the manpage)",
		"colors  (preview the current color scheme)",
		"config  (edit the main configuration file)",
		"cs      (manage color schemes)",
		"dup     (duplicate files)",
		"ds      (deselect files)",
		"exp     (export filenames to a temporary file)",
		"ext     (turn external/shell commands on/off)",
		"f       (change to the next visited directory)",
		"fc      (toggle the file-counter)",
		"ff      (toggle list-directories-first)",
		"ft      (set a file filter)",
		"fz      (print directories full size - long view only)",
		"hh      (toggle hidden files)",
		"history (manage the commands history)",
		"icons   (toggle icons)",
		"j       (jump to a visited directory)",
		"k       (toggle follow-links - long view only)",
		"kk      (toggle max-filename-len)",
		"kb      (manage keybindings)",
		"l       (create a symbolic link)",
		"le      (edit a symbolic link)",
		"ll      (toggle the long-view)",
		"lm      (toggle the light-mode)",
		"log     (manage logs)",
		"m       (move/rename files)",
		"md      (create directories)",
		"media   (mount/unmount storage devices)",
		"mf      (limit the number of listed files)",
		"mm      (manage opening applications)",
		"mp      (change to a mountpoint)",
		"msg     (print program messages)",
		"n       (create files/directories)",
		"net     (manage remote resources)",
		"o       (open file)",
		"oc      (change ownership of files)",
		"opener  (set a custom file opener)",
		"ow      (open file with...)",
		"p       (print file properties)",
		"pc      (change permissions of files)",
		"pf      (manage profiles)",
		"pg      (run the file-pager)",
		"pin     (pin a directory)",
		"pp      (print file properties - follow links/full dir size)",
		"prompt  (switch/edit prompt)",
		"q       (quit)",
		"r       (remove files)",
		"rf      (refresh/clear the screen)",
		"rl      (reload the configuration file)",
		"rr      (remove files in bulk)",
		"sb      (access the selection box)",
		"s       (select files)",
		"st      (change file sort order)",
		"stats   (print file statistics)",
		"tag     (tag files)",
		"te      (toggle the executable bit on files)",
		"tips    (print tips)",
		"t       (trash files)",
		"u       (restore trashed files using a menu)",
		"unpin   (unpin the pinned directory)",
		"ver     (print version information)",
		"view    (preview files in the current directory)",
		"vv      (copy and bulk-rename files at once)",
		"ws      (switch workspaces)",
		"x       (launch a new instance of clifm)",
		"X       (launch a new instance of clifm as root)",
		NULL
	};

	char *name;
	while ((name = cmd_desc[i++]))
		return strdup(name);

	return (char *)NULL;
}

/* Generate completions for command CMD using a modified version of
 * fish's manpages parser. */
static int
gen_shell_cmd_comp(char *cmd)
{
	if (!cmd || !*cmd || !data_dir || !*data_dir)
		return FUNC_FAILURE;

	char manpage_parser_file[PATH_MAX + 1];
	snprintf(manpage_parser_file, sizeof(manpage_parser_file),
		"%s/%s/tools/manpages_comp_gen.py", data_dir, PROGRAM_NAME);

	char *c[] = {manpage_parser_file, "-k", cmd, NULL};
	return launch_execv(c, FOREGROUND, E_MUTE);
}

/* Get short and long options for command CMD, store them in the EXT_OPTS
 * array and return the number of options found. */
static int
get_shell_cmd_opts(char *cmd)
{
	*ext_opts[0] = '\0';
	if (!cmd || !*cmd || !user.home
	|| (conf.suggestions == 1 && wrong_cmd == 1))
		return FUNC_FAILURE;

	char p[PATH_MAX + 1];
	snprintf(p, sizeof(p), "%s/.local/share/%s/completions/%s.clifm",
		user.home, PROGRAM_NAME, cmd);

	struct stat a;
	if (stat(p, &a) == -1) {
		/* Comp file does not exist. Try to generate via manpages_comp_gen.py */
		if (gen_shell_cmd_comp(cmd) != FUNC_SUCCESS || stat(p, &a) == -1)
			return FUNC_FAILURE;
	}

	int fd;
	FILE *fp = open_fread(p, &fd);
	if (!fp)
		return FUNC_FAILURE;

	int n = 0;
	char line[NAME_MAX]; *line = '\0';
	while (fgets(line, (int)sizeof(line), fp)) {
		if (n >= MAX_EXT_OPTS)
			break;
		if (!*line || *line == '#' || *line == '\n')
			continue;

		size_t l = strnlen(line, sizeof(line));
		if (l > 0) {
			while (line[l - 1] == '\n')
				line[l - 1] = '\0';
		}

		/* Get short option */
		char *opt_str = strstr(line, "-s ");
		char *opt_end = (char *)NULL;

		if (opt_str && opt_str[1] && opt_str[2] && opt_str[3]) {
			opt_end = strchr(opt_str + 3, ' ');
			if (opt_end) *opt_end = '\0';
			snprintf(ext_opts[n], MAX_EXT_OPTS_LEN, "-%s", opt_str + 3);
			if (opt_end) *opt_end = ' ';
			n++;
		}

		/* Get long option (-OPT or --OPT) */
		char *long_str = strstr((opt_end && opt_end[1])
			? opt_end + 1 : line, "-l ");

		if (!long_str)
			long_str = strstr((opt_end && opt_end[1])
				? opt_end + 1 : line, "-o ");

		if (long_str && long_str[1] && long_str[2] && long_str[3]) {
			char *long_end = strchr(long_str + 3, ' ');
			if (long_end) *long_end = '\0';

			/* Some long opts are written as optOPT: remove OPT */
			/* long_str + 3 is the beginning of the option name, so that OPT could
			 * begin at long_str + 4, but not before. */
			char *t = long_str[4] ? long_str + 4 : (char *)NULL;
			while (t && *t) {
				if (*t >= 'A' && *t <= 'Z') {
					*t = '\0';
					break;
				}
				t++;
			}

			if (long_str[1] == 'o')
				snprintf(ext_opts[n], MAX_EXT_OPTS_LEN, "-%s", long_str + 3);
			else
				snprintf(ext_opts[n], MAX_EXT_OPTS_LEN, "--%s", long_str + 3);
			if (long_end)
				*long_end = ' ';
			n++;
		}
	}

	*ext_opts[n] = '\0'; /* Mark the end of the options array */
	fclose(fp);
	return n;
}

/* Used for history and search pattern completion */
static char *
hist_generator(const char *text, int state)
{
	if (!history)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(*text == '!' ? text + 1 : text);
	}

	while ((name = history[i++].cmd) != NULL) {
		if (*text == '!') {
			if (len == 0
			|| (*name == text[1] && strncmp(name, text + 1, len) == 0)
			|| (conf.fuzzy_match == 1 && tabmode != STD_TAB
			&& fuzzy_match((char *)(text + 1), name, len, FUZZY_HISTORY) > 0))
				return strdup(name);
		} else {
			/* Restrict the search to what seems to be a pattern:
			 * The string before the first slash or space (not counting the initial
			 * slash, used to fire up the search function) must contain a pattern
			 * metacharacter */
			if (!*name || !name[1])
				continue;
			char *ret = strpbrk(name + 1, conf.search_strategy == GLOB_ONLY
					? " /*?[{" : " /*?[{|^+$.");
			if (!ret || *ret == ' ' || *ret == '/')
				continue;

			return strdup(name);
		}
	}

	return (char *)NULL;
}

/* Returns the path corresponding to be bookmark name TEXT */
static char *
bm_paths_generator(const char *text, int state)
{
	if (!bookmarks || bm_n == 0)
		return (char *)NULL;

	static int i;
	char *bname, *bpath;

	if (state == 0)
		i = 0;

	while (i < (int)bm_n) {
		bname = bookmarks[i].name;
		bpath = bookmarks[i].path;
		i++;

		if (!bname || !bpath || (conf.case_sens_list == 1 ? strcmp(bname, text)
		: strcasecmp(bname, text)) != 0)
			continue;

		const size_t plen = strlen(bpath);

		if (plen > 1 && bpath[plen - 1] == '/')
			bpath[plen - 1] = '\0';

		char *p = abbreviate_file_name(bpath);
		char *ret = strdup(p ? p : bpath);

		if (p != bpath)
			free(p);

		return ret;
	}

	return (char *)NULL;
}

/* Used for the 'unset' command */
static char *
env_vars_generator(const char *text, int state)
{
	if (!environ)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((name = environ[i++]) != NULL) {
		if (conf.case_sens_path_comp ? strncmp(name, text, len) == 0
		: strncasecmp(name, text, len) == 0) {
			char *p = strchr(name, '=');
			if (!p)
				continue;
			*p = '\0';
			char *q = strdup(name);
			*p = '=';
			return q;
		}
	}

	return (char *)NULL;
}

/* Complete environment variables ($VAR) */
static char *
environ_generator(const char *text, int state)
{
	if (!environ)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text + 1);
	}

	while ((name = environ[i++]) != NULL) {
		if (conf.case_sens_path_comp ? strncmp(name, text + 1, len) == 0
		: strncasecmp(name, text + 1, len) == 0) {
			char *p = strrchr(name, '=');
			if (!p)
				continue;
			*p = '\0';
			char tmp[NAME_MAX];
			snprintf(tmp, sizeof(tmp), "$%s", name);
			char *q = strdup(tmp);
			*p = '=';
			return q;
		}
	}

	return (char *)NULL;
}

/* Expand string into matching path in the jump database. Used by
 * j, jc, and jp commands */
static char *
jump_generator(const char *text, int state)
{
	static int i;
	char *name;

	if (state == 0)
		i = 0;

	if (!jump_db)
		return (char *)NULL;

	/* Look for matches in the dirhist list */
	while ((name = jump_db[i++].path) != NULL) {

		if (i > 0 && jump_db[i - 1].rank == JUMP_ENTRY_PURGED)
			continue;

		/* Exclude CWD */
		if (name[1] == workspaces[cur_ws].path[1]
		&& strcmp(name, workspaces[cur_ws].path) == 0)
			continue;
		/* Filter by parent */
		if (rl_line_buffer[1] == 'p') {
			if ((conf.case_sens_dirjump == 1 ? strstr(workspaces[cur_ws].path, name)
			: xstrcasestr(workspaces[cur_ws].path, name)) == NULL)
				continue;
		}
		/* Filter by child */
		else if (rl_line_buffer[1] == 'c') {
			if ((conf.case_sens_dirjump == 1 ? strstr(name, workspaces[cur_ws].path)
			: xstrcasestr(name, workspaces[cur_ws].path)) == NULL)
				continue;
		}

		if ((conf.case_sens_dirjump == 1 ? strstr(name, text)
		: xstrcasestr(name, (char *)text)) != NULL)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
cschemes_generator(const char *text, int state)
{
	if (!color_schemes)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((name = color_schemes[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

#ifndef _NO_PROFILES
/* Used by profiles completion */
static char *
profiles_generator(const char *text, int state)
{
	if (!profile_names)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((name = profile_names[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}
#endif /* !_NO_PROFILES */

static char *
filenames_gen_text(const char *text, int state)
{
	static filesn_t i;
	static size_t len = 0;
	static int fuzzy_str_type = 0;
	static int line_has_space = 0;
	static int is_cd_cmd = 0;
	static int (*cmp_func)(const char *, const char *, size_t) = strncmp;

	char *name;
	rl_filename_completion_desired = 1;
	if (state == 0) {
		i = 0;
		len = strlen(text);
		cmp_func = conf.case_sens_path_comp == 1 ? strncmp : strncasecmp;
		fuzzy_str_type = (conf.fuzzy_match == 1 && contains_utf8(text) == 1)
			? FUZZY_FILES_UTF8 : FUZZY_FILES_ASCII;
		if (rl_line_buffer) {
			line_has_space = (strchr(rl_line_buffer, ' ') != NULL);
			is_cd_cmd = (*rl_line_buffer == 'c' && rl_line_buffer[1] == 'd'
				&& rl_line_buffer[2] == ' ');
		} else {
			line_has_space = is_cd_cmd = 0;
		}
	}

	/* Check list of currently displayed files for a match */
	while (i < files && (name = file_info[i].name) != NULL) {
		i++;
		/* If first word, filter files according to autocd and auto-open values. */
		if (((conf.suggestions == 1 && words_num == 1) || line_has_space == 0)
		&& ( (file_info[i - 1].dir == 1 && conf.autocd == 0)
		|| (file_info[i - 1].dir == 0 && conf.auto_open == 0) ))
			continue;

		/* If cd, list only directories. */
		if (is_cd_cmd == 1 && file_info[i - 1].dir == 0)
			continue;

		if (cmp_func(name, text, len) == 0)
			return strdup(name);

		if (conf.fuzzy_match == 0 || tabmode == STD_TAB || rl_point < rl_end)
			continue;

		if (len == 0 || fuzzy_match((char *)text, name, len, fuzzy_str_type) > 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
dirhist_generator(const char *text, int state)
{
	if (!old_pwd || dirhist_total_index == 0)
		return (char *)NULL;

	static int i;
	static size_t len;
	static int fuzzy_str_type;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
		fuzzy_str_type = (conf.fuzzy_match == 1 && contains_utf8(text) == 1)
			? FUZZY_FILES_UTF8 : FUZZY_FILES_ASCII;
	}

	while ((name = old_pwd[i++]) != NULL) {
		if (*name == KEY_ESC)
			continue;

		if (!text || !*text)
			return strdup(name);

		if (conf.fuzzy_match == 1) {
			if (fuzzy_match((char *)text, name, len, fuzzy_str_type) > 0)
				return strdup(name);
		} else {
			if ((conf.case_sens_path_comp == 1 ? strstr(name, text)
#if defined(_BE_POSIX)
			: xstrcasestr(name, (char *)text)) != NULL)
#else
			: xstrcasestr(name, text)) != NULL)
#endif /* _BE_POSIX */
				return strdup(name);
		}
	}

	return (char *)NULL;
}

/* Used by commands completion (external commands only) */
static char *
bin_cmd_generator_ext(const char *text, int state)
{
	if (!bin_commands)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((name = bin_commands[i++]) != NULL) {
		if (is_internal_cmd(name, ALL_CMDS, 1, 1) == 1)
			continue;
		if (!text || !*text
		|| (*text == *name && strncmp(name, text, len) == 0))
			return strdup(name);
	}

	return (char *)NULL;
}

/* Used by commands completion */
static char *
bin_cmd_generator(const char *text, int state)
{
	if (!bin_commands)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((name = bin_commands[i++]) != NULL) {
		if (!text || !*text)
			return strdup(name);
		if (*text == *name && strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
sort_num_generator(const char *text, int state)
{
	static size_t i;
	char *name;
	rl_filename_completion_desired = 1;

	if (state == 0)
		i = 0;

	int num_text = atoi(text);
	if (num_text == INT_MIN
	|| (conf.light_mode == 1 && !ST_IN_LIGHT_MODE(num_text)))
		return (char *)NULL;

	static char *const sorts[] = {
	    "none",
	    "name",
	    "size",
	    "atime",
	    "btime",
	    "ctime",
	    "mtime",
	    "version",
	    "extension",
	    "inode",
	    "owner",
	    "group",
	    "blocks",
	    "links",
	    "type",
	    NULL
	};

	while (i <= SORT_TYPES && (name = sorts[i++]) != NULL) {
		if (*name == *sorts[num_text]
		&& strcmp(name, sorts[num_text]) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
aliases_generator(const char *text, int state)
{
	if (aliases_n == 0)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((name = aliases[i++].name) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
kb_func_names_gen(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((name = kb_cmds[i++].name) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
file_templates_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((name = file_templates[i++]) != NULL) {
		if (conf.case_sens_path_comp ? strncmp(name, text, len) == 0
		: strncasecmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
nets_generator(const char *text, int state)
{
	if (!remotes)
		return (char *)NULL;

	static int i;
	static int is_unmount, is_mount;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);

		/* Let's find out whether we have a 'mount' or 'unmount' subcommands */
		if (*(rl_line_buffer + 4) == 'u' && (*(rl_line_buffer + 5) == ' '
		|| strncmp(rl_line_buffer + 5, "nmount ", 7) == 0))
			is_unmount = 1;
		else
			is_unmount = 0;

		if (*(rl_line_buffer + 4) == 'm' && (*(rl_line_buffer + 5) == ' '
		|| strncmp(rl_line_buffer + 5, "ount ", 5) == 0))
			is_mount = 1;
		else
			is_mount = 0;
	}

	while ((name = remotes[i++].name) != NULL) {
		if ((conf.case_sens_path_comp ? strncmp(name, text, len)
		: strncasecmp(name, text, len)) == 0) {
			if (is_unmount == 1) { /* List only mounted resources */
				if (i > 0 && remotes[i - 1].mounted == 1)
					return strdup(name);
			} else if (is_mount == 1) { /* List only unmounted resources */
				if (i > 0 && remotes[i - 1].mounted == 0)
					return strdup(name);
			} else {
				return strdup(name);
			}
		}
	}

	return (char *)NULL;
}

static char *
sort_name_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	const char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((name = sort_methods[i++].name) != NULL) {
		if (conf.light_mode == 1 && i > 0
		&& !ST_IN_LIGHT_MODE(sort_methods[i - 1].num))
			continue;
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
workspaces_generator(const char *text, int state)
{
	static int i;
	static size_t len;

	if (state == 0) {
		i = 0;
		len = text ? strlen(text) : 0;
	}

	if (text && *text >= '1' && *text <= MAX_WS + '0' && !*(text + 1))
		return (char *)NULL;

	while (i < MAX_WS) {
		if (cur_comp_type == TCMP_WS_PREFIX && !workspaces[i].path) {
			i++; /* If 'w:', skip unset workspaces */
			continue;
		}

		if (!workspaces[i].name) {
			if (len == 0) {
				char t[MAX_INT_STR + 3];
				snprintf(t, sizeof(t), "%d", i + 1);
				i++;
				return strdup(t);
			}
		} else {
			if (len == 0 || (TOUPPER(*workspaces[i].name) == TOUPPER(*text)
			&& strncasecmp(workspaces[i].name, text, len) == 0)) {
				char *ret = strdup(workspaces[i].name);
				i++;
				return ret;
			}
		}
		i++;
	}

	return (char *)NULL;
}

static char *
sel_entries_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while (i < (int)sel_n && (name = sel_elements[i++].name) != NULL) {
		if (strncmp(name, text, len) == 0) {
			char *p = abbreviate_file_name(name);
			char *ret = strdup(p ? p : name);
			if (p && p != name)
				free(p);
			return ret;
		}
	}

	return (char *)NULL;
}

static char *
prompts_generator(const char *text, int state)
{
	if (prompts_n == 0)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while (i < (int)prompts_n && (name = prompts[i++].name) != NULL) {
		if ((conf.case_sens_list == 1 ? strncmp(name, text, len)
		: strncasecmp(name, text, len)) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

/* Expand tilde and resolve dot expressions in the glob expression TEXT */
static char *
expand_tilde_glob(char *text)
{
	if (!text || !*text || (*text != '~' && !strstr(text, "/..")))
		return (char *)NULL;

	char *ls = strrchr(text, '/');
	if (!ls)
		return (char *)NULL;

	*ls = '\0';
	char *q = normalize_path(text, strlen(text));
	*ls = '/';
	if (!q)
		return (char *)NULL;

	char *g = *(ls + 1) ? ls + 1 : (char *)NULL;
	size_t len = strlen(q) + 2 + (g ? strlen(g) : 0);
	char *tmp = xnmalloc(len, sizeof(char));
	snprintf(tmp, len, "%s/%s", q, g);
	free(q);

	return tmp;
}

static char **
rl_mime_list(void)
{
#define WAIT_MSG " [wait...]"
	if (term_caps.suggestions != 0)
		{ HIDE_CURSOR; fputs(WAIT_MSG, stdout); fflush(stdout); }

	char **t = xnmalloc((size_t)files + 2, sizeof(char *));
	t[0] = xnmalloc(1, sizeof(char));
	*t[0] = '\0';
	t[1] = (char *)NULL;
	char buf[PATH_MAX + 1];

	size_t n = 1;
	filesn_t i = files;
	while (--i >= 0) {
		if (file_info[i].user_access == 0 && file_info[i].type == DT_REG)
			continue;

		char *name = file_info[i].name;
		if (virtual_dir == 1) {
			*buf = '\0';
			if (xreadlink(XAT_FDCWD, file_info[i].name, buf, sizeof(buf)) == -1
			|| !*buf)
				continue;
			name = buf;
		}

		char *m = (name && *name) ? xmagic(name, MIME_TYPE) : (char *)NULL;
		if (!m)
			continue;

		size_t j, found = 0;
		for (j = 1; j < n; j++) {
			if (*t[j] == *m && strcmp(t[j], m) == 0) {
				found = 1;
				break;
			}
		}

		if (found == 1) {
			free(m);
			continue;
		} else {
			t[n] = savestring(m, strlen(m));
			free(m);
			n++;
			t[n] = (char *)NULL;
		}
	}

	if (term_caps.suggestions != 0) {
		MOVE_CURSOR_LEFT((int)sizeof(WAIT_MSG) - 1);
		ERASE_TO_RIGHT; UNHIDE_CURSOR;
	}

	if (n == 1)
		{ free(t[0]); free(t); return (char **)NULL; }

	t = xnrealloc(t, n + 1, sizeof(char *));

	if (rl_sort_completion_matches == 1)
		qsort(t, n, sizeof(*t), (QSFUNC *)compare_strings);

#undef WAIT_MSG
	return t;
}

/* Returns the list of files in the current directory whose MIME type
 * contains the string TEXT */
static char **
rl_mime_files(const char *text)
{
	if (!text || !*text)
		return (char **)NULL;

	if (term_caps.suggestions != 0)
		{ HIDE_CURSOR; fputs(" [wait...]", stdout); fflush(stdout); }

	char **t = xnmalloc((size_t)files + 2, sizeof(char *));
	t[0] = xnmalloc(1, sizeof(char));
	*t[0] = '\0';
	char buf[PATH_MAX + 1];

	filesn_t i, n = 1;
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

		char *p = strstr(m, text);
		free(m);

		if (!p) continue;

		t[n] = savestring(name, strlen(name));
		n++;
	}

	t[n] = (char *)NULL;

	if (term_caps.suggestions != 0)
		{ MOVE_CURSOR_LEFT(10); ERASE_TO_RIGHT; UNHIDE_CURSOR; }

	if (n == 1)
		{ free(t[0]); free(t); return (char **)NULL; }

	t = xnrealloc(t, (size_t)n + 1, sizeof(char *));
	return t;
}

/* Return the list of matches for the glob expression TEXT or NULL if
 * there are no matches. */
static char **
rl_glob(char *text)
{
	char *tmp = expand_tilde_glob(text);
	glob_t globbuf;

	if (glob(tmp ? tmp : text, 0, NULL, &globbuf) != FUNC_SUCCESS) {
		globfree(&globbuf);
		free(tmp);
		return (char **)NULL;
	}

	free(tmp);

	if (globbuf.gl_pathc == 1) {
		char **matches = xnmalloc(globbuf.gl_pathc + 2, sizeof(char *));
		char *basename = strrchr(globbuf.gl_pathv[0], '/');
		if (basename && *(++basename)) {
			char c = *basename;
			*basename = '\0';
			matches[0] =
				savestring(globbuf.gl_pathv[0], strlen(globbuf.gl_pathv[0]));
			*basename = c;
			matches[1] = savestring(basename, strlen(basename));
			matches[2] = (char *)NULL;
		} else {
			matches[0] =
				savestring(globbuf.gl_pathv[0], strlen(globbuf.gl_pathv[0]));
			matches[1] = (char *)NULL;
		}
		globfree(&globbuf);
		return matches;
	}

	size_t i, j = 1;
	char **matches = xnmalloc(globbuf.gl_pathc + 3, sizeof(char *));

	/* If /path/to/dir/GLOB<TAB>, /path/to/dir goes to slot 0 */
	char *last_word = get_last_chr(rl_line_buffer, ' ', rl_point);
	if (last_word)
		last_word++;
	else
		last_word = rl_line_buffer;

	char *str = (last_word && *last_word)
		? unescape_str(last_word, 0) : (char *)NULL;
	char *word = str ? str : (char *)NULL;

	int char_copy = -1;
	char *basename = (char *)NULL;
	if (word && word[1]) {
		basename = strrchr(word, '/');
		if (basename && *(++basename)) {
			char_copy = (int)*basename;
			*basename = '\0';
		}
	}

	if (char_copy != -1) {
		matches[0] = savestring(word, strlen(word));
		*basename = (char)char_copy;
	} else {
		matches[0] = xnmalloc(1, sizeof(char));
		*matches[0] = '\0';
	}

	free(str);

	for (i = 0; i < globbuf.gl_pathc; i++) {
		if (SELFORPARENT(globbuf.gl_pathv[i]))
			continue;
		matches[j] =
			savestring(globbuf.gl_pathv[i], strlen(globbuf.gl_pathv[i]));
		j++;
	}
	matches[j] = (char *)NULL;

	globfree(&globbuf);
	return matches;
}

#ifndef _NO_TRASH
/* Return the list of currently trashed files matching TEXT, or NULL. */
static char **
rl_trashed_files(const char *text)
{
	if (!trash_files_dir || !*trash_files_dir)
		return (char **)NULL;

	if (xchdir(trash_files_dir, NO_TITLE) == -1)
		return (char **)NULL;

	struct dirent **t = (struct dirent **)NULL;
	int n = scandir(trash_files_dir, &t, NULL, alphasort);

	xchdir(workspaces[cur_ws].path, NO_TITLE);

	if (n == - 1)
		return (char **)NULL;

	if (n == 2) {
		free(t[0]);
		free(t[1]);
		free(t);
		return (char **)NULL;
	}

	char *p = unescape_str((char *)text, 0);
	char *f = p ? p : (char *)text;

	char **tfiles = xnmalloc((size_t)n + 2, sizeof(char *));
	if (f) {
		tfiles[0] = savestring(f, strlen(f));
	} else {
		tfiles[0] = xnmalloc(1, sizeof(char));
		*tfiles[0] = '\0';
	}

	int nn = 1, i;
	size_t tlen = f ? strlen(f) : 0;
	for (i = 0; i < n; i++) {
		char *name = t[i]->d_name;
		if (SELFORPARENT(name) || !f || strncmp(f, name, tlen) != 0) {
			free(t[i]);
			continue;
		}
		tfiles[nn] = savestring(name, strlen(name));
		nn++;
		free(t[i]);
	}
	free(t);

	tfiles[nn] = (char *)NULL;

	/* If only one match */
	if (nn == 2) {
		char *d = escape_str(tfiles[1]);
		free(tfiles[1]);
		tfiles[1] = (char *)NULL;
		if (d) {
			size_t len = strlen(d);
			tfiles[0] = xnrealloc(tfiles[0], len + 1, sizeof(char));
			xstrsncpy(tfiles[0], d, len + 1);
			free(d);
		}
	}

	free(p);
	return tfiles;
}
#endif /* _NO_TRASH */

#ifndef _NO_TAGS
static char *
tags_generator(const char *text, int state)
{
	if (tags_n == 0 || !tags)
		return (char *)NULL;

	static int i;
	static size_t len, p = 0;
	char *name;

	if (state == 0) {
		i = 0;
		if (cur_comp_type == TCMP_TAGS_T)
			p = 2;
		else if (cur_comp_type == TCMP_TAGS_C)
			p = 1;
		else
			p = 0;

		len = *(text + p) ? strlen(text + p) : 0;
	}

	while ((name = tags[i++]) != NULL) {
		if (strncmp(name, text + p, len) != 0)
			continue;
		if (cur_comp_type == TCMP_TAGS_C) {
			char tmp[NAME_MAX];
			snprintf(tmp, NAME_MAX, ":%s", name);
			return strdup(tmp);
		} else if (cur_comp_type == TCMP_TAGS_T) {
			char tmp[NAME_MAX];
			snprintf(tmp, NAME_MAX, "t:%s", name);
			return strdup(tmp);
		} else {
			return strdup(name);
		}
	}

	return (char *)NULL;
}

static char *
tag_entries_generator(const char *text, int state)
{
	UNUSED(text);
	static int i;
	char *name;

	if (state == 0)
		i = 0;

	if (!tagged_files)
		return (char *)NULL;

	while (i < tagged_files_n && (name = tagged_files[i++]->d_name) != NULL) {
		if (SELFORPARENT(name))
			continue;

		char *p = (char *)NULL, *q = name;
		if (strchr(name, '\\')) {
			p = unescape_str(name, 0);
			q = p;
		}

		reinsert_slashes(q);

		char tmp[PATH_MAX + 1], *r = (char *)NULL;
		snprintf(tmp, sizeof(tmp), "/%s", q);
		int free_tmp = 0;
		r = home_tilde(tmp, &free_tmp);
		q = strdup(r ? r : tmp);

		size_t len = q ? strlen(q) : 0;
		if (len > 1 && q[len - 1] == '/')
			q[len - 1] = '\0';

		free(p);
		if (free_tmp == 1)
			free(r);

		return q;
	}

	return (char *)NULL;
}

static char **
check_tagged_files(char *tag)
{
	if (!is_tag(tag))
		return (char **)NULL;

	tagged_files_n = 0;

	char dir[PATH_MAX + 1];
	snprintf(dir, sizeof(dir), "%s/%s", tags_dir, tag);
	int n = scandir(dir, &tagged_files, NULL, alphasort);
	if (n == -1)
		return (char **)NULL;

	if (n == 2) {
		free(tagged_files[0]);
		free(tagged_files[1]);
		free(tagged_files);
		tagged_files = (struct dirent **)NULL;
		return (char **)NULL;
	}

	tagged_files_n = n;
	char **_matches = rl_completion_matches("", &tag_entries_generator);
	while (--n >= 0)
		free(tagged_files[n]);
	free(tagged_files);
	tagged_files = (struct dirent **)NULL;
	tagged_files_n = 0;

	return _matches;
}

static char *
get_cur_tag(void)
{
	char *p = strrchr(rl_line_buffer, ':');
	if (!p || !*(++p))
		return (char *)NULL;

	char *q = p;
	while (*q) {
		if (*q == ' ' && (q != p || *(q - 1) != '\\')) {
			*q = '\0';
			char *tag = savestring(p, strlen(p));
			*q = ' ';
			if (is_tag(tag))
				return tag;
			free(tag);
		}
		q++;
	}

	return (char *)NULL;
}
#endif /* _NO_TAGS */

/* Generate possible arguments for a shell command. Arguments should have
 * been previously loaded by get_ext_options() and stored in ext_opts array */
static char *
ext_options_generator(const char *text, int state)
{
	static int i;
	static size_t len;
	const char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while (*(name = ext_opts[i++])) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static size_t
rl_count_words(char **w, char **start)
{
	size_t start_word = 0, full_word = 0;
	size_t n = count_words(&start_word, &full_word);
	char *lb = rl_line_buffer;

	static char first_word[NAME_MAX];
	*first_word = '\0';
	*w = (char *)NULL;

	if (full_word != 0) {
		lb[full_word] = '\0';
		const char *q = lb + start_word;
		xstrsncpy(first_word, q, sizeof(first_word));
		lb[full_word] = ' ';
		*w = first_word;
		if (lb && rl_end > 0 && lb[rl_end - 1] == ' ')
			n++;
	}

	*start = lb ? lb + start_word : (char *)NULL;
	return n;
}

/* Readline returned a single match: let's swap the first and second fields
 * of the returned array (A), so that the match is listed instead of
 * automatically inserted into the command line (by tab_complete() in tabcomp.c) */
static void
rl_swap_fields(char ***a)
{
	*a = xnrealloc(*a, 3, sizeof(char *));
	(*a)[1] = strdup((*a)[0]);
	*(*a)[0] = '\0';
	(*a)[2] = (char *)NULL;
}

/* Return a list of options for the command named CMD_NAME. */
static char **
fill_opts(const char *cmd_name, const char *word_start, const size_t w)
{
#define MAX_OPTS 23
	struct cmd_opts_t {
		const char *cmd;
		char *opts[MAX_OPTS];
	};

	static struct cmd_opts_t cmd_opts[] = {
		{"acd", {"on", "off", "status", NULL}},
		{"ao", {"on", "off", "status", NULL}},
		{"ext", {"on", "off", "status", NULL}},
		{"ff", {"on", "off", "status", NULL}},
		{"hf", {"on", "off", "first", "last", "status", NULL}},
		{"hh", {"on", "off", "first", "last", "status", NULL}},
		{"hidden", {"on", "off", "first", "last", "status", NULL}},
		{"pg", {"on", "off", "once", "status", NULL}},
		{"pager", {"on", "off", "once", "status", NULL}},
		{"cl", {"on", "off", NULL}},
		{"icons", {"on", "off", NULL}},
		{"ll", {"on", "off", NULL}},
		{"lv", {"on", "off", NULL}},
		{"lm", {"on", "off", NULL}},
		{"fz", {"on", "off", NULL}},
		{"config", {"edit", "dump", "reload", "reset", NULL}},
		{"actions", {"list", "edit", NULL}},
		{"log", {"cmd", "msg", NULL}},
		{"mm", {"open", "info", "edit", "import", NULL}},
		{"mime", {"open", "info", "edit", "import", NULL}},
		{"pf", {"set", "list", "add", "del", "rename", NULL}},
		{"profile", {"set", "list", "add", "del", "rename", NULL}},
		{"prompt", {"set", "list", "unset", "edit", "reload", NULL}},
		{"pwd", {"-L", "-P", NULL}},
		{"tag", {"add", "del", "list", "list-full", "merge", "new",
			"rename", "untag", NULL}},
		{"view", {"edit", "purge", NULL}},
		{"net", {"mount", "unmount", "list", "edit", NULL}},
		{"history", {"edit", "clear", "on", "off", "status",
			"show-time", NULL}},
		{"help", {"archives", "autocommands", "basics", "bookmarks",
			"commands", "desktop-notifications", "dir-jumper", "file-details",
			"file-filters", "file-previews", "image-previews",
			"file-tags", "navigation", "plugins", "profiles", "remotes",
			"resource-opener", "search", "security", "selection",
			"theming", "trash", NULL}},
		{"b", {"hist", "clear", NULL}},
		{"f", {"hist", "clear", NULL}},
		{"kb", {"list", "bind", "edit", "conflict", "reset", "readline", NULL}},
		{"keybinds", {"list", "bind", "edit", "conflict", "reset",
			"readline", NULL}},
	};

	static char *c_opts[MAX_OPTS];
	/* Clear c_opts. */
	memset(c_opts, 0, sizeof(c_opts));

	if (w == 2) {
		const size_t cmd_count = sizeof(cmd_opts) / sizeof(cmd_opts[0]);
		for (size_t i = 0; i < cmd_count; i++) {
			if (*cmd_name != *cmd_opts[i].cmd
			|| strcmp(cmd_name, cmd_opts[i].cmd) != 0)
				continue;

			size_t j;
			for (j = 0; j < MAX_OPTS && cmd_opts[i].opts[j] != NULL; j++)
				c_opts[j] = cmd_opts[i].opts[j];

			break;
		}
	} else if (w == 3 && word_start && (strncmp(word_start, "log msg ", 8) == 0
	|| strncmp(word_start, "log cmd ", 8) == 0)) {
		c_opts[0] = "list"; c_opts[1] = "on"; c_opts[2] = "off";
		c_opts[3] = "status"; c_opts[4] = "clear"; c_opts[5] = NULL;
	}

#undef MAX_OPTS
	return !c_opts[0] ? (char **)NULL : c_opts;
}

/* Return an array of options, matching TEXT, for the command CMD_NAME. */
static char **
complete_options(const char *text, const char *cmd_name, const char *cmd_start,
	const size_t words_n)
{
	if (!cmd_name || !cmd_start)
		return (char **)NULL;

	char **c_opts = fill_opts(cmd_name, cmd_start, words_n);
	if (!c_opts || !c_opts[0])
		return (char **)NULL;

	size_t n;
	for (n = 0; c_opts[n]; n++);
	char **matches = xnmalloc(n + 2, sizeof(char *));
	matches[0] = savestring("", 1);

	const size_t len = (!text || !*text) ? 0 : strlen(text);
	char *name;

	n = 1;
	size_t i = 0;
	while ((name = c_opts[i++]) != NULL) {
		if (len == 0 || strncmp(name, text, len) == 0)
			matches[n++] = strdup(name);
	}

	if (n == 1) { /* No matches. */
		free(matches[0]);
		free(matches);
		return (char **)NULL;
	}

	if (n == 2) { /* A single match. */
		free(matches[0]);
		matches[0] = matches[1];
		matches[1] = (char *)NULL;
	} else { /* Multiple matches. */
		matches[n] = NULL;
	}

	return matches;
}

static char *
groups_generator(const char *text, int state)
{
#if defined(__ANDROID__)
	UNUSED(text); UNUSED(state);
	return (char *)NULL;
#else
	static size_t len;
	const struct group *p;

	if (state == 0)
		len = text[1] ? wc_xstrlen(text + 1) : 0;

	while ((p = getgrent())) {
		if (!p->gr_name) break;
		if (len == 0 || strncmp(p->gr_name, text + 1, len) == 0)
			return strdup(p->gr_name);
	}

	return (char *)NULL;
#endif /* __ANDROID__ */
}

static char *
owners_generator(const char *text, int state)
{
#if defined(__ANDROID__)
	UNUSED(text); UNUSED(state);
	return (char *)NULL;
#else
	static size_t len;
	const struct passwd *p;

	if (state == 0)
		len = wc_xstrlen(text);

	while ((p = getpwent())) {
		if (!p->pw_name) break;
		if (len == 0 || strncmp(p->pw_name, text, len) == 0)
			return strdup(p->pw_name);
	}

	return (char *)NULL;
#endif /* __ANDROID__ */
}

static char *
users_generator(const char *text, int state)
{
#if defined(__ANDROID__)
	UNUSED(text); UNUSED(state);
	return (char *)NULL;
#else
	static size_t len;
	const struct passwd *p;

	if (state == 0)
		len = strlen(text);

	while ((p = getpwent())) {
		if (!p->pw_name) break;
		if (len == 0 || strncmp(p->pw_name, text, len) == 0) {
			char t[NAME_MAX];
			snprintf(t, sizeof(t), "~%s", p->pw_name);
			return strdup(t);
		}
	}

	return (char *)NULL;
#endif /* __ANDROID__ */
}

#ifndef _NO_TAGS
static int
tag_complete(const char *text, char *start)
{
	char *l = start;
	int comp = 0;
	if (l[1] && l[2] == ' ') {
		switch (l[1]) {
		case 'a': /* fallthough */
		case 'u':
			if (text && *text == ':') { /* We have a tag name */
				comp = 1; cur_comp_type = TCMP_TAGS_C;
			} else if (l[1] == 'u') { /* We have a tagged file */
				comp = 2;
			}
			break;
		case 'd': /* fallthough */
		case 'l': /* fallthough */
		case 'm': /* fallthough */
//		case 'n': /* Just a new tag name: no completion */
		case 'y':
			if (l[1] == 'd' || l[1] == 'l') flags |= MULTI_SEL;
			comp = 1; cur_comp_type = TCMP_TAGS_S; break;
		default: break;
		}
	} else { /* MATCH LONG OPTIONS */
		if (strncmp(l, "tag ", 4) != 0) {
			return comp;
		}
		char *p = l + 4;
		if (!*p || strncmp(p, "untag ", 6) == 0) {
			if (text && *text == ':') { /* We have a tag name */
				comp = 1; cur_comp_type = TCMP_TAGS_C;
			} else if (*p == 'u') { /* We have a tagged file */
				comp = 2;
			}
		} else if (strncmp(p, "del ", 4) == 0 || strncmp(p, "list ", 5) == 0
		/*|| strncmp(p, "new ", 4) == 0 */ || strncmp(p, "rename ", 7) == 0
		|| strncmp(p, "merge ", 6) == 0) {
			if (*p == 'd' || *p == 'r' || *p == 'l') flags |= MULTI_SEL;
			comp = 1; cur_comp_type = TCMP_TAGS_S;
		} else if (text && *text == ':') {
			comp = 1; cur_comp_type = TCMP_TAGS_C;
		}
	}

	return comp;
}
#endif /* !_NO_TAGS */

static int
check_file_type_opts(const char c)
{
	switch (c) {
	case 'b': return stats.block_dev > 0;
	case 'c': return stats.char_dev > 0;
	case 'd': return stats.dir > 0;
	case 'D': return stats.empty_dir > 0;
#ifdef SOLARIS_DOORS
	case 'O': return stats.door > 0;
	case 'P': return stats.port > 0;
#endif /* SOLARIS_DOORS */
	case 'f': return stats.reg > 0;
	case 'F': return stats.empty_reg > 0;
	case 'h': return stats.multi_link > 0;
	case 'l': return stats.link > 0;
	case 'L': return stats.broken_link > 0;
	case 'p': return stats.fifo > 0;
	case 's': return stats.socket > 0;
	case 'x': return stats.exec > 0;
	case 'o': return stats.other_writable > 0;
	case 't': return stats.sticky > 0;
	case 'u': return stats.suid > 0;
	case 'g': return stats.sgid > 0;
	case 'C': return stats.caps > 0;
	default: return 0;
	}
}

static char *
file_types_opts_generator(const char *text, int state)
{
	UNUSED(text);
	static int i;

	if (state == 0)
		i = 0;

	static char *const ft_opts[] = {
		"b (Block device)",
		"c (Character device)",
		"d (Directory)",
		"D (Empty directory)",
#ifdef SOLARIS_DOORS
		"O (Door)",
		"P (Port)",
#endif /* SOLARIS_DOORS */
		"f (Regular file)",
		"F (Empty regular file)",
		"h (Multi-hardlink file)",
		"l (Symbolic link)",
		"L (Broken symbolic link)",
		"p (FIFO-pipe)",
		"s (Socket)",
		"x (Executable)",
		"o (Other writable)",
		"t (Sticky)",
		"u (SUID)",
		"g (SGID)",
		"C (Capabilities)",
		NULL
	};

	char *name;
	while ((name = ft_opts[i++])) {
		if (check_file_type_opts(*name) == 1)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
file_types_generator(const char *text, int state)
{
	static filesn_t i;
	const char *name;

	if (state == 0)
		i = 0;

	char *ret = (char *)NULL;
	while (i < files && (name = file_info[i].name)) {
		switch (*text) {
		case 'b':
			if (file_info[i].type == DT_BLK)
				ret = strdup(name);
			break;
		case 'c':
			if (file_info[i].type == DT_CHR)
				ret = strdup(name);
			break;
		case 'C':
			if (file_info[i].color == ca_c)
				ret = strdup(name);
			break;
		case 'd':
			if (file_info[i].dir == 1)
				ret = strdup(name);
			break;
		case 'D':
			if (file_info[i].color == ed_c)
				ret = strdup(name);
			break;
#ifdef SOLARIS_DOORS
		case 'O':
			if (file_info[i].type == DT_DOOR)
				ret = strdup(name);
			break;
		case 'P':
			if (file_info[i].type == DT_PORT)
				ret = strdup(name);
			break;
#endif /* SOLARIS_DOORS */
		case 'f':
			if (file_info[i].type == DT_REG)
				ret = strdup(name);
			break;
		case 'F':
			if (file_info[i].color == ef_c)
				ret = strdup(name);
			break;
		case 'h':
			if (file_info[i].dir == 0 && file_info[i].linkn > 1)
				ret = strdup(name);
			break;
		case 'l':
			if (file_info[i].type == DT_LNK)
				ret = strdup(name);
			break;
		case 'L':
			if (file_info[i].color == or_c)
				ret = strdup(name);
			break;
		case 'o':
			if (file_info[i].color == tw_c || file_info[i].color == ow_c)
				ret = strdup(name);
			break;
		case 't':
			if (file_info[i].color == tw_c || file_info[i].color == st_c)
				ret = strdup(name);
			break;

		case 'p':
			if (file_info[i].type == DT_FIFO)
				ret = strdup(name);
			break;
		case 's':
			if (file_info[i].type == DT_SOCK)
				ret = strdup(name);
			break;
		case 'x':
			if (file_info[i].exec == 1)
				ret = strdup(name);
			break;
		case 'u':
			if (file_info[i].color == su_c)
				ret = strdup(name);
			break;
		case 'g':
			if (file_info[i].color == sg_c)
				ret = strdup(name);
			break;
		default: break;
		}

		i++;
		if (ret)
			return ret;
	}

	return (char *)NULL;
}

static char **
rl_fastback(char *s)
{
	if (!s || !*s)
		return (char **)NULL;

	char *p = fastback(s);
	if (!p)
		return (char **)NULL;

	if (!*p) {
		free(p);
		return (char **)NULL;
	}

	char **matches = xnmalloc(2, sizeof(char *));
	matches[0] = savestring(p, strlen(p));
	matches[1] = (char *)NULL;

	free(p);

	return matches;
}

#ifndef _NO_LIRA
/* Return 1 if the command STR accepts 'edit' as subcommand. Otherwise,
 * return 0. */
static int
cmd_takes_edit(const char *str)
{
	static char *const cmds[] = {
		"actions",
		"bm", "bookmarks",
		"config",
		"cs", "colorschemes",
		"history",
		"kb", "keybinds",
		"mm", "mime",
		"net",
		"prompt",
		"view",
		NULL
	};

	size_t i;
	for (i = 0; cmds[i]; i++)
		if (*str == *cmds[i] && strcmp(str + 1, cmds[i] + 1) == 0)
			return 1;

	return 0;
}

/* Return 1 if command in STR is an internal command and takes a text editor as
 * parameter. Otherwise, return 0. */
static int
is_edit(const char *str, const size_t words_n)
{
	if (!str || !*str)
		return 0;

	if (words_n > 2 && *str == 'r' && str[1] == 'r' && str[2] == ' ')
		return 1;

	char *space = strchr(str, ' ');
	if (!space || space[1] != 'e' || !space[2])
		return 0;

	*space = '\0';
	if (cmd_takes_edit(str) != 1) {
		*space = ' ';
		return 0;
	}
	*space = ' ';

	if (strncmp(space + 2, "dit ", 4) != 0)
		return 0;

	return 1;
}
#endif /* !_NO_LIRA */

static char **
complete_bookmark_names(char *text, const size_t words_n, int *exit_status)
{
	*exit_status = FUNC_SUCCESS;

	/* rl_line_buffer is either "bm " or "bookmarks " */
	const char *arg = rl_line_buffer + (rl_line_buffer[1] == 'o' ? 9 : 2);

	if (arg && arg[1] == 'a' && (arg[2] == ' '
	|| strncmp(arg + 1, "add ", 4) == 0)) {
		if (words_n > 3) /* Do not complete anything after "bm add FILE" */
			rl_attempted_completion_over = 1;
		else /* 'bm add': complete with path completion */
			*exit_status = FUNC_FAILURE;
		return (char **)NULL;
	}

	/* If not 'bm add' complete with bookmarks */
#ifndef _NO_SUGGESTIONS
	if (suggestion.type != FILE_SUG)
		rl_attempted_completion_over = 1;
#endif /* !_NO_SUGGESTIONS */
	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &bookmarks_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_BOOKMARK;
	return matches;
}

static char **
complete_ranges(char *text)
{
	char *dash = strchr(text, '-');
	if (!dash || dash[1] < '0' || dash[1] > '9')
		return (char **)NULL;

	*dash = '\0';
	if (!is_number(text) || !is_number(dash + 1)) {
		*dash = '-';
		return (char **)NULL;
	}

	const int a = atoi(text) - 1;
	const int b = atoi(dash + 1) - 1;
	*dash = '-';

	if (a < 0 || b < 0 || a >= b || (filesn_t)b >= files)
		return (char **)NULL;

	char **matches = xnmalloc((size_t)(b - a) + 3, sizeof(char *));
	matches[0] = savestring("", 1);
	size_t j = 1;
	for (int i = a; i <= b; i++)
		matches[j++] = savestring(file_info[i].name, file_info[i].bytes);
	matches[j] = NULL;

	cur_comp_type = TCMP_RANGES;
	return matches;
}

#ifndef _NO_LIRA
static char **
complete_open_with(char *text, char *start)
{
	char *arg = start + 3; /* "ow " */
	char *space = strrchr(arg, ' ');
	if (space)
		*space = '\0';

	char **matches = mime_open_with_tab(arg, text, 0);

	if (space)
		*space = ' ';

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_OPENWITH;
	return matches;
}
#endif /* !_NO_LIRA */

static char **
complete_file_type_filter(char *text)
{
	char **matches = (char **)NULL;

	if (!text[1]) {
		matches = rl_completion_matches(text, &file_types_opts_generator);
		if (!matches)
			return (char **)NULL;

		if (!matches[1])
			rl_swap_fields(&matches);

		cur_comp_type = TCMP_FILE_TYPES_OPTS;
		return matches;
	}

	if (text[2])
		return (char **)NULL;

	matches = rl_completion_matches(text + 1, &file_types_generator);
	if (!matches)
		return (char **)NULL;

	if (!matches[1])
		rl_swap_fields(&matches);
	else
		flags |= MULTI_SEL;

	cur_comp_type = TCMP_FILE_TYPES_FILES;
	return matches;
}

static char **
complete_mime_type_filter(char *text)
{
	char **matches = (char **)NULL;

	if (text[1]) {
		if ((matches = rl_mime_files(text + 1)) == NULL)
			return (char **)NULL;

		cur_comp_type = TCMP_MIME_FILES; /* Same as TCMP_FILE_TYPES_FILES */
		rl_filename_completion_desired = 1;
		flags |= MULTI_SEL;
		return matches;
	}

	if ((matches = rl_mime_list()) == NULL)
		return (char **)NULL;

	cur_comp_type = TCMP_MIME_LIST;
	return matches;
}

static char **
complete_glob(char *text)
{
	char **matches = (char **)NULL;

	char *p = (*rl_line_buffer == '/' && rl_end > 1
		&& !strchr(rl_line_buffer + 1, ' ')
		&& !strchr(rl_line_buffer + 1, '/'))
			? (text + 1) : text;

	if ((matches = rl_glob(p)) == NULL)
		return (char **)NULL;

#ifndef _NO_SUGGESTIONS
	if (conf.suggestions == 1 && wrong_cmd == 1)
		recover_from_wrong_cmd();
#endif /* !_NO_SUGGESTIONS */
	if (!matches[1])
		rl_swap_fields(&matches);

	cur_comp_type = TCMP_GLOB;
	rl_filename_completion_desired = 1;

	if (words_num > 1)
		flags |= MULTI_SEL;

	return matches;
}

/* Return a pointer to the beginning of the last name in the current
 * command line. */
static char *
get_cmd_name(void)
{
	if (!rl_line_buffer || !*rl_line_buffer)
		return (char *)NULL;

	char *lb = rl_line_buffer;
	char *opt = (char *)NULL;
	char *name = (char *)NULL;

	/* Truncate the command line before the first option word (starting
	 * with a dash): "sudo cmd --opt" -> "sudo cmd" */
	while (*lb) {
		if (*lb == ' ' && lb[1] == '-') {
			*lb = '\0';
			opt = lb;
			break;
		}
		lb++;
	}

	/* Get a pointer to the beginning of the last name in the truncated
	 * command line. */
	lb = rl_line_buffer;
	while (*lb) {
		if (!name && *lb != ' ') {
			name = lb;
		} else {
			if (*lb == ' ' && lb[1] != ' ')
				name = lb + 1;
		}
		lb++;
	}

	if (opt)
		*opt = ' ';

	return name;
}

static char **
complete_shell_cmd_opts(char *text)
{
	char cmd[NAME_MAX + 1]; *cmd = '\0';
	char *name = get_cmd_name();
	if (!name)
		return (char **)NULL;

	char *space = strchr(name, ' ');
	if (space) {
		*space = '\0';
		xstrsncpy(cmd, name, sizeof(cmd));
		*space = ' ';
	}

	if (*cmd && get_shell_cmd_opts(cmd) > 0)
		return rl_completion_matches(text, &ext_options_generator);

	return (char **)NULL;
}

#ifndef _NO_TAGS
static char **
complete_tag_names(char *text, char *start)
{
	char **matches = (char **)NULL;

	int comp = tag_complete(text, start);
	if (comp != 1 && comp != 2)
		return (char **)NULL;

	if (comp == 1) {
		matches = rl_completion_matches(text, &tags_generator);
		if (!matches) {
			cur_comp_type = TCMP_NONE;
			return (char **)NULL;
		}

		return matches;
	}

	/* comp == 2
	 * Let's match tagged files for the untag function. */
	char *c_tag = get_cur_tag();
	matches = check_tagged_files(c_tag);
	free(c_tag);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_TAGS_F;
	return matches;
}

static char **
complete_tag_names_t(char *text)
{
	cur_comp_type = TCMP_TAGS_T;

	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &tags_generator);
	free(p);

	if (!matches) {
		cur_comp_type = TCMP_NONE;
		return (char **)NULL;
	}

	flags |= MULTI_SEL;
	return matches;
}

static char **
complete_tags(char *text)
{
	if (!text[2])
		return complete_tag_names_t(text);

	free(cur_tag);
	cur_tag = savestring(text + 2, strlen(text + 2));
	char **matches = check_tagged_files(cur_tag);

	if (!matches) {
		free(cur_tag);
		cur_tag = (char *)NULL;
		return (char **)NULL;
	}

	if (!matches[1])
		rl_swap_fields(&matches);

	cur_comp_type = TCMP_TAGS_F;
	return matches;
}
#endif /* !_NO_TAGS */

static char **
complete_bookmark_paths(char *text)
{
	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &bm_paths_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	if (!matches[1])
		rl_swap_fields(&matches);

	cur_comp_type = TCMP_BM_PATHS;
	return matches;
}

static char **
complete_bookmark_names_b(char *text)
{
	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &bookmarks_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	flags |= MULTI_SEL;
	cur_comp_type = TCMP_BM_PREFIX;
	return matches;
}

static char **
complete_bookmarks(char *text, const size_t words_n)
{
	if (text[2]) {
		char **matches = complete_bookmark_paths(text + 2);
		if (matches)
			return matches;
	}

	if (words_n || conf.autocd == 1 || conf.auto_open == 1)
		return complete_bookmark_names_b(text);

	return (char **)NULL;
}

static char **
complete_ownership(const char *text)
{
	char **matches = (char **)NULL;

	rl_attempted_completion_over = 1;
	char *sc = strchr(text, ':');
	if (!sc) {
		matches = rl_completion_matches(text, &owners_generator);
		endpwent();
	} else {
		matches = rl_completion_matches(sc, &groups_generator);
		endgrent();
	}

	if (matches) {
		cur_comp_type = TCMP_OWNERSHIP;
		return matches;
	}

	return (char **)NULL;
}

static char **
complete_dirhist(char *text, const size_t words_n)
{
	rl_attempted_completion_over = 1;
	if (words_n > 2)
		return (char **)NULL;

	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &dirhist_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	if (!matches[1])
		rl_swap_fields(&matches);

	cur_comp_type = TCMP_DIRHIST;
	return matches;
}

static char **
complete_backdir(char *text, const size_t words_n)
{
	rl_attempted_completion_over = 1;
	if (words_n != 2)
		return (char **)NULL;

	int n = 0;
	char *p = unescape_str(text, 0);
	char **matches = get_bd_matches(p ? p : text, &n, BD_TAB);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_BACKDIR;
	return matches;
}

static char **
complete_workspaces(char *text)
{
	rl_attempted_completion_over = 1;
	if (words_num > 2)
		return (char **)NULL;

	rl_sort_completion_matches = 0;
	char *t = (*text == 'w' && text[1] == ':') ? text + 2 : text;
	char *p = unescape_str(t, 0);

	const enum comp_type ct = cur_comp_type;
	cur_comp_type = t != text ? TCMP_WS_PREFIX : TCMP_WORKSPACES;

	char **matches = rl_completion_matches(p ? p : t, &workspaces_generator);
	free(p);

	if (matches)
		return matches;

	cur_comp_type = ct;

	rl_sort_completion_matches = 1;
	return (char **)NULL;
}

static int
int_cmd_no_filename(char *start)
{
	char *line = start;
	char *space = strchr(line, ' ');
	if (!space)
		return 0;

	*space = '\0';
	flags |= STATE_COMPLETING;
	if (is_internal_cmd(line, NO_FNAME_NUM, 1, 1)) {
		rl_attempted_completion_over = 1;
		*space = ' ';
		flags &= ~STATE_COMPLETING;
		return 1;
	}

	flags &= ~STATE_COMPLETING;
	*space = ' ';
	return 0;
}

static char **
complete_net(char *text)
{
	rl_attempted_completion_over = 1;
	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &nets_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_NET;
	return matches;
}

static char **
complete_sort_num(const char *text, const size_t words_n)
{
	rl_attempted_completion_over = 1;
	if (words_n != 2)
		return (char **)NULL;

	const int n = atoi(text);
	if (n < 0 || n > SORT_TYPES)
		return (char **)NULL;

	char **matches = rl_completion_matches(text, &sort_num_generator);
	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_SORT;
	return matches;
}

static char **
complete_sort(const char *text, const size_t words_n)
{
	rl_attempted_completion_over = 1;
	if (words_n > 2)
		return (char **)NULL;

	if (text && *text && is_number(text))
		return complete_sort_num(text, words_n);

	char **matches = rl_completion_matches(text, &sort_name_generator);
	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_SORT;
	return matches;
}

#ifndef _NO_PROFILES
static char **
complete_profiles(char *text, const size_t words_n)
{
	rl_attempted_completion_over = 1;
	if (words_n > 3)
		return (char **)NULL;

	const char *lb = rl_line_buffer;
	if (strncmp(lb, "pf add ", 7) == 0 || strncmp(lb, "pf list ", 8) == 0
	|| strncmp(lb, "profile add ", 12) == 0 || strncmp(lb, "profile list ", 13) == 0)
		return (char **)NULL;

	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &profiles_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_PROF;
	return matches;
}
#endif /* !_NO_PROFILES */

static char **
complete_colorschemes(char *text, const size_t words_n)
{
	rl_attempted_completion_over = 1;
	if (words_n != 2)
		return (char **)NULL;

	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &cschemes_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_CSCHEME;
	return matches;
}

static char **
complete_file_templates(char *text)
{
	rl_attempted_completion_over = 1;
	char *p = unescape_str(text, 0);
	char **matches =
		rl_completion_matches(p ? p : text, &file_templates_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_FILE_TEMPLATES;
	return matches;
}

static char **
complete_desel(const char *text)
{
	rl_attempted_completion_over = 1;
	char **matches = sel_n > 0
		? rl_completion_matches(text, &sel_entries_generator) : NULL;
	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_DESEL;
	return matches;
}

#ifndef _NO_TRASH
static char **
complete_trashed_files(const char *text, const enum comp_type flag)
{
	rl_attempted_completion_over = 1;
	char **matches = trash_n > 0 ? rl_trashed_files(text) : NULL;
	if (!matches)
		return (char **)NULL;

	if (tabmode == STD_TAB && conf.colorize == 1)
		/* Only used to remove trash extension from files so
		 * that we can correctly set file color by extension. */
		flags |= STATE_COMPLETING;

	cur_comp_type = flag;
	return matches;
}
#endif /* !_NO_TRASH */

static char **
complete_prompt_names(char *text, const size_t words_n)
{
	rl_attempted_completion_over = 1;
	if (words_n > 3)
		return (char **)NULL;

	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &prompts_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_PROMPTS;
	return matches;
}

static char **
complete_kb_func_names(const char *text, const size_t words_n)
{
	rl_attempted_completion_over = 1;
	if (words_n != 3)
		return (char **)NULL;

	char **matches = rl_completion_matches(text, &kb_func_names_gen);
	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_NET; /* Same behavior */
	return matches;
}

static char **
complete_alias_names(const char *text, const size_t words_n)
{
	rl_attempted_completion_over = 1;
	if (words_n > 2 || aliases_n == 0)
		return (char **)NULL;

	char **matches = rl_completion_matches(text, &aliases_generator);
	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_ALIAS;
	return matches;
}

static char **
complete_jump(const char *text)
{
	char **matches = rl_completion_matches(text, &jump_generator);
	if (!matches)
		return (char **)NULL;

	if (!matches[1])
		rl_swap_fields(&matches);

	cur_comp_type = TCMP_JUMP;
	return matches;
}

static char **
complete_sel_keyword(const char *text, const size_t words_n)
{
	if (words_n == 1 && text[1] != ':') /* Only "s:" is allowed as first word */
		return (char **)NULL;

	char **matches = rl_completion_matches("", &sel_entries_generator);
	if (!matches)
		return (char **)NULL;

	if (!matches[1])
		rl_swap_fields(&matches);

	cur_comp_type = TCMP_SEL;
	return matches;
}

static char **
get_filename_by_eln(const filesn_t n)
{
	const int is_cd_cmd = (rl_line_buffer && *rl_line_buffer == 'c'
		&& rl_line_buffer[1] == 'd' && rl_line_buffer[2] == ' ');

	if (!file_info[n].name || !*file_info[n].name
	|| (is_cd_cmd == 1 && file_info[n].dir == 0))
		return (char **)NULL;

	char **matches = xnmalloc(2, sizeof(char *));
	matches[0] = savestring(file_info[n].name, file_info[n].bytes);
	matches[1] = NULL;

#ifndef _NO_SUGGESTIONS
	if (suggestion_buf)
		clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

	return matches;
}

static char **
complete_eln(char *text, const size_t words_n, char *cmd_name)
{
	filesn_t n = 0;

	if (!is_number(text) || (n = xatof(text)) < 1 || n > files)
		return (char **)NULL;

	/* First word */
	if (words_n == 1) {
		if ((alt_prompt != 0 && alt_prompt != FILES_PROMPT)
		|| (file_info[n - 1].dir == 1 && conf.autocd == 0)
		|| (file_info[n - 1].dir == 0 && conf.auto_open == 0))
			return (char **)NULL;
	} else { /* Second word or more */
		if (alt_prompt == FILES_PROMPT || alt_prompt == OWNERSHIP_PROMPT
		|| should_expand_eln(text, cmd_name) == 0)
			return (char **)NULL;
	}

	char **matches = get_filename_by_eln(n - 1);
	if (!matches)
		return (char **)NULL;

	rl_filename_completion_desired = 1;
	cur_comp_type = TCMP_ELN;
	return matches;
}

static char **
complete_history(char *text)
{
	char *p = unescape_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &hist_generator);
	free(p);
	if (matches)
		cur_comp_type = TCMP_HIST;
	return matches;
}

static char **
complete_bookmarks_prompt(const char *text)
{
	rl_attempted_completion_over = 1;
	char **matches = rl_completion_matches(text, &bookmarks_generator);
	if (matches)
		cur_comp_type = TCMP_NET;
	return matches;
}

static char **
complete_cmd_desc(const char *text)
{
	char **matches = rl_completion_matches(text, &int_cmds_generator);
	if (matches)
		cur_comp_type = TCMP_CMD_DESC;
	return matches;
}

static char **
complete_fastback(char *text)
{
	char **matches = rl_fastback((char *)text);
	if (!matches)
		return (char **)NULL;

	if (*matches[0] != '/' || matches[0][1])
		rl_filename_completion_desired = 1;

	cur_comp_type = TCMP_PATH;
	return matches;
}

static char **
complete_users(const char *text)
{
	char **matches = rl_completion_matches(text, &users_generator);
	endpwent();
	if (matches)
		cur_comp_type = TCMP_USERS;
	return matches;
}

static char **
complete_environ(const char *text)
{
	char **matches = rl_completion_matches(text, &environ_generator);
	if (matches)
		cur_comp_type = TCMP_ENVIRON;
	return matches;
}

/* Handle tab completion.
 *
 * This function has three main blocks:
 * 1) General expansions: These expansions can be performed in any part of
 *    the current input string (mostly special expansions, like ELN's,
 *    bookmarks, sel keyword, tags, and so on).
 * 2) First word only (mostly command names).
 * 3) Second word or more (mostly command parameters).
 *
 * If the function returns NULL, readline will attempt to perform path
 * completion (via my_rl_path_completion()).
 * */
char **
my_rl_completion(const char *text, const int start, const int end)
{
	/* 0. First, we need some initialization. */

	UNUSED(end);
	cur_comp_type = TCMP_NONE;
	flags &= ~MULTI_SEL;

	char **matches = (char **)NULL;
	char *cmd_name = (char *)NULL;
	char *cmd_start = (char *)NULL;
	const size_t words_n = rl_count_words(&cmd_name, &cmd_start);
	char *s = cmd_start;

	static size_t sudo_len = 0;
	if (sudo_len == 0)
		sudo_len = (sudo_cmd && *sudo_cmd) ? strlen(sudo_cmd) : 0;

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1 && rl_point < rl_end)
	/* Prevent the inserted word from being printed using the current color,
	 * say, options or quotes color.
	 * Drawback: whatever comes next to our word will be decolorized as well.
	 * But no color is better than wrong (and partially) colored word. */
		cur_color = (char *)NULL;
#endif /* !_NO_HIGHLIGHT */

	/* Do not complete when the cursor is on a word. E.g., dir/_ilename */
	if (rl_point < rl_end && rl_line_buffer[rl_point] != ' ') {
		rl_attempted_completion_over = 1;
		return (char **)NULL;
	}

				/* ##########################
				 * # 1. GENERAL EXPANSIONS  #
				 * ########################## */
	 /* These expansions are made no matter the word position in the cmd line. */

	/* ##### ELN EXPANSION ##### */
	if (*text >= '1' && *text <= '9'
	&& (matches = complete_eln((char *)text, words_n, cmd_name)))
		return matches;

	/* alt_prompt is set (non-zero) whenever we are using an alternative prompt. */
	if (alt_prompt != 0)
		goto FIRST_WORD_COMP;

	/* #### FILE TYPE EXPANSION #### */
	if (*text == '=' && (matches = complete_file_type_filter((char *)text)))
		return matches;

	/* #### MIME TYPE EXPANSION #### */
	if (*text == '@' && (matches = complete_mime_type_filter((char *)text)))
		return matches;

	/* #### FASTBACK EXPANSION #### */
	if (*text == '.' && text[1] == '.' && text[2] == '.'
	&& (matches = complete_fastback((char *)text)))
		return matches;

	/* #### WILDCARDS EXPANSION #### */
	char *g = strpbrk(text, GLOB_CHARS);
	/* Expand only glob expressions in the last path component */
	if (g && !(rl_end == 2 && *rl_line_buffer == '/'
	&& rl_line_buffer[1] == '*') && !strchr(g, '/')
	&& access(text, F_OK) != 0) {
		if ((matches = complete_glob((char *)text)))
			return matches;
	}

	/* #### USERS EXPANSION (~) #### */
	if (*text == '~' && text[1] != '/' && (matches = complete_users(text + 1)))
		return matches;

	/* ##### ENVIRONMENT VARIABLES ##### */
	if (*text == '$' && text[1] != '(' && (matches = complete_environ(text)))
		return matches;

#ifndef _NO_TAGS
	/* ##### TAGS (t:) ##### */
	if (tags_n > 0 && *text == 't' && text[1] == ':'
	&& (matches = complete_tags((char *)text)))
		return matches;
#endif /* !_NO_TAGS */

	/* #### BOOKMARKS (b:) #### */
	if (*text == 'b' && text[1] == ':'
	&& (matches = complete_bookmarks((char *)text, words_n)))
		return matches;

	/* ##### WORKSPACES (w:) ##### */
	if ((words_n > 1 || conf.autocd == 1) && *text == 'w' && text[1] == ':'
	&& (matches = complete_workspaces((char *)text)))
		return matches;

	/* ##### SEL KEYWORD (both "sel" and "s:") ##### */
	if (sel_n > 0 && *text == 's' && (text[1] == ':'
	|| strcmp(text, "sel") == 0)
	&& (matches = complete_sel_keyword(text, words_n)))
		return matches;

	/* ##### HISTORY COMPLETION ("!") ##### */
	if (*text == '!' && (matches = complete_history((char *)text)))
		return matches;

FIRST_WORD_COMP:
	if (start == 0) {

					/* #######################
					 * # 2. FIRST WORD ONLY  #
					 * ####################### */

		/* #### OWNERSHIP EXPANSION ####
		 * Used only by the 'oc' command to edit files ownership. */
		if (alt_prompt == OWNERSHIP_PROMPT)
			return complete_ownership(text);

		/* Bookmarks screen: expand bookmark names */
		if (alt_prompt == BOOKMARKS_PROMPT)
			return complete_bookmarks_prompt(text);

		/* #### CMD<TAB> #### */
		if (alt_prompt == 0 && ((*text == 'c' && text[1] == 'm'
		&& text[2] == 'd' && !text[3]) || strcmp(text, "commands") == 0))
			return complete_cmd_desc(text);

		/* SEARCH PATTERNS COMPLETION */
		if (alt_prompt == 0 && *text == '/' && text[1] == '*'
		&& (matches = complete_history((char *)text)))
			return matches;

		/* Complete with files in CWD */
		if ((conf.autocd == 1 || conf.auto_open == 1) && (alt_prompt == 0
		|| alt_prompt == FILES_PROMPT) && *text != '/'
		&& (matches = rl_completion_matches(text, &filenames_gen_text))) {
			cur_comp_type = TCMP_PATH;
			return matches;
		}

		/* If neither autocd nor auto-open, try to complete with command names,
		 * except when TEXT is "/" */
		if (alt_prompt == 0 && (conf.autocd == 0 || *text != '/' || text[1])
		&& (matches = rl_completion_matches(text, &bin_cmd_generator))) {
			cur_comp_type = TCMP_CMD;
			return matches;
		}

		return (char **)NULL;
	}

				/* ##########################
				 * # 3. SECOND WORD OR MORE #
				 * ########################## */

	if (alt_prompt != 0) /* Disable completion for alternative prompts. */
		return (char **)NULL;

	/* Complete options for internal commands. */
	if ((matches = complete_options(text, cmd_name, cmd_start, words_n)))
		return matches;

	/* Command names completion for words after process separator: ; | && */
	if (words_n == 1 && rl_end > 0 && rl_line_buffer[rl_end - 1] != ' '
	/* No command name contains slashes. */
	&& (*text != '/' || !strchr(text, '/'))) {
		if ((matches = rl_completion_matches(text, &bin_cmd_generator))) {
			cur_comp_type = TCMP_CMD;
			return matches;
		}
	}

	/* #### SUDO COMPLETION (e.g., "sudo <TAB>") #### */
	if (sudo_len > 0 && words_n == 2 && s && strncmp(s, sudo_cmd, sudo_len) == 0
	&& s[sudo_len] == ' ') {
		if ((matches = rl_completion_matches(text, &bin_cmd_generator_ext))) {
			cur_comp_type = TCMP_CMD;
			return matches;
		}
	}

#ifndef _NO_TAGS
	/* #### TAG NAMES COMPLETION #### */
	/* 't? TAG' and 't? :tag' */
	if (tags_n > 0 && s && *s == 't' && rl_end > 2
	&& (matches = complete_tag_names((char *)text, s)))
		return matches;
#endif /* !_NO_TAGS */

	/* #### DIRECTORY HISTORY COMPLETION (dh) #### */
	if (s && *s == 'd' && s[1] == 'h' && s[2] == ' ' && !strchr(text, '/'))
		return complete_dirhist((char *)text, words_n);

	/* #### BACKDIR COMPLETION (bd) #### */
	if (*text != '/' && s && *s == 'b' && s[1] == 'd' && s[2] == ' ')
		return complete_backdir((char *)text, words_n);

#ifndef _NO_LIRA
	/* #### OPENING APPS FOR INTERNAL CMDS TAKING 'EDIT' AS SUBCOMMAND */
	if (s && is_edit(s, words_n) == 1 && config_file) {
		/* mime_open_with_tab needs a filename to match against the
		 * mimelist file and get the list of opening applications.
		 * Now, since here we are listing apps to open config files,
		 * i.e. text files, any config file will do the trick, in this
		 * case, the main config file (CONFIG_FILE). */
		if ((matches = mime_open_with_tab(config_file, text, 1))) {
			cur_comp_type = TCMP_OPENWITH;
			return matches;
		}
	}

	/* #### OPEN WITH #### */
	if (rl_end > 4 && s && *s == 'o' && s[1] == 'w' && s[2] == ' '
	&& s[3] && s[3] != ' ' && words_n >= 3)
		return complete_open_with((char *)text, s);
#endif /* _NO_LIRA */

	/* #### PROMPT (only for 'prompt set') #### */
	if (s && *s == 'p' && s[1] == 'r' && strncmp(s, "prompt set " , 11) == 0)
		return complete_prompt_names((char *)text, words_n);

#ifndef _NO_TRASH
	/* ### UNTRASH ### */
	if (s && *s == 'u' && (s[1] == ' ' || (s[1] == 'n'
	&& (strncmp(s, "untrash ", 8) == 0 || strncmp(s, "undel ", 6) == 0))))
		return complete_trashed_files(text, TCMP_UNTRASH);

	/* ### TRASH DEL ### */
	if (s && *s == 't' && (s[1] == ' ' || s[1] == 'r')
	&& (strncmp(s, "t del ", 6) == 0 || strncmp(s, "trash del ", 10) == 0))
		return complete_trashed_files(text, TCMP_TRASHDEL);
#endif /* !_NO_TRASH */

	/* ### DESELECT COMPLETION (ds, desel) ### */
	if (s && *s == 'd' && (strncmp(s, "ds ", 3) == 0
	|| strncmp(s, "desel ", 6) == 0))
		return complete_desel(text);

	/* ### DIRJUMP COMPLETION ### */
	/* j, jc, jp commands */
	if (s && *s == 'j' && (s[1] == ' ' || ((s[1] == 'c' || s[1] == 'p')
	&& s[2] == ' ')))
		return complete_jump(text);

	/* ### BOOKMARKS COMPLETION ### */
	if (s && *s == 'b' && (s[1] == 'm' || s[1] == 'o')
	&& (strncmp(s, "bm ", 3) == 0 || strncmp(s, "bookmarks ", 10) == 0)) {
		int exit_status = FUNC_SUCCESS;
		matches = complete_bookmark_names((char *)text, words_n, &exit_status);
		if (exit_status == FUNC_SUCCESS)
			return matches;
	}

	/* ### FILE TEMPLATES COMPLETION ('n/new' command) ### */
	if (file_templates && s && *s == 'n' && (s[1] == ' ' || (s[1] == 'e'
	&& s[2] == 'w' && s[3] == ' '))) {
		char *p = strrchr(text, '@');
		if (p)
			return complete_file_templates(p + 1);
	}

	/* ### ALIASES COMPLETION ### */
	if (s && *s == 'a' && strncmp(s, "alias ", 6) == 0
	&& strncmp(s + 6, "import ", 7) != 0)
		return complete_alias_names(text, words_n);

	/* ### KEYBIND FUNCS COMPLETION ### */
	if (s && *s == 'k' && strncmp(s, "kb bind ", 8) == 0)
		return complete_kb_func_names(text, words_n);

	/* ### COLOR SCHEMES COMPLETION ### */
	if (conf.colorize == 1 && s && *s == 'c' && ((s[1] == 's' && s[2] == ' ')
	|| strncmp(s, "colorschemes ", 13) == 0))
		return complete_colorschemes((char *)text, words_n);

#ifndef _NO_PROFILES
	/* ### PROFILES COMPLETION ### */
	if (s && *s == 'p' && (strncmp(s, "pf ", 3) == 0
	|| strncmp(s, "profile ", 8) == 0))
		return complete_profiles((char *)text, words_n);
#endif /* !_NO_PROFILES */

	/* ### SORT COMMAND COMPLETION ### */
	if (s && *s == 's' && (strncmp(s, "st ", 3) == 0
	|| strncmp(s, "sort ", 5) == 0))
		return complete_sort(text, words_n);

	/* ### WORKSPACES COMPLETION ### */
	if (s && *s == 'w' && strncmp(s, "ws ", 3) == 0)
		return complete_workspaces((char *)text);

	/* ### COMPLETIONS FOR THE 'UNSET' COMMAND ### */
	if (s && *s == 'u' && strncmp(s, "unset ", 6) == 0)
		return rl_completion_matches(text, &env_vars_generator);

	/* ### NET COMMAND COMPLETION ### */
	if (s && *s == 'n' && strncmp(s, "net ", 4) == 0)
		return complete_net((char *)text);

	/* If we have an internal command not dealing with filenames,
	 * do not perform any further completion. */
	if (s && int_cmd_no_filename(s) == 1)
		return (char **)NULL;

	/* Arguments for shell commands. */
	if (*text == '-' && (matches = complete_shell_cmd_opts((char *)text)))
		return matches;

	/* ELN ranges */
	if (*text >= '0' && *text <= '9'
	&& (matches = complete_ranges((char *)text)))
		return matches;

	/* Finally, try to complete with filenames in CWD. */
	if ((matches = rl_completion_matches(text, &filenames_gen_text))) {
		cur_comp_type = TCMP_PATH;
		return matches;
	}

	/* ### PATH COMPLETION ### */
	/* If none of the above, readline will attempt path completion
	 * instead via my_rl_path_completion(). */
	return (char **)NULL;
}

/* Load readline initialization file (inputrc)
 * Check order:
 * 1) INPUTRC environment variable
 * 2) ~/.config/clifm/readline.clifm
 * 3) ~/.inputrc
 * 4) /etc/inputrc
 * If neither 1 nor 2 exist, readline will try to read 3 and 4 by default) */
static void
set_rl_init_file(void)
{
	struct stat a;
	char *p = getenv("INPUTRC");
	if (xargs.secure_env != 1 && xargs.secure_env_full != 1
	&& p && stat(p, &a) != -1) {
		rl_read_init_file(p);
		return;
	}

	if (!config_dir_gral || !*config_dir_gral)
		return;

	size_t len = strlen(config_dir_gral) + 16;
	char *rl_file = xnmalloc(len, sizeof(char));
	snprintf(rl_file, len, "%s/readline.clifm", config_dir_gral);

	/* This file should have been imported by import_rl_file (config.c).
	 * In case it wasn't, let's create here a skeleton: if not found,
	 * readline refuses to colorize history lines. */
	if (stat(rl_file, &a) == -1) {
		int fd;
		FILE *fp = open_fwrite(rl_file, &fd);
		if (!fp) {
			err('w', PRINT_PROMPT, "%s: fopen: %s: %s\n", PROGRAM_NAME,
				rl_file, strerror(errno));
			free(rl_file);
			return;
		}

		fprintf(fp, "# This is readline's configuration file for %s\n",
			PROGRAM_NAME_UPPERCASE);
		fclose(fp);
	}

	rl_read_init_file(rl_file);
	free(rl_file);
}

#ifdef CLIFM_TEST_INPUT
/* Use the file specified by CLIFM_TEST_INPUT_FILE environment variable as an
 * alternative input source instead of stdin.
 * Each line in this file will be executed as if it were entered in the
 * command line. */
static void
set_rl_input_file(void)
{
	char *input_file = getenv("CLIFM_TEST_INPUT_FILE");
	if (!input_file || !*input_file) {
		xerror(_("%s: An input file must be provided via the "
			"CLIFM_TEST_INPUT_FILE environment variable\n"), PROGRAM_NAME);
		UNHIDE_CURSOR;
		exit(FUNC_FAILURE);
	}

	FILE *fstream = fopen(input_file, "r");
	if (!fstream) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, input_file, strerror(errno));
		UNHIDE_CURSOR;
		exit(FUNC_FAILURE);
	}

	rl_instream = fstream;
}
#endif /* CLIFM_TEST_INPUT */

int
initialize_readline(void)
{
#ifdef CLIFM_TEST_INPUT
	set_rl_input_file();
#endif /* CLIFM_TEST_INPUT */

#ifdef VANILLA_READLINE
	return FUNC_SUCCESS;
#endif /* VANILLA_READLINE */

	/* Set the name of the program using readline. Mostly used for
	 * conditional constructs in the inputrc file. */
	rl_readline_name = PROGRAM_NAME;

	disable_rl_conflicting_kbinds();
	set_rl_init_file();

	/* The character that introduces a history event. Defaults to '!'.
	 * Setting this to 0 inhibits history expansion. */
//	history_expansion_char = '!';

	/* Enable tab auto-completion for commands (in PATH) in case of
	  * first entered string (if autocd and/or auto-open are enabled, check
	  * for paths as well). The second and later entered strings will
	  * be autocompleted with paths instead, just like in Bash, or with
	  * completion for custom commands. I use a custom completion
	  * function to add custom completions, since readline's internal
	  * completer only performs path completion */

	/* Define a function for path completion.
	 * NULL means to use filename_entry_function (), the default
	 * filename completer. */
	rl_completion_entry_function = my_rl_path_completion;

	/* Pointer to alternative function to create matches.
	 * Function is called with TEXT, START, and END.
	 * START and END are indices in RL_LINE_BUFFER saying what the
	 * boundaries of TEXT are.
	 * If this function exists and returns NULL then call the value of
	 * rl_completion_entry_function to try to match, otherwise use the
	 * array of strings returned. */
	rl_attempted_completion_function = my_rl_completion;
	rl_ignore_completion_duplicates = 1;

	/* I'm using here a custom quoting function. If not specified,
	 * readline uses the default internal function. */
	rl_filename_quoting_function = my_rl_quote;

	/* Tell readline what char to use for quoting. This is only the
	 * readline internal quoting function, and for custom ones, like the
	 * one I use above. However, custom quoting functions, though they
	 * need to define their own quoting chars, won't be called at all
	 * if this variable isn't set. */
	rl_completer_quote_characters = "\"'";
	rl_completer_word_break_characters = " ";

	/* Whenever readline finds any of the following chars, it will call
	 * the quoting function. */
	rl_filename_quote_characters = " \t\n\"\\'`@$><=,;|&{[()]}?!*^#";
	/* According to readline documentation, the following string is
	 * the default and the one used by Bash: " \t\n\"\\'`@$><=;|&{(". */

	/* Executed immediately before calling the completer function, it
	 * tells readline if a space char, which is a word break character
	 * (see the above rl_completer_word_break_characters variable) is
	 * quoted or not. If it is, readline then passes the whole string
	 * to the completer function (e.g., "user\ file"), and if not, only
	 * wathever it found after the space char (e.g., "file").
	 * Thanks to George Brocklehurst for pointing out this function:
	 * https://thoughtbot.com/blog/tab-completion-in-gnu-readline. */
	rl_char_is_quoted_p = quote_detector;

	/* Define a function to handle suggestions and syntax highlighting. */
	rl_getc_function = my_rl_getc;

	/* This function is executed inmediately before path completion. So,
	 * if the string to be completed is, for instance, "user\ file" (see
	 * the above comment), this function should return the dequoted
	 * string so it won't conflict with system filenames: you want
	 * "user file", because "user\ file" does not exist, and, in this
	 * latter case, readline won't find any matches. */
	rl_filename_dequoting_function = unescape_str;

	/* Initialize the keyboard bindings function. */
	readline_kbinds();

	/* Copy the list of quote chars to a global variable to be used
	 * later by some of the program functions like split_str(),
	 * my_rl_quote(), is_quote_char(), and my_rl_dequote(). */
	quote_chars = savestring(rl_filename_quote_characters,
	    strlen(rl_filename_quote_characters));

	return FUNC_SUCCESS;
}
