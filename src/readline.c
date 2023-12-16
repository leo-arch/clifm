/* readline.c -- functions to control the behavior of readline,
 * specially completions. It also introduces both the syntax highlighting
 * and the suggestions system (via my_rl_getc) */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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

/* The following functions are taken from Bash (1.14.7), licensed under
 * GPL-1.0-or-later:
 * my_rl_getc
 * my_rl_path_completion
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#include <stdio.h>
#include <string.h>
#include <strings.h> /* str(n)casecmp() */
#include <unistd.h>
#include <errno.h>
#include <limits.h>
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
#include "exec.h"
#include "fuzzy_match.h"
#include "keybinds.h"
#include "navigation.h"
#include "readline.h"
#include "tabcomp.h"
#include "mime.h"
#include "tags.h"

#ifndef _NO_SUGGESTIONS
# include "suggestions.h"
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif /* !_NO_HIGHLIGHT */

#define DEL_EMPTY_LINE     1
#define DEL_NON_EMPTY_LINE 2

/* The maximum number of bytes we need to contain any Unicode code point
 * as a C string: 4 bytes plus a trailing nul byte. */
#define UTF8_MAX_LEN 5

#define RL_EMACS_MODE 1
#define RL_VI_MODE    0

#define SUGGEST_ONLY             0
#define RL_INSERT_CHAR           1
#define SKIP_CHAR                2
#define SKIP_CHAR_NO_REDISPLAY   3

#define MAX_EXT_OPTS NAME_MAX
#define MAX_EXT_OPTS_LEN NAME_MAX

static char ext_opts[MAX_EXT_OPTS][MAX_EXT_OPTS_LEN];
#ifndef _NO_TAGS
static struct dirent **tagged_files = (struct dirent **)NULL;
static int tagged_files_n = 0;
#endif /* !_NO_TAGS */

/* Get user input (y/n, uppercase is allowed) using _MSG as message.
 * The question will be repeated until 'y' or 'n' is entered.
 * Returns 1 if 'y' or 0 if 'n'. */
int
rl_get_y_or_n(const char *_msg)
{
	char *answer = (char *)NULL;
	while (!answer) {
		answer = rl_no_hist(_msg);
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
				{ free(answer); return 1; }
			else
				{ free(answer); answer = (char *)NULL; continue; }
		case 'n': /* fallthrough */
		case 'N':
			if (!answer[1] || (answer[1] == 'o' && !answer[2]))
				{ free(answer); return 0; }
			else
				{ free(answer); answer = (char *)NULL; continue; }
		default: free(answer); answer = (char *)NULL; continue;
		}
	}

	return 0; /* Never reached */
}

/* Generate a list of internal commands and a brief description
 * for cmd<TAB> */
static char *
int_cmds_generator(const char *text, int state)
{
	UNUSED(text);
	static int i;

	if (!state)
		i = 0;

	static char *const cmd_desc[] = {
		"/       (search for files)",
		"ac      (archive-compress files)",
		"acd     (set autocd on/off)",
		"actions (manage actions-plugins)",
		"ad      (dearchive-decompress files)",
		"alias   (list aliases)",
		"ao      (set auto-open on/off)",
		"b       (go back in the directory history list)",
		"bd      (change to a parent directory)",
		"bl      (create symbolic links in bulk)",
		"bb      (clean up non-ASCII file names)",
		"bm      (manage bookmarks)",
		"br      (batch rename files)",
		"c       (copy files)",
		"cd      (change directory)",
		"cl      (set columns on/off)",
		"cmd     (jump to the COMMANDS section in the manpage)",
		"colors  (print currently used file type colors)",
		"config  (edit the main configuration file)",
		"cs      (manage color schemes)",
		"dup     (duplicate files)",
		"ds      (deselect files)",
		"exp     (export file names to a temporary file)",
		"ext     (set external-shell commands on/off)",
		"f       (go forth in the directory history list)",
		"fc      (set the files counter on/off)",
		"ff      (toggle list-directories-first on/off)",
		"ft      (set a files filter)",
		"fz      (print directories full size - long view only)",
		"hh      (toggle show-hidden-files on/off)",
		"history (manage the commands history)",
		"icons   (set icons on/off)",
		"j       (jump to a visited directory)",
		"kb      (manage keybindings)",
		"l       (create a symbolic link)",
		"le      (edit a symbolic link)",
		"ll      (toggle long/detail view on/off)",
		"lm      (toggle light mode on/off)",
		"log     (manage logs)",
		"m       (move files)",
		"md      (create directories)",
		"media   (mount-unmount storage devices)",
		"mf      (limit the number of listed files)",
		"mm      (manage default opening applications)",
		"mp      (change to a mountpoint)",
		"msg     (print system messages)",
		"n       (create files/directories)",
		"net     (manage remote resources)",
		"o       (open file)",
		"oc      (change files ownership)",
		"opener  (set a custom resource opener)",
		"ow      (open file with...)",
		"p       (print files properties)",
		"pc      (change files permissions)",
		"pf      (manage profiles)",
		"pg      (set the files pager on/off)",
		"pin     (pin a directory)",
		"pp      (print files properties - follow links/full dir size)",
		"prompt  (switch/edit prompt)",
		"q       (quit)",
		"Q       (exit - cd on quit)",
		"r       (remove files)",
		"rf      (refresh/clear the screen)",
		"rl      (reload the configuration file)",
		"rr      (remove files in bulk)",
		"sb      (access the selection box)",
		"s       (select files)",
		"st      (change files sorting order)",
		"stats   (print files statistics)",
		"tag     (tag files)",
		"te      (toggle the executable bit on files)",
		"tips    (print tips)",
		"t       (trash files)",
		"u       (restore trashed files using a menu)",
		"unpin   (unpin the pinned directory)",
		"ver     (print version information)",
		"view    (preview files in the current directory)",
		"vv      (copy and rename files in bulk at once)",
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
 * fish's manpages parser */
static int
gen_shell_cmd_comp(char *cmd)
{
	if (!cmd || !*cmd || !data_dir || !*data_dir)
		return EXIT_FAILURE;

	char manpage_parser_file[PATH_MAX];
	snprintf(manpage_parser_file, sizeof(manpage_parser_file),
		"%s/%s/tools/manpages_comp_gen.py", data_dir, PROGRAM_NAME);

	char *c[] = {manpage_parser_file, "-k", cmd, NULL};
	return launch_execv(c, FOREGROUND, E_MUTE);
}

/* Get short and long options for command CMD, store them in the EXT_OPTS
 * array and return the number of options found */
static int
get_shell_cmd_opts(char *cmd)
{
	*ext_opts[0] = '\0';
	if (!cmd || !*cmd || !user.home
	|| (conf.suggestions == 1 && wrong_cmd == 1))
		return EXIT_FAILURE;

	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/.local/share/%s/completions/%s.clifm",
		user.home, PROGRAM_NAME, cmd);

	struct stat a;
	if (stat(p, &a) == -1) {
		if (gen_shell_cmd_comp(cmd) == EXIT_FAILURE || stat(p, &a) == -1)
			return EXIT_FAILURE;
	}

	int fd;
	FILE *fp = open_fread(p, &fd);
	if (!fp)
		return EXIT_FAILURE;

	int n = 0;
	char line[NAME_MAX];
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
		char *q = strstr(line, "-s "), *qq = (char *)NULL;
		if (q && *(q + 1) && *(q + 2) && *(q + 3)) {
			qq = strchr(q + 3, ' ');
			if (qq) *qq = '\0';
			snprintf(ext_opts[n], MAX_EXT_OPTS_LEN, "-%s", q + 3);
			if (qq)	*qq = ' ';
			n++;
		}

		/* Get long option (-OPT or --OPT) */
		q = strstr((qq && *(qq + 1)) ? qq + 1 : line, "-l ");
		if (!q)
			q = strstr((qq && *(qq + 1)) ? qq + 1 : line, "-o ");
		if (q && *(q + 1) && *(q + 2) && *(q + 3)) {
			qq = strchr(q + 3, ' ');
			if (qq)	*qq = '\0';

			/* Some long opts are written as optOPT: remove OPT */
			/* q + 3 is the beginning of the option name, so that OPT could
			 * begin at q + 4, but not before */
			char *t = *(q + 4) ? q + 4 : (char *)NULL;
			while (t && *t) {
				if (*t >= 'A' && *t <= 'Z') {
					*t = '\0';
					break;
				}
				t++;
			}

			if (*(q + 1) == 'o')
				snprintf(ext_opts[n], MAX_EXT_OPTS_LEN, "-%s", q + 3);
			else
				snprintf(ext_opts[n], MAX_EXT_OPTS_LEN, "--%s", q + 3);
			if (qq)	*qq = ' ';
			n++;
		}
	}

	*ext_opts[n] = '\0'; /* Mark the end of the options array */
	fclose(fp);
	return n;
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
	if (bell == BELL_VISIBLE) {
		rl_extend_line_buffer(2);
		*rl_line_buffer = ' ';
		*(rl_line_buffer + 1) = '\0';
		rl_end = rl_point = 1;
	}

	rl_ring_bell();

	if (bell == BELL_VISIBLE) {
		rl_delete_text(0, rl_end);
		rl_end = rl_point = 0;
	}
}
#endif /* !_NO_SUGGESTIONS */

/* Return the number of bytes in a UTF-8 sequence by inspecting only the
 * leading byte (C).
 * Taken from
 * https://stackoverflow.com/questions/22790900/get-length-of-multibyte-utf-8-sequence */
static int
utf8_bytes(unsigned char c)
{
    c >>= 4;
    c &= 7;

    if (c == 4)
		return 2;

	return c - 3;
}

/* Construct a wide-char byte by byte
 * This function is called multiple times until we get a full wide-char.
 * Each byte (C), in each subsequent call, is appended to a string (WC_STR),
 * until we have a complete multi-byte char (WC_BYTES were copied into WC_STR),
 * in which case we insert the character into the readline buffer (my_rl_getc
 * will then trigger the suggestions system using the updated input buffer) */
