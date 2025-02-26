/* term.c -- terminal management functions */

/*
 * This file is part of Clifm
 *
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

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

/* enable_raw_mode, disable_raw_mode, and get_cursor_position functions are
 * taken from https://github.com/antirez/linenoise/blob/master/linenoise.c, licensed
 * under BSD-2-Clause.
 * All changes are licenced under GPL-2.0-or-later. */

#include "helpers.h"

#include <signal.h> /* signal */
#include <stdlib.h> /* getenv */
#include <string.h> /* strchr */
#include <termios.h>
#include <unistd.h> /* isatty */
#include <errno.h>

#include "aux.h" /* xatoi */
#include "misc.h" /* set_signals_to_ignore, handle_stdin */
#include "term_info.h"

#define TRUE_COLOR 16777216

static struct termios bk_term_attrs;
static struct termios orig_term_attrs;
static int reset_term = 0;

/* Set the terminal title using the OSC-2 escape sequence. */
void
set_term_title(char *dir)
{
	int free_tmp = 0;
	char *tmp = (dir && *dir) ? home_tilde(dir, &free_tmp) : (char *)NULL;

	if (!tmp)
		printf("\x1b]2;%s\x1b\\", PROGRAM_NAME);
	else
		printf("\x1b]2;%s - %s\x1b\\", PROGRAM_NAME, tmp);

	fflush(stdout);

	if (free_tmp == 1)
		free(tmp);
}

/* Inform the underlying terminal about the new working directory using
 * the OSC-7 escape sequence. For more info see
 * https://midnight-commander.org/ticket/3088
 * See also https://github.com/MidnightCommander/mc/commit/6b44fce839b5c2e85fdfb8e1d4e5052c7f1c1c3a
 * for the Midnight Commander implementation (added recently, in version 4.8.32).
 * Opinions are quite divided reagarding this escape code, mostly from the
 * side of terminal emulators: whether to support it or not, and if yes,
 * how to implement it. We, as a client of the terminal, just emit the code,
 * and it's up to the terminal to decide what to do with it. */
void
report_cwd(char *dir)
{
	if (!dir || !*dir)
		return;

	char *uri = url_encode(dir, 0);
	if (!uri || !*uri) {
		free(uri);
		return;
	}

	printf("\x1b]7;file://%s%s\x1b\\", *hostname ? hostname : "", uri);
	fflush(stdout);

	free(uri);
}

static pid_t
get_own_pid(void)
{
	const pid_t pid = getpid();
	return (pid < 0) ? 0 : pid;
}

static int
check_nest_level(void)
{
	/* If running on a fully sanitized environment, no variable is imported
	 * at all, but CLIFMLVL is nevertheless consulted (by xsecure_env()) to
	 * know whether we are running a nested instance, in which case
	 * NESTING_LEVEL is set to 2. */
	if (xargs.secure_env_full == 1 && nesting_level == 2)
		return 2;

	char *level = getenv("CLIFMLVL");
	if (!level)
		goto FALLBACK;

	int a = atoi(level);
	if (a < 1 || a > MAX_SHELL_LEVEL)
		goto FALLBACK;

	return a + 1;

FALLBACK:
	return (getenv("CLIFM") ? 2 : 1);
}

/* Keep track of attributes of the shell. Make sure the shell is running
 * interactively as the foreground job before proceeding.
 * Based on https://www.gnu.org/software/libc/manual/html_node/Initializing-the-Shell.html#Initializing-the-Shell
 * */
void
init_shell(void)
{
	if (isatty(STDIN_FILENO) == 0) { /* Shell is not interactive */
		errno = 0;
		exit_code = handle_stdin();
		return;
	}

	reset_term = 1;

	if ((nesting_level = check_nest_level()) > 1) {
		set_signals_to_ignore();
		own_pid = get_own_pid();
		tcgetattr(STDIN_FILENO, &orig_term_attrs);
		return;
	}

	own_pid = get_own_pid();
	pid_t shell_pgid = 0;

	/* Loop until we are in the foreground */
	while (tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp()))
		kill(- shell_pgid, SIGTTIN);

	/* Ignore interactive and job-control signals */
	set_signals_to_ignore();
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	/* Put ourselves in our own process group */
	shell_pgid = getpid();
	setpgid(shell_pgid, shell_pgid);
