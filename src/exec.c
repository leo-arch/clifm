/* exec.c -- functions controlling the execution of programs */

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

#include "helpers.h"

#ifdef __OpenBSD__
# include <sys/dirent.h>
#endif /* __OpenBSD__ */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include <limits.h>

#include "actions.h"
#ifndef _NO_ARCHIVING
# include "archives.h"
#endif /* !_NO_ARCHIVING */
#include "aux.h"
#include "bookmarks.h"
#include "checks.h"
#include "colors.h"
#include "config.h"
#include "exec.h"
#include "file_operations.h"
#include "history.h"
#include "init.h"
#include "jump.h"
#include "keybinds.h"
#include "listing.h"
#include "mime.h"
#include "misc.h"
#include "navigation.h"
#ifndef _NO_PROFILES
# include "profiles.h"
#endif /* !_NO_PROFILES */
#include "prompt.h"
#include "properties.h"
#include "readline.h"
#include "remotes.h"
#include "search.h"
#include "selection.h"
#include "sort.h"
#ifndef _NO_TRASH
# include "trash.h"
#endif /* !_NO_TRASH */
#include "messages.h"
#include "media.h"
#ifndef _NO_BLEACH
# include "name_cleaner.h"
#endif /* !_NO_BLEACH */
#include "sanitize.h"
#include "tags.h"
#include "tabcomp.h"

static char *
get_new_filename(char *cur_name)
{
	char _prompt[NAME_MAX];
	snprintf(_prompt, sizeof(_prompt), _("Enter new name (Ctrl-d to quit)\n"
		"\001%s\002>\001%s\002 "), mi_c, tx_c);

	char *new_name = (char *)NULL;
	while (!new_name) {
		int quoted = 0;
		new_name = get_newname(_prompt, cur_name, &quoted);
		UNUSED(quoted);

		if (!new_name) /* The user pressed Ctrl-d */
			return (char *)NULL;

		if (is_blank_name(new_name) == 1) {
			free(new_name);
			new_name = (char *)NULL;
		}
	}

	size_t l = strlen(new_name);
	if (l > 0 && new_name[l - 1] == ' ') {
		l--;
		new_name[l] = '\0';
	}

	char *n = normalize_path(new_name, l);
	free(new_name);

	return n;
}

/* Run CMD via execve() and refresh the screen in case of success
 * skip_force might be true (1) only when coming from cp_mv_file(), that is,
 * for the c and m commands, which take -f,--force as parameter to
 * intruct cp/mv to run non-interactivelly (no -i) */
int
run_and_refresh(char **cmd, const int skip_force)
{
	if (!cmd)
		return EXIT_FAILURE;

	char *new_name = (char *)NULL;
	if (xrename == 1) {
		if (!cmd[1])
			return EINVAL;

		/* If we have a number, either it was not expanded by parse_input_str(),
		 * in which case it is an invalid ELN, or it was expanded to a file
		 * named as a number. Let's check if we have such file name in the
		 * files list. */
		if (is_number(cmd[1])) {
			int i = (int)files;
			while (--i >= 0) {
				if (*cmd[1] != *file_info[i].name)
					continue;
				if (strcmp(cmd[1], file_info[i].name) == 0)
					break;
			}
			if (i == -1) {
				xerror(_("%s: %s: No such ELN\n"), PROGRAM_NAME, cmd[1]);
				xrename = 0;
				return ENOENT;
			}
		} else {
			char *p = dequote_str(cmd[1], 0);
			struct stat a;
			int ret = lstat(p ? p : cmd[1], &a);
			free(p);
			if (ret == -1) {
				xrename = 0;
				xerror("m: %s: %s\n", cmd[1], strerror(errno));
				return errno;
			}
		}

		new_name = get_new_filename(cmd[1]);
		if (!new_name)
			return EXIT_SUCCESS;
	}

	char **tcmd = xnmalloc(3 + args_n + 2, sizeof(char *));
	size_t n = 0;
	char *p = strchr(cmd[0], ' ');
	if (p && *(p + 1)) {
		*p = '\0';
		p++;
		tcmd[0] = savestring(cmd[0], strlen(cmd[0]));
		tcmd[1] = savestring(p, strlen(p));
		n += 2;
	} else {
		tcmd[0] = savestring(cmd[0], strlen(cmd[0]));
		n++;
	}

	/* wcp does not support end of options (--) */
	if (strcmp(cmd[0], "wcp") != 0) {
		tcmd[n] = savestring("--", 2);
		n++;
	}

	size_t i;
	for (i = 1; cmd[i]; i++) {
		/* The -f,--force parameter is internal. Skip it.
		 * It instructs cp/mv to run non-interactively (no -i param) */
		if (skip_force == 1 && i == 1 && is_force_param(cmd[i]) == 1)
			continue;
		p = dequote_str(cmd[i], 0);
		if (!p)
			continue;
		tcmd[n] = savestring(p, strlen(p));
		free(p);
		n++;
	}

	if (cmd[1] && !cmd[2] && *cmd[0] == 'c' && *(cmd[0] + 1) == 'p'
	&& *(cmd[0] + 2) == ' ') {
		tcmd[n][0] = '.';
		tcmd[n][1] = '\0';
		n++;
	} else {
		if (new_name) {
			p = (char *)NULL;
			if (*new_name == '~')
				p = tilde_expand(new_name);
			char *q = p ? p : new_name;
			tcmd[n] = savestring(q, strlen(q));
			free(new_name);
			free(p);
			n++;
		}
	}

	tcmd[n] = (char *)NULL;
	int ret = launch_execv(tcmd, FOREGROUND, E_NOFLAG);

	for (i = 0; i < n; i++)
		free(tcmd[i]);
	free(tcmd);

	if (ret != EXIT_SUCCESS)
		return ret;

	/* Error messages are printed by launch_execv() itself */

	/* If 'rm sel' and command is successful, deselect everything */
	if (is_sel && *cmd[0] == 'r' && cmd[0][1] == 'm' && (!cmd[0][2]
	|| cmd[0][2] == ' ')) {
		int j = (int)sel_n;
		while (--j >= 0)
			free(sel_elements[j].name);
		sel_n = 0;
		save_sel();
	}

#ifdef NO_FS_EVENTS_MONITOR
	if (conf.autols == 1 && cmd[1] && strcmp(cmd[1], "--help") != 0
	&& strcmp(cmd[1], "--version") != 0)
		reload_dirlist();
#endif /* NO_FS_EVENTS_MONITOR */

	return EXIT_SUCCESS;
}

int
get_exit_code(const int status, const int exec_flag)
{
	if (WIFSIGNALED(status))
	/* As required by exit(1p), we return a value greater than 128 (EXEC_SIGINT) */
		return (EXEC_SIGINT + WTERMSIG(status));
	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	return exec_flag == EXEC_BG_PROC ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
run_in_foreground(const pid_t pid)
{
	int status = 0;

	/* The parent process calls waitpid() on the child */
	if (waitpid(pid, &status, 0) > 0)
		return get_exit_code(status, EXEC_FG_PROC);

	/* waitpid() failed */
	int ret = errno;
	xerror("%s: waitpid: %s\n", PROGRAM_NAME, strerror(errno));
	return ret;
}

static int
run_in_background(const pid_t pid)
{
	int status = 0;

	if (waitpid(pid, &status, WNOHANG) == -1) {
		int ret = errno;
		xerror("%s: waitpid: %s\n", PROGRAM_NAME, strerror(errno));
		return ret;
	}

	zombies++;

	return get_exit_code(status, EXEC_BG_PROC);
}

/* Implementation of system(3).
 * Unlike system(3), which runs a command using '/bin/sh' as the executing
 * shell, xsystem() uses a custom shell (user.shell) specified via CLIFM_SHELL
 * or SHELL, falling back to '/bin/sh' only if none of these variables are set. */
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
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);

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
		return EXEC_NULLPARAM;

	int status = xsystem(cmd);

	int exit_status = get_exit_code(status, EXEC_FG_PROC);

	if (flags & DELAYED_REFRESH) {
		flags &= ~DELAYED_REFRESH;
		reload_dirlist();
	}

	return exit_status;
}

/* Execute a command and return the corresponding exit status. The exit
 * status could be: zero, if everything went fine, or a non-zero value
 * in case of error. The function takes as first arguement an array of
 * strings containing the command name to be executed and its arguments
 * (cmd), an integer (bg) specifying if the command should be
 * backgrounded (1) or not (0), and a flag to control file descriptors */
