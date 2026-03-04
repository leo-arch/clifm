/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* spawn.c -- Execute system commands */

#include "helpers.h"

#include <ctype.h>    /* isspace */
#include <errno.h>    /* errno */
#ifndef _BE_POSIX
# include <paths.h>
# ifndef _PATH_DEVNULL
#  define _PATH_DEVNULL "/dev/null"
# endif /* _PATH_DEVNULL */
#else
# define _PATH_DEVNULL "/dev/null"
#endif /* !_BE_POSIX */
#include <signal.h>   /* sigaction */
#include <string.h>   /* strerror */
#include <unistd.h>   /* fork, execl, execvp, dup2, close, _exit */
#include <sys/wait.h> /* waitpid */

#include "listing.h"  /* reload_dirlist */
#include "misc.h"     /* xerror */

struct term_modes_backup {
	int mouse;
	int kitty;
};

static const char *
skip_blank_chars(const char *s)
{
	if (!s)
		return NULL;

	while (*s && isspace((unsigned char)*s))
		s++;

	return s;
}

static int
get_next_word(const char **s, char *buf, const size_t buf_len)
{
	if (!s || !*s || !buf || buf_len == 0)
		return 0;

	const char *p = skip_blank_chars(*s);
	if (!p || !*p)
		return 0;

	size_t i = 0;
	while (*p && !isspace((unsigned char)*p)) {
		if (i + 1 < buf_len)
			buf[i++] = *p;
		p++;
	}
	buf[i] = '\0';
	*s = p;
	return i > 0;
}

static int
is_fullscreen_cmd_name(const char *cmd)
{
	if (!cmd || !*cmd)
		return 0;

	const char *base = strrchr(cmd, '/');
	base = base ? base + 1 : cmd;

	return (strcmp(base, "vi") == 0
		|| strcmp(base, "vim") == 0
		|| strcmp(base, "nvim") == 0
		|| strcmp(base, "nano") == 0
		|| strcmp(base, "emacs") == 0
		|| strcmp(base, "less") == 0
		|| strcmp(base, "more") == 0
		|| strcmp(base, "man") == 0
		|| strcmp(base, "view") == 0);
}

static int
cmdline_needs_screen_refresh(const char *cmdline)
{
	if (!cmdline || !*cmdline)
		return 0;

	char word[NAME_MAX];
	const char *p = cmdline;
	if (get_next_word(&p, word, sizeof(word)) == 0)
		return 0;

	if (is_fullscreen_cmd_name(word) == 1)
		return 1;

	if (strcmp(word, "sudo") == 0 || strcmp(word, "doas") == 0) {
		if (get_next_word(&p, word, sizeof(word)) == 0)
			return 0;
		return is_fullscreen_cmd_name(word);
	}

	return 0;
}

static int
argv_needs_screen_refresh(const char **cmd)
{
	if (!cmd || !cmd[0] || !*cmd[0])
		return 0;

	return is_fullscreen_cmd_name(cmd[0]);
}

static void
reload_after_fullscreen_cmd(const int already_reloaded, const char *cmdline,
	const char **cmdv)
{
	if (conf.mouse_support != 1 || already_reloaded == 1)
		return;

	if ((cmdline && cmdline_needs_screen_refresh(cmdline) == 1)
	|| (cmdv && argv_needs_screen_refresh(cmdv) == 1))
		reload_dirlist();
}

/* Temporarily restore terminal input modes before launching a foreground
 * child process so external apps don't inherit clifm-specific protocols. */
static struct term_modes_backup
disable_term_modes_for_child(const int bg)
{
	struct term_modes_backup bk = {0};

	if (bg != 0 || isatty(STDIN_FILENO) == 0 || isatty(STDOUT_FILENO) == 0)
		return bk;

	if (mouse_enabled == 1) {
		UNSET_MOUSE_TRACKING;
		mouse_enabled = 0;
		bk.mouse = 1;
	}

	if (xargs.kitty_keys == 1) {
		UNSET_KITTY_KEYS;
		bk.kitty = 1;
	}

	if (bk.mouse == 1 || bk.kitty == 1)
		fflush(stdout);

	return bk;
}

static void
restore_term_modes_after_child(const struct term_modes_backup *bk)
{
	if (!bk || isatty(STDIN_FILENO) == 0 || isatty(STDOUT_FILENO) == 0)
		return;

	if (bk->kitty == 1)
		SET_KITTY_KEYS;

	if (bk->mouse == 1) {
		SET_MOUSE_TRACKING;
		mouse_enabled = 1;
	}

	if (bk->mouse == 1 || bk->kitty == 1)
		fflush(stdout);
}

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

	memset(&sa, 0, sizeof(sa));
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
	const char *shell_path = user.shell;
	const char *shell_name = user.shell_basename;
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
		execl(shell_path, shell_name, "-c", cmd, NULL);
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

	const struct term_modes_backup bk = disable_term_modes_for_child(FOREGROUND);
	const int status = xsystem(cmd);
	restore_term_modes_after_child(&bk);

	const int exit_status = get_exit_code(status, EXEC_FG_PROC);
	int reloaded = 0;

	if (flags & DELAYED_REFRESH) {
		flags &= ~DELAYED_REFRESH;
		reload_dirlist();
		reloaded = 1;
	}

	reload_after_fullscreen_cmd(reloaded, cmd, NULL);

	return exit_status;
}

/* Execute a command and return the corresponding exit status. The exit
 * status could be: zero, if everything went fine, or a non-zero value
 * in case of error. The function takes as first argument an array of
 * strings containing the command name to be executed and its arguments
 * (cmd), an integer (bg) specifying if the command should be
 * backgrounded (1) or not (0), and a flag to control file descriptors. */
int
launch_execv(const char **cmd, const int bg, const int xflags)
{
	if (!cmd)
		return EINVAL;

	int status = 0;
	const struct term_modes_backup bk = disable_term_modes_for_child(bg);
	pid_t pid = fork();
	int reloaded = 0;

	if (pid < 0) {
		xerror("%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		restore_term_modes_after_child(&bk);
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

		execvp(cmd[0], (char *const *)cmd);
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
				reloaded = 1;
			}
		}
	}

	if (bg == 1 && status == FUNC_SUCCESS && xargs.open != 1) {
		reload_dirlist();
		reloaded = 1;
	}

	if (bg == 0)
		reload_after_fullscreen_cmd(reloaded, NULL, cmd);

	restore_term_modes_after_child(&bk);
	return status;
}
