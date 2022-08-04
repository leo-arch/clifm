/* readline.c -- functions to control the behaviour of readline,
 * specially completions. It also introduces the suggestions system
 * via my_rl_getc function */

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

#include "helpers.h"

//#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/stat.h>
//#endif
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#ifdef __OpenBSD__
# include <strings.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
# include <ereadline/readline/history.h>
#else
# include <readline/readline.h>
# include <readline/history.h>
#endif

#include "misc.h"
#include "aux.h"
#include "checks.h"
#include "exec.h"
#include "keybinds.h"
#include "navigation.h"
#include "readline.h"
#include "tabcomp.h"
#include "mime.h"
#include "tags.h"

#ifndef _NO_SUGGESTIONS
# include "suggestions.h"
#endif

#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif

#if !defined(S_IFREG) || !defined(S_IFDIR)
# include <sys/stat.h>
#endif

#if !defined(_NO_SUGGESTIONS) && defined(__FreeBSD__)
int freebsd_sc_console = 0;
#endif /* __FreeBSD__ */

#define DEL_EMPTY_LINE     1
#define DEL_NON_EMPTY_LINE 2

struct dirent **tagged_files = (struct dirent **)NULL;
int tagged_files_n = 0;

#define MAX_EXT_OPTS NAME_MAX
#define MAX_EXT_OPTS_LEN NAME_MAX
char ext_opts[MAX_EXT_OPTS][MAX_EXT_OPTS_LEN];

/* Generate completions for command CMD using a modified version of
 * fish's manpages parser */
static int
gen_shell_cmd_comp(char *cmd)
{
	if (!cmd || !*cmd || !data_dir || !*data_dir)
		return EXIT_FAILURE;

	char manpage_parser_file[PATH_MAX];
	snprintf(manpage_parser_file, PATH_MAX - 1,
		"%s/%s/tools/manpages_comp_gen.py", data_dir, PNL);

	char *c[] = {manpage_parser_file, "-k", cmd, NULL};
	return launch_execve(c, FOREGROUND, E_MUTE);
}

/* Get short and long options for command CMD, store them in the EXT_OPTS
 * array and return the number of options found */
static int
get_shell_cmd_opts(char *cmd)
{
	*ext_opts[0] = '\0';
	if (!cmd || !*cmd || !user.home || (suggestions == 1 && wrong_cmd == 1))
		return EXIT_FAILURE;

	char p[PATH_MAX];
//	snprintf(p, PATH_MAX, "%s/.local/share/%s/completions/%s.cfm", user.home, PNL, cmd);
	snprintf(p, PATH_MAX, "%s/.local/share/%s/completions/%s.clifm", user.home, PNL, cmd);

	struct stat a;
	if (stat(p, &a) == -1) {
		if (gen_shell_cmd_comp(cmd) == EXIT_FAILURE || stat(p, &a) == -1)
			return EXIT_FAILURE;
	}

	int fd;
	FILE *fp = open_fstream_r(p, &fd);
	if (!fp)
		return EXIT_FAILURE;

	int n = 0;
	char line[NAME_MAX];
	while (fgets(line, (int)sizeof(line), fp)) {
		if (n >= MAX_EXT_OPTS)
			break;
		if (!*line || *line == '#' || *line == '\n')
			continue;

		size_t l = strlen(line);
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
	close_fstream(fp, fd);
	return n;
}

/* Delete key implementation */
static void
xdelete(void)
{
#ifndef _NO_SUGGESTIONS
	if (suggestion.printed && suggestion_buf)
		remove_suggestion_not_end();
#endif /* !_NO_SUGGESTIONS */

	if (rl_point == rl_end)
		return;

	int bk = rl_point;
	int mlen = mblen(rl_line_buffer + rl_point, __MB_LEN_MAX);

	rl_point = bk;
	char *s = rl_copy_text(rl_point + mlen, rl_end);
	rl_end = rl_point;
	rl_insert_text(s);
	free(s);
	rl_point = bk;
}

/* Backspace implementation */
static void
xbackspace(void)
{
	if (rl_point != rl_end) {
		if (rl_point) {
			int bk = rl_point, cc = 0;
			char *s = rl_copy_text(rl_point, rl_end);
			while ((rl_line_buffer[rl_point - 1] & 0xc0) == 0x80) {
				rl_point--;
				cc++;
			}
			rl_point--;
			rl_end = rl_point;
			rl_insert_text(s);
			free(s);
			rl_point = bk - 1 - cc;
		}
#ifndef _NO_SUGGESTIONS
		if (suggestion.printed && suggestion_buf)
			remove_suggestion_not_end();
#endif /* !_NO_SUGGESTIONS */
	} else {
#ifndef _NO_SUGGESTIONS
		if (suggestion_buf)
			clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
		if (rl_end) {
			while ((rl_line_buffer[rl_end - 1] & 0xc0) == 0x80) {
				rl_line_buffer[rl_end - 1] = '\0';
				rl_point--;
				rl_end--;
			}
			rl_line_buffer[rl_end - 1] = '\0';
			rl_point--;
			rl_end--;
		}
	}
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
		rl_end = rl_point = 0;
	}
}
#endif

