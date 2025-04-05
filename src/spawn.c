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

/* spawn.c -- Execute system commands */

#include "helpers.h"

#include <errno.h>    /* errno */
#ifndef _BE_POSIX
# include <paths.h>
# ifndef _PATH_DEVNULL
#  define _PATH_DEVNULL "/dev/null"
# endif /* _PATH_DEVNULL */
#else
# define _PATH_DEVNULL "/dev/null"
#endif /* !_BE_POSIX */
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
# include <signal.h>  /* signal */
#endif /* BSD || __sun */
#include <string.h>   /* strerror */
#include <unistd.h>   /* fork, execl, execvp, dup2, close, _exit */
#include <sys/wait.h> /* waitpid */

#include "listing.h"  /* reload_dirlist */
#include "misc.h"     /* xerror */

int
get_exit_code(const int status, const int exec_flag)
{
	if (WIFSIGNALED(status))
	/* As required by exit(1p), we return a value greater than 128 (E_SIGINT) */
		return (E_SIGINT + WTERMSIG(status));
	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	return exec_flag == EXEC_BG_PROC ? FUNC_SUCCESS : FUNC_FAILURE;
}

static int
run_in_foreground(const pid_t pid)
{
	int status = 0;

	/* The parent process calls waitpid() on the child */
	if (waitpid(pid, &status, 0) > 0)
		return get_exit_code(status, EXEC_FG_PROC);

	/* waitpid() failed */
	const int ret = errno;
	xerror("%s: waitpid: %s\n", PROGRAM_NAME, strerror(errno));
	return ret;
}

static int
run_in_background(const pid_t pid)
{
	int status = 0;

	if (waitpid(pid, &status, WNOHANG) == -1) {
		const int ret = errno;
		xerror("%s: waitpid: %s\n", PROGRAM_NAME, strerror(errno));
		return ret;
	}

	zombies++;

	return get_exit_code(status, EXEC_BG_PROC);
}

/* Enable/disable signals for external commands.
 * Used by launch_execl() and launch_execv(). */
static void
set_cmd_signals(void)
{
	struct sigaction sa;

	memset(&sa, '\0', sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_DFL;

	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGTSTP, &sa, NULL);
}

/* Implementation of system(3).
 * Unlike system(3), which runs a command using '/bin/sh' as the executing
 * shell, xsystem() uses a custom shell (user.shell) specified via CLIFM_SHELL
 * or SHELL, falling back to '/bin/sh' only if none of these variables is set. */
static int
xsystem(const char *cmd)
{
	char *shell_path = user.shell;
	char *shell_name = user.shell_basename;
	if (!shell_path || !*shell_path || !shell_name || !*shell_name) {
		shell_path = "/bin/sh";
		shell_name = "sh";
	}

	int status = 0;
	pid_t pid = fork();

	if (pid < 0) {
		return (-1);
	} else if (pid == 0) {
		set_cmd_signals();
		execl(shell_path, shell_name, "-c", cmd, (char *)NULL);
		_exit(errno);
	} else {
		if (waitpid(pid, &status, 0) == pid)
			return status;

		return (-1);
	}
}

/* Execute a command using the system shell.
 *
 * The shell to be used is specified via CLIFM_SHELL or SHELL environment
 * variables (in this order). If none is set, '/bin/sh' is used instead.
 *
 * The shell takes care of special functions such as pipes and stream redirection,
 * special chars like wildcards, quotes, and escape sequences.
 *
 * Use only when the shell is needed; otherwise, launch_execv() should be
 * used instead. */
int
launch_execl(const char *cmd)
{
	if (!cmd || !*cmd)
		return EINVAL;

	const int status = xsystem(cmd);

	const int exit_status = get_exit_code(status, EXEC_FG_PROC);

	if (flags & DELAYED_REFRESH) {
		flags &= ~DELAYED_REFRESH;
		reload_dirlist();
	}

	return exit_status;
}

/* Execute a command and return the corresponding exit status. The exit
 * status could be: zero, if everything went fine, or a non-zero value
 * in case of error. The function takes as first argument an array of
 * strings containing the command name to be executed and its arguments
 * (cmd), an integer (bg) specifying if the command should be
 * backgrounded (1) or not (0), and a flag to control file descriptors. */
int
launch_execv(char **cmd, const int bg, const int xflags)
{
	if (!cmd)
		return EINVAL;

	int status = 0;
	pid_t pid = fork();

	if (pid < 0) {
		xerror("%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	}

	if (pid == 0) {
		if (bg == 0) {
			/* If the program runs in the foreground, reenable signals only
			 * for the child, in case they were disabled for the parent. */
			set_cmd_signals();
		}

		if (xflags) {
			int fd = open(_PATH_DEVNULL, O_WRONLY, 0200);
			if (fd == -1) {
				xerror("%s: '%s': %s\n", PROGRAM_NAME,
					_PATH_DEVNULL, strerror(errno));
				_exit(errno);
			}

			if (xflags & E_NOSTDIN)
				dup2(fd, STDIN_FILENO);
			if (xflags & E_NOSTDOUT)
				dup2(fd, STDOUT_FILENO);
			if (xflags & E_NOSTDERR)
				dup2(fd, STDERR_FILENO);
			if ((xflags & E_SETSID) && setsid() == (pid_t)-1) {
				xerror("%s: setsid: %s\n", PROGRAM_NAME, strerror(errno));
				close(fd);
				_exit(errno);
			}

			close(fd);
		}

		execvp(cmd[0], cmd);
		/* These error messages will be printed only if E_NOSTDERR is unset.
		 * Otherwise, the caller should print the error messages itself. */
		if (errno == ENOENT) {
			xerror("%s: %s: %s\n", PROGRAM_NAME, cmd[0], NOTFOUND_MSG);
			_exit(E_NOTFOUND); /* 127, as required by exit(1p). */
		} else {
			xerror("%s: %s: %s\n", PROGRAM_NAME, cmd[0], strerror(errno));
			if (errno == EACCES || errno == ENOEXEC)
				_exit(E_NOEXEC); /* 126, as required by exit(1p). */
			else
				_exit(errno);
		}
	}

	/* Get command status (pid > 0) */
	else {
		if (bg == 1) {
			status = run_in_background(pid);
		} else {
			status = run_in_foreground(pid);
			if ((flags & DELAYED_REFRESH) && xargs.open != 1) {
				flags &= ~DELAYED_REFRESH;
				reload_dirlist();
			}
		}
	}

	if (bg == 1 && status == FUNC_SUCCESS && xargs.open != 1)
		reload_dirlist();

	return status;
}