/*	if (setpgid(shell_pgid, shell_pgid) < 0) {
		// This fails with EPERM when running as 'term -e clifm'
		err(0, NOPRINT_PROMPT, "%s: setpgid: %s\n", PROGRAM_NAME, strerror(errno));
		exit(errno);
	} */

	/* Grab control of the terminal */
	tcsetpgrp(STDIN_FILENO, shell_pgid);

	/* Save default terminal attributes for shell */
	tcgetattr(STDIN_FILENO, &orig_term_attrs);
}

int
restore_shell(void)
{
	if (reset_term == 0)
		return 0;

	return tcsetattr(STDIN_FILENO,
		TCSANOW, (const struct termios *)&orig_term_attrs);
}

/* Set the terminal into raw mode. Return 0 on success and -1 on error */
int
enable_raw_mode(const int fd)
{
	struct termios raw;

	if (!isatty(STDIN_FILENO))
		goto FAIL;

	if (tcgetattr(fd, &bk_term_attrs) == -1)
		goto FAIL;

	raw = bk_term_attrs;  /* modify the original mode */
	/* input modes: no break, no CR to NL, no parity check, no strip char,
	 * no start/stop output control. */
	raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	/* output modes - disable post processing */
	raw.c_oflag &= (tcflag_t)~(OPOST);
	/* control modes - set 8 bit chars */
	raw.c_cflag |= (CS8);
	/* local modes - echoing off, canonical off, no extended functions,
	 * no signal chars (^Z,^C) */
	raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer. */
    /* We want read to return every single byte, without timeout. */
	raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

	/* Put terminal in raw mode after flushing */
	if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
		goto FAIL;

	return 0;

FAIL:
	errno = ENOTTY;
	return (-1);
}

int
disable_raw_mode(const int fd)
{
	return tcsetattr(fd, TCSAFLUSH, &bk_term_attrs) == -1
		? FUNC_FAILURE : FUNC_SUCCESS;
}

/* Use the "ESC [6n" escape sequence to query the cursor position (both
 * vertical and horizontal) and store both values in C (columns) and L (lines).
 * Returns 0 on success and 1 on error. */
int
get_cursor_position(int *c, int *l)
{
	char buf[32];
	unsigned int i = 0;

	if (enable_raw_mode(STDIN_FILENO) == -1)
		return FUNC_FAILURE;

	/* 1. Ask the terminal about cursor position */
	const size_t cpr_len = sizeof(CPR_CODE) - 1;
	if (write(STDOUT_FILENO, CPR_CODE, cpr_len) != (ssize_t)cpr_len) {
		disable_raw_mode(STDIN_FILENO);
		return FUNC_FAILURE;
	}

	/* 2. Read the response: "ESC [ rows ; cols R" */
	int read_err = 0;
	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, buf + i, 1) != 1) { /* flawfinder: ignore */
			read_err = 1;
			break;
		}

		if (buf[i] == 'R')
			break;
		i++;
	}

	buf[i] = '\0';

	if (disable_raw_mode(STDIN_FILENO) == -1 || read_err == 1)
		return FUNC_FAILURE;

	/* 3. Parse the response */
	if (*buf != KEY_ESC || *(buf + 1) != '[' || !*(buf + 2))
		return FUNC_FAILURE;

	char *p = strchr(buf + 2, ';');
	if (!p || !*(p + 1))
		return FUNC_FAILURE;

	*p = '\0';
	*l = atoi(buf + 2); *c = atoi(p + 1);
	if (*l == INT_MIN || *c == INT_MIN)
		return FUNC_FAILURE;

	return FUNC_SUCCESS;
}