static int
construct_wide_char(unsigned char c)
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
		return SKIP_CHAR_NO_REDISPLAY; /* Incomplete wide char: do not trigger suggestions */
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
rl_exclude_input(unsigned char c)
{
	/* Delete or backspace keys. */
	int _del = 0;

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
#ifndef _NO_SUGGESTIONS
			if (rl_point != rl_end && suggestion.printed) {
				/* This should be the delete key. */
				remove_suggestion_not_end();
			} else {
				if (suggestion.printed)
					clear_suggestion(CS_FREEBUF);
			}
			return RL_INSERT_CHAR;
#endif /* !_NO_SUGGESTIONS */
		}

		else if (c == '3' && rl_point < rl_end) {
			xdelete();
			_del = DEL_NON_EMPTY_LINE;
			goto END;
		}

		/* Handle history events. If a suggestion has been printed and
		 * a history event is triggered (usually via the Up and Down arrow
		 * keys), the suggestion buffer won't be freed. Let's do it here. */
#ifndef _NO_SUGGESTIONS
		else if ((c == 'A' || c == 'B') && suggestion_buf)
			clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

		else {
			if (c == 'C' || c == 'D')
				cmdhist_flag = 0;
		}

		return RL_INSERT_CHAR;
	}

	if (c == 4 && rl_point < rl_end) { /* 4 == EOT (Ctrl-D) */
		xdelete();
		_del = DEL_NON_EMPTY_LINE;
		goto END;
	}

	if (c == KEY_KILL) { /* 21 == Ctrl-U (kill line) */
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

	/* Multi-byte (UTF8) char. */
	if ((c & 0xc0) == 0xc0 || (c & 0xc0) == 0x80)
		return construct_wide_char(c);

	if (c != KEY_ESC)
		cmdhist_flag = 0;

	/* Skip ESC, backspace, Enter, and TAB keys. */
	switch (c) {
		case KEY_DELETE: /* fallthrough */
		case KEY_BACKSPACE:
			_del = (rl_point == 0 && rl_end == 0) ? DEL_EMPTY_LINE : DEL_NON_EMPTY_LINE;
			xbackspace();
			if (rl_end == 0 && cur_color != tx_c) {
				cur_color = tx_c;
				fputs(tx_c, stdout);
			}
			goto END;

		case KEY_ENTER:
#ifndef _NO_SUGGESTIONS
			if (suggestion.printed && suggestion_buf)
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
				|| suggestion.type == JCMD_SUG) {
					clear_suggestion(CS_FREEBUF);
					return RL_INSERT_CHAR;
				}
			}
#endif /* !_NO_SUGGESTIONS */
			return RL_INSERT_CHAR;

		default: break;
	}

	char t[2];
	t[0] = (char)c;
	t[1] = '\0';
	rl_insert_text(t);

	int s = 0;

END:
#ifndef _NO_SUGGESTIONS
	s = strcntchrlst(rl_line_buffer, ' ');
	/* Do not take into account final spaces. */
	if (s >= 0 && !rl_line_buffer[s + 1])
		s = -1;
	if (rl_point != rl_end && c != KEY_ESC) {
		if (rl_point < s) {
			if (suggestion.printed)
				remove_suggestion_not_end();
		}
		if (wrong_cmd == 1) { /* Wrong cmd and we are on the first word. */
			char *fs = strchr(rl_line_buffer, ' ');
			if (fs && rl_line_buffer + rl_point <= fs)
				s = -1;
		}
	}
#else
	UNUSED(s);
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 0) {
		if (_del > 0) {
# ifndef _NO_SUGGESTIONS
			/* Since we have removed a char, let's check if there is
			 * a suggestion available using the modified input line. */
			if (wrong_cmd == 1 && s == -1 && rl_end > 0) {
				/* If a suggestion is found, the normal prompt will be
				 * restored and wrong_cmd will be set to zero. */
				rl_suggestions((unsigned char)rl_line_buffer[rl_end - 1]);
				return SKIP_CHAR;
			}
			if (rl_point == 0 && rl_end == 0) {
				if (wrong_cmd == 1)
					recover_from_wrong_cmd();
				if (_del == DEL_EMPTY_LINE)
					leftmost_bell();
			}
# endif /* !_NO_SUGGESTIONS */

# ifdef NO_BACKWARD_SUGGEST
			return SKIP_CHAR;
# endif /* NO_BACKWARD_SUGGEST */
		}
		return SUGGEST_ONLY;
	}

	if (wrong_cmd == 0)
		recolorize_line();
#endif /* !_NO_HIGHLIGHT */

	if (_del > 0) {
#ifndef _NO_SUGGESTIONS
		if (wrong_cmd == 1 && s == -1 && rl_end > 0) {
			rl_suggestions((unsigned char)rl_line_buffer[rl_end - 1]);
			return SKIP_CHAR;
		}

		if (rl_point == 0 && rl_end == 0) {
			if (wrong_cmd == 1)
				recover_from_wrong_cmd();
			if (_del == DEL_EMPTY_LINE)
				leftmost_bell();
		}
#endif /* !_NO_SUGGESTIONS */

#ifdef NO_BACKWARD_SUGGEST
		return SKIP_CHAR;
#endif /* NO_BACKWARD_SUGGEST */
	}

	return SUGGEST_ONLY;
}

/* Unicode aware implementation of readline's rl_expand_prompt()
 * Returns the amount of terminal columns taken by the last prompt line,
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

/* Get amount of visible chars in the last line of the prompt (STR). */
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

	char point = rl_line_buffer[rl_point];
	/* Continue only if leading or continuation multi-byte */
	if ((point & 0xc0) != 0xc0 && (point & 0xc0) != 0x80)
		return;

	int mlen = mblen(rl_line_buffer + rl_point, MB_CUR_MAX);
	rl_point += mlen > 0 ? mlen - 1 : 0;
}

/* Custom implementation of readline's rl_getc() hacked to introduce
 * suggestions, alternative TAB completion, and syntax highlighting.
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
		if (result > 0 && result == sizeof(unsigned char)) {
			/* Ctrl-d (empty command line only). Let's check the previous
			 * char wasn't ESC to prevent Ctrl-Alt-d to be taken as Ctrl-d */
			if (c == 4 && prev != KEY_ESC && rl_nohist == 0
			&& (!rl_line_buffer || !*rl_line_buffer))
				rl_quit(0, 0);

			prev = c;

			if (rl_end == 0) {
				if (conf.highlight == 1)
					rl_redisplay();
			}

			/* Syntax highlighting is made from here */
			int ret = rl_exclude_input(c);
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
		reading from is empty!  Return EOF in that case. */
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
#endif /* EWOULDBLOCK */

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
 * alternative interface */