static int
rl_exclude_input(unsigned char c)
{
	/* If del or backspace, highlight, but do not suggest */
	int _del = 0;

	/* Disable suggestions while in vi command mode and reenable them
	 * when changing back to insert mode */
	if (rl_editing_mode == 0) {
		if (rl_readline_state & RL_STATE_VICMDONCE) {
			if (c == 'i') {
				rl_readline_state &= (unsigned long)~RL_STATE_VICMDONCE;
#ifndef _NO_SUGGESTIONS
			} else if (suggestion.printed) {
				clear_suggestion(CS_FREEBUF);
				return 1;
#endif /* !_NO_SUGGESTIONS */
			} else {
				return 1;
			}
		}
	}

	/* Skip escape sequences, mostly arrow keys */
	if (rl_readline_state & RL_STATE_MOREINPUT) {
		if (c == '~') {
#ifndef _NO_SUGGESTIONS
			if (rl_point != rl_end && suggestion.printed) {
				/* This should be the delete key */
				remove_suggestion_not_end();
			} else {
				if (suggestion.printed)
					clear_suggestion(CS_FREEBUF);
			}
			return 1;
#endif /* !_NO_SUGGESTIONS */
		}

		else if (c == '3' && rl_point != rl_end) {
			xdelete();
			_del = DEL_NON_EMPTY_LINE;
			goto END;
		}

		/* Handle history events. If a suggestion has been printed and
		 * a history event is triggered (usually via the Up and Down arrow
		 * keys), the suggestion buffer won't be freed. Let's do it here */
#ifndef _NO_SUGGESTIONS
		else if ((c == 'A' || c == 'B') && suggestion_buf)
			clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */

		else {
			if (c == 'C' || c == 'D')
				cmdhist_flag = 0;
		}

		return 1;
	}

	/* Skip control characters (0 - 31) except backspace (8), tab(9),
	 * enter (13), and escape (27) */
	if (c < 32 && c != BS && c != _TAB && c != ENTER && c != _ESC)
		return 1;

	/* Multi-byte char. Send it directly to the input buffer. We can't
	 * process it here, since we process only single bytes */
	if (c > 127 || (c & 0xc0) == 0x80)
		return 1;

	if (c != _ESC)
		cmdhist_flag = 0;

	/* Skip backspace, Enter, and TAB keys */
	switch(c) {
		case DELETE: /* fallthrough */
/*			if (rl_point != rl_end && suggestion.printed)
				clear_suggestion(CS_FREEBUF);
			goto FAIL; */

		case BS:
			_del = (rl_point == 0 && rl_end == 0) ? DEL_EMPTY_LINE : DEL_NON_EMPTY_LINE;
			xbackspace();
			if (rl_end == 0 && cur_color != tx_c) {
				cur_color = tx_c;
				fputs(tx_c, stdout);
			}
			goto END;

		case ENTER:
#ifndef _NO_SUGGESTIONS
			if (suggestion.printed && suggestion_buf)
				clear_suggestion(CS_FREEBUF);
#endif /* !_NO_SUGGESTIONS */
			cur_color = tx_c;
			fputs(tx_c, stdout);
			return 1;

		case _ESC:
			return 1;

		case _TAB:
#ifndef _NO_SUGGESTIONS
			if (suggestion.printed) {
				if (suggestion.nlines >= 2 || suggestion.type == ELN_SUG
				|| suggestion.type == BOOKMARK_SUG
				|| suggestion.type == ALIAS_SUG
				|| suggestion.type == JCMD_SUG) {
					clear_suggestion(CS_FREEBUF);
					return 1;
				}
			}
#endif /* !_NO_SUGGESTIONS */
			return 1;

		default: break;
	}

/*	if (c <= 127) {
		char t[2];
		t[0] = (char)c;
		t[1] = '\0';
		rl_insert_text(t);
	} else if (wcn >= sizeof(wc) || (c & 0xc0) != 0x80) {
		wc[wcn] = '\0';
		rl_insert_text(wc);
		memset(wc, '\0', sizeof(wc));
		wcn = 0;
		wc[wcn++] = (char)c;
	} else {
		wc[wcn++] = (char)c;
		return -2;
	} */

	char t[2];
	t[0] = (char)c;
	t[1] = '\0';
	rl_insert_text(t);

	int s = 0;

END:
#ifndef _NO_SUGGESTIONS
	s = strcntchrlst(rl_line_buffer, ' ');
	/* Do not take into account final spaces */
	if (s >= 0 && !rl_line_buffer[s + 1])
		s = -1;
	if (rl_point != rl_end && c != _ESC) {
		if (rl_point < s) {
			if (suggestion.printed)
				remove_suggestion_not_end();
		}
		if (wrong_cmd == 1) { /* Wrong cmd and we are on the first word */
			char *fs = strchr(rl_line_buffer, ' ');
			if (fs && rl_line_buffer + rl_point <= fs)
				s = -1;
		}
	}
#else
	UNUSED(s);
#endif /* !_NO_SUGGESTIONS */

#ifndef _NO_HIGHLIGHT
	if (highlight == 0) {
		if (_del > 0) {
#ifndef _NO_SUGGESTIONS
			/* Since we have removed a char, let's check if there is
			 * a suggestion available using the modified input line */
			if (wrong_cmd == 1 && s == -1 && rl_end > 0) {
				/* If a suggestion is found, the normal prompt will be
				 * restored and wrong_cmd will be set to zero */
				rl_suggestions((unsigned char)rl_line_buffer[rl_end - 1]);
				return 2;
			}
			if (rl_point == 0 && rl_end == 0) {
				if (wrong_cmd == 1)
					recover_from_wrong_cmd();
				if (_del == DEL_EMPTY_LINE)
					leftmost_bell();
			}
#endif /* !_NO_SUGGESTIONS */
			return 2;
		}
		return 0;
	}

	if (wrong_cmd == 0) {
//		if (rl_point < rl_end) {
//		if (rl_point < rl_end || _del > 0) {
		recolorize_line();
/*		} else {
			flags |= NO_RECOLOR_LINE;
			rl_highlight(rl_line_buffer, rl_point ? (size_t)rl_point - 1 : 0, SET_COLOR);
		} */
	}
#endif /* !_NO_HIGHLIGHT */

	if (_del > 0) {
#ifndef _NO_SUGGESTIONS
		if (wrong_cmd == 1 && s == -1 && rl_end > 0) {
			rl_suggestions((unsigned char)rl_line_buffer[rl_end - 1]);
			return 2;
		}
		if (rl_point == 0 && rl_end == 0) {
			if (wrong_cmd == 1)
				recover_from_wrong_cmd();
			if (_del == DEL_EMPTY_LINE)
				leftmost_bell();
		}
#endif /* !_NO_SUGGESTIONS */
		return 2;
	}

	return 0;
}

/* Print the corresponding file name in the xrename prompt */
static int
prompt_xrename(void)
{
	char *p = (char *)NULL;
	if (*(rl_line_buffer + 1) == ' ')
		p = rl_line_buffer + 2;
	else /* We have a fused parameter */
		p = rl_line_buffer + 1;

	size_t plen = strlen(p);
	char pp[NAME_MAX];
	xstrsncpy(pp, p, sizeof(pp));

	if (plen > 0) {
		while (pp[--plen] == ' ')
			pp[plen] = '\0';
	}

	if (is_number(pp)) {
		int ipp = atoi(pp);
		if (ipp > 0 && ipp <= (int)files) {
			rl_replace_line(file_info[ipp - 1].name, 1);
			rl_point = rl_end = (int)strlen(file_info[ipp - 1].name);
		} else {
			xrename = 0;
			return EXIT_FAILURE;
		}
	} else {
		char *dstr = dequote_str(pp, 0);
		if (!dstr) {
//			fprintf(stderr, _("%s: %s: Error dequoting file name\n"), PROGRAM_NAME, pp);
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: %s: Error dequoting file name\n"),
				PROGRAM_NAME, pp);
			xrename = 0;
			return EXIT_FAILURE;
		}
		rl_replace_line(dstr, 1);
		rl_point = rl_end = (int)strlen(dstr);
		free(dstr);
	}

	rl_redisplay();
	xrename = 0;

	return EXIT_SUCCESS;
}

/* This function is automatically called by readline() to handle input.
 * Taken from Bash 1.14.7 and modified to fit our needs. Used
 * to introduce the suggestions system */