/* Return 1 if the running terminal is sixel capable, 0 if not, or -1 on error. */
static int
check_sixel_support(void)
{
	char buf[64];
	unsigned int i = 0;

	if (enable_raw_mode(STDIN_FILENO) == -1)
		return (-1);

	/* 1. Ask the terminal for device attributes (primary DA) */
	if (write(STDOUT_FILENO, "\x1b[c", 3) != 3) {
		disable_raw_mode(STDIN_FILENO);
		return (-1);
	}

	/* 2. Read the response: "n;n;n;...c" */
	int read_err = 0;
	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, buf + i, 1) != 1) { /* flawfinder: ignore */
			read_err = 1;
			break;
		}

		if (buf[i] == 'c')
			break;
		i++;
	}

	buf[i] = '\0';

	if (disable_raw_mode(STDIN_FILENO) == -1 || read_err == 1)
		return (-1);

	/* 3. Parse the response. We're looking for sixel support (4).
	 * See https://vt100.net/docs/vt510-rm/DA1.html */
	i = 0;
	while (buf[i]) {
		if ((i == 0 || buf[i - 1] == ';') && buf[i] == '4'
		&& (!buf[i + 1] || buf[i + 1] == ';'))
			return 1;
		i++;
	}

	return 0;
}

/* Return 1 if the running terminal supports Unicode, 0 if not, or -1 on error.
 * Based on https://unix.stackexchange.com/questions/184345/detect-how-much-of-unicode-my-terminal-supports-even-through-screen*/
static int
check_unicode_support(void)
{
	char buf[64];
	unsigned int i = 0;

	if (enable_raw_mode(STDIN_FILENO) == -1)
		return (-1);

	/* 1. Ask the terminal to print a 3-byte Unicode character that takes
	 * 1 terminal column, then request the cursor position, and finally
	 * clear the line. */
	if (write(STDOUT_FILENO, "\r\xe2\x88\xb4\x1b[6n\x1b[1K\r", 13) != 13) {
		disable_raw_mode(STDIN_FILENO);
		return (-1);
	}

	/* 2. Read the response: "...;nR" */
	int read_err = 0;
	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, buf + i, 1) != 1) { /* flawfinder: ignore */
			read_err = 1;
			break;
		}

		if (buf[i] == 'R')
			break;
		i++;
	}

	buf[i] = '\0';

	if (disable_raw_mode(STDIN_FILENO) == -1 || read_err == 1)
		return (-1);

	/* 3. Parse the response. If we get 2, we have Unicode support. */
	char *p = strchr(buf, ';');
	if (p && p[1] == '2' && !p[2])
		return 1;

	return 0;
}

/* See https://github.com/termstandard/colors#truecolor-detection */
static int
check_truecolor(void)
{
	char *c = getenv("COLORTERM");

	if (c && ((*c == 't' && strcmp(c + 1, "ruecolor") == 0)
	|| (*c == '2' && strcmp(c + 1, "4bit") == 0) ) )
		return 1;

	return 0;
}

static void
set_term_caps(const int i)
{
	const int true_color = check_truecolor();
	term_caps.unicode = 0;

	if (i == -1) { /* TERM not found in our terminfo database */
		term_caps.color = true_color == 1 ? TRUE_COLOR : 0;
		if (term_caps.color <= 8)
			memset(dim_c, '\0', sizeof(dim_c));
		/* All fields of the term_caps struct are already set to zero */
		return;
	}

	term_caps.home = TERM_INFO[i].home;
	term_caps.hide_cursor = TERM_INFO[i].hide_cursor;
	term_caps.clear = TERM_INFO[i].ed;
	term_caps.del_scrollback = TERM_INFO[i].del_scrollback;
	term_caps.req_cur_pos = TERM_INFO[i].req_cur_pos;
	term_caps.req_dev_attrs = TERM_INFO[i].req_dev_attrs;

	term_caps.color = true_color == 1 ? TRUE_COLOR
		: (TERM_INFO[i].color > 0 ? TERM_INFO[i].color : 0);
	if (term_caps.color <= 8)
		memset(dim_c, '\0', sizeof(dim_c));

	term_caps.suggestions = (TERM_INFO[i].cub == 1 && TERM_INFO[i].ed == 1
		&& TERM_INFO[i].el == 1) ? 1 : 0;

	term_caps.pager = (TERM_INFO[i].cub == 0 || TERM_INFO[i].el == 0) ? 0 : 1;
}