int
launch_execv(char **cmd, const int bg, const int xflags)
{
	if (!cmd)
		return EXEC_NULLPARAM;

	int status = 0;
	pid_t pid = fork();
	if (pid < 0) {
		xerror("%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	} else if (pid == 0) {
		if (bg == 0) {
			/* If the program runs in the foreground, reenable signals only
			 * for the child, in case they were disabled for the parent. */
			signal(SIGHUP, SIG_DFL);
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
		}

		if (xflags) {
			int fd = open("/dev/null", O_WRONLY, 0200);

			if (xflags & E_NOSTDIN)
				dup2(fd, STDIN_FILENO);
			if (xflags & E_NOSTDOUT)
				dup2(fd, STDOUT_FILENO);
			if (xflags & E_NOSTDERR)
				dup2(fd, STDERR_FILENO);

			close(fd);
		}

		execvp(cmd[0], cmd);
		/* These error messages will be printed only if E_NOSTDERR is not set.
		 * Otherwise, the caller should print the error messages itself. */
		if (errno == ENOENT) {
			xerror("%s: %s: %s\n", PROGRAM_NAME, cmd[0], NOTFOUND_MSG);
			_exit(EXEC_NOTFOUND); /* 127, as required by exit(1p). */
		} else {
			xerror("%s: %s: %s\n", PROGRAM_NAME, cmd[0], strerror(errno));
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

	if (bg == 1 && status == EXIT_SUCCESS && xargs.open != 1)
		reload_dirlist();

	return status;
}

/* Prevent the user from killing the program via the 'kill',
 * 'pkill' or 'killall' commands, from within CliFM itself.
 * Otherwise, the program will be forcefully terminated without
 * freeing allocated memory. */
static inline int
graceful_quit(char **args)
{
	size_t i;
	for (i = 1; i <= args_n; i++) {
		if ((strcmp(args[0], "kill") == 0 && atoi(args[i]) == (int)own_pid)
		|| ((strcmp(args[0], "killall") == 0 || strcmp(args[0], "pkill") == 0)
		&& bin_name && strcmp(args[i], bin_name) == 0)) {
			fprintf(stderr, _("%s: To gracefully quit enter 'q'\n"),
				PROGRAM_NAME);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

#if !defined(__CYGWIN__)
/* Get current modification time for each path in PATH and compare it to
 * the cached time (in paths[n].mtime).
 * Return 1 if at least one timestamp differs, or 0 otherwise. */
static int
check_paths_timestamps(void)
{
	if (path_n == 0)
		return EXIT_SUCCESS;

	struct stat a;
	int i = (int)path_n;
	int status = EXIT_SUCCESS;

	while (--i >= 0) {
		if (paths[i].path && stat(paths[i].path, &a) != -1
		&& a.st_mtime != paths[i].mtime) {
			paths[i].mtime = a.st_mtime;
			status = EXIT_FAILURE;
		}
	}

	return status;
}

/* Reload the list of available commands in PATH for TAB completion.
 * Why? If this list is not updated, whenever some new program is
 * installed, renamed, or removed from some of the paths in PATH
 * while in CliFM, this latter needs to be restarted in order
 * to be able to recognize the new program for TAB completion */
static inline void
reload_binaries(void)
{
	if (check_paths_timestamps() == EXIT_SUCCESS)
		return;

	if (bin_commands) {
		int j = (int)path_progsn;
		while (--j >= 0)
			free(bin_commands[j]);
		free(bin_commands);
		bin_commands = (char **)NULL;
	}

	if (paths) {
		int j = (int)path_n;
		while (--j >= 0)
			free(paths[j].path);
		free(paths);
	}

	path_n = (size_t)get_path_env();
	get_path_programs();
}
#endif /* !__CYGWIN__ */

/* Little export implementation. What it lacks? Command substitution
 * Export variables in ARG, in the form "VAR=VALUE", to the environment */
static int
export_var_function(char **args)
{
	if (!args || !args[0] || !*args[0]) {
		xerror("%s\n", _("export: A parameter, in the form VAR=VALUE, "
			"is required"));
		return EXIT_FAILURE;
	}

	if (IS_HELP(args[0])) {
		puts(EXPORT_VAR_USAGE);
		return EXIT_SUCCESS;
	}

	int status = EXIT_SUCCESS;
	size_t i;
	for (i = 0; args[i]; i++) {
		/* ARG might have been escaped by parse_input_str(), in the command
		 * and parameter substitution block. Let's deescape it */
		char *ds = dequote_str(args[i], 0);
		if (!ds) {
			xerror("%s\n", _("export: Error dequoting argument"));
			status = EXIT_FAILURE;
			continue;
		}

		char *p = strchr(ds, '=');
		if (!p || !*(p + 1)) {
			xerror(_("export: %s: Empty assignement\n"), ds);
			free(ds);
			status = EXIT_FAILURE;
			continue;
		}

		errno = 0;

		*p = '\0';
		if (setenv(ds, p + 1, 1) == -1) {
			status = errno;
			xerror("export: %s\n", strerror(errno));
		}
		*p = '=';

		free(ds);
	}

	return status;
}

static inline char *
construct_shell_cmd(char **args)
{
	/* If the command starts with either ':' or ';', it has bypassed all clifm
	 * expansions. At this point we don't care about it: skip this char. */
	int bypass = (*args[0] == ';' || *args[0] == ':');
	size_t i;
	size_t total_len = bg_proc == 1 ? 2 : 0; /* 2 == '&' + NUL byte */
	size_t cmd_len = 0;

	for (i = 0; args[i]; i++)
		total_len += strlen(args[i]) + 1; /* 1 == space */

	char *cmd = (char *)xnmalloc(total_len + 1, sizeof(char));

	for (i = 0; args[i]; i++) {
		char *src = (i == 0 && bypass == 1) ? args[i] + 1 : args[i];
		xstrsncpy(cmd + cmd_len, src, total_len + 1);
		cmd_len += strlen(src) + 1;
		cmd[cmd_len - 1] = ' ';
		cmd[cmd_len] = '\0';
	}

	if (bg_proc == 1) {
		cmd[cmd_len] = '&';
		cmd[cmd_len + 1] = '\0';
	} else {
		cmd[cmd_len - 1] = '\0'; /* Remove trailing space */
	}

	return cmd;
}

static inline int
check_shell_cmd_conditions(char **args)
{
	/* No command name ends with a slash */
	size_t len = (args && args[0]) ? strlen(args[0]) : 0;
	if (len > 0 && args[0][len - 1] == '/') {
		xerror("%s: %s: %s\n", conf.autocd == 1 ? "cd" : "open",
			args[0], strerror(ENOENT));
		return conf.autocd == 1 ? EXIT_FAILURE : EXEC_NOTFOUND;
	}

	/* Prevent ungraceful exit */
	if ((*args[0] == 'k' || *args[0] == 'p') && (strcmp(args[0], "kill") == 0
	|| strcmp(args[0], "killall") == 0 || strcmp(args[0], "pkill") == 0)) {
		if (graceful_quit(args) != EXIT_SUCCESS)
			return EXIT_FAILURE;
	}

	if (conf.ext_cmd_ok == 0) {
		xerror(_("%s: External commands are not allowed. "
			"Run 'ext on' to enable them.\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int
run_shell_cmd(char **args)
{
	int ret = check_shell_cmd_conditions(args);
	if (ret != EXIT_SUCCESS)
		return ret;

	char *cmd = construct_shell_cmd(args);

	/* Calling the system shell is vulnerable to command injection, true.
	 * But it is the user here who is directly running the command: this
	 * should not be taken as an untrusted source. */
	int exit_status = launch_execl(cmd);
	free(cmd);

/* For the time being, this is too slow on Cygwin. */
#if !defined(__CYGWIN__)
	reload_binaries();
#endif /* !__CYGWIN__ */

	return exit_status;
}

/* Free everything and exit the program */
static void
_quit(char **args, const int exit_status)
{
	if (!args || !args[0])
		return;

	if (*args[0] == 'Q')
		conf.cd_on_quit = 1;

	if (args[1] && *args[1] == '-' && IS_HELP(args[1])) {
		puts(QUIT_HELP);
		return;
	}

	int i = (int)args_n + 1;
	while (--i >= 0)
		free(args[i]);
	free(args);

	exit(exit_status);
}

static int
set_max_files(char **args)
{
	if (!args[1]) { /* Inform about the current value */
		if (max_files == -1)
			puts(_("Max files: unset"));
		else
			printf(_("Max files: %d\n"), max_files);
		return EXIT_SUCCESS;
	}

	if (IS_HELP(args[1])) { puts(_(MF_USAGE)); return EXIT_SUCCESS;	}

	if (*args[1] == 'u' && strcmp(args[1], "unset") == 0) {
		max_files = -1;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(_("Max files unset\n"));
		return EXIT_SUCCESS;
	}

	if (*args[1] == '0' && !args[1][1]) {
		max_files = 0;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(_("Max files set to %d\n"), max_files);
		return EXIT_SUCCESS;
	}

	long inum = strtol(args[1], NULL, 10);
	if (inum == LONG_MAX || inum == LONG_MIN || inum <= 0) {
		xerror(_("%s: %s: Invalid number\n"), PROGRAM_NAME, args[1]);
		return (exit_code = EXIT_FAILURE);
	}

	max_files = (int)inum;
	if (conf.autols == 1) reload_dirlist();
	print_reload_msg(_("Max files set to %d\n"), max_files);

	return EXIT_SUCCESS;
}

static int
dirs_first_function(const char *arg)
{
	if (conf.autols == 0)
		return EXIT_SUCCESS;

	if (!arg)
		return rl_toggle_dirs_first(0, 0);

	if (IS_HELP(arg)) {
		puts(_(FF_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		printf(_("Directories first is %s\n"),
			conf.list_dirs_first == 1 ? _("enabled") : _("disabled"));
	}  else if (*arg == 'o' && strcmp(arg, "on") == 0) {
		conf.list_dirs_first = 1;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(_("Directories first enabled\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.list_dirs_first = 0;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(_("Directories first disabled\n"));
	}

	return exit_status;
}

static int
filescounter_function(const char *arg)
{
	if (!arg) {
		puts(_(FC_USAGE));
		return EXIT_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "on") == 0) {
		conf.files_counter = 1;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(_("Files counter enabled\n"));
		return EXIT_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.files_counter = 0;
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(_("Files counter disabled\n"));
		return EXIT_SUCCESS;
	}

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		if (conf.files_counter == 1)
			puts(_("The files counter is enabled"));
		else
			puts(_("The files counter is disabled"));
		return EXIT_SUCCESS;
	}

	fprintf(stderr, "%s\n", _(FC_USAGE));
	return EXIT_FAILURE;
}

static int
pager_function(const char *arg)
{
	if (!arg || IS_HELP(arg)) {
		puts(_(PAGER_USAGE));
		return EXIT_SUCCESS;
	}

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		switch (conf.pager) {
		case 0: puts(_("The pager is disabled")); break;
		case 1: puts(_("The pager is enabled")); break;
		default: printf(_("The pager is set to %d\n"), conf.pager); break;
		}

		return EXIT_SUCCESS;
	}

	if (is_number(arg)) {
		int n = atoi(arg);
		if (n == INT_MIN) {
			xerror("%s\n", _("pg: xatoi: Error converting to integer"));
			return EXIT_FAILURE;
		}
		conf.pager = n;
		printf(_("Pager set to %d\n"), n);
		return EXIT_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "on") == 0) {
		conf.pager = 1;
		puts(_("Pager enabled"));
		return EXIT_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.pager = 0;
		puts(_("Pager disabled"));
		return EXIT_SUCCESS;
	}

	fprintf(stderr, "%s\n", _(PAGER_USAGE));
	return EXIT_FAILURE;
}

static int
ext_cmds_function(const char *arg)
{
	if (!arg || IS_HELP(arg)) {
		puts(_(EXT_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;
	if (*arg == 's' && strcmp(arg, "status") == 0) {
		printf(_("External commands are %s\n"),
			conf.ext_cmd_ok ? _("allowed") : _("not allowed"));
	} else if (*arg == 'o' && strcmp(arg, "on") == 0) {
		conf.ext_cmd_ok = 1;
		puts(_("External commands allowed"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.ext_cmd_ok = 0;
		puts(_("External commands disallowed"));
	} else {
		fprintf(stderr, "%s\n", _(EXT_USAGE));
		exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
autocd_function(const char *arg)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(AUTOCD_USAGE));
		return EXIT_FAILURE;
	}

	if (strcmp(arg, "on") == 0) {
		conf.autocd = 1;
		puts(_("Autocd enabled"));
	} else if (strcmp(arg, "off") == 0) {
		conf.autocd = 0;
		puts(_("Autocd disabled"));
	} else if (strcmp(arg, "status") == 0) {
		printf(_("Autocd is %s\n"), conf.autocd == 1
			? _("enabled") : _("disabled"));
	} else if (IS_HELP(arg)) {
		puts(_(AUTOCD_USAGE));
	} else {
		fprintf(stderr, "%s\n", _(AUTOCD_USAGE));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int
auto_open_function(const char *arg)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(AUTO_OPEN_USAGE));
		return EXIT_FAILURE;
	}

	if (strcmp(arg, "on") == 0) {
		conf.auto_open = 1;
		puts(_("Auto-open enabled"));
	} else if (strcmp(arg, "off") == 0) {
		conf.auto_open = 0;
		puts(_("Auto-open disabled"));
	} else if (strcmp(arg, "status") == 0) {
		printf(_("Auto-open is %s\n"), conf.auto_open == 1
			? _("enabled") : _("disabled"));
	} else if (IS_HELP(arg)) {
		puts(_(AUTO_OPEN_USAGE));
	} else {
		fprintf(stderr, "%s\n", _(AUTO_OPEN_USAGE));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static int
columns_function(const char *arg)
{
	if (!arg || IS_HELP(arg)) {
		puts(_(COLUMNS_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	if (*arg == 'o' && arg[1] == 'n' && !arg[2]) {
		conf.columned = 1;
		if (conf.autols == 1) {
			free_dirlist();
			exit_status = list_dir();
		}
		print_reload_msg(_("Columns enabled\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.columned = 0;
		if (conf.autols == 1) {
			free_dirlist();
			exit_status = list_dir();
		}
		print_reload_msg(_("Columns disabled\n"));
	} else {
		fprintf(stderr, "%s\n", _(COLUMNS_USAGE));
		return EXIT_FAILURE;
	}

	return exit_status;
}

static int
icons_function(const char *arg)
{
#ifdef _NO_ICONS
	UNUSED(arg);
	xerror(_("%s: icons: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
	return EXIT_SUCCESS;
#else
	if (!arg || IS_HELP(arg)) {
		puts(_(ICONS_USAGE));
		return EXIT_SUCCESS;
	}

	if (*arg == 'o' && arg[1] == 'n' && !arg[2]) {
		conf.icons = 1;
		int ret = EXIT_SUCCESS;
		if (conf.autols == 1) { free_dirlist(); ret = list_dir(); }
		print_reload_msg(_("Icons enabled\n"));
		return ret;
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		conf.icons = 0;
		int ret = EXIT_SUCCESS;
		if (conf.autols == 1) { free_dirlist(); ret = list_dir(); }
		print_reload_msg(_("Icons disabled\n"));
		return ret;
	} else {
		fprintf(stderr, "%s\n", _(ICONS_USAGE));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
#endif /* _NO_ICONS */
}

static int
msgs_function(const char *arg)
{
	if (arg && IS_HELP(arg)) {
		puts(_(MSG_USAGE));
		return EXIT_SUCCESS;
	}

	if (arg && strcmp(arg, "clear") == 0) {
		if (!msgs_n) {
			printf(_("%s: No messages\n"), PROGRAM_NAME);
			return EXIT_SUCCESS;
		}

		size_t i;
		for (i = 0; i < (size_t)msgs_n; i++)
			free(messages[i]);

		if (conf.autols == 1)
			reload_dirlist();
		print_reload_msg(_("Messages cleared\n"));
		msgs_n = msgs.error = msgs.warning = msgs.notice = 0;
		pmsg = NOMSG;
		return EXIT_SUCCESS;
	}

	if (msgs_n) {
		size_t i;
		for (i = 0; i < (size_t)msgs_n; i++)
			printf("%s", messages[i]);
	} else {
		printf(_("%s: No messages\n"), PROGRAM_NAME);
	}

	return EXIT_SUCCESS;
}

static int
opener_function(const char *arg)
{
	if (!arg) {
		printf("opener: %s\n", conf.opener ? conf.opener : "lira (built-in)");
		return EXIT_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(_(OPENER_USAGE));
		return EXIT_SUCCESS;
	}

	if (conf.opener) {
		free(conf.opener);
		conf.opener = (char *)NULL;
	}

	if (strcmp(arg, "default") != 0 && strcmp(arg, "lira") != 0)
		conf.opener = savestring(arg, strlen(arg));
	printf(_("Opener set to '%s'\n"), conf.opener
		? conf.opener : "lira (built-in)");

	return EXIT_SUCCESS;
}

static int
lightmode_function(const char *arg)
{
	if (!arg)
		return rl_toggle_light_mode(0, 0);

	if (IS_HELP(arg)) {
		puts(LM_USAGE);
		return EXIT_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "on") == 0) {
		if (conf.light_mode == 1) {
			puts(_("Light mode already on"));
			return EXIT_SUCCESS;
		}
		conf.light_mode = 1;
		if (conf.autols == 1)
			reload_dirlist();
		print_reload_msg(_("Light mode enabled\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		if (conf.light_mode == 0) {
			puts(_("Light mode already off"));
			return EXIT_SUCCESS;
		}
		conf.light_mode = 0;
		if (conf.autols == 1)
			reload_dirlist();
		print_reload_msg(_("Light mode disabled\n"));
	} else {
		puts(LM_USAGE);
	}

	return EXIT_SUCCESS;
}

static size_t
get_largest_alias_name(void)
{
	int i = (int)aliases_n;
	size_t largest = 0;
	while (--i >= 0) {
		size_t len = strlen(aliases[i].name);
		if (len > largest)
			largest = len;
	}

	return largest;
}

static int
list_aliases(void)
{
	if (aliases_n == 0) {
		printf(_("%s: No aliases found\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	size_t i, largest = get_largest_alias_name();
	for (i = 0; i < aliases_n; i++) {
		printf("%-*s %s->%s %s\n", (int)largest,
			aliases[i].name, mi_c, df_c, aliases[i].cmd);
	}

	return EXIT_SUCCESS;
}

static int
print_alias(const char *name)
{
	if (!name || !*name)
		return EXIT_FAILURE;

	if (aliases_n == 0) {
		printf(_("%s: No aliases found\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	int i = (int)aliases_n;
	while (--i >= 0) {
		if (aliases[i].name && *name == *aliases[i].name
		&& strcmp(name, aliases[i].name) == 0) {
			printf("alias %s='%s'\n", aliases[i].name,
				aliases[i].cmd ? aliases[i].cmd : 0);
			return EXIT_SUCCESS;
		}
	}

	xerror(_("%s: %s: No such alias\n"), PROGRAM_NAME, name);
	return EXIT_FAILURE;
}

static int
alias_function(char **args)
{
	if (!args[1]) {
		list_aliases();
		return EXIT_SUCCESS;
	}

	if (IS_HELP(args[1])) {
		puts(_(ALIAS_USAGE));
		return EXIT_SUCCESS;
	}

	if (*args[1] == 'i' && strcmp(args[1], "import") == 0) {
		if (!args[2]) {
			puts(_(ALIAS_USAGE));
			return EXIT_SUCCESS;
		}
		return alias_import(args[2]);
	}

	if (*args[1] == 'l' && (strcmp(args[1], "ls") == 0
	|| strcmp(args[1], "list") == 0))
		return list_aliases();

	return print_alias(args[1]);
}

static int
hidden_files_function(const char *arg)
{
	if (!arg)
		return rl_toggle_hidden_files(0, 0);

	if (IS_HELP(arg)) {
		puts(_(HF_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	if (strcmp(arg, "status") == 0) {
		printf(_("Show-hidden-files is %s\n"), conf.show_hidden
			? _("enabled") : _("disabled"));
	} else if (strcmp(arg, "off") == 0) {
		conf.show_hidden = 0;
		if (conf.autols == 1) {
			free_dirlist();
			exit_status = list_dir();
		}
		print_reload_msg(_("Hidden files disabled\n"));
	} else if (strcmp(arg, "on") == 0) {
		conf.show_hidden = 1;
		if (conf.autols == 1)
			reload_dirlist();
		print_reload_msg(_("Hidden files enabled\n"));
	}

	return exit_status;
}

static int
_toggle_exec(char **args)
{
	if (!args[1] || IS_HELP(args[1]))
		{ puts(_(TE_USAGE)); return EXIT_SUCCESS; }

	int exit_status = EXIT_SUCCESS;
	size_t i, n = 0;
	for (i = 1; args[i]; i++) {
		struct stat attr;
		if (strchr(args[i], '\\')) {
			char *tmp = dequote_str(args[i], 0);
			if (tmp) {
				xstrsncpy(args[i], tmp, strlen(tmp) + 1);
				free(tmp);
			}
		}

		if (lstat(args[i], &attr) == -1) {
			xerror("stat: %s: %s\n", args[i], strerror(errno));
			exit_status = EXIT_FAILURE;
			continue;
		}

		if (toggle_exec(args[i], attr.st_mode) == EXIT_FAILURE)
			exit_status = EXIT_FAILURE;
		else
			n++;
	}

	if (n > 0) {
		if (conf.autols == 1 && exit_status == EXIT_SUCCESS)
			reload_dirlist();

		print_reload_msg(_("Toggled executable bit on %zu %s\n"),
			n, n > 1 ? _("files") : _("file"));
	}

	return exit_status;
}

static int
pin_function(char *arg)
{
	if (arg) {
		if (IS_HELP(arg)) {
			puts(PIN_USAGE);
			return EXIT_SUCCESS;
		}
		return pin_directory(arg);
	}

	if (pinned_dir)
		printf(_("Pinned file: %s\n"), pinned_dir);
	else
		puts(_("No pinned file"));

	return EXIT_SUCCESS;
}

static int
props_function(char **args)
{
	if (!args[1] || IS_HELP(args[1])) {
		fprintf(stderr, "%s\n", _(PROP_USAGE));
		return EXIT_SUCCESS;
	}

	int full_dirsize = args[0][1] == 'p'; /* Command is 'pp' */
	return properties_function(args + 1, full_dirsize);
}

static int
ow_function(char **args)
{
#ifndef _NO_LIRA
	if (args[1]) {
		if (IS_HELP(args[1])) {
			puts(_(OW_USAGE));
			return EXIT_SUCCESS;
		}
		return mime_open_with(args[1], args[2] ? args + 2 : NULL);
	}
	puts(_(OW_USAGE));
	return EXIT_SUCCESS;
#else
	UNUSED(args);
	xerror("%s: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	return EXIT_FAILURE;
#endif
}

static int
refresh_function(const int old_exit_code)
{
	refresh_screen();
	return old_exit_code;
}

static int
export_files_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(EXPORT_FILES_USAGE));
		return EXIT_SUCCESS;
	}

	char *ret = _export(args, 1);
	if (ret) {
		printf(_("Files exported to: %s\n"), ret);
		free(ret);
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

static int
_bookmarks_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(BOOKMARKS_USAGE));
		return EXIT_SUCCESS;
	}

	/* Disable keyboard shortcuts. Otherwise, the function will
	 * still be waiting for input while the screen have been taken
	 * by another function */
	kbind_busy = 1;
	/* Disable TAB completion while in Bookmarks */
	rl_attempted_completion_function = NULL;
	int exit_status = bookmarks_function(args);
	/* Reenable TAB completion */
	rl_attempted_completion_function = my_rl_completion;
	/* Reenable keyboard shortcuts */
	kbind_busy = 0;

	return exit_status;
}

static int
desel_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(DESEL_USAGE));
		return EXIT_SUCCESS;
	}

	kbind_busy = 1;
	rl_attempted_completion_function = NULL;
	int exit_status = deselect(args);
	rl_attempted_completion_function = my_rl_completion;
	kbind_busy = 0;

	return exit_status;
}

static int
new_instance_function(char **args)
{
	int exit_status = EXIT_SUCCESS;

	if (args[1]) {
		if (IS_HELP(args[1])) {
			puts(_(X_USAGE));
			return EXIT_SUCCESS;
		} else if (*args[0] == 'x') {
			exit_status = new_instance(args[1], 0);
		} else { /* Run as root */
			exit_status = new_instance(args[1], 1);
		}
	} else {
		/* Run new instance in CWD */
		if (*args[0] == 'x')
			exit_status = new_instance(workspaces[cur_ws].path, 0);
		else
			exit_status = new_instance(workspaces[cur_ws].path, 1);
	}

	return exit_status;
}

static int
reload_function(void)
{
	int exit_status = reload_config();
	conf.welcome_message = 0;

	if (conf.autols == 1) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
		print_reload_msg("Configuration file reloaded\n");
	}

	return exit_status;
}

/* MODE could be either MEDIA_LIST (mp command) or MEDIA_MOUNT (media command) */
static int
media_function(char *arg, const int mode)
{
	if (arg && IS_HELP(arg)) {
		if (mode == MEDIA_LIST)
			puts(_(MOUNTPOINTS_USAGE));
		else
			puts(_(MEDIA_USAGE));
		return EXIT_SUCCESS;
	}

	kbind_busy = 1;
	rl_attempted_completion_function = NULL;
	int exit_status = media_menu(mode);
	rl_attempted_completion_function = my_rl_completion;
	kbind_busy = 0;

	return exit_status;
}

static int
_cd_function(char *arg)
{
	if (arg && IS_HELP(arg)) {
		puts(_(CD_USAGE));
		return EXIT_SUCCESS;
	}

	return cd_function(arg, CD_PRINT_ERROR);
}

static int
_sort_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(SORT_USAGE));
		return EXIT_SUCCESS;
	}

	return sort_function(args);
}

static inline int
check_pinned_file(char **args)
{
	int i = (int)args_n + 1;
	while (--i >= 0) {
		if (*args[i] == ',' && !args[i][1]) {
			xerror(_("%s: No pinned file\n"), PROGRAM_NAME);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

static inline int
check_actions(char **args)
{
	if (actions_n == 0)
		return (-1);

	int i = (int)actions_n;
	while (--i >= 0) {
		if (*args[0] == *usr_actions[i].name
		&& strcmp(args[0], usr_actions[i].name) == 0) {

			// REMOVE ONCE THE DH PLUGIN'S DEPRECATION PERIOD IS OVER
			if (*usr_actions[i].name == 'd' && usr_actions[i].name[1] == 'h'
			&& !usr_actions[i].name[2]) {
				_err('n', PRINT_PROMPT, _("%s: The 'dh' plugin is deprecated. "
					"Use the built-in 'dh' command instead disabling the "
					"'dh' plugin ('actions edit'). Once done, run 'dh --help' "
					"for more information about the new command.\n"),
					PROGRAM_NAME);
			}

			setenv("CLIFM_PLUGIN_NAME", usr_actions[i].name, 1);
			int ret = run_action(usr_actions[i].value, args);
			unsetenv("CLIFM_PLUGIN_NAME");
			return ret;
		}
	}

	return (-1);
}

static int
launch_shell(char **args)
{
	if (!args[0][1]) {
		/* If just ":" or ";", launch the default shell */
		char *cmd[] = {user.shell, NULL};
		if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (args[0][1] == ';' || args[0][1] == ':') {
		/* If double semi colon or colon (or ";:" or ":;") */
		xerror(_("%s: '%s': Syntax error\n"), PROGRAM_NAME, args[0]);
		return EXIT_FAILURE;
	}

	return (-1);
}

static inline void
expand_and_deescape(char **arg, char **deq_str)
{
	if (*(*arg) == '~') {
		char *exp_path = tilde_expand(*arg);
		if (exp_path) {
			free(*arg);
			*arg = exp_path;
		}
	}

	/* Deescape the string (only if file name) */
	if (strchr(*arg, '\\'))
		*deq_str = dequote_str(*arg, 0);
}

static inline int
_open_file(char **args, const int i)
{
	if (conf.autocd && (file_info[i].type == DT_DIR || file_info[i].dir == 1))
		return cd_function(args[0], CD_PRINT_ERROR);

	if (conf.auto_open && (file_info[i].type == DT_REG
	|| file_info[i].type == DT_LNK)) {
		char *cmd[] = {"open", args[0], args[1], NULL};
		return open_function(cmd);
	}

	return (-1);
}

/* Try to autocd or auto-open dir/file
 * Only autocd or auto-open here if not absolute path and if there
 * is no second argument or if second argument is "&"
 * If just 'edit', do not try to open a file named 'edit': always run
 * the 'edit' command */
static inline int
check_auto_first(char **args)
{
	if (!args || !args[0] || !*args[0])
		return (-1);

	if (*args[0] == '/' || (conf.autocd == 0 && conf.auto_open == 0)
	|| (args[1] && (*args[1] != '&' || args[1][1])))
		return (-1);

	if (*args[0] == 'e' && strcmp(args[0], "edit") == 0)
		return (-1);

	char *deq_str = (char *)NULL;
	if (conf.autocd == 1 || conf.auto_open == 1)
		expand_and_deescape(&args[0], &deq_str);

	char *tmp = deq_str ? deq_str : args[0];
	size_t len = strlen(tmp);
	int rem_slash = 0;
	if (len > 0 && tmp[len - 1] == '/') {
		rem_slash = 1;
		tmp[len - 1] = '\0';
	}

	if (conf.autocd == 1 && cdpath_n > 0 && !args[1]
	&& cd_function(tmp, CD_NO_PRINT_ERROR) == EXIT_SUCCESS) {
		free(deq_str);
		return EXIT_SUCCESS;
	}

	int i = (int)files;
	while (--i >= 0) {
		if (*tmp != *file_info[i].name || strcmp(tmp, file_info[i].name) != 0)
			continue;

		free(deq_str);
		deq_str = (char *)NULL;
		int ret = _open_file(args, i);
		if (ret != -1)
			return ret;
		break;
	}

	if (rem_slash == 1)
		tmp[len - 1] = '/';

	free(deq_str);
	return (-1);
}

static inline int
auto_open_file(char **args, char *tmp)
{
	char *cmd[] = {"open", tmp, (args_n >= 1) ? args[1]
		: NULL, (args_n >= 2) ? args[2] : NULL, NULL};
	args_n++;
	int ret = open_function(cmd);
	args_n--;
	free(tmp);

	return ret;
}

static inline int
autocd_dir(char *tmp)
{
	int ret = EXIT_SUCCESS;

	if (conf.autocd) {
		ret = cd_function(tmp, CD_PRINT_ERROR);
	} else {
		xerror(_("%s: cd: %s: Is a directory\n"), PROGRAM_NAME, tmp);
		ret = EISDIR;
	}
	free(tmp);

	return ret;
}

/* Try to autocd or auto-open */
/* If there is a second word (parameter, ARGS[1]) and this word starts
 * with a dash, do not take the first word (ARGS[0]) as a file to be
 * opened, but as a command to be executed */
static inline int
check_auto_second(char **args)
{
	if (args[1] && *args[1] == '-')
		return (-1);

	char *tmp = savestring(args[0], strlen(args[0]));

	if (strchr(tmp, '\\')) {
		char *dstr = dequote_str(tmp, 0);
		if (dstr) {
			free(tmp);
			tmp = dstr;
		}
	}

	if (conf.autocd == 1 && cdpath_n > 0 && !args[1]) {
		int ret = cd_function(tmp, CD_NO_PRINT_ERROR);
		if (ret == EXIT_SUCCESS)
			{ free(tmp); return EXIT_SUCCESS; }
	}

	struct stat attr;
	if (stat(tmp, &attr) != 0)
		{ free(tmp); return (-1); }

	if (conf.autocd == 1 && S_ISDIR(attr.st_mode) && !args[1])
		return autocd_dir(tmp);

	/* Regular, non-executable file, or exec file not in PATH
	 * not ./file and not /path/to/file */
	if (conf.auto_open == 1 && S_ISREG(attr.st_mode)
	&& (!(attr.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
	|| (!is_bin_cmd(tmp) && !(*tmp == '.' && *(tmp + 1) == '/')
	&& *tmp != '/' ) ) )
		return auto_open_file(args, tmp);

	free(tmp);
	return (-1);
}

static int
colors_function(char *arg)
{
	if (arg && IS_HELP(arg))
		puts(_(COLORS_USAGE));
	else
		color_codes();
	return EXIT_SUCCESS;
}

static int
ls_function(void)
{
	free_dirlist();
	int ret = list_dir();
	get_sel_files();

	return ret;
}

static int
lira_function(char **args)
{
#ifndef _NO_LIRA
	return mime_open(args);
#else
	UNUSED(args);
	xerror("%s: lira: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	return EXIT_FAILURE;
#endif
}

static inline int
check_comments(const char *name)
{
	if (*name != '#')
		return EXIT_FAILURE;

	/* Skip lines starting with '#' if there is no such file name
	 * in the current directory. This implies that no command starting
	 * with '#' will be executed */
	struct stat a;
	if (lstat(name, &a) == -1)
		return EXIT_SUCCESS;

	if (conf.autocd == 1 && S_ISDIR(a.st_mode))
		return EXIT_FAILURE;

	if (conf.auto_open == 1 && !S_ISDIR(a.st_mode))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
print_stats(void)
{
	if (conf.light_mode == 1)
		puts(_("Running in light mode: Some files statistics are not available\n"));

	printf(_("Total files:                 %zu\n\
Directories:                 %zu\n\
Regular files:               %zu\n\
Executable files:            %zu\n\
Hidden files:                %zu\n\
SUID files:                  %zu\n\
SGID files:                  %zu\n\
Files w/capabilities:        %zu\n\
FIFO/pipes:                  %zu\n\
Sockets:                     %zu\n\
Block devices:               %zu\n\
Character devices:           %zu\n\
Symbolic links:              %zu\n\
Broken symbolic links:       %zu\n\
Multi-link files:            %zu\n\
Files w/extended attributes: %zu\n\
Other-writable files:        %zu\n\
Sticky files:                %zu\n\
Unknown file types:          %zu\n\
Unstatable files:            %zu\n\
"),
	files, stats.dir, stats.reg, stats.exec, stats.hidden, stats.suid,
	stats.sgid, stats.caps, stats.fifo, stats.socket, stats.block_dev,
	stats.char_dev, stats.link, stats.broken_link, stats.multi_link,
	stats.extended, stats.other_writable, stats.sticky, stats.unknown,
	stats.unstat);

#ifdef __sun
	printf("Doors:                 %zu\n", stats.door);
#endif /* __sun */

	return EXIT_SUCCESS;
}

static int
_trash_function(char **args, int *_cont)
{
#ifndef _NO_TRASH
	if (args[1] && IS_HELP(args[1])) {
		puts(_(TRASH_USAGE));
		*_cont = 0;
		return EXIT_SUCCESS;
	}

	int exit_status = trash_function(args);

	if (is_sel) { /* If 'tr sel', deselect everything */
		int i = (int)sel_n;
		while (--i >= 0)
			free(sel_elements[i].name);
		sel_n = 0;
		if (save_sel() != 0)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
#else
	UNUSED(args);
	xerror(_("%s: trash: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
	*_cont = 0;
	return EXIT_FAILURE;
#endif /* !_NO_TRASH */
}

static int
_untrash_function(char **args, int *_cont)
{
#ifndef _NO_TRASH
	if (args[1] && IS_HELP(args[1])) {
		puts(_(UNTRASH_USAGE));
		*_cont = 0;
		return EXIT_SUCCESS;
	}

	kbind_busy = 1;
	rl_attempted_completion_function = NULL;
	int exit_status = untrash_function(args);
	rl_attempted_completion_function = my_rl_completion;
	kbind_busy = 0;

	return exit_status;
#else
	UNUSED(args);
	xerror("%s: trash: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	*_cont = 0;
	return EXIT_FAILURE;
#endif /* !_NO_TRASH */
}

static int
toggle_full_dir_size(const char *arg)
{
	if (!arg || !*arg || IS_HELP(arg)) {
		puts(_(FZ_USAGE));
		return EXIT_SUCCESS;
	}

	if (*arg != 'o') {
		xerror(_("%s: %s: Invalid argument. Try 'fz -h'\n"), PROGRAM_NAME, arg);
		return EXIT_FAILURE;
	}

	if (*(arg + 1) == 'n' && !*(arg + 2)) {
		if (conf.full_dir_size == 1) {
			puts(_("Full directory size is already enabled"));
		} else {
			conf.full_dir_size = 1;
			if (conf.autols == 1) reload_dirlist();
			print_reload_msg(_("Full directory size enabled\n"));
		}
		return EXIT_SUCCESS;
	}

	if (*(arg + 1) == 'f' && *(arg + 2) == 'f' && !*(arg + 3)) {
		if (conf.full_dir_size == 0) {
			puts(_("Full directory size is already disabled"));
		} else {
			conf.full_dir_size = 0;
			if (conf.autols == 1) reload_dirlist();
			print_reload_msg(_("Full directory size disabled\n"));
		}
		return EXIT_SUCCESS;
	}

	xerror(_("%s: %s: Invalid argument. Try 'fz -h'\n"), PROGRAM_NAME, arg);
	return EXIT_FAILURE;
}

static void
set_cp_cmd(char **cmd, const int cp_force)
{
	int bk_cp_cmd = conf.cp_cmd;
	if (cp_force == 1) {
		if (conf.cp_cmd == CP_ADVCP)
			conf.cp_cmd = CP_ADVCP_FORCE;
		else if (conf.cp_cmd == CP_CP)
			conf.cp_cmd = CP_CP_FORCE;
	}

	size_t dst_len = 0;

	switch (conf.cp_cmd) {
	case CP_ADVCP:
		dst_len = strlen(_DEF_ADVCP_CMD);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_ADVCP_CMD, dst_len + 1);
		break;
	case CP_ADVCP_FORCE:
		dst_len = strlen(_DEF_ADVCP_CMD_FORCE);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_ADVCP_CMD_FORCE, dst_len + 1);
		break;
	case CP_WCP:
		dst_len = strlen(_DEF_WCP_CMD);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_WCP_CMD, dst_len + 1);
		break;
	case CP_RSYNC:
		dst_len = strlen(_DEF_RSYNC_CMD);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_RSYNC_CMD, dst_len + 1);
		break;
	case CP_CP_FORCE:
		dst_len = strlen(_DEF_CP_CMD_FORCE);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_CP_CMD_FORCE, dst_len + 1);
		break;
	case CP_CP: /* fallthrough */
	default:
		dst_len = strlen(_DEF_CP_CMD);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_CP_CMD, dst_len + 1);
		break;
	}

	conf.cp_cmd = bk_cp_cmd;
}

static void
set_mv_cmd(char **cmd, const int mv_force)
{
	int bk_mv_cmd = conf.mv_cmd;
	if (mv_force == 1) {
		if (conf.mv_cmd == MV_ADVMV)
			conf.mv_cmd = MV_ADVMV_FORCE;
		else if (conf.mv_cmd == MV_MV)
			conf.mv_cmd = MV_MV_FORCE;
	}

	size_t dst_len = 0;

	switch (conf.mv_cmd) {
	case MV_ADVMV:
		dst_len = strlen(_DEF_ADVMV_CMD);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_ADVMV_CMD, dst_len + 1);
		break;
	case MV_ADVMV_FORCE:
		dst_len = strlen(_DEF_ADVMV_CMD);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_ADVMV_CMD_FORCE, dst_len + 1);
		break;
	case MV_MV_FORCE:
		dst_len = strlen(_DEF_MV_CMD_FORCE);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_MV_CMD_FORCE, dst_len + 1);
		break;
	case MV_MV: /* fallthrough */
	default:
		dst_len = strlen(_DEF_MV_CMD);
		*cmd = (char *)xrealloc(*cmd, (dst_len + 1) * sizeof(char));
		xstrsncpy(*cmd, _DEF_MV_CMD, dst_len + 1);
		break;
	}

	conf.mv_cmd = bk_mv_cmd;
}

/* Let's check we have not left any zombie process behind. This happens
 * whenever we run a process in the background via launch_execv. */
static void
check_zombies(void)
{
	int status = 0;
	if (waitpid(-1, &status, WNOHANG) > 0)
		zombies--;
}

/* Return 1 if STR is a path, zero otherwise. */
static int
is_path(char *str)
{
	if (!str || !*str)
		return 0;
	if (strchr(str, '\\')) {
		char *p = dequote_str(str, 0);
		if (!p)
			return 0;
		int ret = access(p, F_OK);
		free(p);
		if (ret != 0)
			return 0;
	} else {
		if (access(str, F_OK) != 0)
			return 0;
	}

	return 1;
}

/* Replace "\S" by "S" in the memory address pointed to by s */
static void
remove_backslash(char **s)
{
	if (!*s || !*(*s))
		return;

	char *p = *s;

	while (*(*s)) {
		*(*s) = *(*s + 1);
		(*s)++;
	}

	*(*s) = '\0';

	*s = p;
}

static int
preview_edit(char *app)
{
	if (!config_dir)
		return EXIT_FAILURE;

	size_t len = config_dir_len + 15;
	char *file = (char *)xnmalloc(len, sizeof(char));
	snprintf(file, len, "%s/preview.clifm", config_dir);

	int ret = EXIT_SUCCESS;
	if (app) {
		char *cmd[] = { app, file, NULL };
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	} else {
		ret = open_file(file);
	}

	free(file);
	return ret;
}

static int
preview_function(char **args)
{
#ifdef _NO_FZF
	xerror("%s: view: fzf: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	return EXIT_FAILURE;
#endif /* _NO_FZF */

	if (args && args[0]) {
		if (IS_HELP(args[0])) {
			puts(VIEW_USAGE);
			return EXIT_SUCCESS;
		}
		if (*args[0] == 'e' && strcmp(args[0], "edit") == 0)
			return preview_edit(args[1]);
	}

	size_t seln_bk = sel_n;

	int fzf_preview_bk = conf.fzf_preview;
	enum tab_mode tabmode_bk = tabmode;

	if (tabmode != FZF_TAB) {
		char *p = get_cmd_path("fzf");
		if (!p) {
			_err(0, NOPRINT_PROMPT, "%s: fzf: %s\n", PROGRAM_NAME, NOTFOUND_MSG);
			return EXEC_NOTFOUND; /* 127, as required by exit(1) */
		}
		free(p);
	}

	conf.fzf_preview = 1;
	tabmode = FZF_TAB;

	rl_delete_text(0, rl_end);
	rl_point = rl_end = 0;
	rl_redisplay();

	flags |= PREVIEWER;
	tab_complete('?');
	flags &= ~PREVIEWER;

	tabmode = tabmode_bk;
	conf.fzf_preview = fzf_preview_bk;

	if (sel_n > seln_bk) {
		save_sel();
		get_sel_files();
	}

	if (conf.autols == 1) {
		putchar('\n');
		reload_dirlist();
	}
#if RL_READLINE_VERSION >= 0x0700
	else { /* Only available since readline 7.0 */
		rl_clear_visible_line();
	}
#endif

	if (sel_n > seln_bk) {
		print_reload_msg(_("%zu file(s) selected\n"), sel_n - seln_bk);
		print_reload_msg(_("%zu total selected file(s)\n"), sel_n);
	}

	return EXIT_SUCCESS;
}

static int
dirhist_function(char *dir)
{
	if (!dir || !*dir) {
		print_dirhist(NULL);
		return EXIT_SUCCESS;
	}

	if (IS_HELP(dir)) {
		puts(DH_USAGE);
		return EXIT_SUCCESS;
	}

	if (*dir == '!' && is_number(dir + 1)) {
		int n = atoi(dir + 1);
		if (n <= 0 || n > dirhist_total_index) {
			xerror(_("dh: %d: No such entry number\n"), n);
			return EXIT_FAILURE;
		}

		n--;
		if (!old_pwd[n] || *old_pwd[n] == _ESC) {
			xerror("%s\n", _("dh: Invalid history entry"));
			return EXIT_FAILURE;
		}

		return cd_function(old_pwd[n], CD_PRINT_ERROR);
	}

	if (*dir != '/' || !strchr(dir + 1, '/')) {
		print_dirhist(dir);
		return EXIT_SUCCESS;
	}

	return cd_function(dir, CD_PRINT_ERROR);
}

static int
long_view_function(const char *arg)
{
	if (!arg)
		return rl_toggle_long_view(0, 0);

	if (IS_HELP(arg) || (strcmp(arg, "on") != 0 && strcmp(arg, "off") != 0)) {
		puts(LL_USAGE);
		return EXIT_SUCCESS;
	}

	conf.long_view = arg[1] == 'n' ? 1 : 0;
	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(_("Long view %s\n"), arg[1] == 'n'
		? _("enabled") : _("disabled"));

	return EXIT_SUCCESS;
}

/* Remove variables specificied in VAR from the environment */
static int
unset_function(char **var)
{
	if (!var || !var[0]) {
		printf("unset: A variable name is required\n");
		return EXIT_SUCCESS;
	}

	if (IS_HELP(var[0])) {
		puts(UNSET_USAGE);
		return EXIT_SUCCESS;
	}

	int status = EXIT_SUCCESS;
	size_t i;
	for (i = 0; var[i]; i++) {
		if (unsetenv(var[i]) == -1) {
			status = errno;
			xerror("unset: %s: %s\n", var[i], strerror(errno));
		}
	}

	return status;
}

static int
run_log_cmd(char **args)
{
	if (config_ok == 0) {
		xerror(_("%s: Log function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!args[0] || IS_HELP(args[0])) {
		puts(LOG_USAGE);
		return EXIT_SUCCESS;
	}

	if (*args[0] == 'c' && strcmp(args[0], "cmd") == 0) {
		if (!args[1] || strcmp(args[1], "list") == 0)
			return print_logs(CMD_LOGS);

		if (*args[1] == 's' && strcmp(args[1], "status") == 0) {
			printf(_("log: Command logs are %s\n"), (conf.log_cmds == 1)
				? _("enabled") : _("disabled"));
			return EXIT_SUCCESS;
		}

		if (*args[1] == 'o' && strcmp(args[1], "on") == 0) {
			conf.log_cmds = 1;
			puts(_("log: Command logs enabled"));
			return EXIT_SUCCESS;
		}

		if (*args[1] == 'o' && strcmp(args[1], "off") == 0) {
			conf.log_cmds = 0;
			puts(_("log: Command logs disabled"));
			return EXIT_SUCCESS;
		}

		if (*args[1] == 'c' && strcmp(args[1], "clear") == 0) {
			int ret = clear_logs(CMD_LOGS);
			if (ret == EXIT_SUCCESS)
				printf("log: Command logs cleared\n");
			return ret;
		}
	}

	if (*args[0] == 'm' && strcmp(args[0], "msg") == 0) {
		if (!args[1] || strcmp(args[1], "list") == 0)
			return print_logs(MSG_LOGS);

		if (*args[1] == 's' && strcmp(args[1], "status") == 0) {
			printf(_("log: Message logs are %s\n"), (conf.log_msgs == 1)
				? _("enabled") : _("disabled"));
			return EXIT_SUCCESS;
		}

		if (*args[1] == 'o' && strcmp(args[1], "on") == 0) {
			conf.log_msgs = 1;
			puts(_("log: Message logs enabled"));
			return EXIT_SUCCESS;
		}

		if (*args[1] == 'o' && strcmp(args[1], "off") == 0) {
			conf.log_msgs = 0;
			puts(_("log: Message logs disabled"));
			return EXIT_SUCCESS;
		}

		if (*args[1] == 'c' && strcmp(args[1], "clear") == 0) {
			int ret = clear_logs(MSG_LOGS);
			if (ret == EXIT_SUCCESS)
				puts(_("log: Message logs cleared"));
			return ret;
		}
	}

	fprintf(stderr, "%s\n", LOG_USAGE);
	return EXIT_FAILURE;
}

/* Take the command entered by the user, already splitted into substrings
 * by parse_input_str(), and call the corresponding function. Return zero
 * in case of success and one in case of error

 * Exit flag. exit_code is zero (sucess) by default. In case of error
 * in any of the functions below, it will be set to one (failure).
 * This will be the value returned by this function. Used by the \z
 * escape code in the prompt to print the exit status of the last
 * executed command */
int
exec_cmd(char **comm)
{
	if (zombies > 0)
		check_zombies();
	fputs(df_c, stdout);

	int old_exit_code = exit_code;
	exit_code = EXIT_SUCCESS;

	/* Remove backslash in front of command names: used to bypass alias names */
	if (*comm[0] == '\\' && *(comm[0] + 1))
		remove_backslash(&comm[0]);

	/* Skip comments */
	if (check_comments(comm[0]) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	/* Warn when using the ',' keyword and there's no pinned file */
	if (check_pinned_file(comm) == EXIT_FAILURE)
		return (exit_code = EXIT_FAILURE);

	/* User defined actions */
	if ((exit_code = check_actions(comm)) != -1)
		return exit_code;

	/* User defined variables */
	if (flags & IS_USRVAR_DEF) {
		flags &= ~IS_USRVAR_DEF;
		return (exit_code = create_usr_var(comm[0]));
	}

	/* Check if we need to send input directly to the system shell */
	if (*comm[0] == ';' || *comm[0] == ':')
		if ((exit_code = launch_shell(comm)) != -1)
			return exit_code;

	/* # AUTOCD & AUTO-OPEN (1) # */
	/* rl_dispatching is set to 1 if coming from a keybind: we have a
	 * command, not a file name. So, skip this check */
	if (rl_dispatching == 0 && (exit_code = check_auto_first(comm)) != -1)
		return exit_code;

	exit_code = EXIT_SUCCESS;

	/* The more often a function is used, the more on top should it be
	 * in this if...else chain. It will be found faster this way. */

	/* ####################################################
	 * #                 BUILTIN COMMANDS                 #
	 * ####################################################*/

	/*          ############### CD ##################     */
	if (*comm[0] == 'c' && comm[0][1] == 'd' && !comm[0][2])
		return (exit_code = _cd_function(comm[1]));

	/*         ############### OPEN ##################     */
	else if (*comm[0] == 'o' && (!comm[0][1] || strcmp(comm[0], "open") == 0))
		return (exit_code = open_function(comm));

	/*         ############## BACKDIR ##################     */
	else if (*comm[0] == 'b' && comm[0][1] == 'd' && !comm[0][2])
		return (exit_code = backdir(comm[1]));

	/*      ################ OPEN WITH ##################     */
	else if (*comm[0] == 'o' && comm[0][1] == 'w' && !comm[0][2])
		return (exit_code = ow_function(comm));

	/*   ################ DIRECTORY JUMPER ##################     */
	else if (*comm[0] == 'j' && (!comm[0][1] || ((comm[0][1] == 'c'
//	|| comm[0][1] == 'p' || comm[0][1] == 'e' || comm[0][1] == 'o'
	|| comm[0][1] == 'p' || comm[0][1] == 'e'
	|| comm[0][1] == 'l') && !comm[0][2])))
		return (exit_code = dirjump(comm, NO_SUG_JUMP));

	/*       ############### REFRESH ##################     */
	else if (*comm[0] == 'r' && ((comm[0][1] == 'f' && !comm[0][2])
	|| strcmp(comm[0], "refresh") == 0))
		return (exit_code = refresh_function(old_exit_code));

	/*     ################# BOOKMARKS ##################     */
	else if (*comm[0] == 'b' && ((comm[0][1] == 'm' && !comm[0][2])
	|| strcmp(comm[0], "bookmarks") == 0))
		return (exit_code = _bookmarks_function(comm));

	/*  ############# BACK, FORTH, and DH (dirhist) ##################     */
	else if (*comm[0] == 'b' && (!comm[0][1] || strcmp(comm[0], "back") == 0))
		return (exit_code = back_function(comm));

	else if (*comm[0] == 'f' && (!comm[0][1] || strcmp(comm[0], "forth") == 0))
		return (exit_code = forth_function(comm));

	else if ((*comm[0] == 'b' || *comm[0] == 'f' || *comm[0] == 'd')
	&& comm[0][1] == 'h' && !comm[0][2])
		return (exit_code = dirhist_function(comm[1]));

	/*    ############### BULK REMOVE ##################     */
	else if (*comm[0] == 'r' && comm[0][1] == 'r' && !comm[0][2])
		exit_code = bulk_remove(comm[1], comm[1] ? comm[2] : NULL);

	/*     ################# TAGS ##################     */
	else if (*comm[0] == 't'
	&& (((comm[0][1] == 'a' || comm[0][1] == 'd' || comm[0][1] == 'l'
	|| comm[0][1] == 'm' || comm[0][1] == 'n' || comm[0][1] == 'u'
	|| comm[0][1] == 'y') && !comm[0][2]) || strcmp(comm[0], "tag") == 0))
#ifndef _NO_TAGS
		exit_code = tags_function(comm);
#else
	{
		xerror("%s: tag: %s\n", PROGRAM_NAME, NOT_AVAILABLE);
		return EXIT_FAILURE;
	}
#endif /* !_NO_TAGS */

	/*     ################# NEW FILE ##################     */
	else if (*comm[0] == 'n' && (!comm[0][1] || strcmp(comm[0], "new") == 0))
		exit_code = create_files(comm + 1);

	/*     ############### DUPLICATE FILE ##################     */
	else if (*comm[0] == 'd' && (!comm[0][1] || strcmp(comm[0], "dup") == 0))
		exit_code = dup_file(comm);

#if defined(__HAIKU__) || defined(__CYGWIN__)
	else if ((*comm[0] == 'c' || *comm[0] == 'r' || *comm[0] == 'm'
	|| *comm[0] == 't' || *comm[0] == 'u' || *comm[0] == 'l')
	&& (strcmp(comm[0], "cp") == 0 || strcmp(comm[0], "rm") == 0
	|| strcmp(comm[0], "mkdir") == 0 || strcmp(comm[0], "unlink") == 0
	|| strcmp(comm[0], "touch") == 0 || strcmp(comm[0], "ln") == 0
	|| strcmp(comm[0], "chmod") == 0))
		return (exit_code = run_and_refresh(comm, 0));
#endif /* __HAIKU__ || __CYGWIN__ */

	/*     ############### COPY AND MOVE ##################     */
	/* c, m, v, vv, or p, paste commands */
	else if ((*comm[0] == 'c' && !comm[0][1])

	|| (*comm[0] == 'm' && !comm[0][1])

	|| (*comm[0] == 'v' && (!comm[0][1] || (comm[0][1] == 'v'
	&& !comm[0][2])))

	|| (*comm[0] == 'p' && strcmp(comm[0], "paste") == 0)) {
		int copy_and_rename = 0, use_force = 0;

		/* c, v, vv, and paste commands */
		if (((*comm[0] == 'c' || *comm[0] == 'v') && !comm[0][1])
		|| (*comm[0] == 'v' && comm[0][1] == 'v' && !comm[0][2])
		|| strcmp(comm[0], "paste") == 0) {

			if (comm[1] && IS_HELP(comm[1])) {
				if (*comm[0] == 'v' && comm[0][1] == 'v' && !comm[0][2])
					puts(_(VV_USAGE));
				else
					puts(_(WRAPPERS_USAGE));
				return EXIT_SUCCESS;
			}

			if (*comm[0] == 'v' && comm[0][1] == 'v' && !comm[0][2])
				copy_and_rename = 1;

			use_force = is_force_param(comm[1]);
			set_cp_cmd(&comm[0], use_force);

		} else if (*comm[0] == 'm' && !comm[0][1]) {
			if (comm[1] && IS_HELP(comm[1])) {
				puts(_(WRAPPERS_USAGE));
				return EXIT_SUCCESS;
			}
			if (!sel_is_last && comm[1] && !comm[2])
				xrename = 1;

			use_force = is_force_param(comm[1]);
			set_mv_cmd(&comm[0], use_force);
		}

		kbind_busy = 1;
		exit_code = cp_mv_file(comm, copy_and_rename, use_force);
		kbind_busy = 0;
	}

	/*         ############# (UN)TRASH ##################     */
	else if (*comm[0] == 't' && (!comm[0][1] || strcmp(comm[0], "tr") == 0
	|| strcmp(comm[0], "trash") == 0)) {
		int _cont = 1;
		exit_code = _trash_function(comm, &_cont);
		if (_cont == 0)
			return exit_code;
	}
	
	else if (*comm[0] == 'u' && (!comm[0][1] || strcmp(comm[0], "undel") == 0
	|| strcmp(comm[0], "untrash") == 0)) {
		int _cont = 1; /* Tells whether to continue or return right here */
		exit_code = _untrash_function(comm, &_cont);
		if (_cont == 0)
			return exit_code;
	}

	/*     ############### SELECTION ##################     */
	else if (*comm[0] == 's' && (!comm[0][1] || strcmp(comm[0], "sel") == 0)) {
		return (exit_code = sel_function(comm));
	}

	else if (*comm[0] == 's' && (strcmp(comm[0], "sb") == 0
	|| strcmp(comm[0], "selbox") == 0)) {
		show_sel_files(); return EXIT_SUCCESS;
	}

	else if (*comm[0] == 'd' && (strcmp(comm[0], "ds") == 0
	|| strcmp(comm[0], "desel") == 0))
		return (exit_code = desel_function(comm));

	else if (*comm[0] == 'l' && !comm[0][1]) {
		exit_code = symlink_file(comm + 1);
		goto CHECK_EVENTS;
	}

	else if (*comm[0] == 'l' && comm[0][1] == 'e' && !comm[0][2]) {
		exit_code = edit_link(comm[1]);
		goto CHECK_EVENTS;
	}

	/*  ############# SOME SHELL CMD WRAPPERS ###############  */
	else if (strcmp(comm[0], "r") == 0 || strcmp(comm[0], "md") == 0) {
		/* This help is only for c, m, r, and md commands */
		if (comm[1] && IS_HELP(comm[1])) {
			puts(_(WRAPPERS_USAGE)); return EXIT_SUCCESS;
		}

		if (*comm[0] == 'r' && !comm[0][1]) {
			exit_code = remove_file(comm);
			goto CHECK_EVENTS;
		}

		if (*comm[0] == 'm' && comm[0][1] == 'd' && !comm[0][2]) {
			comm[0] = (char *)xrealloc(comm[0], 9 * sizeof(char));
			/* -p is POSIX: it should be there for all mkdir implementations. */
			xstrsncpy(comm[0], "mkdir -p", 9);

			kbind_busy = 1;
			exit_code = run_and_refresh(comm, 0);
			kbind_busy = 0;
		}
	}

	/*    ########### TOGGLE LONG VIEW ##################     */
	else if (*comm[0] == 'l' && (comm[0][1] == 'v' || comm[0][1] == 'l')
	&& !comm[0][2])
		return (exit_code = long_view_function(comm[1]));

	/*    ############# PREVIEW FILES ##################     */
	else if (*comm[0] == 'v' && strcmp(comm[0], "view") == 0)
		return (exit_code = preview_function(comm + 1));

	/*    ############## TOGGLE EXEC ##################     */
	else if (*comm[0] == 't' && comm[0][1] == 'e' && !comm[0][2])
		return (exit_code = _toggle_exec(comm));

	/*    ########### OWNERSHIP CHANGER ###############     */
	else if (*comm[0] == 'o' && comm[0][1] == 'c' && !comm[0][2])
		return (exit_code = set_file_owner(comm));

	/*    ########### PERMISSIONS CHANGER ###############     */
	else if (*comm[0] == 'p' && comm[0][1] == 'c' && !comm[0][2])
		return (exit_code = set_file_perms(comm));

	/*    ############### (UN)PIN FILE ##################     */
	else if (*comm[0] == 'p' && strcmp(comm[0], "pin") == 0)
		return (exit_code = pin_function(comm[1]));

	else if (*comm[0] == 'u' && strcmp(comm[0], "unpin") == 0)
		return (exit_code = unpin_dir());

	/*    ############### PROMPT ##################     */
	else if (*comm[0] == 'p' && strcmp(comm[0], "prompt") == 0)
		return (exit_code = prompt_function(comm + 1));

	/*    ############### PROPERTIES ##################     */
	else if (*comm[0] == 'p' && (!comm[0][1] || strcmp(comm[0], "pr") == 0
	|| strcmp(comm[0], "pp") == 0 || strcmp(comm[0], "prop") == 0))
		return (exit_code = props_function(comm));

	/*     ############### SEARCH ##################     */
	else if ( (*comm[0] == '/' && is_path(comm[0]) == 0)
	|| (*comm[0] == '/' && !comm[0][1] && comm[1] && *comm[1] == '-'
	&& IS_HELP(comm[1])) )
		return (exit_code = search_function(comm));

	/*      ############## HISTORY ##################     */
	else if (*comm[0] == '!' && comm[0][1] != ' ' && comm[0][1] != '\t'
	&& comm[0][1] != '\n' && comm[0][1] != '=' && comm[0][1] != '(') {
		if (comm[1] && IS_HELP(comm[1])) {
			printf("%s\n", HISTEXEC_USAGE);
			return EXIT_SUCCESS;
		}
		exit_code = run_history_cmd(comm[0] + 1);
	}

	/*    ############### BATCH LINK ##################     */
	else if (*comm[0] == 'b' && comm[0][1] == 'l' && !comm[0][2])
		exit_code = batch_link(comm);

	/*    ############### BULK RENAME ##################     */
	else if (*comm[0] == 'b' && ((comm[0][1] == 'r' && !comm[0][2])
	|| strcmp(comm[0], "bulk") == 0))
		exit_code = bulk_rename(comm);

	/*      ################ SORT ##################     */
	else if (*comm[0] == 's' && ((comm[0][1] == 't' && !comm[0][2])
	|| strcmp(comm[0], "sort") == 0))
		return (exit_code = _sort_function(comm));

	/*    ############ FILE NAMES CLEANER ############## */
	else if (*comm[0] == 'b' && ((comm[0][1] == 'b' && !comm[0][2])
	|| strcmp(comm[0], "bleach") == 0)) {
#ifndef _NO_BLEACH
		exit_code = bleach_files(comm);
#else
		xerror("%s: bleach: %s\n", PROGRAM_NAME, NOT_AVAILABLE);
		return EXIT_FAILURE;
#endif /* !_NO_BLEACH */
	}

	/*   ################ ARCHIVER ##################     */
	else if (*comm[0] == 'a' && ((comm[0][1] == 'c' || comm[0][1] == 'd')
	&& !comm[0][2])) {
#ifndef _NO_ARCHIVING
		if (!comm[1] || IS_HELP(comm[1])) {
			puts(_(ARCHIVE_USAGE));
			return EXIT_SUCCESS;
		}
                                  /* Either 'c' or 'd' */
		exit_code = archiver(comm, comm[0][1]);
#else
		xerror("%s: archiver: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
		return EXIT_FAILURE;
#endif /* !_NO_ARCHIVING */
	}

	/* ##################################################
	 * #                 MINOR FUNCTIONS                #
	 * ##################################################*/

	else if (*comm[0] == 'w' && comm[0][1] == 's' && !comm[0][2])
		return (exit_code = handle_workspaces(comm + 1));

	else if (*comm[0] == 's' && strcmp(comm[0], "stats") == 0)
		return (exit_code = print_stats());

	else if (*comm[0] == 'f' && ((comm[0][1] == 't' && !comm[0][2])
	|| strcmp(comm[0], "filter") == 0))
		return (exit_code = filter_function(comm[1]));

	else if (*comm[0] == 'f' && comm[0][1] == 'z' && !comm[0][2])
		return (exit_code = toggle_full_dir_size(comm[1]));

	else if (*comm[0] == 'c' && ((comm[0][1] == 'l' && !comm[0][2])
	|| strcmp(comm[0], "columns") == 0))
		return (exit_code = columns_function(comm[1]));

	else if (*comm[0] == 'i' && strcmp(comm[0], "icons") == 0)
		return (exit_code = icons_function(comm[1]));

	else if (*comm[0] == 'c' && ((comm[0][1] == 's' && !comm[0][2])
	|| strcmp(comm[0], "colorschemes") == 0))
		return (exit_code = cschemes_function(comm));

	else if (*comm[0] == 'k' && ((comm[0][1] == 'b' && !comm[0][2])
	|| strcmp(comm[0], "keybinds") == 0))
		return (exit_code = kbinds_function(comm));

	else if (*comm[0] == 'e' && strcmp(comm[0], "exp") == 0)
		return (exit_code = export_files_function(comm));

	else if (*comm[0] == 'o' && strcmp(comm[0], "opener") == 0)
		return (exit_code = opener_function(comm[1]));

	else if (*comm[0] == 't' && strcmp(comm[0], "tips") == 0)
		{ print_tips(1); return EXIT_SUCCESS; }

	else if (*comm[0] == 'a' && strcmp(comm[0], "actions") == 0)
		return (exit_code = actions_function(comm));

	else if (*comm[0] == 'l' && comm[0][1] == 'm' && !comm[0][2])
		return (exit_code = lightmode_function(comm[1]));

	else if (*comm[0] == 'r' && ((comm[0][1] == 'l' && !comm[0][2])
	|| strcmp(comm[0], "reload") == 0))
		return (exit_code = reload_function());

	/* #### NEW INSTANCE #### */
	else if ((*comm[0] == 'x' || *comm[0] == 'X') && !comm[0][1])
		return (exit_code = new_instance_function(comm));

	/* #### NET #### */
	else if (*comm[0] == 'n' && (strcmp(comm[0], "net") == 0))
		return (exit_code = remotes_function(comm));

	/* #### MIME #### */
	else if (*comm[0] == 'm' && ((comm[0][1] == 'm' && !comm[0][2])
	|| strcmp(comm[0], "mime") == 0))
		return (exit_code = lira_function(comm));

	else if (conf.autols == 0 && *comm[0] == 'l' && comm[0][1] == 's'
	&& !comm[0][2])
		return (exit_code = ls_function());

	/* #### PROFILE #### */
	else if (*comm[0] == 'p' && ((comm[0][1] == 'f' && !comm[0][2])
	|| strcmp(comm[0], "prof") == 0 || strcmp(comm[0], "profile") == 0))
#ifndef _NO_PROFILES
		return (exit_code = profile_function(comm));
#else
		{ xerror("%s: prof: %s\n", PROGRAM_NAME, NOT_AVAILABLE);
		return EXIT_FAILURE; }
#endif /* !_NO_PROFILES */

	/* #### MOUNTPOINTS #### */
	else if (*comm[0] == 'm' && ((comm[0][1] == 'p' && !comm[0][2])
	|| strcmp(comm[0], "mountpoints") == 0))
		return (exit_code = media_function(comm[1], MEDIA_LIST));

	/* #### MEDIA #### */
	else if (*comm[0] == 'm' && strcmp(comm[0], "media") == 0)
		return (exit_code = media_function(comm[1], MEDIA_MOUNT));

	/* #### MAX FILES #### */
	else if (*comm[0] == 'm' && comm[0][1] == 'f' && !comm[0][2])
		return set_max_files(comm);

	/* #### EXT #### */
	else if (*comm[0] == 'e' && comm[0][1] == 'x' && comm[0][2] == 't'
	&& !comm[0][3])
		return (exit_code = ext_cmds_function(comm[1]));

	/* #### PAGER #### */
	else if (*comm[0] == 'p' && ((comm[0][1] == 'g' && !comm[0][2])
	|| strcmp(comm[0], "pager") == 0))
		return (exit_code = pager_function(comm[1]));

	/* #### FILES COUNTER #### */
	else if (*comm[0] == 'f' && ((comm[0][1] == 'c' && !comm[0][2])
	|| strcmp(comm[0], "filescounter") == 0))
		return (exit_code = filescounter_function(comm[1]));

	/* #### DIRECTORIES FIRST #### */
	else if ((*comm[0] == 'f' && comm[0][1] == 'f' && !comm[0][2])
	|| (*comm[0] == 'd' && strcmp(comm[0], "dirs-first") == 0))
		return (exit_code = dirs_first_function(comm[1]));

	/* #### LOG #### */
	else if (*comm[0] == 'l' && strcmp(comm[0], "log") == 0)
		return (exit_code = run_log_cmd(comm + 1));

	/* #### MESSAGES #### */
	else if (*comm[0] == 'm' && (strcmp(comm[0], "msg") == 0
	|| strcmp(comm[0], "messages") == 0))
		return (exit_code = msgs_function(comm[1]));

	/* #### ALIASES #### */
	else if (*comm[0] == 'a' && strcmp(comm[0], "alias") == 0)
		return (exit_code = alias_function(comm));

	/* #### EDIT, CONFIG #### */
	/* The 'edit' command is deprecated */
	else if ((*comm[0] == 'e' && strcmp(comm[0], "edit") == 0)
	|| (*comm[0] == 'c' && strcmp(comm[0], "config") == 0))
		return (exit_code = edit_function(comm));

	/* #### HISTORY #### */
	else if (*comm[0] == 'h' && strcmp(comm[0], "history") == 0)
		return (exit_code = history_function(comm));

	/* #### HIDDEN FILES #### */
	else if (*comm[0] == 'h' && (((comm[0][1] == 'f' || comm[0][1] == 'h')
	&& !comm[0][2]) || strcmp(comm[0], "hidden") == 0))
		return (exit_code = hidden_files_function(comm[1]));

	/* #### AUTOCD #### */
	else if (*comm[0] == 'a' && (strcmp(comm[0], "acd") == 0
	|| strcmp(comm[0], "autocd") == 0))
		return (exit_code = autocd_function(comm[1]));

	/* #### AUTO-OPEN #### */
	else if (*comm[0] == 'a' && ((comm[0][1] == 'o' && !comm[0][2])
	|| strcmp(comm[0], "auto-open") == 0))
		return (exit_code = auto_open_function(comm[1]));

	/* #### COMMANDS #### */
	else if (*comm[0] == 'c' && (strcmp(comm[0], "cmd") == 0
	|| strcmp(comm[0], "commands") == 0))
		return (exit_code = list_commands());

	/* #### PATH, CWD #### */
	else if ((*comm[0] == 'p' && (strcmp(comm[0], "pwd") == 0
	|| strcmp(comm[0], "path") == 0)) || strcmp(comm[0], "cwd") == 0) {
		return (exit_code = pwd_function(comm[1]));
	}

	/* #### HELP #### */
	else if ((*comm[0] == '?' && !comm[0][1]) || strcmp(comm[0], "help") == 0) {
		return (exit_code = quick_help(comm[1]));
	}

	/* #### EXPORT #### */
	else if (*comm[0] == 'e' && strcmp(comm[0], "export") == 0) {
		return (exit_code = export_var_function(comm + 1));
	}

	/* #### UMASK #### */
	else if (*comm[0] == 'u' && strcmp(comm[0], "umask") == 0) {
		return (exit_code = umask_function(comm[1]));
	}

	/* #### UNSET #### */
	else if (*comm[0] == 'u' && strcmp(comm[0], "unset") == 0) {
		return (exit_code = unset_function(comm + 1));
	}

	/* These functions just print stuff, so that the value of exit_code
	 * is always zero, that is to say, success */
	else if (*comm[0] == 'c' && strcmp(comm[0], "colors") == 0) {
		colors_function(comm[1]); return EXIT_SUCCESS;
	}

	else if (*comm[0] == 'v' && (strcmp(comm[0], "ver") == 0
	|| strcmp(comm[0], "version") == 0)) {
		version_function();	return EXIT_SUCCESS;
	}

	else if (*comm[0] == 'f' && comm[0][1] == 's' && !comm[0][2]) {
		free_software(); return EXIT_SUCCESS;
	}

	else if (*comm[0] == 'b' && strcmp(comm[0], "bonus") == 0) {
		bonus_function(); return EXIT_SUCCESS;
	}

	else if (*comm[0] == 's' && strcmp(comm[0], "splash") == 0) {
		splash(); return EXIT_SUCCESS;
	}

	/* #### QUIT #### */
	else if ((*comm[0] == 'q' && (!comm[0][1] || strcmp(comm[0], "quit") == 0))
	|| (*comm[0] == 'e' && strcmp(comm[0], "exit") == 0)
	|| (*comm[0] == 'Q' && !comm[0][1]))
		_quit(comm, old_exit_code);

	else {
		/* #  AUTOCD & AUTO-OPEN (2) # */
		if ((exit_code = check_auto_second(comm)) != -1)
			return exit_code;

		if (bin_name && *bin_name && *comm[0] == *bin_name
		&& strcmp(comm[0], bin_name) == 0) {
			fprintf(stderr, _("%s: To launch a new instance use the 'x' "
				"command\n"), PROGRAM_NAME);
			return (exit_code = EXIT_FAILURE);
		}

		/* #  EXTERNAL/SHELL COMMANDS # */
		if ((exit_code = run_shell_cmd(comm)) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}

CHECK_EVENTS:
	if (conf.autols == 0)
		return exit_code;

#if defined(LINUX_INOTIFY)
	if (watch)
		read_inotify();
#elif defined(BSD_KQUEUE)
	if (watch && event_fd >= 0)
		read_kqueue();
#elif defined(GENERIC_FS_MONITOR)
	struct stat a;
	if (curdir_mtime != 0 && stat(workspaces[cur_ws].path, &a) != -1
	&& curdir_mtime != a.st_mtime)
		reload_dirlist();
#endif /* LINUX_INOTIFY */

	return exit_code;
}

static inline void
run_chained_cmd(char **cmd, size_t *err_code)
{
	size_t i;
	char **alias_cmd = check_for_alias(cmd);
	if (alias_cmd) {
		if (exec_cmd(alias_cmd) != 0)
			*err_code = 1;
		for (i = 0; alias_cmd[i]; i++)
			free(alias_cmd[i]);
		free(alias_cmd);
	} else {
		if ((flags & FAILED_ALIAS) || exec_cmd(cmd) != 0)
			*err_code = 1;
		flags &= ~FAILED_ALIAS;
		for (i = 0; i <= args_n; i++)
			free(cmd[i]);
		free(cmd);
	}
}

/* Execute chained commands (cmd1;cmd2 and/or cmd1 && cmd2). The function
 * is called by parse_input_str() if some non-quoted double ampersand or
 * semicolon is found in the input string AND at least one of these
 * chained commands is internal */
void
exec_chained_cmds(char *cmd)
{
	if (!cmd) return;

	size_t i = 0, cmd_len = strlen(cmd);
	for (i = 0; i < cmd_len; i++) {
		char *str = (char *)NULL;
		size_t len = 0, cond_exec = 0, error_code = 0;

		/* Get command */
		str = (char *)xcalloc(strlen(cmd) + 1, sizeof(char));
		while (cmd[i] && cmd[i] != '&' && cmd[i] != ';') {
			str[len] = cmd[i];
			len++;
			i++;
		}

		if (!*str) {
			free(str);
			continue;
		}

		if (cmd[i] == '&') cond_exec = 1;

		char **tmp_cmd = parse_input_str((*str == ' ') ? str + 1 : str);
		free(str);

		if (!tmp_cmd) continue;

		run_chained_cmd(tmp_cmd, &error_code);

		/* Do not continue if the execution was condtional and
		 * the previous command failed */
		if (cond_exec && error_code)
			break;
	}
}

static inline void
run_profile_line(char *cmd)
{
	if (xargs.secure_cmds == 1
	&& sanitize_cmd(cmd, SNT_PROFILE) != EXIT_SUCCESS)
		return;

	args_n = 0;
	char **cmds = parse_input_str(cmd);
	if (!cmds)
		return;

	no_log = 1;
	exec_cmd(cmds);
	no_log = 0;

	int i = (int)args_n + 1;
	while (--i >= 0)
		free(cmds[i]);
	free(cmds);

	args_n = 0;
}

void
exec_profile(void)
{
	if (config_ok == 0 || !profile_file)
		return;

	FILE *fp = fopen(profile_file, "r");
	if (!fp)
		return;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!*line || *line == '\n' || *line == '#')
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		if (int_vars == 1 && strchr(line, '=') && !IS_DIGIT(*line))
			create_usr_var(line);
		else
			run_profile_line(line);
	}

	free(line);
	fclose(fp);
}