static int
my_rl_getc(FILE *stream)
{
	int result;
	unsigned char c;

#if defined(__GO32__)
	if (isatty(0))
		return (getkey() & 0x7F);
#endif /* __GO32__ */

#ifndef _NO_FZF
	if (xargs.fzftab == 1 || warning_prompt == 1) {
#else
	if (warning_prompt == 1) {
#endif /* !_NO_FZF */
		if (prompt_offset == UNSET) {
#ifndef __FreeBSD__
			get_cursor_position(&curcol, &currow);
			prompt_offset = curcol;
#else
			if ((flags & GUI) || freebsd_sc_console == 1) {
				get_cursor_position(&curcol, &currow);
				prompt_offset = curcol;
			} else {
				prompt_offset = 6;
			}
#endif /* __FreeBSD__ */
		}
	}

	if (xrename) {
		/* We are using a secondary prompt for the xrename function */
		if (prompt_xrename() == EXIT_FAILURE)
			return (EOF);
	}

	while(1) {
		result = (int)read(fileno(stream), &c, sizeof(unsigned char)); /* flawfinder: ignore */
		if (result > 0 && result == sizeof(unsigned char)) {
			if (c == 4 && control_d_exits == 1) /* Ctrl-d */
				rl_quit(0, 0);

			if (rl_end == 0) {
				fzf_open_with = 0;
				if (highlight == 1)
					rl_redisplay();
			}

			/* 24 == Ctrl-x */
			if (_xrename) {
/*				if (RL_ISSTATE(RL_STATE_METANEXT)) {
					if (c == 'A' || c == 'B' || c == 'C' || c == 'D')
						return c;
					if ((control_d_exits == 0 && c == 4) || c == 24) {
						xrename = _xrename = 0;
						return (EOF);
					}
					return 0;
				} */

/*				if (c == _ESC && !RL_ISSTATE(RL_STATE_MOREINPUT)
				&& !RL_ISSTATE(RL_STATE_METANEXT))
					continue; */ /* Skip single ESC key presses */

				if ((control_d_exits == 0 && c == 4) || c == 24) {
					xrename = _xrename = 0;
					return (EOF);
				}

				return c;
			}

			/* Syntax highlighting is made from here */
			int ret = rl_exclude_input(c);
			if (ret == 1)
				return c;

#ifndef _NO_SUGGESTIONS
			if (ret != 2 && ret != -2 && !_xrename && suggestions) {
				/* rl_suggestions returns -1 is C was inserted before
				 * the end of the current line, in which case we don't
				 * want to return it here (otherwise, it would be added
				 * to rl_line_buffer) */
# ifdef __FreeBSD__
			/* For the time being, suggestions do not work on the FreeBSD
			 * console (vt). The escape code to retrieve the current cursor
			 * position doesn't seem to work. Switching the console to 'sc'
			 * solves the issue */
				if (flags & GUI) {
					rl_suggestions(c);
				} else {
					if (freebsd_sc_console)
						rl_suggestions(c);
				}
# elif defined(__NetBSD__)  /* __FreeBSD__ */
				if (flags & GUI)
					rl_suggestions(c);
# else
				rl_suggestions(c);
# endif /* __NetBSD__ */
			}
#endif /* !_NO_SUGGESTIONS */
			if (ret != -2)
				rl_redisplay();
			continue;
		}
		/* If zero characters are returned, then the file that we are
		reading from is empty!  Return EOF in that case. */
		if (result == 0)
			return (EOF);

		/* read(3) either failed (returned -1) or is > sizeof(unsigned char) */
#if defined(EWOULDBLOCK)
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

#if !defined(__GO32__)
      /* If the error that we received was SIGINT, then try again,
	 this is simply an interrupted system call to read().
	 Otherwise, some error ocurred, also signifying EOF. */
		if (errno != EINTR)
			return (EOF);
#endif /* !__GO32__ */
	}
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
	int bk = suggestions;
	suggestions = 0;
	rl_nohist = rl_notab = 1;
	char *input = readline(prompt);
	rl_notab = rl_nohist = 0;
	suggestions = bk;

	if (input) {
		/* Make sure input isn't empty string */
		if (!*input) {
			free(input);
			return (char *)NULL;
		}

		/* Check we have some non-blank char */
		int no_blank = 0;
		char *p = input;

		while (*p) {
			if (*p != ' ' && *p != '\n' && *p != '\t') {
				no_blank = 1;
				break;
			}
			p++;
		}

		if (!no_blank) {
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
	p = (char *)xnmalloc((text_len * 2) + 1, sizeof(char));
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
	if (!text || !*text)
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
		p = (char *)NULL;
		if (!tmp_text)
			return (char *)NULL;
	}

	if (*text == '.' && text[1] == '.' && text[2] == '.') {
		char *p = savestring(text, strlen(text));
		tmp_text = fastback(p);

		free(p);
		p = (char *)NULL;

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

		if (!*text)
			text = ".";

		if (text_len)
			dirname = savestring(p, text_len);
		else
			dirname = savestring("", 1);

		if (dirname[0] == '.' && dirname[1] == '/')
			exec = 1; /* Only executable files and directories are allowed */
		else
			exec = 0;

		/* Get everything after last slash */
		temp = strrchr(dirname, '/');

		if (temp) {
			strcpy(filename, ++temp);
			*temp = '\0';
		} else {
			strcpy(dirname, ".");
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
		if (text_len > FILE_URI_PREFIX_LEN && IS_FILE_URI(text))
			d = dirname + FILE_URI_PREFIX_LEN;

		directory = opendir(d);
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

	while (directory && (ent = readdir(directory))) {
#if !defined(_DIRENT_HAVE_D_TYPE)
		struct stat attr;
		if (!dirname || (*dirname == '.' && !*(dirname + 1)))
			xstrsncpy(tmp, ent->d_name, PATH_MAX);
		else
			snprintf(tmp, PATH_MAX, "%s%s", dirname, ent->d_name);

		if (lstat(tmp, &attr) == -1)
			continue;
		type = get_dt(attr.st_mode);
#else
		type = ent->d_type;
#endif /* !_DIRENT_HAVE_D_TYPE */

		if (((suggestions == 1 && nwords == 1) || !strchr(rl_line_buffer, ' '))
		&& ((type == DT_DIR && autocd == 0) || (type != DT_DIR && auto_open == 0)))
			continue;

		/* Only dir names for cd */
		if ((suggestions == 0 || nwords > 1) && xargs.fuzzy_match == 1
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
						snprintf(tmp, PATH_MAX, "%s%s", dirname, ent->d_name);
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
						snprintf(tmp, PATH_MAX, "%s%s", dirname, ent->d_name);
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
				if (type == DT_DIR || (type == DT_REG && access(ent->d_name, X_OK) == 0))
					match = 1;
			}

			/* If "/path/./", list only executable regular files */
			else if (exec_path) {
				if (type == DT_REG || type == DT_DIR) {
					/* dir_tmp is dirname less "./", already allocated before
					 * the while loop */
					snprintf(tmp, PATH_MAX, "%s%s", dir_tmp, ent->d_name);

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
			/* Check for possible matches, first using regular matching and
			 * then, if no match, try fuzzy matching (if enabled) */
			if (case_sens_path_comp == 1) {
				if (*ent->d_name != *filename
				/* Check 2nd char as well before calling strncasecmp() */
				|| (filename_len > 1 && *(ent->d_name + 1)
				&& *(ent->d_name + 1) != *(filename + 1))

				|| (strncmp(filename, ent->d_name, filename_len) != 0) ) {
					if (xargs.fuzzy_match == 0 || *filename == '-')
						continue;
					if (flags & STATE_SUGGESTING) {
						if (!*_fmatch && fuzzy_match(filename, ent->d_name, case_sens_path_comp) == 1) {
							if (!dirname || (*dirname == '.' && !*(dirname + 1)))
								xstrsncpy(_fmatch, ent->d_name, sizeof(_fmatch));
							else
								snprintf(_fmatch, sizeof(_fmatch), "%s%s", dirname, ent->d_name);
							continue;
						} else {
							continue;
						}
					} else {
						if (fuzzy_match(filename, ent->d_name, case_sens_path_comp) == 0)
							continue;
					}
				}

			} else {
				if (TOUPPER(*ent->d_name) != TOUPPER(*filename)
				/* Check 2nd char as well before calling strncasecmp() */
				|| (filename_len > 1 && *(ent->d_name + 1)
				&& TOUPPER(*(ent->d_name + 1)) != TOUPPER(*(filename + 1)))

				|| (strncasecmp(filename, ent->d_name, filename_len) != 0)) {
					if (xargs.fuzzy_match == 0 || *filename == '-')
						continue;
					if (flags & STATE_SUGGESTING) {
						if (!*_fmatch && fuzzy_match(filename, ent->d_name, case_sens_path_comp) == 1) {
							if (!dirname || (*dirname == '.' && !*(dirname + 1)))
								xstrsncpy(_fmatch, ent->d_name, sizeof(_fmatch));
							else
								snprintf(_fmatch, sizeof(_fmatch), "%s%s", dirname, ent->d_name);
							continue;
						} else {
							continue;
						}
					} else {
						if (fuzzy_match(filename, ent->d_name, case_sens_path_comp) == 0)
							continue;
					}
				}
			}

			if (*rl_line_buffer == 'c'
			&& strncmp(rl_line_buffer, "cd ", 3) == 0) {
				ret = -1;

				switch (type) {
				case DT_LNK:
					if (dirname[0] == '.' && !dirname[1]) {
						ret = get_link_ref(ent->d_name);
					} else {
						snprintf(tmp, PATH_MAX, "%s%s", dirname, ent->d_name);
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
						snprintf(tmp, PATH_MAX, "%s%s", dirname, ent->d_name);
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

			else if (exec) {
				if (type == DT_REG && access(ent->d_name, X_OK) == 0)
					match = 1;
			}

			else if (exec_path) {
				if (type == DT_REG || type == DT_DIR) {
					snprintf(tmp, PATH_MAX, "%s%s", dir_tmp, ent->d_name);
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
				temp = (char *)xnmalloc(dirlen + strlen(ent->d_name) + 2,
									sizeof(char));
				strcpy(temp, dirname);
				// Canonicalization cuts off any final slash present.
				// We need to add it back.

				if (dirname[dirlen - 1] != '/') {
					temp[dirlen] = '/';
					temp[dirlen + 1] = '\0';
				}
			} else { */
			temp = (char *)xnmalloc(strlen(users_dirname) +
					strlen(ent->d_name) + 1, sizeof(char));
			strcpy(temp, users_dirname);
			strcat(temp, ent->d_name);
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
	if (!bookmark_names)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	/* Look for bookmarks in bookmark names for a match */
	while ((name = bookmark_names[i++]) != NULL) {
		if ((case_sensitive ? strncmp(name, text, len)
		: strncasecmp(name, text, len)) == 0)
			return strdup(name);
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
			if (len == 0 || (*name == *(text + 1) && strncmp(name, text + 1, len) == 0)
			|| (xargs.fuzzy_match == 1
			&& fuzzy_match((char *)(text + 1), name, case_sens_path_comp) == 1))
				return strdup(name);
		} else {
			/* Restrict the search to what seems to be a pattern:
			 * The string before the first slash or space (not counting the initial
			 * slash, used to fire up the search function) must contain a pattern
			 * metacharacter */
			if (!*name || !*(name + 1))
				continue;
			char *ret = strpbrk(name + 1, search_strategy == GLOB_ONLY
					? " /*?[{" : " /*?[{|^+$.");
			if (!ret || *ret == ' ' || *ret == '/')
				continue;
			if (len == 0 || (*text == *name && strncmp(name, text, len) == 0))
				return strdup(name);
		}
	}

	return (char *)NULL;
}

static char *
bm_paths_generator(const char *text, int state)
{
	if (!bookmarks || bm_n == 0)
		return (char *)NULL;

	static int i;
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = (text && *text + 1 && *text + 2) ? strlen(text + 2) : 0;
	}

	while (i < (int)bm_n && (name = bookmarks[i++].path) != NULL) {
		if (len == 0 || (*name == *(text + 2) && strncmp(name, text + 2, len) == 0)
		|| (xargs.fuzzy_match == 1
		&& fuzzy_match((char *)(text + 2), name, case_sens_path_comp) == 1)) {
			size_t nlen = strlen(name);
			if (nlen > 1 && name[nlen - 1] == '/')
				name[nlen - 1] = '\0';
			char *p = abbreviate_file_name(name);
			char *ret = strdup(p ? p : name);
			if (p != name)
				free(p);
			return ret;
		}
	}

	return (char *)NULL;
}

/* Used by environment variables completion */
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

	/* Look for cmd history entries for a match */
	while ((name = environ[i++]) != NULL) {
		if (case_sens_path_comp ? strncmp(name, text + 1, len) == 0
		: strncasecmp(name, text + 1, len) == 0) {
			char *p = strrchr(name, '=');
			if (!p)
				continue;
			*p = '\0';
			char tmp[NAME_MAX];
			snprintf(tmp, NAME_MAX, "$%s", name);
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
		/* Exclude CWD */
		if (name[1] == workspaces[cur_ws].path[1]
		&& strcmp(name, workspaces[cur_ws].path) == 0)
			continue;
		/* Filter by parent */
		if (rl_line_buffer[1] == 'p') {
			if ((case_sens_dirjump == 1 ? strstr(workspaces[cur_ws].path, name)
			: strcasestr(workspaces[cur_ws].path, name)) == NULL)
				continue;
		}
		/* Filter by child */
		else if (rl_line_buffer[1] == 'c') {
			if ((case_sens_dirjump == 1 ? strstr(name, workspaces[cur_ws].path)
			: strcasestr(name, workspaces[cur_ws].path)) == NULL)
				continue;
		}

		if ((case_sens_dirjump == 1 ? strstr(name, text)
		: strcasestr(name, (char *)text)) != NULL)
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
	} /* The state variable is zero only the first time the function is
	called, and a non-zero positive in later calls. This means that i
	and len will be necessarilly initialized the first time */

	/* Look for color schemes in color_schemes for a match */
	while ((name = color_schemes[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

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
	} /* The state variable is zero only the first time the function is
	called, and a non-zero positive in later calls. This means that i
	and len will be necessarilly initialized the first time */

	/* Look for profiles in profile_names for a match */
	while ((name = profile_names[i++]) != NULL) {
		if (strncmp(name, text, len) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

static char *
filenames_gen_text(const char *text, int state)
{
	static size_t i, len = 0;
	char *name;
	rl_filename_completion_desired = 1;
	if (!state) { /* state is zero only the first time readline is executed */
		i = 0;
		len = strlen(text);
	}

	/* Check list of currently displayed files for a match */
	while (i < files && (name = file_info[i].name) != NULL) {
		i++;
		/* If first word, filter files according to autocd and auto-open values */
		if (((suggestions == 1 && nwords == 1) || !strchr(rl_line_buffer, ' '))
		&& ( (file_info[i - 1].dir == 1 && autocd == 0)
		|| (file_info[i - 1].dir == 0 && auto_open == 0) ))
			continue;

		/* If cd, list only directories */
		if ((suggestions == 0 || nwords > 1 || (rl_end > 0 && rl_line_buffer[rl_end - 1] == ' '))
		&& rl_line_buffer && *rl_line_buffer == 'c' && rl_line_buffer[1] == 'd'
		&& rl_line_buffer[2] == ' ' && file_info[i - 1].dir == 0)
			continue;

		if (case_sens_path_comp ? strncmp(name, text, len) == 0
		: strncasecmp(name, text, len) == 0)
			return strdup(name);
		if (xargs.fuzzy_match == 0 || (*text == '.' && text[1] == '.') || *text == '-')
			continue;
		if (len == 0 || fuzzy_match((char *)text, name, case_sens_path_comp) == 1)
			return strdup(name);
	}

	return (char *)NULL;
}

/* Used by ELN expansion */
static char *
filenames_gen_eln(const char *text, int state)
{
	static size_t i;
	char *name;
	rl_filename_completion_desired = 1;

	if (!state)
		i = 0;

	int num_text = atoi(text);
	if (num_text == INT_MIN)
		return (char *)NULL;

	/* Check list of currently displayed files for a match */
	while (i < files && (name = file_info[i++].name) != NULL) {
		if (*name == *file_info[num_text - 1].name
		&& strcmp(name, file_info[num_text - 1].name) == 0) {
			if (nwords > 1 && rl_line_buffer && *rl_line_buffer == 'c' && rl_line_buffer[1] == 'd'
			&& rl_line_buffer[2] == ' ' && file_info[num_text - 1].dir == 0)
				continue;
#ifndef _NO_SUGGESTIONS
			if (suggestion_buf)
				clear_suggestion(CS_FREEBUF);
#endif
			return strdup(name);
		}
	}

	return (char *)NULL;
}

/* Used by ELN ranges expansion */
static char *
filenames_gen_ranges(const char *text, int state)
{
	static int i;
	char *name;
	rl_filename_completion_desired = 1;

	if (!state)
		i = 0;

	char *r = strchr(text, '-');
	if (!r)
		return (char *)NULL;

	*r = '\0';
	int a = atoi(text);
	int b = atoi(r + 1);
	if (a == INT_MIN || b == INT_MIN)
		return (char *)NULL;
	*r = '-';
	if (a >= b)
		return (char *)NULL;

	/* Check list of currently displayed files for a match */
	while (i < (int)files && (name = file_info[i++].name) != NULL) {
		if (i >= a && i <= b) {
#ifndef _NO_SUGGESTIONS
			if (suggestion_buf)
				clear_suggestion(CS_FREEBUF);
#endif
			return strdup(name);
		}
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

	static char *sorts[] = {
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

	/* Check list of currently displayed files for a match */
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
	static size_t len;
	char *name;

	if (!state) {
		i = 0;
		len = strlen(text);
	}

	while ((name = remotes[i++].name) != NULL) {
		if (case_sens_path_comp ? strncmp(name, text, len)
		: strncasecmp(name, text, len) == 0)
			return strdup(name);
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

	while ((name = __sorts[i++].name) != NULL) {
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
//			return strdup(name);
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
		if ((case_sensitive ? strncmp(name, text, len)
		: strncasecmp(name, text, len)) == 0)
			return strdup(name);
	}

	return (char *)NULL;
}

/* Return the list of matches for the glob expression TEXT or NULL if no matches */
static char **
rl_glob(const char *text)
{
	glob_t globbuf;

	if (glob(text, 0, NULL, &globbuf) != EXIT_SUCCESS) {
		globfree(&globbuf);
		return (char **)NULL;
	}

	if (globbuf.gl_pathc == 1) {
		char **t = (char **)xnmalloc(globbuf.gl_pathc + 1, sizeof(char *));
		t[0] = savestring(globbuf.gl_pathv[0], strlen(globbuf.gl_pathv[0]));
		t[1] = (char *)NULL;
		globfree(&globbuf);
		return t;
	}

	size_t i, j = 1;
	char **t = (char **)xnmalloc(globbuf.gl_pathc + 3, sizeof(char *));
	t[0] = xnmalloc(1, sizeof(char));
	*t[0] = '\0';

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

/* Return the list of currently trashed files matching TEXT, or NULL */
static char **
rl_trashed_files(const char *text)
{
#ifdef _NO_TRASH
	UNUSED(text);
	return (char **)NULL;
#else
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

	char **tfiles = (char **)xnmalloc((size_t)n + 2, sizeof(char *));
	if (text) {
		tfiles[0] = savestring(text, strlen(text));
	} else {
		tfiles[0] = (char *)xnmalloc(1, sizeof(char));
		*tfiles[0] = '\0';
	}

	int nn = 1, i;
	size_t tlen = text ? strlen(text) : 0;
	for (i = 0; i < n; i++) {
		char *name = t[i]->d_name;
		if (SELFORPARENT(name) || !text || strncmp(text, name, tlen) != 0) {
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
			tfiles[0] = (char *)xrealloc(tfiles[0], (strlen(d) + 1) * sizeof(char));
			strcpy(tfiles[0], d);
			free(d);
		}
	}

	return tfiles;
#endif /* _NO_TRASH */
}

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
		snprintf(tmp, PATH_MAX, "/%s", q);
		int free_tmp = 0;
		r = home_tilde(tmp, &free_tmp);
		q = strdup(r ? r : tmp);
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
	snprintf(dir, PATH_MAX, "%s/%s", tags_dir, tag);
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

	if (w != 2) /* Complete internal options only for the second word */
		return (char *)NULL;

#define MAX_OPTS 18
	char *_opts[MAX_OPTS] = {0};

	/* acd, ao, ext, ff, hf, pg, uc */
	if ( ( *l == 'a' && ((l[1] == 'o' && l[2] == ' ') || strncmp(l, "acd ", 4) == 0) )
	|| (*l == 'e' && strncmp(l, "ext ", 4) == 0)
	|| (*l == 'f' && l[1] == 'f' && l[2] == ' ')
	|| (*l == 'h' && l[1] == 'f' && l[2] == ' ')
	|| (*l == 'p' && l[1] == 'g' && l[2] == ' ')
	|| (*l == 'u' && l[1] == 'c' && l[2] == ' ') ) {
		_opts[0] = "on"; _opts[1] = "off"; _opts[2] = "status"; _opts[3] = NULL;
	/* cl, icons */
	} else if ( (*l == 'c' && l[1] == 'l' && l[2] == ' ')
	|| (*l == 'i' && strncmp(l, "icons ", 6) == 0) ) {
		_opts[0] = "on"; _opts[1] = "off"; _opts[2] = NULL;
	/* pf */
	} else if (*l == 'p' && l[1] == 'f' && l[2] == ' ') {
		_opts[0] = "list"; _opts[1] = "set"; _opts[2] = "add"; _opts[3] = "del"; _opts[4] = NULL;
	/* tag */
	} else if (*l == 't' && l[1] == 'a' && l[2] == 'g' && l[3] == ' ') {
		_opts[0] = "list"; _opts[1] = "new"; _opts[2] = "remove";
		_opts[3] = "rename"; _opts[4] = "untag"; _opts[5] = "merge"; _opts[6] = NULL;
	/* mm */
	} else if (*l == 'm' && l[1] == 'm' && l[2] == ' ') {
		_opts[0] = "info"; _opts[1] = "edit"; _opts[2] = "import"; _opts[3] = NULL;
	/* history */
	} else if (*l == 'h' && strncmp(l, "history ", 8) == 0) {
		_opts[0] = "edit"; _opts[1] = "clear"; _opts[2] = "on";
		_opts[3] = "off"; _opts[4] = "status"; _opts[5] = NULL;
	/* help topics */
	} else if (*l == 'h' && l[1] == 'e' && strncmp(l, "help ", 5) == 0) {
		_opts[0] = "archives";
		_opts[1] = "autocommands";
		_opts[2] = "basics";
		_opts[3] = "bookmarks";
		_opts[4] = "desktop-notifications";
		_opts[5] = "dir-jumper";
		_opts[6] = "file-details";
		_opts[7] = "file-tags";
		_opts[8] = "navigation";
		_opts[9] = "plugins";
		_opts[10] = "profiles";
		_opts[11] = "remotes";
		_opts[12] = "resource-opener";
		_opts[13] = "selection";
		_opts[14] = "search";
		_opts[15] = "theming";
		_opts[16] = "trash";
		_opts[17] = NULL;
	/* b, f */
	} else if ((*l == 'b' && l[1] == ' ') || (*l == 'f' && l[1] == ' ')) {
		_opts[0] = "hist"; _opts[1] = "clear"; _opts[2] = NULL;
	/* fz */
	} else if (*l == 'f' && l[1] == 'z' && l[2] == ' ') {
		_opts[0] = "on"; _opts[1] = "off"; _opts[2] = NULL;
	} else {
		/* kb */
		if (*l == 'k' && l[1] == 'b' && l[2] == ' ') {
			_opts[0] = "edit"; _opts[1] = "reset"; _opts[2] = NULL;
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
users_generator(const char *text, int state)
{
#if defined(__ANDROID__)
	UNUSED(text); UNUSED(state);
	return (char *)NULL;
#else
	static size_t len;
	const struct passwd *p;
	rl_filename_completion_desired = 1;

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

static int
tag_complete(const char *text)
{
	char *l = rl_line_buffer;
	int comp = 0;
	if (*(l + 1) && *(l + 2) == ' ') {
		switch(*(l + 1)) {
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
//		case 'n': /* fallthough */
		case 'y':
			if (*(l + 1) == 'd' || *(l + 1) == 'l') flags |= MULTI_SEL;
			comp = 1; cur_comp_type = TCMP_TAGS_S; break;
		default: break;
		}
	} else { /* MATCH LONG OPTIONS */
		if (strncmp(l, "tag ", 4) != 0)
			return comp;
		char *p = l + 4;
		if (!*p || strncmp(p, "untag ", 6) == 0) {
			if (text && *text == ':') { /* We have a tag name */
				comp = 1; cur_comp_type = TCMP_TAGS_C;
			} else if (*p == 'u') { /* We have a tagged file */
				comp = 2;
			}
		} else if (strncmp(p, "remove ", 7) == 0 || strncmp(p, "list ", 5) == 0
		/*|| strncmp(p, "new ", 4) == 0 */ || strncmp(p, "rename ", 7) == 0
		|| strncmp(p, "merge ", 6) == 0) {
			if (*p == 'r' || *p == 'l') flags |= MULTI_SEL;
			comp = 1; cur_comp_type = TCMP_TAGS_S;
		}
	}

	return comp;
}

static char *
file_types_opts_generator(const char *text, int state)
{
	UNUSED(text);
	static int i;

	if (!state)
		i = 0;

	static char *ft_opts[] = {
		"b", /* Block device */
		"c", /* Character device */
		"d", /* Directory */
		"f", /* Regular file */
		"h", /* Multi-hardlink files */
		"l", /* Symbolic link */
		"p", /* FIFO/Pipe */
		"s", /* Socket */
		"x", /* Executable file */
		"o", /* Other-writable file */
		"t", /* File with the sticky bit set */
		"u", /* SUID file */
		"g", /* SGID file */
		"C", /* File with capabilities */
		NULL
	};

	char *name;
	while ((name = ft_opts[i++]))
		return strdup(name);

	return (char *)NULL;
}

static char *
file_types_generator(const char *text, int state)
{
	static size_t i;
	const char *name;

	if (!state)
		i = 0;

	char *ret = (char *)NULL;
	while (i < files && (name = file_info[i].name)) {
		switch(*text) {
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
		case 'f':
			if (file_info[i].dir == 0)
				ret = strdup(name);
			break;
		case 'h':
			if (file_info[i].dir == 0 && file_info[i].linkn > 2)
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

char **
my_rl_completion(const char *text, int start, int end)
{
	char **matches = (char **)NULL;
	cur_comp_type = TCMP_NONE;
	flags &= ~MULTI_SEL;
	UNUSED(end);

	while (*text == '\\')
		++text;

	if (!_xrename && *text == '=') {
		if (!*(text + 1)) {
			matches = rl_completion_matches(text, &file_types_opts_generator);
			if (matches) {
				cur_comp_type = TCMP_FILE_TYPES_OPTS;
				return matches;
			}
		} else if (!*(text + 2)) {
			matches = rl_completion_matches(text + 1, &file_types_generator);
			if (matches) {
				flags |= MULTI_SEL;
				cur_comp_type = TCMP_FILE_TYPES_FILES;
				return matches;
			}
		}
	}

	char *g = strpbrk(text, GLOB_CHARS);
	// Expand only glob expressions in the last path component
	if (!_xrename && g && !strchr(g, '/') && access(text, F_OK) != 0) {
		char *p = (*rl_line_buffer == '/' && rl_end > 1 && !strchr(rl_line_buffer + 1, ' ')
			&& !strchr(rl_line_buffer + 1, '/'))
			? (char *)(text + 1) : (char *)text;
		if ((matches = rl_glob(p)) != NULL) {
			cur_comp_type = TCMP_GLOB;
			rl_filename_completion_desired = 1;
			flags |= MULTI_SEL;
			return matches;
		}
	}

	if (start == 0) { /* Only for the first entered word */
		/* If the xrename function (for the m command) is running
		 * only filenames completion is available */

		if (!_xrename && *text == '~' && *(text + 1) != '/') {
			matches = rl_completion_matches(text + 1, &users_generator);
			endpwent();
			if (matches) {
				cur_comp_type = TCMP_USERS;
				return matches;
			}
		}

		/* HISTORY CMD AND SEARCH PATTERNS COMPLETION */
		if (!_xrename && (*text == '!' || *text == '/')) {
			matches = rl_completion_matches(text, &hist_generator);
			if (matches) {
				cur_comp_type = TCMP_HIST;
				return matches;
			}
		}

		/* ENVIRONMENT VARIABLES */
		if (!_xrename && *text == '$' && *(text + 1) != '(') {
			matches = rl_completion_matches(text, &environ_generator);
			if (matches) {
				cur_comp_type = TCMP_ENVIRON;
				return matches;
			}
		}

		/* If autocd or auto-open, try to expand ELN's first */
		if (autocd == 1 || auto_open == 1) {
			if (*text >= '1' && *text <= '9') {
				int n = atoi(text);

				if (is_number(text) && n > 0 && n <= (int)files
				&& ( (file_info[n - 1].dir == 1 && autocd == 1)
				|| (file_info[n - 1].dir == 0 && auto_open == 1) ) ) {
					matches = rl_completion_matches(text, &filenames_gen_eln);
					if (matches) {
						cur_comp_type = TCMP_ELN;
						return matches;
					}
				}
			}

			/* Compĺete with files in CWD */
			if (!text || *text != '/') {
				matches = rl_completion_matches(text, &filenames_gen_text);
				if (matches) {
					cur_comp_type = TCMP_PATH;
					return matches;
				}
			}

			/* #### WILDCARDS EXPANSION #### */
/*			char *g = strpbrk(text, GLOB_CHARS);
			if (g && !strchr(g, '/') && (matches = rl_glob(text))) {
				cur_comp_type = TCMP_GLOB;
				rl_filename_completion_desired = 1;
				flags |= MULTI_SEL;
				return matches;
			} */
		}

		/* BOOKMARKS COMPLETION */
		if (!_xrename && (autocd || auto_open) && expand_bookmarks) {
			matches = rl_completion_matches(text, &bookmarks_generator);
			if (matches)
				return matches;
		}

		/* If neither autocd nor auto-open, try to complete with command names */
		if (!_xrename) {
			matches = rl_completion_matches(text, &bin_cmd_generator);
			if (matches) {
				cur_comp_type = TCMP_CMD;
				return matches;
			}
		}
	}

	else { /* Second word or more */
		if (_xrename)
			return (char **)NULL;
		/* Command names completion for words after process separator:
		 * ; | && */
		if (nwords == 1 && rl_end > 0 && rl_line_buffer[rl_end - 1] != ' '
		/* No command name contains slashes */
		&& (*text != '/' || !strchr(text, '/'))) {
			matches = rl_completion_matches(text, &bin_cmd_generator);
			if (matches) {
				cur_comp_type = TCMP_CMD;
				return matches;
			}
		}

		if (*text == 'b' && *(text + 1) == ':') {
			matches = rl_completion_matches(text, &bm_paths_generator);
			if (matches) {
				flags |= MULTI_SEL;
				rl_filename_completion_desired = 1;
				cur_comp_type = TCMP_BM_PATHS;
				return matches;
			}
		}

		char *lb = rl_line_buffer;
#ifndef _NO_TAGS
		/* #### TAGS COMPLETION #### */
		/* 1. Expand tag expressions (t:TAG) into tagged files */
		if (fzftab == 1 && tags_n > 0 && *text == 't'
		&& *(text + 1) == ':' && *(text + 2)) {
			free(cur_tag);
			cur_tag = savestring(text + 2, strlen(text + 2));
			matches = check_tagged_files(cur_tag);
			if (matches) {
				cur_comp_type = TCMP_TAGS_F;
				return matches;
			}
			free(cur_tag);
			cur_tag = (char *)NULL;
		}

		/* 2. tag expressions (t:TAG)*/
		if (*lb != ';' && *lb != ':' && tags_n > 0 && *text == 't' && *(text + 1) == ':') {
			cur_comp_type = TCMP_TAGS_T;
			matches = rl_completion_matches(text, &tags_generator);
			if (matches)
				return matches;
			cur_comp_type = TCMP_NONE;
		}

		/* 3. 't? TAG' and 't? :tag' */
		if (tags_n > 0 && *lb == 't' && rl_end > 2) {
			int comp = tag_complete(text);
			if (comp == 1) {
				matches = rl_completion_matches(text, &tags_generator);
				if (matches)
					return matches;
				cur_comp_type = TCMP_NONE;
			} else if (comp == 2) {
				/* Let's match tagged files for the untag function */
				char *_tag = get_cur_tag();
				matches = check_tagged_files(_tag);
				free(_tag);
				if (matches) {
					cur_comp_type = TCMP_TAGS_F;
					return matches;
				}
			}
		}
#endif

		/* ##### ENVIRONMENT VARIABLES ##### */
		if (*text == '$' && *(text + 1) != '(') {
			matches = rl_completion_matches(text, &environ_generator);
			if (matches) {
				cur_comp_type = TCMP_ENVIRON;
				return matches;
			}
		}

		/* ##### ~USERS ##### */
		if (*text == '~' && *(text + 1) != '/') {
			matches = rl_completion_matches(text + 1, &users_generator);
			endpwent();
			if (matches) {
				cur_comp_type = TCMP_USERS;
				return matches;
			}
		}

		/* #### WILDCARDS EXPANSION #### */
/*		char *g = strpbrk(text, GLOB_CHARS);
		// Expand only glob expressions in the last path component
		if (g && !strchr(g, '/') && access(text, F_OK) != 0
		&& (matches = rl_glob(text))) {
			cur_comp_type = TCMP_GLOB;
			rl_filename_completion_desired = 1;
			flags |= MULTI_SEL;
			return matches;
		} */

		/* #### BACKDIR COMPLETION #### */
		if (*text != '/' && nwords <= 2 && rl_end >= 3
		&& *lb == 'b' && lb[1] == 'd' && lb[2] == ' ') {
			if (nwords < 2 || (rl_end && lb[rl_end - 1] != ' ')) {
				int n = 0;
				matches = get_bd_matches(text, &n, BD_TAB);
				if (matches) {
					cur_comp_type = TCMP_BACKDIR;
					return matches;
				}
			}
		}

#ifndef _NO_LIRA
		/* #### OPEN WITH #### */
		if (rl_end > 4 && *lb == 'o' && lb[1] == 'w' && lb[2] == ' '
		&& lb[3] && lb[3] != ' ') {
			char *p = rl_line_buffer + 3;
			char *s = strrchr(p, ' ');
			if (s)
				*s = '\0';
			matches = mime_open_with_tab(p, text);
			if (s)
				*s = ' ';
			if (matches) {
				cur_comp_type = TCMP_OPENWITH;
				return matches;
			}
		}
#endif /* _NO_LIRA */

		if (*lb == 'p' && lb[1] == 'r' && strncmp(lb, "prompt " , 7) == 0) {
			matches = rl_completion_matches(text, &prompts_generator);
			if (matches) {
				cur_comp_type = TCMP_PROMPTS;
				return matches;
			}
		}

		/* ### UNTRASH ### */
		if (*lb == 'u' && (lb[1] == ' ' || (lb[1] == 'n'
		&& (strncmp(lb, "untrash ", 8) == 0 || strncmp(lb, "undel ", 6) == 0)))) {
			matches = rl_trashed_files(text);
			if (matches) {
				cur_comp_type = TCMP_UNTRASH;
				return matches;
			}
		}

		/* ### TRASH DEL ### */
		if (*lb == 't' && (lb[1] == ' ' || lb[1] == 'r')
		&& (strncmp(lb, "t del ", 6) == 0
		|| strncmp(lb, "tr del ", 7) == 0
		|| strncmp(lb, "trash del ", 10) == 0)) {
			matches = rl_trashed_files(text);
			if (matches) {
				cur_comp_type = TCMP_TRASHDEL;
				return matches;
			}
		}

		/* #### ELN AND JUMP ORDER EXPANSION ### */
		/* Perform this check only if the first char of the string to be
		 * completed is a number in order to prevent an unnecessary call
		 * to atoi */
		if (*lb != ';' && *lb != ':' && *text >= '0' && *text <= '9') {
			/* Check ranges */
			char *r = strchr(text, '-');
			if (r && *(r + 1) >= '0' && *(r + 1) <= '9') {
				*r = '\0';
				if (is_number(text) && is_number(r + 1)) {
					*r = '-';
					matches = rl_completion_matches(text, &filenames_gen_ranges);
					if (matches) {
						cur_comp_type = TCMP_RANGES;
						return matches;
					}
				} else {
					*r = '-';
				}
			}

			int n = atoi(text);
			if (n == INT_MIN)
				return (char **)NULL;

			/* Dirjump: jo command */
			if (*lb == 'j' && lb[1] == 'o' && lb[2] == ' ') {
				if (is_number(text) && n > 0 && n <= (int)jump_n
				&& jump_db[n - 1].path) {
					char *p = jump_db[n - 1].path;
					matches = (char **)xrealloc(matches, 2 * sizeof(char **));
					matches[0] = savestring(p, strlen(p));
					matches[1] = (char *)NULL;
					cur_comp_type = TCMP_PATH;
					rl_filename_completion_desired = 1;
					return matches;
				}
			}

			/* Sort number expansion */
			if (*lb == 's' && (strncmp(lb, "st ", 3) == 0
			|| strncmp(lb, "sort ", 5) == 0)
			&& is_number(text) && n >= 0 && n <= SORT_TYPES) {
				matches = rl_completion_matches(text, &sort_num_generator);
				if (matches) {
					cur_comp_type = TCMP_SORT;
					return matches;
				}
			}

			/* ELN expansion */
			if (is_number(text) && n > 0 && n <= (int)files && __expand_eln(text) == 1) {
				matches = rl_completion_matches(text, &filenames_gen_eln);
				if (matches) {
					cur_comp_type = TCMP_ELN;
					return matches;
				}
			}
		}

		/* ### SEL KEYWORD EXPANSION ### */
		if (*lb != ';' && *lb != ':' && sel_n && *text == 's'
		&& strncmp(text, "sel", 3) == 0) {
			matches = rl_completion_matches("", &sel_entries_generator);
			if (matches) {
				cur_comp_type = TCMP_SEL;
				return matches;
			}
		}

		/* ### DESELECT COMPLETION ### */
		if (sel_n && *lb == 'd'
		&& (strncmp(lb, "ds ", 3) == 0
		|| strncmp(lb, "desel ", 6) == 0)) {
			matches = rl_completion_matches(text, &sel_entries_generator);
			if (matches) {
				cur_comp_type = TCMP_DESEL;
				return matches;
			}
		}

		/* ### DIRJUMP COMPLETION ### */
		/* j, jc, jp commands */
		if (*lb == 'j' && (lb[1] == ' '	|| ((lb[1] == 'c' || lb[1] == 'p')
		&& lb[2] == ' ') || strncmp(lb, "jump ", 5) == 0)) {
			matches = rl_completion_matches(text, &jump_generator);
			if (matches) {
				cur_comp_type = TCMP_JUMP;
				return matches;
			}
		}

		/* ### BOOKMARKS COMPLETION ### */
		if (*lb == 'b' && (lb[1] == 'm' || lb[1] == 'o')
		&& (strncmp(lb, "bm ", 3) == 0
		|| strncmp(lb, "bookmarks ", 10) == 0)) {
			char *q = strchr(lb, ' ');
			/* If 'bm add' do path completion instead */
			if (!(q && *(q + 1) == 'a' && (*(q + 2) == ' '
			|| strncmp(q + 1, "add ", 4) == 0)) ) {
#ifndef _NO_SUGGESTIONS
				if (suggestion.type != FILE_SUG)
					rl_attempted_completion_over = 1;
#endif
				matches = rl_completion_matches(text, &bookmarks_generator);
				if (matches) {
					cur_comp_type = TCMP_BOOKMARK;
					return matches;
				}
			}
		}

		/* ### ALIASES COMPLETION ### */
		if (aliases_n > 0 && *lb == 'a' && strncmp(lb, "alias ", 6) == 0) {
			matches = rl_completion_matches(text, &aliases_generator);
			if (matches) {
				cur_comp_type = TCMP_ALIAS;
				return matches;
			}
		}

		/* ### COLOR SCHEMES COMPLETION ### */
		if (*lb == 'c' && ((lb[1] == 's' && lb[2] == ' ')
		|| strncmp(lb, "colorschemes ", 13) == 0)) {
			matches = rl_completion_matches(text, &cschemes_generator);
			if (matches) {
				cur_comp_type = TCMP_CSCHEME;
				return matches;
			}
		}

		/* ### PROFILES COMPLETION ### */
		if (*lb == 'p' && (lb[1] == 'r'	|| lb[1] == 'f')
		&& (strncmp(lb, "pf set ", 7) == 0
		|| strncmp(lb, "profile set ", 12) == 0
		|| strncmp(lb, "pf del ", 7) == 0
		|| strncmp(lb, "profile del ", 12) == 0)) {
#ifndef _NO_SUGGESTIONS
			if (suggestion.type != FILE_SUG)
				rl_attempted_completion_over = 1;
#endif /* _NO_SUGGESTIONS */
			matches = rl_completion_matches(text, &profiles_generator);
			if (matches) {
				cur_comp_type = TCMP_PROF;
				return matches;
			}
		}

		if (*lb != ';' && *lb != ':' && expand_bookmarks) {
			matches = rl_completion_matches(text, &bookmarks_generator);
			if (matches) {
				cur_comp_type = TCMP_BOOKMARK;
				return matches;
			}
		}

		/* ### SORT COMMAND COMPLETION ### */
		if (*lb == 's' && (strncmp(lb, "st ", 3) == 0
		|| strncmp(lb, "sort ", 5) == 0)) {
			rl_attempted_completion_over = 1;
			matches = rl_completion_matches(text, &sort_name_generator);
			if (matches) {
				cur_comp_type = TCMP_SORT;
				return matches;
			}
		}

		/* ### WORKSPACES COMPLETION ### */
		if (*lb == 'w' && strncmp(lb, "ws ", 3) == 0 && nwords <= 2) {
			rl_sort_completion_matches = 0;
			rl_attempted_completion_over = 1;
			matches = rl_completion_matches(text, &workspaces_generator);
			if (matches) {
				cur_comp_type = TCMP_WORKSPACES;
				return matches;
			}
			rl_sort_completion_matches = 1;
		}

		/* ### NET COMMAND COMPLETION ### */
		if (*lb == 'n' && strncmp(lb, "net ", 4) == 0) {
			matches = rl_completion_matches(text, &nets_generator);
			if (matches) {
				cur_comp_type = TCMP_NET;
				return matches;
			}
		}

		rl_sort_completion_matches = 0;
		/* Complete with specific options for internal commands */
		if ((matches = rl_completion_matches(text, &options_generator)))
			return matches;
		rl_sort_completion_matches = 1;

		/* If we have an internal command not dealing with file names,
		 * do not perform completion */
		char *p = strchr(lb, ' ');
		if (p) {
			*p = '\0';
			flags |= STATE_COMPLETING;
			if (is_internal_c(lb) && !is_internal_f(lb)) {
				rl_attempted_completion_over = 1;
				*p = ' ';
				flags &= ~STATE_COMPLETING;
				return (char **)NULL;
			}
			flags &= ~STATE_COMPLETING;
			*p = ' ';
		}

		/* Let's try to complete arguments for shell commands */
		if (*text == '-') {
			char lw[NAME_MAX]; *lw = '\0'; /* Last word before the dash */
			char *a = strrchr(rl_line_buffer, ' ');
			if (a) {
				*a = '\0';
				char *b = strrchr(rl_line_buffer, ' ');
				xstrsncpy(lw, (b && *(b + 1)) ? b + 1 : rl_line_buffer, sizeof(lw));
				*a = ' ';
			}
			if (*lw && get_shell_cmd_opts(lw) > 0
			&& (matches = rl_completion_matches(text, &ext_options_generator)) ) {
//				cur_comp_type = TCMP_EXT_OPTS;
				return matches;
			}
		}

		/* Finally, try to complete with filenames in CWD */
		if ((matches = rl_completion_matches(text, &filenames_gen_text))) {
			cur_comp_type = TCMP_PATH;
			return matches;
		}
	}

	/* ### PATH COMPLETION ### */
	/* If none of the above, readline will attempt path completion
	 * instead via my_rl_path_completion() */
	return matches;
}

/* Load readline initialization file (inputrc)
 * Check order:
 * 1) INPUTRC environment variable
 * 2) ~/.config/clifm/readline.clifm
 * 3) ~/.inputrc
 * 4) /etc/inputrc
 * If neither 1 nor 2 exist, readline will try to read 3 and 4 by default) */
static inline void
set_rl_init_file(void)
{
	char *p = getenv("INPUTRC");
	if (p) {
		rl_read_init_file(p);
		return;
	}

	if (!config_dir_gral || !*config_dir_gral)
		return;

/*	char *rl_file = (char *)xnmalloc(strlen(config_dir_gral) + 14, sizeof(char));
	sprintf(rl_file, "%s/readline.cfm", config_dir_gral); */
	char *rl_file = (char *)xnmalloc(strlen(config_dir_gral) + 16, sizeof(char));
	sprintf(rl_file, "%s/readline.clifm", config_dir_gral);
	rl_read_init_file(rl_file);
	free(rl_file);
}
/*
int
rl_signals(void)
{
	if (flags & DELAYED_REFRESH) {
		flags &= ~DELAYED_REFRESH;
		puts("EVENT HOOK DELAYED!");
//		fflush(stdout);
//		sleep(2);
//		get_term_size();
//		reload_dirlist();
	}
	return EXIT_SUCCESS;
} */

int
initialize_readline(void)
{
	/* Set the name of the program using readline. Mostly used for
	 * conditional constructs in the inputrc file */
	rl_readline_name = argv_bk[0];

	set_rl_init_file();

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
	 * the quoting function */
	rl_filename_quote_characters = " \t\n\"\\'`@$><=,;|&{[()]}?!*^#";
	/* According to readline documentation, the following string is
	 * the default and the one used by Bash: " \t\n\"\\'`@$><=;|&{(" */

	/* Executed immediately before calling the completer function, it
	 * tells readline if a space char, which is a word break character
	 * (see the above rl_completer_word_break_characters variable) is
	 * quoted or not. If it is, readline then passes the whole string
	 * to the completer function (ex: "user\ file"), and if not, only
	 * wathever it found after the space char (ex: "file")
	 * Thanks to George Brocklehurst for pointing out this function:
	 * https://thoughtbot.com/blog/tab-completion-in-gnu-readline*/
	rl_char_is_quoted_p = quote_detector;

	/* Define a function to handle suggestions and syntax highlighting */
	rl_getc_function = my_rl_getc;

	/* This function is executed inmediately before path completion. So,
	 * if the string to be completed is, for instance, "user\ file" (see
	 * the above comment), this function should return the dequoted
	 * string so it won't conflict with system file names: you want
	 * "user file", because "user\ file" does not exist, and, in this
	 * latter case, readline won't find any matches */
	rl_filename_dequoting_function = dequote_str;

	/* Initialize the keyboard bindings function */
	readline_kbinds();

	/* Copy the list of quote chars to a global variable to be used
	 * later by some of the program functions like split_str(),
	 * my_rl_quote(), is_quote_char(), and my_rl_dequote() */
	quote_chars = savestring(rl_filename_quote_characters,
	    strlen(rl_filename_quote_characters));

//	rl_event_hook = rl_signals;

#if !defined(_NO_SUGGESTIONS) && defined(__FreeBSD__)
	if (!(flags & GUI) && getenv("CLIFM_FREEBSD_CONSOLE_SC"))
		freebsd_sc_console = 1;
#endif

	return EXIT_SUCCESS;
}