/* Check whether current terminal (ENV_TERM) supports colors and requesting
 * cursor position (needed to print suggestions). If not, disable the
 * feature accordingly. */
static void
check_term_support(const char *env_term)
{
	if (!env_term || !*env_term) {
		set_term_caps(-1);
		return;
	}

	size_t i;
	const size_t len = strlen(env_term);
	int index = -1;

	for (i = 0; TERM_INFO[i].name; i++) {
		if (*env_term != *TERM_INFO[i].name || len != TERM_INFO[i].len
		|| strcmp(env_term, TERM_INFO[i].name) != 0)
			continue;

		index = (int)i;
		break;
	}

	if (index == -1) {
		err('w', PRINT_PROMPT, _("%s: '%s': Terminal type not supported. "
			"Limited functionality is expected.\n"), PROGRAM_NAME, env_term);
	}

	set_term_caps(index);
}

/* Try to detect what kind of image capability the running terminal supports
 * (sixel, ueberzug, kitty protocol, and ansi).
 * Write the result into the CLIFM_IMG_SUPPORT environment variable.
 * This variable will be read by the clifmimg script to generate images using
 * the specified method. */
static void
check_img_support(const char *env_term)
{
	if (getenv("CLIFM_FIFO_UEBERZUG")) { /* Variable set by the clifmrun script */
		setenv("CLIFM_IMG_SUPPORT", "ueberzug", 1);
		flags |= UEBERZUG_IMG_PREV;
	} else if (getenv("KITTY_WINDOW_ID")) {
		/* KITTY_WINDOW_ID is guaranteed to be defined if running on the
		 * kitty terminal. See https://github.com/kovidgoyal/kitty/issues/957 */
		setenv("CLIFM_IMG_SUPPORT", "kitty", 1);
	} else if ((term_caps.req_dev_attrs == 1 && check_sixel_support() == 1)
	/* Yaft supports sixel (and DA request), but does not report it. */
	|| (*env_term == 'y' && strcmp(env_term, "yaft-256color") == 0)) {
		setenv("CLIFM_IMG_SUPPORT", "sixel", 1);
	} else {
		setenv("CLIFM_IMG_SUPPORT", "ansi", 1);
	}
}

void
check_term(void)
{
	char *t = getenv("TERM");
	if (!t || !*t) {
		t = "xterm";
		err('w', PRINT_PROMPT, _("%s: TERM variable unset. Running in xterm "
			"compatibility mode.\n"), PROGRAM_NAME);
	}

	check_term_support(t);

	/* Skip below checks if STDOUT is not interactive (this includes running
	 * Clifm from 'fzf --preview', i.e. tab completion), or if not required
	 * (--ls, --stat, --stat-full, and --open). */
	if (xargs.list_and_quit == 1 || xargs.stat > 0 || xargs.open == 1
	|| isatty(STDOUT_FILENO) == 0)
		return;

#ifdef __FreeBSD__
	if (!(flags & GUI))
		return;
#endif /* __FreeBSD__ */

	check_img_support(t);

	/* At this point, term_caps.unicode is zero */
	if (xargs.unicode == 1 || (xargs.unicode == UNSET
	&& term_caps.req_cur_pos == 1 && check_unicode_support() == 1))
		term_caps.unicode = 1;
}