static int
alt_rl_getc(FILE *stream)
{
	int result;
	unsigned char c;

	while (1) {
		result = (int)read(fileno(stream), &c, sizeof(unsigned char)); /* flawfinder: ignore */
		if (result > 0 && result == sizeof(unsigned char)) {

			if (c == 4 || c == 24) { /* 4 == Ctrl-d && 24 == Ctrl-x */
				MOVE_CURSOR_UP(1);
				return (EOF);
			}

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
#endif /* EWOULDBLOCK */

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

static int cb_running = 0;
/* Callback function called for each line when accept-line executed, EOF
 * seen, or EOF character read. This sets a flag and returns; it could
 * also call exit(3). */
static void
cb_linehandler(char *line)
{
	/* alt_rl_getc returns EOF in case of Ctrl-d and Ctrl-x, in which case
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
		}

		free(line);
	}
}

static int
alt_rl_prompt(const char *_prompt, const char *line)
{
	cb_running = 1;
	kbind_busy = 1;
	rl_getc_function = alt_rl_getc;
	int highlight_bk = conf.highlight;
	conf.highlight = 0;

	/* Install the line handler */
	rl_callback_handler_install(_prompt, cb_linehandler);

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
	return EXIT_SUCCESS;
}

char *
secondary_prompt(const char *_prompt, const char *line)
{
	alt_rl_prompt(_prompt, line);

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
 * rl_filename_quote_characters */
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
rl_no_hist(const char *prompt)
{
	int bk = conf.suggestions;
	conf.suggestions = 0;
	rl_nohist = rl_notab = kbind_busy = 1;
//	rl_inhibit_completion = 1;

	char *input = readline(prompt);

//	rl_inhibit_completion = 0;
	rl_notab = rl_nohist = kbind_busy = 0;
	conf.suggestions = bk;

	if (!input) /* Ctrl-d */
		return savestring("q", 1);

	if (input) {
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

	return (char *)NULL;
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
my_rl_quote(char *text, int mt, char *qp)
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

/* This is the filename_completion_function() function of an old Bash
 * release (1.14.7) modified to fit CliFM needs */
char *
my_rl_path_completion(const char *text, int state)
{
	if (!text || !*text || xrename == 2)
		return (char *)NULL;
	/* state is zero before completion, and 1 ... n after getting
	 * possible completions. Example:
	 * cd Do[TAB] -> state 0
	 * cuments/ -> state 1
	 * wnloads/ -> state 2
	 * */

	/* Dequote string to be completed (text), if necessary */
	static char *tmp_text = (char *)NULL;

	if (strchr(text, '\\')) {
		char *p = savestring(text, strlen(text));
		tmp_text = dequote_str(p, 0);
		free(p);
		if (!tmp_text)
			return (char *)NULL;
	}

/*	int rl_complete_with_tilde_expansion = 0; */
	/* ~/Doc -> /home/user/Doc */

	static DIR *directory;
	static char *filename = (char *)NULL;
	static char *dirname = (char *)NULL;
	static char *users_dirname = (char *)NULL;
	static size_t filename_len;
	static int match, ret;
	struct dirent *ent = (struct dirent *)NULL;
	static int exec = 0, exec_path = 0;
	static char *dir_tmp = (char *)NULL;
	static char tmp[PATH_MAX];

	/* If we don't have any state, then do some initialization. */
	if (!state) {
		char *temp;

		if (dirname)
			free(dirname);
		if (filename)
			free(filename);
		if (users_dirname)
			free(users_dirname);

		/* tmp_text is true whenever text was dequoted */
		char *p = tmp_text ? tmp_text : (char *)text;
		size_t text_len = strlen(p);
		if (text_len)
			filename = savestring(p, text_len);
		else
			filename = savestring("", 1);

//		if (!*text)
//			text = ".";

		if (text_len) {
			dirname = savestring(p, text_len);
		} else {
			dirname = xnmalloc(2, sizeof(char));
			*dirname = '\0';
			dirname[1] = '\0';
//			dirname = savestring("", 1);
		}

		exec = (*dirname == '.' && dirname[1] == '/');
/*		if (dirname[0] == '.' && dirname[1] == '/')
			exec = 1; // Only executable files and directories are allowed
		else
			exec = 0; */

		/* Get everything after last slash */
		temp = strrchr(dirname, '/');

		if (temp) {
			/* At this point, FILENAME has been allocated with TEXT_LEN bytes. */
			xstrsncpy(filename, ++temp, text_len + 1);
			*temp = '\0';
		} else {
			*dirname = '.';
			dirname[1] = '\0';
		}

		/* We aren't done yet.  We also support the "~user" syntax. */
		/* Save the version of the directory that the user typed. */
		size_t dirname_len = strlen(dirname);

		users_dirname = savestring(dirname, dirname_len);

		char *temp_dirname;
		int replace_dirname;

		temp_dirname = tilde_expand(dirname);

		free(dirname);
		dirname = temp_dirname;

		replace_dirname = 0;

		if (rl_directory_completion_hook)
			replace_dirname = (*rl_directory_completion_hook)(&dirname);

		if (replace_dirname) {
			free(users_dirname);
			users_dirname = savestring(dirname, dirname_len);
		}

		char *d = dirname;
		if (text_len > FILE_URI_PREFIX_LEN && IS_FILE_URI(p))
			d = dirname + FILE_URI_PREFIX_LEN;

		/* Resolve special expression in the resulting directory */
		char *e = (char *)NULL;
		if (strstr(d, "/.."))
			e = normalize_path(d, strlen(d));
		if (!e)
			e = d;

		directory = opendir(e);
		if (e != d)
			free(e);

		filename_len = strlen(filename);

		rl_filename_completion_desired = 1;
	}

	if (tmp_text) {
		free(tmp_text);
		tmp_text = (char *)NULL;
	}

	/* Now that we have some state, we can read the directory. If we found
	 * a match among files in dir, break the loop and print the match */
	match = 0;

	size_t dirname_len = 0;
	if (dirname)
		dirname_len = strlen(dirname);

	/* This block is used only in case of "/path/./" to remove the ending "./"
	 * from dirname and to be able to perform thus the executable check via access() */
	exec_path = 0;

	if (dirname_len > 2 && dirname[dirname_len - 3] == '/'
	&& dirname[dirname_len - 2] == '.' && dirname[dirname_len - 1] == '/') {
		dir_tmp = savestring(dirname, dirname_len);

		if (dir_tmp) {
			dir_tmp[dirname_len - 2] = '\0';
			exec_path = 1;
		}
	}

	/* ############### COMPLETION FILTER ################## */
	/* #        This is the heart of the function         #
	 * #################################################### */
	mode_t type;
	int fuzzy_str_type = (conf.fuzzy_match == 1 && contains_utf8(filename) == 1)
		? FUZZY_FILES_UTF8 : FUZZY_FILES_ASCII;
	int best_fz_score = 0;

	while (directory && (ent = readdir(directory))) {
#if !defined(_DIRENT_HAVE_D_TYPE)
		struct stat attr;
		if (!dirname || (*dirname == '.' && !*(dirname + 1)))
			xstrsncpy(tmp, ent->d_name, sizeof(tmp));
		else
			snprintf(tmp, sizeof(tmp), "%s%s", dirname, ent->d_name);

		if (lstat(tmp, &attr) == -1)
			continue;
		type = get_dt(attr.st_mode);
#else
		type = ent->d_type;
#endif /* !_DIRENT_HAVE_D_TYPE */

		if (((conf.suggestions == 1 && words_num == 1)
		|| !strchr(rl_line_buffer, ' '))
		&& ((type == DT_DIR && conf.autocd == 0)
		|| (type != DT_DIR && conf.auto_open == 0)))
			continue;

		/* Only dir names for cd */
		if ((conf.suggestions == 0 || words_num > 1) && conf.fuzzy_match == 1
		&& rl_line_buffer && *rl_line_buffer == 'c' && rl_line_buffer[1] == 'd'
		&& rl_line_buffer[2] == ' ' && type != DT_DIR)
			continue;

		/* If the user entered nothing before TAB (ex: "cd [TAB]") */
		if (!filename_len) {
			/* Exclude "." and ".." as possible completions */
			if (SELFORPARENT(ent->d_name))
				continue;

			/* If 'cd', match only dirs or symlinks to dir */
			if (*rl_line_buffer == 'c'
			&& strncmp(rl_line_buffer, "cd ", 3) == 0) {
				ret = -1;

				switch (type) {
				case DT_LNK:
					if (dirname[0] == '.' && !dirname[1]) {
						ret = get_link_ref(ent->d_name);
					} else {
						snprintf(tmp, sizeof(tmp), "%s%s", dirname, ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR)
						match = 1;
					break;

				case DT_DIR: match = 1; break;
				default: break;
				}
			}

			/* If 'open', allow only reg files, dirs, and symlinks */
			else if (*rl_line_buffer == 'o'
			&& (strncmp(rl_line_buffer, "o ", 2) == 0
			|| strncmp(rl_line_buffer, "open ", 5) == 0)) {
				ret = -1;

				switch (type) {
				case DT_LNK:
					if (dirname[0] == '.' && !dirname[1]) {
						ret = get_link_ref(ent->d_name);
					} else {
						snprintf(tmp, sizeof(tmp), "%s%s", dirname, ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR || ret == S_IFREG)
						match = 1;

					break;

				case DT_REG: /* fallthrough */
				case DT_DIR: match = 1; break;

				default: break;
				}
			}

			/* If 'trash', allow only reg files, dirs, symlinks, pipes
			 * and sockets. You should not trash a block or a character
			 * device */
			else if (*rl_line_buffer == 't'
			&& (strncmp(rl_line_buffer, "t ", 2) == 0
			|| strncmp(rl_line_buffer, "tr ", 2) == 0
			|| strncmp(rl_line_buffer, "trash ", 6) == 0)) {

				if (type != DT_BLK && type != DT_CHR)
					match = 1;
			}

			/* If "./", list only executable regular files and directories */
			else if (exec) {
				if (type == DT_DIR
				|| (type == DT_REG && access(ent->d_name, X_OK) == 0))
					match = 1;
			}

			/* If "/path/./", list only executable regular files */
			else if (exec_path) {
				if (type == DT_REG || type == DT_DIR) {
					/* dir_tmp is dirname less "./", already allocated before
					 * the while loop */
					snprintf(tmp, sizeof(tmp), "%s%s", dir_tmp, ent->d_name);

					if (type == DT_DIR || access(tmp, X_OK) == 0)
						match = 1;
				}
			}

			/* No filter for everything else. Just print whatever is there */
			else {
				match = 1;
			}
		}

		/* If there is at least one char to complete (ex: "cd .[TAB]") */
		else {
			/* Let's check for possible matches */
			if (conf.fuzzy_match == 0 || rl_point < rl_end
			|| (*filename == '.' && *(filename + 1) == '.')
			|| *filename == '-'
			|| (tabmode == STD_TAB && !(flags & STATE_SUGGESTING))) {
				if ( (conf.case_sens_path_comp == 0
					? TOUPPER(*ent->d_name) != TOUPPER(*filename)
					: *ent->d_name != *filename)
					|| (conf.case_sens_path_comp == 0
					? strncasecmp(filename, ent->d_name, filename_len) != 0
					: strncmp(filename, ent->d_name, filename_len) != 0) )
						continue;
			} else {

				/* ############### FUZZY MATCHING ################## */

				if (flags & STATE_SUGGESTING) {
					int r = 0;
					/* Do not fuzzy suggest if not at the end of the line */
					if (rl_point == rl_end
					&& (r = fuzzy_match(filename, ent->d_name,
					filename_len, fuzzy_str_type)) > best_fz_score) {
						if (!dirname || (*dirname == '.' && !*(dirname + 1))) {
							xstrsncpy(fz_match, ent->d_name, sizeof(fz_match));
						} else {
							snprintf(fz_match, sizeof(fz_match), "%s%s",
								dirname, ent->d_name);
						}

						/* We look for matches ranked 4 or 5. If none of them is
						 * found, we take the closest ranked match (1-3)
						 * If we set this value to 1, the first match will be returned,
						 * which makes the computation much faster */
						if (r != TARGET_BEGINNING_BONUS) {
							best_fz_score = r;
							continue;
						}
					} else {
						continue;
					}
				} else {
					/* This is for TAB completion: accept all matches */
					if (fuzzy_match(filename, ent->d_name, filename_len, fuzzy_str_type) == 0)
						continue;
				}
			}
			/* ################################################ */

			if (*rl_line_buffer == 'c'
			&& strncmp(rl_line_buffer, "cd ", 3) == 0) {
				ret = -1;

				switch (type) {
				case DT_LNK:
					if (dirname[0] == '.' && !dirname[1]) {
						ret = get_link_ref(ent->d_name);
					} else {
						snprintf(tmp, sizeof(tmp), "%s%s", dirname, ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR)
						match = 1;
					break;

				case DT_DIR: match = 1; break;

				default: break;
				}
			}

			else if (*rl_line_buffer == 'o'
			&& (strncmp(rl_line_buffer, "o ", 2) == 0
			|| strncmp(rl_line_buffer, "open ", 5) == 0)) {
				ret = -1;

				switch (type) {
				case DT_REG: /* fallthrough */
				case DT_DIR: match = 1; break;

				case DT_LNK:
					if (dirname[0] == '.' && !dirname[1]) {
						ret = get_link_ref(ent->d_name);
					} else {
						snprintf(tmp, sizeof(tmp), "%s%s", dirname, ent->d_name);
						ret = get_link_ref(tmp);
					}

					if (ret == S_IFDIR || ret == S_IFREG)
						match = 1;
					break;

				default: break;
				}
			}

			else if (*rl_line_buffer == 't'
			&& (strncmp(rl_line_buffer, "t ", 2) == 0
			|| strncmp(rl_line_buffer, "tr ", 3) == 0
			|| strncmp(rl_line_buffer, "trash ", 6) == 0)) {
				if (type != DT_BLK && type != DT_CHR)
					match = 1;
			}

/*			else if (exec) {
				if (type == DT_REG && access(ent->d_name, X_OK) == 0)
					match = 1;
			} */

			else if (exec_path) {
				if (type == DT_REG || type == DT_DIR) {
					snprintf(tmp, sizeof(tmp), "%s%s", dir_tmp, ent->d_name);
					if (type == DT_DIR || access(tmp, X_OK) == 0)
						match = 1;
				}
			}

			else {
				match = 1;
			}
		}

		if (match)
			break;
	}

	if (dir_tmp) { /* == exec_path */
		free(dir_tmp);
		dir_tmp = (char *)NULL;
	}

	/* readdir() returns NULL on reaching the end of directory stream.
	 * So that if entry is NULL, we have no matches */
	if (!ent) { /* == !match */
		if (directory) {
			closedir(directory);
			directory = (DIR *)NULL;
		}

		free(dirname);
		dirname = (char *)NULL;

		free(filename);
		filename = (char *)NULL;

		free(users_dirname);
		users_dirname = (char *)NULL;

		return (char *)NULL;
	}

	/* We have a match */
	else {
		cur_comp_type = TCMP_PATH;
		char *temp = (char *)NULL;

		if (dirname && (dirname[0] != '.' || dirname[1])) {
/*			if (rl_complete_with_tilde_expansion && *users_dirname == '~') {
				size_t dirlen = strlen(dirname);
				size_t len = dirlen + strlen(ent->d_name) + 1;
				temp = (char *)xnmalloc(len + 1, sizeof(char));
				xstrsncpy(temp, dirname, len + 1);
				// Canonicalization cuts off any final slash present.
				// We need to add it back.

				if (dirname[dirlen - 1] != '/') {
					temp[dirlen] = '/';
					temp[dirlen + 1] = '\0';
				}
			} else { */
			size_t temp_len = strlen(users_dirname) + strlen(ent->d_name) + 1;
			temp = xnmalloc(temp_len, sizeof(char));
			snprintf(temp, temp_len, "%s%s", users_dirname, ent->d_name);
//			strcpy(temp, users_dirname);
			/* If fast_back == 1 and filename is empty, we have the
			 * root dir: do not append anything else */
//			if (fast_back == 0 || (filename && *filename))
//			strcat(temp, ent->d_name);
		} else {
			temp = savestring(ent->d_name, strlen(ent->d_name));
		}

		if (flags & STATE_SUGGESTING) {
			if (directory) {
				closedir(directory);
				directory = (DIR *)NULL;
			}

			free(dirname);
			dirname = (char *)NULL;

			free(filename);
			filename = (char *)NULL;

			free(users_dirname);
			users_dirname = (char *)NULL;
		}

		return (temp);
	}
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
	char *name;

	if (!state) {
		/* The state variable is zero only the first time the function is
		 * called, and a non-zero positive in later calls. This means that
		 * I and LEN will be necessarilly initialized the first time */
		i = 0;
		prefix = (*text == 'b' && *(text + 1) == ':') ? 2 : 0;
		len = strlen(text + prefix);
	}

	/* Look for bookmarks in bookmark names for a match */
	while ((size_t)i < bm_n) {
		name = bookmarks[i++].name;
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

/* Used for history and search pattern completion */
static char *
hist_generator(const char *text, int state)
{
	if (!history)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(*text == '!' ? text + 1 : text);
	}

	while ((name = history[i++].cmd) != NULL) {
		if (*text == '!') {
			if (len == 0
			|| (*name == *(text + 1) && strncmp(name, text + 1, len) == 0)
			|| (conf.fuzzy_match == 1 && tabmode != STD_TAB
			&& fuzzy_match((char *)(text + 1), name, len, FUZZY_HISTORY) > 0))
				return strdup(name);
		} else {
			/* Restrict the search to what seems to be a pattern:
			 * The string before the first slash or space (not counting the initial
			 * slash, used to fire up the search function) must contain a pattern
			 * metacharacter */
			if (!*name || !*(name + 1))
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
	char *name, *_path;

	if (!state)
		i = 0;

	while (i < (int)bm_n) {
		name = bookmarks[i].name;
		_path = bookmarks[i].path;
		i++;

		if (!name || !_path || (conf.case_sens_list == 1 ? strcmp(name, text)
		: strcasecmp(name, text)) != 0)
			continue;

		size_t plen = strlen(_path);

		if (plen > 1 && _path[plen - 1] == '/')
			_path[plen - 1] = '\0';

		char *p = abbreviate_file_name(_path);
		char *ret = strdup(p ? p : _path);

		if (p != _path)
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

	if (!state) {
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

	if (!state) {
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

	if (!state)
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

	if (!state) {
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

	if (!state) {
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
	char *name;
	rl_filename_completion_desired = 1;
	if (!state) {
		i = 0;
		len = strlen(text);
	}

	int fuzzy_str_type = (conf.fuzzy_match == 1 && contains_utf8(text) == 1)
		? FUZZY_FILES_UTF8 : FUZZY_FILES_ASCII;

	/* Check list of currently displayed files for a match */
	while (i < files && (name = file_info[i].name) != NULL) {
		i++;
		/* If first word, filter files according to autocd and auto-open values */
		if (((conf.suggestions == 1 && words_num == 1) || !strchr(rl_line_buffer, ' '))
		&& ( (file_info[i - 1].dir == 1 && conf.autocd == 0)
		|| (file_info[i - 1].dir == 0 && conf.auto_open == 0) ))
			continue;

		/* If cd, list only directories */
		if ((conf.suggestions == 0 || words_num > 1
		|| (rl_end > 0 && rl_line_buffer[rl_end - 1] == ' '))
		&& rl_line_buffer && *rl_line_buffer == 'c' && rl_line_buffer[1] == 'd'
		&& rl_line_buffer[2] == ' ' && file_info[i - 1].dir == 0)
			continue;

		if (conf.case_sens_path_comp ? strncmp(name, text, len) == 0
		: strncasecmp(name, text, len) == 0)
			return strdup(name);
		if (conf.fuzzy_match == 0 || tabmode == STD_TAB || rl_point < rl_end)
			continue;
		if (len == 0
		|| fuzzy_match((char *)text, name, len, fuzzy_str_type) > 0)
			return strdup(name);
	}

	return (char *)NULL;
}

/* Used by ELN expansion */
static char *
filenames_gen_eln(const char *text, int state)
{
	static filesn_t i;
	char *name;
	rl_filename_completion_desired = 1;

	if (!state)
		i = 0;

	const filesn_t num_text = xatof(text);
	if (num_text < 1 || num_text > files)
		return (char *)NULL;

	/* Check list of currently displayed files for a match */
	while (i < files && (name = file_info[i++].name) != NULL) {
		if (*name == *file_info[num_text - 1].name
		&& strcmp(name, file_info[num_text - 1].name) == 0) {
			if (words_num > 1 && rl_line_buffer && *rl_line_buffer == 'c'
			&& rl_line_buffer[1] == 'd'
			&& rl_line_buffer[2] == ' ' && file_info[num_text - 1].dir == 0)
				continue;
#ifndef _NO_SUGGESTIONS
			if (suggestion_buf)
				clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
			return strdup(name);
		}
	}

	return (char *)NULL;
}

/* Used by ELN ranges expansion */
static char *
filenames_gen_ranges(const char *text, int state)
{
	static filesn_t i;
	char *name;
	rl_filename_completion_desired = 1;

	if (!state)
		i = 0;

	char *r = strchr(text, '-');
	if (!r)
		return (char *)NULL;

	*r = '\0';
	const filesn_t a = xatof(text);
	const filesn_t b = xatof(r + 1);
	if (a < 1 || a > files || b < 1 || b > files)
		return (char *)NULL;
	*r = '-';
	if (a >= b)
		return (char *)NULL;

	while (i < files && (name = file_info[i++].name) != NULL) {
		if (i >= a && i <= b) {
#ifndef _NO_SUGGESTIONS
			if (suggestion_buf)
				clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
			return strdup(name);
		}
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

	if (!state) {
		i = 0;
		len = strlen(text);
		fuzzy_str_type = (conf.fuzzy_match == 1	&& contains_utf8(text) == 1)
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

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while ((name = bin_commands[i++]) != NULL) {
		if (is_internal_c(name) == 1)
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

	if (!state) {
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

	if (!state)
		i = 0;

	int num_text = atoi(text);
	if (num_text == INT_MIN)
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

	if (!state) {
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
nets_generator(const char *text, int state)
{
	if (!remotes)
		return (char *)NULL;

	static int i;
	static int is_unmount, is_mount;
	static size_t len;
	char *name;

	if (!state) {
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
		if (conf.case_sens_path_comp ? strncmp(name, text, len)
		: strncasecmp(name, text, len) == 0) {
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

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while ((name = sort_methods[i++].name) != NULL) {
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

	if (!state) {
		i = 0;
		len = text ? strlen(text) : 0;
	}

	if (text && *text >= '1' && *text <= MAX_WS + '0' && !*(text + 1))
		return (char *)NULL;

	while (i < MAX_WS) {
		if (!workspaces[i].name) {
			if (len == 0) {
				char t[12];
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

	if (!state) {
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

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while (i < (int)prompts_n && (name = prompts[i++].name) != NULL) {
		if ((conf.case_sens_list ? strncmp(name, text, len)
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

#ifndef _NO_MAGIC
static char **
rl_mime_list(void)
{
	if (term_caps.suggestions != 0)
		{ HIDE_CURSOR; fputs(" [wait...]", stdout); fflush(stdout); }

	char **t = xnmalloc((size_t)files + 2, sizeof(char *));
	t[0] = xnmalloc(1, sizeof(char));
	*t[0] = '\0';
	t[1] = (char *)NULL;

	size_t n = 1;
	filesn_t i = files;
	while (--i >= 0) {
		if (file_info[i].color == nf_c) /* No access to file */
			continue;
		char *m = xmagic(file_info[i].name, MIME_TYPE);
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

	if (term_caps.suggestions != 0)
		{ MOVE_CURSOR_LEFT(10); ERASE_TO_RIGHT; UNHIDE_CURSOR; }

	if (n == 1)
		{ free(t[0]); free(t); return (char **)NULL; }

	t = xnrealloc(t, n + 1, sizeof(char *));
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

	filesn_t i, n = 1;
	for (i = 0; i < files; i++) {
		char *m = xmagic(file_info[i].name, MIME_TYPE);
		if (!m) continue;

		char *p = strstr(m, text);
		free(m);

		if (!p) continue;

		t[n] = savestring(file_info[i].name, strlen(file_info[i].name));
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
#endif /* !_NO_MAGIC */

/* Return the list of matches for the glob expression TEXT or NULL if no matches */
static char **
rl_glob(char *text)
{
	char *tmp = expand_tilde_glob(text);
	glob_t globbuf;

	if (glob(tmp ? tmp : text, 0, NULL, &globbuf) != EXIT_SUCCESS) {
		globfree(&globbuf);
		free(tmp);
		return (char **)NULL;
	}

	free(tmp);

	if (globbuf.gl_pathc == 1) {
		char **t = xnmalloc(globbuf.gl_pathc + 2, sizeof(char *));
		char *p = strrchr(globbuf.gl_pathv[0], '/');
		if (p && *(++p)) {
			char c = *p;
			*p = '\0';
			t[0] = savestring(globbuf.gl_pathv[0], strlen(globbuf.gl_pathv[0]));
			*p = c;
			t[1] = savestring(p, strlen(p));
			t[2] = (char *)NULL;
		} else {
			t[0] = savestring(globbuf.gl_pathv[0], strlen(globbuf.gl_pathv[0]));
			t[1] = (char *)NULL;
		}
		globfree(&globbuf);
		return t;
	}

	size_t i, j = 1;
	char **t = xnmalloc(globbuf.gl_pathc + 3, sizeof(char *));

	/* If /path/to/dir/GLOB<TAB>, /path/to/dir goes to slot 0 */
	int c = -1;
	char *ls = get_last_chr(rl_line_buffer, ' ', rl_point), *q = (char *)NULL;
	char *ds = ls ? dequote_str(ls, 0) : (char *)NULL;
	char *p = ds ? ds : (ls ? ls : (char *)NULL);

	if (p && *(++p)) {
		q = strrchr(p, '/');
		if (q && *(++q)) {
			c = *q;
			*q = '\0';
		}
	}

	if (c != -1) {
		t[0] = savestring(p, strlen(p));
		*q = (char)c;
	} else {
		t[0] = xnmalloc(1, sizeof(char));
		*t[0] = '\0';
	}

	free(ds);

	for (i = 0; i < globbuf.gl_pathc; i++) {
		if (SELFORPARENT(globbuf.gl_pathv[i]))
			continue;
		t[j] = savestring(globbuf.gl_pathv[i], strlen(globbuf.gl_pathv[i]));
		j++;
	}
	t[j] = (char *)NULL;

	globfree(&globbuf);
	return t;
}

#ifndef _NO_TRASH
/* Return the list of currently trashed files matching TEXT, or NULL */
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

	char *p = dequote_str((char *)text, 0);
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

	if (!state) {
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

	if (!state)
		i = 0;

	if (!tagged_files)
		return (char *)NULL;

	while (i < tagged_files_n && (name = tagged_files[i++]->d_name) != NULL) {
		if (SELFORPARENT(name))
			continue;

		char *p = (char *)NULL, *q = name;
		if (strchr(name, '\\')) {
			p = dequote_str(name, 0);
			q = p;
		}

		reinsert_slashes(q);

		char tmp[PATH_MAX], *r = (char *)NULL;
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

	char dir[PATH_MAX];
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

	if (!state) {
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
rl_count_words(void)
{
	if (!rl_line_buffer)
		return 0;

	char *p = rl_line_buffer;
	while (*p == ' ')
		++p;
	if (!*p)
		return 0;

	size_t c = 1;
	while (*p) {
		if (*p == ' ' && p > rl_line_buffer && *(p - 1) != '\\' && *(p + 1) != ' ')
			c++;
		p++;
	}

	return c;
}

/* THIS IS AWFUL! WRITE A BETTER IMPLEMENTATION */
/* Complete with options for specific commands */
static char *
options_generator(const char *text, int state)
{
	char *l = rl_line_buffer;
	if (!l || !*l)
		return (char *)NULL;

	static size_t len = 0, w = 0;
	static int i;
	char *name;

	if (state == 0) {
		i = 0;
		len = strlen(text);
		w = rl_count_words();
	}

#define MAX_OPTS 22
	char *_opts[MAX_OPTS] = {0};

	/* acd, ao, ext, ff, hf, pg (w == 2 -> second word only) */
	if (w == 2 && ( ( *l == 'a' && ((l[1] == 'o' && l[2] == ' ')
	|| strncmp(l, "acd ", 4) == 0) )
	|| (*l == 'e' && strncmp(l, "ext ", 4) == 0)
	|| (*l == 'f' && (l[1] == 'f' || l[1] == 'c') && l[2] == ' ')
	|| (*l == 'h' && (l[1] == 'f' || l[1] == 'h') && l[2] == ' ')
	|| (*l == 'p' && l[1] == 'g' && l[2] == ' ') ) ) {
		_opts[0] = "on"; _opts[1] = "off"; _opts[2] = "status"; _opts[3] = NULL;

	/* cl, icons, ll-lv, lm, and fz */
	} else if (w == 2 && ( (*l == 'c' && l[1] == 'l' && l[2] == ' ')
	|| (*l == 'i' && strncmp(l, "icons ", 6) == 0)
	|| (*l == 'l' && (l[1] == 'v' || l[1] == 'l' || l[1] == 'm')
	&& l[2] == ' ')
	|| (*l == 'f' && l[1] == 'z' && l[2] == ' ') ) ) {
		_opts[0] = "on"; _opts[1] = "off"; _opts[2] = NULL;

	/* config */
	} else if (w == 2 && *l == 'c' && strncmp(l, "config ", 7) == 0) {
		_opts[0] = "edit"; _opts[1] = "dump"; _opts[2] = "reset";
		_opts[3] = NULL;

	/* actions */
	} else if (w == 2 && *l == 'a' && strncmp(l, "actions ", 8) == 0) {
		_opts[0] = "list"; _opts[1] = "edit"; _opts[2] = NULL;

	/* log */
	} else if (w == 3 && *l == 'l' && (strncmp(l, "log msg ", 8) == 0
	|| strncmp(l, "log cmd ", 8) == 0) ) {
		_opts[0] = "list"; _opts[1] = "on"; _opts[2] = "off";
		_opts[3] = "status"; _opts[4] = "clear"; _opts[5] = NULL;
	} else if (w == 2 && *l == 'l' && strncmp(l, "log ", 4) == 0) {
		_opts[0] = "cmd"; _opts[1] = "msg"; _opts[2] = NULL;

	/* mime */
	} else if (w == 2 && *l == 'm' && ((l[1] == 'm' && l[2] == ' ')
	|| strncmp(l, "mime ", 5) == 0)) {
		_opts[0] = "open"; _opts[1] = "info"; _opts[2] = "edit";
		_opts[3] = "import"; _opts[4] = NULL;

#ifndef _NO_PROFILES
	/* profile */
	} else if (w == 2 && ( (*l == 'p' && l[1] == 'f' && l[2] == ' ')
	|| strncmp(l, "profile ", 8) == 0 ) ) {
		_opts[0] = "set"; _opts[1] = "list"; _opts[2] = "add";
		_opts[3] = "del"; _opts[4] = "rename"; _opts[5] = NULL;
#endif /* _NO_PROFILES */

	/* prompt */
	} else if (w == 2 && *l == 'p' && strncmp(l, "prompt ", 7) == 0) {
		_opts[0] = "set"; _opts[1] = "list"; _opts[2] = "unset";
		_opts[3] = "edit"; _opts[4] = "reload"; _opts[5] = NULL;
#ifndef _NO_TAGS

	/* pwd */
	} else if (w == 2 && *l == 'p' && l[1] == 'w' && l[2] == 'd' && l[3] == ' ') {
		_opts[0] = "-L"; _opts[1] = "-P"; _opts[2] = NULL;

	/* tag */
	} else if (w == 2 && *l == 't' && l[1] == 'a' && l[2] == 'g' && l[3] == ' ') {
		_opts[0] = "add"; _opts[1] = "del"; _opts[2] = "list";
		_opts[3] = "list-full"; _opts[4] = "merge"; _opts[5] = "new";
		_opts[6] = "rename"; _opts[7] = "untag"; _opts[8] = NULL;
#endif /* !_NO_TAGS */

	/* net */
	} else if (w == 2 && *l == 'n' && l[1] == 'e'
	&& l[2] == 't' && l[3] == ' ') {
		_opts[0] = "mount"; _opts[1] = "unmount"; _opts[2] = "list";
		_opts[3] = "edit"; _opts[4] = NULL;

	/* history */
	} else if (w == 2 && *l == 'h' && strncmp(l, "history ", 8) == 0) {
		_opts[0] = "edit"; _opts[1] = "clear"; _opts[2] = "on";
		_opts[3] = "off"; _opts[4] = "status"; _opts[5] = "show-time";
		_opts[6] = NULL;

	/* help topics */
	} else if (w == 2 && *l == 'h' && l[1] == 'e'
	&& strncmp(l, "help ", 5) == 0) {
		_opts[0] = "archives";
		_opts[1] = "autocommands";
		_opts[2] = "basics";
		_opts[3] = "bookmarks";
		_opts[4] = "commands";
		_opts[5] = "desktop-notifications";
		_opts[6] = "dir-jumper";
		_opts[7] = "file-details";
		_opts[8] = "file-filters";
		_opts[9] = "file-previews";
		_opts[10] = "file-tags";
		_opts[11] = "navigation";
		_opts[12] = "plugins";
		_opts[13] = "profiles";
		_opts[14] = "remotes";
		_opts[15] = "resource-opener";
		_opts[16] = "search";
		_opts[17] = "security";
		_opts[18] = "selection";
		_opts[19] = "theming";
		_opts[20] = "trash";
		_opts[21] = NULL;

	/* b, f */
	} else if (w == 2 && ( (*l == 'b' && l[1] == ' ')
	|| (*l == 'f' && l[1] == ' ') ) ) {
		_opts[0] = "hist"; _opts[1] = "clear"; _opts[2] = NULL;

	} else {
		/* kb, keybinds */
		if (w == 2 && ( (*l == 'k' && l[1] == 'b' && l[2] == ' ')
		|| strncmp(l, "keybinds ", 9) == 0) ) {
			_opts[0] = "list"; _opts[1] = "edit"; _opts[2] = "reset";
			_opts[3] = "readline"; _opts[4] = NULL;
		}
	}

	if (!_opts[0])
		return (char *)NULL;

	while ((name = _opts[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
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

	if (!state)
		len = *(text + 1) ? wc_xstrlen(text + 1) : 0;

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

	if (!state)
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

	if (!state)
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
tag_complete(const char *text)
{
	char *l = rl_line_buffer;
	int comp = 0;
	if (*(l + 1) && *(l + 2) == ' ') {
		switch (*(l + 1)) {
		case 'a': /* fallthough */
		case 'u':
			if (text && *text == ':') { /* We have a tag name */
				comp = 1; cur_comp_type = TCMP_TAGS_C;
			} else if (*(l + 1) == 'u') { /* We have a tagged file */
				comp = 2;
			}
			break;
		case 'd': /* fallthough */
		case 'l': /* fallthough */
		case 'm': /* fallthough */
//		case 'n': /* Just a new tag name: no completion */
		case 'y':
			if (*(l + 1) == 'd' || *(l + 1) == 'l') flags |= MULTI_SEL;
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
	case 'b': return stats.block_dev > 0 ? 1 : 0;
	case 'c': return stats.char_dev > 0 ? 1 : 0;
	case 'd': return stats.dir > 0 ? 1 : 0;
#ifdef SOLARIS_DOORS
	case 'D': return stats.door > 0 ? 1 : 0;
	case 'P': return stats.port > 0 ? 1 : 0;
#endif /* SOLARIS_DOORS */
	case 'f': return stats.reg > 0 ? 1 : 0;
	case 'h': return stats.multi_link > 0 ? 1 : 0;
	case 'l': return stats.link > 0 ? 1 : 0;
	case 'p': return stats.fifo > 0 ? 1 : 0;
	case 's': return stats.socket > 0 ? 1 : 0;
	case 'x': return stats.exec > 0 ? 1 : 0;
	case 'o': return stats.other_writable > 0 ? 1 : 0;
	case 't': return stats.sticky > 0 ? 1 : 0;
	case 'u': return stats.suid > 0 ? 1 : 0;
	case 'g': return stats.sgid > 0 ? 1 : 0;
	case 'C': return stats.caps > 0 ? 1 : 0;
	default: return 0;
	}
}

static char *
file_types_opts_generator(const char *text, int state)
{
	UNUSED(text);
	static int i;

	if (!state)
		i = 0;

	static char *const ft_opts[] = {
		"b (Block device)",
		"c (Character device)",
		"d (Directory)",
#ifdef SOLARIS_DOORS
		"D (Door)",
		"P (Port)",
#endif /* SOLARIS_DOORS */
		"f (Regular file)",
		"h (Multi-hardlink file)",
		"l (Symbolic link)",
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

	if (!state)
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
#ifdef SOLARIS_DOORS
		case 'D':
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
		case 'h':
			if (file_info[i].dir == 0 && file_info[i].linkn > 1)
				ret = strdup(name);
			break;
		case 'l':
			if (file_info[i].type == DT_LNK)
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
		"edit", // DEPRECATED
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

	char *p = strchr(str, ' ');
	if (!p || *(p + 1) != 'e' || !*(p + 2))
		return 0;

	*p = '\0';
	if (cmd_takes_edit(str) != 1) {
		*p = ' ';
		return 0;
	}
	*p = ' ';

	if (strncmp(p + 2, "dit ", 4) != 0)
		return 0;

	return 1;
}
#endif /* !_NO_LIRA */

static char **
complete_bookmark_names(char *text, const size_t words_n, int *exit_status)
{
	*exit_status = EXIT_SUCCESS;

	// rl_line_buffer is either "bm " or "bookmarks "
	char *q = rl_line_buffer + (rl_line_buffer[1] == 'o' ? 9 : 2);

	if (q && *(q + 1) == 'a' && (*(q + 2) == ' '
	|| strncmp(q + 1, "add ", 4) == 0)) {
		if (words_n > 3) // Do not complete anything after "bm add FILE"
			rl_attempted_completion_over = 1;
		else // 'bm add': complete with path completion
			*exit_status = EXIT_FAILURE;

		return (char **)NULL;
	}

	// If not 'bm add' complete with bookmarks
#ifndef _NO_SUGGESTIONS
	if (suggestion.type != FILE_SUG)
		rl_attempted_completion_over = 1;
#endif /* !_NO_SUGGESTIONS */
	char *p = dequote_str((char *)text, 0);
	char **matches = rl_completion_matches(p ? p : text, &bookmarks_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_BOOKMARK;
	return matches;
}

/*
static char **
complete_dirjump_jo(char *text, const int n, int *exit_status)
{
	*exit_status = EXIT_FAILURE;
	char *lb = rl_line_buffer;

	if (*lb != 'j' || lb[1] != 'o' || lb[2] != ' ')
		return (char **)NULL;

	if (!is_number(text) || n <= 0 || n > (int)jump_n
	|| !jump_db[n - 1].path)
		return (char **)NULL;

	char *p = jump_db[n - 1].path;
	char **matches = (char **)NULL;
	matches = xnrealloc(matches, 2, sizeof(char **));
	matches[0] = savestring(p, strlen(p));
	matches[1] = (char *)NULL;

	cur_comp_type = TCMP_PATH;
	rl_filename_completion_desired = 1;
	*exit_status = EXIT_SUCCESS;

	return matches;
} */

static char **
complete_ranges(char *text, int *exit_status)
{
	*exit_status = EXIT_FAILURE;
	char *r = strchr(text, '-');
	if (!r || *(r + 1) < '0' || *(r + 1) > '9')
		return (char **)NULL;

	*r = '\0';
	if (!is_number(text) || !is_number(r + 1)) {
		*r = '-';
		return (char **)NULL;
	}

	int a = atoi(text);
	int b = atoi(r + 1);
	*r = '-';

	if (a < 1 || b < 1 || a >= b || (filesn_t)b > files)
		return (char **)NULL;

	char **matches = rl_completion_matches(text, &filenames_gen_ranges);
	if (!matches)
		return (char **)NULL;

	*exit_status = EXIT_SUCCESS;
	cur_comp_type = TCMP_RANGES;
	return matches;
}

#ifndef _NO_LIRA
static char **
complete_open_with(char *text)
{
	char *p = rl_line_buffer + 3;
	char *s = strrchr(p, ' ');
	if (s)
		*s = '\0';

	char **matches = mime_open_with_tab(p, text, 0);

	if (s)
		*s = ' ';

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_OPENWITH;
	return matches;
}
#endif /* !_NO_LIRA */

static char **
complete_file_type_filter(char *text, int *exit_status)
{
	*exit_status = EXIT_FAILURE;
	char **matches = (char **)NULL;

	if (!*(text + 1)) {
		matches = rl_completion_matches(text, &file_types_opts_generator);
		if (!matches)
			return (char **)NULL;

		if (!matches[1])
			rl_swap_fields(&matches);

		*exit_status = EXIT_SUCCESS;
		cur_comp_type = TCMP_FILE_TYPES_OPTS;
		return matches;
	}

	if (*(text + 2))
		return (char **)NULL;

	matches = rl_completion_matches(text + 1, &file_types_generator);
	if (!matches)
		return (char **)NULL;

	if (!matches[1])
		rl_swap_fields(&matches);
	else
		flags |= MULTI_SEL;

	*exit_status = EXIT_SUCCESS;
	cur_comp_type = TCMP_FILE_TYPES_FILES;
	return matches;
}

#ifndef _NO_MAGIC
static char **
complete_mime_type_filter(char *text, int *exit_status)
{
	*exit_status = EXIT_FAILURE;
	char **matches = (char **)NULL;

	if (*(text + 1)) {
		if ((matches = rl_mime_files(text + 1)) == NULL)
			return (char **)NULL;

		cur_comp_type = TCMP_MIME_FILES; // Same as TCMP_FILE_TYPES_FILES
		rl_filename_completion_desired = 1;
		flags |= MULTI_SEL;
		*exit_status = EXIT_SUCCESS;
		return matches;
	}

	if ((matches = rl_mime_list()) == NULL)
		return (char **)NULL;

	*exit_status = EXIT_SUCCESS;
	cur_comp_type = TCMP_MIME_LIST;
	return matches;
}
#endif /* !_NO_MAGIC */

static char **
complete_glob(char *text, int *exit_status)
{
	*exit_status = EXIT_FAILURE;
	char **matches = (char **)NULL;

	char *p = (*rl_line_buffer == '/' && rl_end > 1
		&& !strchr(rl_line_buffer + 1, ' ')
		&& !strchr(rl_line_buffer + 1, '/'))
			? (char *)(text + 1)
			: (char *)text;

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

	*exit_status = EXIT_SUCCESS;
	return matches;
}

static char **
complete_shell_cmd_opts(char *text, int *exit_status)
{
	*exit_status = EXIT_FAILURE;
	char **matches = (char **)NULL;

	char lw[NAME_MAX + 1]; *lw = '\0'; /* Last word before the dash */
	char *a = strrchr(rl_line_buffer, ' ');

	if (a) {
		*a = '\0';
		char *b = strrchr(rl_line_buffer, ' ');
		xstrsncpy(lw, (b && *(b + 1)) ? b + 1 : rl_line_buffer, sizeof(lw));
		*a = ' ';
	}

	if (*lw && get_shell_cmd_opts(lw) > 0
	&& (matches = rl_completion_matches(text, &ext_options_generator)) ) {
		*exit_status = EXIT_SUCCESS;
		return matches;
	}

	return (char **)NULL;
}

#ifndef _NO_TAGS
static char **
complete_tag_names(char *text, int *exit_status)
{
	*exit_status = EXIT_FAILURE;
	char **matches = (char **)NULL;

	int comp = tag_complete(text);
	if (comp != 1 && comp != 2)
		return (char **)NULL;

	if (comp == 1) {
		matches = rl_completion_matches(text, &tags_generator);
		if (!matches) {
			cur_comp_type = TCMP_NONE;
			return (char **)NULL;
		}

		*exit_status = EXIT_SUCCESS;
		return matches;
	}

	/* comp == 2
	 * Let's match tagged files for the untag function. */
	char *_tag = get_cur_tag();
	matches = check_tagged_files(_tag);
	free(_tag);

	if (!matches)
		return (char **)NULL;

	*exit_status = EXIT_SUCCESS;
	cur_comp_type = TCMP_TAGS_F;
	return matches;
}

static char **
complete_tag_names_t(char *text, int *exit_status)
{
	*exit_status = EXIT_FAILURE;
	cur_comp_type = TCMP_TAGS_T;

	char *p = dequote_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &tags_generator);
	free(p);

	if (!matches) {
		cur_comp_type = TCMP_NONE;
		return (char **)NULL;
	}

	*exit_status = EXIT_SUCCESS;
	flags |= MULTI_SEL;
	return matches;
}

static char **
complete_tagged_file_names(char *text, int *exit_status)
{
	*exit_status = EXIT_FAILURE;

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

	*exit_status = EXIT_SUCCESS;
	cur_comp_type = TCMP_TAGS_F;
	return matches;
}
#endif /* !_NO_TAGS */

static char **
complete_bookmark_paths(char *text, int *exit_status)
{
	char *t = text + 2;
	char *p = dequote_str(t, 0);
	char **matches = rl_completion_matches(p ? p : t, &bm_paths_generator);
	free(p);

	if (!matches) {
		*exit_status = EXIT_FAILURE;
		return (char **)NULL;
	}

	if (!matches[1])
		rl_swap_fields(&matches);

	*exit_status = EXIT_SUCCESS;
	cur_comp_type = TCMP_BM_PATHS;
	return matches;
}

static char **
complete_bookmark_names_b(char *text, int *exit_status)
{
	*exit_status = EXIT_FAILURE;
	char *p = dequote_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &bookmarks_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	flags |= MULTI_SEL;
	cur_comp_type = TCMP_BM_PREFIX;
	*exit_status = EXIT_SUCCESS;
	return matches;
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
	if (words_n > 2)
		return (char **)NULL;

	char *p = dequote_str(text, 0);
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
	if (words_n != 2)
		return (char **)NULL;

	int n = 0;
	char *p = dequote_str((char *)text, 0);
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
	rl_sort_completion_matches = 0;
	rl_attempted_completion_over = 1;
	char *p = dequote_str(text, 0);

	char **matches = rl_completion_matches(p ? p : text, &workspaces_generator);
	free(p);

	if (matches) {
		cur_comp_type = TCMP_WORKSPACES;
		return matches;
	}

	rl_sort_completion_matches = 1;
	return (char **)NULL;
}

static int
int_cmd_no_filename(void)
{
	char *lb = rl_line_buffer;
	char *p = strchr(lb, ' ');
	if (!p)
		return 0;

	*p = '\0';
	flags |= STATE_COMPLETING;
	if (is_internal_c(lb) && !is_internal_f(lb)) {
		rl_attempted_completion_over = 1;
		*p = ' ';
		flags &= ~STATE_COMPLETING;
		return 1;
	}

	flags &= ~STATE_COMPLETING;
	*p = ' ';
	return 0;
}

static char **
complete_net(char *text)
{
	char *p = dequote_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &nets_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_NET;
	return matches;
}

static char **
complete_sort_names(const char *text, const size_t words_n)
{
	if (words_n > 2)
		return (char **)NULL;

	rl_attempted_completion_over = 1;
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
	if (words_n > 3)
		return (char **)NULL;

	char *lb = rl_line_buffer;
	if (strncmp(lb, "pf add ", 7) == 0 || strncmp(lb, "pf list ", 8) == 0
	|| strncmp(lb, "profile add ", 12) == 0 || strncmp(lb, "profile list ", 13) == 0)
		return (char **)NULL;

# ifndef _NO_SUGGESTIONS
	if (suggestion.type != FILE_SUG)
		rl_attempted_completion_over = 1;
# endif /* _NO_SUGGESTIONS */
	char *p = dequote_str((char *)text, 0);
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
	if (words_n != 2)
		return (char **)NULL;

	char *p = dequote_str(text, 0);
	char **matches = rl_completion_matches(p ? p : text, &cschemes_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_CSCHEME;
	return matches;
}

static char **
complete_desel(const char *text)
{
	rl_attempted_completion_over = 1;
	char **matches = rl_completion_matches(text, &sel_entries_generator);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_DESEL;
	return matches;
}

#ifndef _NO_TRASH
static char **
complete_trashed_files(const char *text, const enum comp_type flag)
{
	char **matches = rl_trashed_files(text);
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
	if (words_n > 3)
		return (char **)NULL;

	char *p = dequote_str((char *)text, 0);
	char **matches = rl_completion_matches(p ? p : text, &prompts_generator);
	free(p);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_PROMPTS;
	return matches;
}

static char **
complete_sort_num(const char *text, const size_t words_n)
{
	if (words_n != 2)
		return (char **)NULL;

	char **matches = rl_completion_matches(text, &sort_num_generator);

	if (!matches)
		return (char **)NULL;

	cur_comp_type = TCMP_SORT;
	return matches;
}

static char **
complete_alias_names(const char *text, const size_t words_n)
{
	if (words_n > 2)
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
complete_sel_keyword(const char *text, int *exit_status, const size_t words_n)
{
	*exit_status = EXIT_FAILURE;

	if (words_n == 1 && text[1] != ':') /* Only "s:" is allowed as first word */
		return (char **)NULL;

	char **matches = rl_completion_matches("", &sel_entries_generator);
	if (!matches)
		return (char **)NULL;

	if (!matches[1])
		rl_swap_fields(&matches);

	*exit_status = EXIT_SUCCESS;
	cur_comp_type = TCMP_SEL;
	return matches;
}

static char **
complete_eln(char *text, int *exit_status, const size_t words_n)
{
	*exit_status = EXIT_FAILURE;
	char **matches = (char **)NULL;
	filesn_t n = 0;

	if (!is_number(text) || (n = xatof(text)) < 1 || n > files)
		return (char **)NULL;

	/* First word */
	if (words_n == 1) {
		if (xrename >= 2 || (file_info[n - 1].dir == 1 && conf.autocd == 0)
		|| (file_info[n - 1].dir == 0 && conf.auto_open == 0))
			return (char **)NULL;
	} else { /* Second word or more */
		if (xrename == 1 || xrename == 3 || should_expand_eln(text) == 0)
			return (char **)NULL;
	}

	matches = rl_completion_matches(text, &filenames_gen_eln);
	if (!matches)
		return (char **)NULL;

	*exit_status = EXIT_SUCCESS;
	cur_comp_type = TCMP_ELN;
	return matches;
}


/* Handle TAB completion.
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

	int exit_status = EXIT_SUCCESS;
	char **matches = (char **)NULL;
	char *lb = rl_line_buffer;
	size_t words_n = rl_count_words();

	static size_t sudo_len = 0;
	if (sudo_len == 0)
		sudo_len = (sudo_cmd && *sudo_cmd) ? strlen(sudo_cmd) : 0;

	int escaped = 0;
	while (*text == '\\') {
		++text;
		escaped = 1;
	}

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1 && rl_point < rl_end)
	/* Prevent the inserted word from being printed using the current color,
	 * say, options or quotes color.
	 * Drawback: whatever comes next to our word will be decolorized as well.
	 * But no color is better than wrong (and partially) colored word. */
		cur_color = (char *)NULL;
#endif /* !_NO_HIGHLIGHT */

	/* Do not complete when the cursor is on a word. Ex: dir/_ilename */
	if (rl_point < rl_end && rl_line_buffer[rl_point] != ' ') {
		rl_attempted_completion_over = 1;
		return (char **)NULL;
	}

				/* ##########################
				 * # 1. GENERAL EXPANSIONS  #
				 * ########################## */

	/* ##### ELN EXPANSION ##### */
	if (escaped == 0 && *text >= '1' && *text <= '9') {
		matches = complete_eln((char *)text, &exit_status, words_n);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}

	/* xrename is set (non-zero) whenever we are using an alternative prompt. */
	if (xrename != 0)
		goto FIRST_WORD_COMP;

	/* #### FILE TYPE EXPANSION #### */
	if (*text == '=') {
		matches = complete_file_type_filter((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}

#ifndef _NO_MAGIC
	/* #### MIME TYPE EXPANSION #### */
	if (*text == '@') {
		matches = complete_mime_type_filter((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}
#endif /* !_NO_MAGIC */

	/* #### FASTBACK EXPANSION #### */
	if (*text == '.' && text[1] == '.' && text[2] == '.') {
		if ((matches = rl_fastback((char *)text)) != NULL) {
			if (*matches[0] != '/' || matches[0][1])
				rl_filename_completion_desired = 1;
			cur_comp_type = TCMP_PATH;
			return matches;
		}
	}

	/* #### WILDCARDS EXPANSION #### */
	char *g = strpbrk(text, GLOB_CHARS);
	/* Expand only glob expressions in the last path component */
	if (g && !(rl_end == 2 && *rl_line_buffer == '/'
	&& *(rl_line_buffer + 1) == '*') && !strchr(g, '/')
	&& access(text, F_OK) != 0) {
		matches = complete_glob((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}

	/* #### USERS EXPANSION (~) #### */
	if (*text == '~' && text[1] != '/') {
		matches = rl_completion_matches(text + 1, &users_generator);
		endpwent();
		if (matches) {
			cur_comp_type = TCMP_USERS;
			return matches;
		}
	}

	/* ##### ENVIRONMENT VARIABLES ##### */
	if (*text == '$' && text[1] != '(') {
		matches = rl_completion_matches(text, &environ_generator);
		if (matches) {
			cur_comp_type = TCMP_ENVIRON;
			return matches;
		}
	}

#ifndef _NO_TAGS
	/* ##### TAGS ##### */
	/* ##### 1. TAGGED FILES (t:NAME<TAB>) ##### */
	if (tags_n > 0 && *text == 't'
	&& text[1] == ':' && text[2]) {
		matches = complete_tagged_file_names((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}

	/* ##### 2. TAG NAMES (t:<TAB>) ##### */
	if (tags_n > 0 && *text == 't' && text[1] == ':') {
		matches = complete_tag_names_t((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}
#endif /* !_NO_TAGS */

	/* #### BOOKMARK PATH (b:FULLNAME) #### */
	if (*text == 'b' && text[1] == ':' && text[2]) {
		matches = complete_bookmark_paths((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}

	/* ##### BOOKMARK NAMES (b:) ##### */
	if ((words_n > 1 || conf.autocd == 1 || conf.auto_open == 1)
	&& *text == 'b' && text[1] == ':') {
		matches = complete_bookmark_names_b((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}

	/* ##### SEL KEYWORD (both "sel" and "s:") ##### */
	if (sel_n > 0 && *text == 's' && (text[1] == ':'
	|| strcmp(text + 1, "el") == 0)) {
		matches = complete_sel_keyword(text, &exit_status, words_n);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}

	/* ##### HISTORY COMPLETION ("!") ##### */
	if (xrename == 0 && *text == '!') {
		char *p = dequote_str((char *)text, 0);
		matches = rl_completion_matches(p ? p : text, &hist_generator);
		free(p);
		if (matches) {
			cur_comp_type = TCMP_HIST;
			return matches;
		}
	}

FIRST_WORD_COMP:
	if (start == 0) {

					/* #######################
					 * # 2. FIRST WORD ONLY  #
					 * ####################### */

		/* #### OWNERSHIP EXPANSION ####
		 * Used only by the 'oc' command to edit files ownership. */
		if (xrename == 3)
			return complete_ownership(text);

		/* #### INTERNAL COMMANDS EXPANSION #### */
		if (xrename == 0 && ((*text == 'c' && text[1] == 'm'
		&& text[2] == 'd' && !text[3]) || strcmp(text, "commands") == 0)) {
			if ((matches = rl_completion_matches(text, &int_cmds_generator))) {
				cur_comp_type = TCMP_CMD_DESC;
				return matches;
			}
		}

		/* SEARCH PATTERNS COMPLETION */
		if (xrename == 0 && *text == '/' && text[1] == '*') {
			char *p = dequote_str((char *)text, 0);
			matches = rl_completion_matches(p ? p : text, &hist_generator);
			free(p);
			if (matches) {
				cur_comp_type = TCMP_HIST;
				return matches;
			}
		}

		if ((conf.autocd == 1 || conf.auto_open == 1) && xrename < 2
		&& (!text || *text != '/')) {
			/* Compĺete with files in CWD */
			matches = rl_completion_matches(text, &filenames_gen_text);
			if (matches) {
				cur_comp_type = TCMP_PATH;
				return matches;
			}
		}

		/* If neither autocd nor auto-open, try to complete with command names,
		 * except when TEXT is "/" */
		if (xrename == 0 && (conf.autocd == 0 || *text != '/' || text[1])) {
			if ((matches = rl_completion_matches(text, &bin_cmd_generator))) {
				cur_comp_type = TCMP_CMD;
				return matches;
			}
		}

		return (char **)NULL;
	}

				/* ##########################
				 * # 3. SECOND WORD OR MORE #
				 * ########################## */

	if (xrename == 1 || xrename == 3)
		return (char **)NULL;

	rl_sort_completion_matches = 0;
	/* Complete with specific options for internal commands */
	if ((matches = rl_completion_matches(text, &options_generator)))
		return matches;
	rl_sort_completion_matches = 1;

	/* Command names completion for words after process separator: ; | && */
	if (words_n == 1 && rl_end > 0 && rl_line_buffer[rl_end - 1] != ' '
	/* No command name contains slashes */
	&& (*text != '/' || !strchr(text, '/'))) {
		if ((matches = rl_completion_matches(text, &bin_cmd_generator))) {
			cur_comp_type = TCMP_CMD;
			return matches;
		}
	}

	/* #### SUDO COMPLETION (ex: "sudo <TAB>") #### */
	if (sudo_len > 0 && words_n == 2 && strncmp(lb, sudo_cmd, sudo_len) == 0
	&& lb[sudo_len] == ' ') {
		matches = rl_completion_matches(text, &bin_cmd_generator_ext);
		if (matches) {
			cur_comp_type = TCMP_CMD;
			return matches;
		}
	}

#ifndef _NO_TAGS
	/* #### TAG NAMES COMPLETION #### */
	/* 't? TAG' and 't? :tag' */
	if (tags_n > 0 && *lb == 't' && rl_end > 2) {
		matches = complete_tag_names((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}
#endif /* !_NO_TAGS */

	/* #### DIRECTORY HISTORY COMPLETION #### */
	if (((*lb == 'b' || *lb == 'f' || *lb == 'd') && lb[1] == 'h'
	&& lb[2] == ' ') && !strchr(text, '/') )
		return complete_dirhist((char *)text, words_n);

	/* #### BACKDIR COMPLETION #### */
	if (*text != '/' && *lb == 'b' && lb[1] == 'd' && lb[2] == ' ')
		return complete_backdir((char *)text, words_n);

#ifndef _NO_LIRA
	/* #### OPENING APPS FOR INTERNAL CMDS TAKING 'EDIT' AS SUBCOMMAND */
	if (is_edit(lb, words_n) == 1 && config_file) {
		/* mime_open_with_tab needs a file name to match against the
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
	if (rl_end > 4 && *lb == 'o' && lb[1] == 'w' && lb[2] == ' '
	&& lb[3] && lb[3] != ' ')
		return complete_open_with((char *)text);
#endif /* _NO_LIRA */

	/* #### PROMPT (only for 'prompt set') #### */
	if (*lb == 'p' && lb[1] == 'r' && strncmp(lb, "prompt set " , 11) == 0)
		return complete_prompt_names((char *)text, words_n);

#ifndef _NO_TRASH
	/* ### UNTRASH ### */
	if (*lb == 'u' && (lb[1] == ' ' || (lb[1] == 'n'
	&& (strncmp(lb, "untrash ", 8) == 0
	|| strncmp(lb, "undel ", 6) == 0))))
		return complete_trashed_files(text, TCMP_UNTRASH);

	/* ### TRASH DEL ### */
	if (*lb == 't' && (lb[1] == ' ' || lb[1] == 'r')
	&& (strncmp(lb, "t del ", 6) == 0
	|| strncmp(lb, "tr del ", 7) == 0
	|| strncmp(lb, "trash del ", 10) == 0))
		return complete_trashed_files(text, TCMP_TRASHDEL);
#endif /* !_NO_TRASH */

	/* #### SOME NUMERIC EXPANSIONS ### */
	/* Perform this check only if the first char of the string to be
	 * completed is a number in order to prevent an unnecessary call
	 * to atoi */
	if (*text >= '0' && *text <= '9') {
		/* Ranges */
		matches = complete_ranges((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;

		int n = atoi(text);
		if (n == INT_MIN)
			return (char **)NULL;

		/* Dirjump: jo command */
/*		matches = complete_dirjump_jo((char *)text, n, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches; */

		/* Sort number */
		if (*lb == 's' && (strncmp(lb, "st ", 3) == 0
		|| strncmp(lb, "sort ", 5) == 0)
		&& is_number(text) && n >= 0 && n <= SORT_TYPES)
			return complete_sort_num(text, words_n);
	}

	/* ### DESELECT COMPLETION ### */
	if (sel_n && *lb == 'd' && (strncmp(lb, "ds ", 3) == 0
	|| strncmp(lb, "desel ", 6) == 0))
		return complete_desel(text);

	/* ### DIRJUMP COMPLETION ### */
	/* j, jc, jp commands */
	if (*lb == 'j' && (lb[1] == ' '	|| ((lb[1] == 'c' || lb[1] == 'p')
	&& lb[2] == ' ') || strncmp(lb, "jump ", 5) == 0))
		return complete_jump(text);

	/* ### BOOKMARKS COMPLETION ### */
	if (*lb == 'b' && (lb[1] == 'm' || lb[1] == 'o')
	&& (strncmp(lb, "bm ", 3) == 0 || strncmp(lb, "bookmarks ", 10) == 0)) {
		matches = complete_bookmark_names((char *)text, words_n, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}

	/* ### ALIASES COMPLETION ### */
	if (aliases_n > 0 && *lb == 'a' && strncmp(lb, "alias ", 6) == 0
	&& strncmp(lb + 6, "import ", 7) != 0)
		return complete_alias_names(text, words_n);

	/* ### COLOR SCHEMES COMPLETION ### */
	if (conf.colorize == 1 && *lb == 'c' && ((lb[1] == 's' && lb[2] == ' ')
	|| strncmp(lb, "colorschemes ", 13) == 0))
		return complete_colorschemes((char *)text, words_n);

#ifndef _NO_PROFILES
	/* ### PROFILES COMPLETION ### */
	if (*lb == 'p' && (strncmp(lb, "pf ", 3) == 0
	|| strncmp(lb, "profile ", 8) == 0))
		return complete_profiles((char *)text, words_n);
#endif /* !_NO_PROFILES */

	/* ### SORT COMMAND COMPLETION ### */
	if (*lb == 's' && (strncmp(lb, "st ", 3) == 0
	|| strncmp(lb, "sort ", 5) == 0))
		return complete_sort_names(text, words_n);

	/* ### WORKSPACES COMPLETION ### */
	if (*lb == 'w' && strncmp(lb, "ws ", 3) == 0 && words_n == 2)
		return complete_workspaces((char *)text);

	/* ### COMPLETIONS FOR THE 'UNSET' COMMAND ### */
	if (*lb == 'u' && strncmp(lb, "unset ", 6) == 0)
		return rl_completion_matches(text, &env_vars_generator);

	/* ### NET COMMAND COMPLETION ### */
	if (*lb == 'n' && strncmp(lb, "net ", 4) == 0)
		return complete_net((char *)text);

	/* If we have an internal command not dealing with file names,
	 * do not perform any further completion. */
	if (int_cmd_no_filename() == 1)
		return (char **)NULL;

	/* Let's try to complete arguments for shell commands. */
	if (*text == '-') {
		matches = complete_shell_cmd_opts((char *)text, &exit_status);
		if (exit_status == EXIT_SUCCESS)
			return matches;
	}

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
	if (!input_file) {
		xerror(_("%s: An input file must be provided via the "
			"CLIFM_TEST_INPUT_FILE environment variable\n"), PROGRAM_NAME);
		UNHIDE_CURSOR;
		exit(EXIT_FAILURE);
	}

	FILE *fstream = fopen(input_file, "r");
	if (!fstream) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, input_file, strerror(errno));
		UNHIDE_CURSOR;
		exit(EXIT_FAILURE);
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
	return EXIT_SUCCESS;
#endif /* VANILLA_READLINE */

	/* Set the name of the program using readline. Mostly used for
	 * conditional constructs in the inputrc file */
	if (bin_name && *bin_name)
		rl_readline_name = bin_name;

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
	 * to the completer function (ex: "user\ file"), and if not, only
	 * wathever it found after the space char (ex: "file").
	 * Thanks to George Brocklehurst for pointing out this function:
	 * https://thoughtbot.com/blog/tab-completion-in-gnu-readline. */
	rl_char_is_quoted_p = quote_detector;

	/* Define a function to handle suggestions and syntax highlighting. */
	rl_getc_function = my_rl_getc;

	/* This function is executed inmediately before path completion. So,
	 * if the string to be completed is, for instance, "user\ file" (see
	 * the above comment), this function should return the dequoted
	 * string so it won't conflict with system file names: you want
	 * "user file", because "user\ file" does not exist, and, in this
	 * latter case, readline won't find any matches. */
	rl_filename_dequoting_function = dequote_str;

	/* Initialize the keyboard bindings function. */
	readline_kbinds();

	/* Copy the list of quote chars to a global variable to be used
	 * later by some of the program functions like split_str(),
	 * my_rl_quote(), is_quote_char(), and my_rl_dequote(). */
	quote_chars = savestring(rl_filename_quote_characters,
	    strlen(rl_filename_quote_characters));

	return EXIT_SUCCESS;
}
