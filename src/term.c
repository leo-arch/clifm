/* term.c -- terminal management functions */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

 * Clifm is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Clifm is distributed in the hope that it will be useful,
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

#include <signal.h> /* signal */
#include <stdlib.h> /* getenv */
#include <string.h> /* strchr */
#include <termios.h>
#include <unistd.h> /* isatty */
#include <errno.h>

#include "aux.h" /* xatoi */
#include "misc.h" /* set_signals_to_ignore, handle_stdin */

static struct termios bk_term_attrs;
static struct termios orig_term_attrs;
static int reset_term = 0;

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

/* Return 1 if the running terminal is sixel capable, 0 if not, or -1 on error. */
int
check_sixel_support(void)
{
	char buf[64];
	unsigned int i = 0;

	if (enable_raw_mode(STDIN_FILENO) == -1)
		return (-1);

	/* 1. Ask the terminal for device attributes (primary DA) */
	const ssize_t qlen = 3;
	if (write(STDOUT_FILENO, "\x1b[c", qlen) != qlen) {
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

/* Use the "ESC [6n" escape sequence to query the cursor position (both
 * vertical and horizontal) and store both values into C (columns) and L (lines).
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
