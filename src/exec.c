/* exec.c -- functions controlling the execution of programs */

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
#ifdef __OpenBSD__
# include <sys/dirent.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <readline/readline.h>
#include <limits.h>

#include "actions.h"
#ifndef _NO_ARCHIVING
# include "archives.h"
#endif
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
#include "profiles.h"
#include "prompt.h"
#include "properties.h"
#include "readline.h"
#include "remotes.h"
#include "search.h"
#include "selection.h"
#include "sort.h"
#include "strings.h"
#ifndef _NO_TRASH
# include "trash.h"
#endif
#include "messages.h"
#include "media.h"
#ifndef _NO_BLEACH
# include "name_cleaner.h"
#endif
#include "sanitize.h"
#include "tags.h"

char **_comm = (char **)NULL;

static char *
get_new_name(void)
{
	char *input = (char *)NULL;

	rl_nohist = 1;

	char m[NAME_MAX];
	snprintf(m, sizeof(m), "Enter new name ('Ctrl-x' to quit)\n"
		"\001%s\002>\001%s\002 ", mi_c, tx_c);

	int bk_wp = warning_prompt;
	int bk_sug = suggestions;
	int bk_hl = highlight;
	int bk_prompt_offset = prompt_offset;
	highlight = 0;
	suggestions = 0;
	warning_prompt = 0;
	prompt_offset = 2;

	while (!input && _xrename) {
		input = readline(m);
		if (!input)
			continue;
		if (!*input || *input == ' ') {
			free(input);
			input = (char *)NULL;
			continue;
		}
	}

	highlight = bk_hl;
	suggestions = bk_sug;
	warning_prompt = bk_wp;
	prompt_offset = bk_prompt_offset;
	rl_nohist = 0;

	return input;
}

/* Run CMD via execve() and refresh the screen in case of success */
int
run_and_refresh(char **cmd)
{
	if (!cmd)
		return EXIT_FAILURE;

	log_function(cmd);

	char *new_name = (char *)NULL;
	if (xrename) {
		/* If we have a number, either it was not expanded by parse_input_str(),
		 * in which case it is an invalid ELN, or it was expanded to a file named
		 * as a number. Let's check if we have such file name in the files list */
		if (is_number(cmd[1])) {
			int i = (int)files;
			while (--i >= 0) {
				if (*cmd[1] != *file_info[i].name)
					continue;
				if (strcmp(cmd[1], file_info[i].name) == 0)
					break;
			}
			if (i == -1) {
				fprintf(stderr, "%s: %s: Invalid ELN\n", PROGRAM_NAME, cmd[1]);
				xrename = 0;
				return EXIT_FAILURE;
			}
		}
		_xrename = 1;
		new_name = get_new_name();
		_xrename = 0;
		if (!new_name) {
			_xrename = 0;
			return EXIT_SUCCESS;
		}
	}

	char **tcmd = xnmalloc(2 + args_n + 2, sizeof(char *));
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

	size_t i;
	for (i = 1; cmd[i]; i++) {
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
	int ret = launch_execve(tcmd, FOREGROUND, E_NOFLAG);

	for (i = 0; tcmd[i]; i++)
		free(tcmd[i]);
	free(tcmd);

	if (ret != EXIT_SUCCESS)
		return ret;

	/* Error messages are printed by launch_execve() itself */

	/* If 'rm sel' and command is successful, deselect everything */
	if (is_sel && *cmd[0] == 'r' && cmd[0][1] == 'm' && (!cmd[0][2]
	|| cmd[0][2] == ' ')) {
		int j = (int)sel_n;
		while (--j >= 0)
			free(sel_elements[j].name);
		sel_n = 0;
		save_sel();
	}

#ifdef __HAIKU__
	if (autols == 1 && cmd[1] && strcmp(cmd[1], "--help") != 0
	&& strcmp(cmd[1], "--version") != 0)
		reload_dirlist();
#endif

	return EXIT_SUCCESS;
}

static int
run_in_foreground(pid_t pid)
{
	int status = 0;

	/* The parent process calls waitpid() on the child */
	if (waitpid(pid, &status, 0) > 0) {
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			/* The program terminated normally and executed successfully */
			return EXIT_SUCCESS;
		} else if (WIFEXITED(status) && WEXITSTATUS(status)) {
			/* Program terminated normally, but returned a non-zero status.
			 * Error codes should be printed by the child process */
			return WEXITSTATUS(status);
		} else {
			/* The program didn't terminate normally. In this case too,
			 * error codes should be printed by the child process */
			return EXCRASHERR;
		}
	} else { /* waitpid() failed */
		int ret = errno;
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: waitpid: %s\n", PROGRAM_NAME, strerror(errno));
		return ret;
	}

	return EXIT_FAILURE; /* Never reached */
}

static int
run_in_background(pid_t pid)
{
	int status = 0;
	pid_t wpid = waitpid(pid, &status, WNOHANG);
	if (wpid == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: waitpid: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	}

	zombies++;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	return EXIT_SUCCESS;
}

/* Execute a command using the system shell (/bin/sh), which takes care
 * of special functions such as pipes and stream redirection, special
 * chars like wildcards, quotes, and escape sequences. Use only when the
 * shell is needed; otherwise, launch_execve() should be used instead. */
int
launch_execle(const char *cmd)
{
	if (!cmd || !*cmd)
		return EXNULLERR;

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);

	if (xargs.secure_cmds == 1 && xargs.secure_env_full == 0
	&& xargs.secure_env == 0)
		sanitize_cmd_environ();

	int ret = system(cmd);

	if (xargs.secure_cmds == 1 && xargs.secure_env_full == 0
	&& xargs.secure_env == 0)
		restore_cmd_environ();

	set_signals_to_ignore();

	int exit_status = 0;
	if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0)
		exit_status = EXIT_SUCCESS;
	else if (WIFEXITED(ret) && WEXITSTATUS(ret))
		exit_status = WEXITSTATUS(ret);
	else
		exit_status = EXCRASHERR;

	if (flags & DELAYED_REFRESH) {
		flags &= ~DELAYED_REFRESH;
		reload_dirlist();
	}

	return exit_status;

/*	// Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid won't
	// be able to catch error codes coming from the child
	signal(SIGCHLD, SIG_DFL);

	flags |= RUNNING_SHELL_CMD;
	flags |= RUNNING_CMD_FG;

	int status, ret = 0;
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	} else if (pid == 0) {
		// Reenable signals only for the child, in case they were
		// disabled for the parent
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);

		// Get shell base name
		char *name = strrchr(user.shell, '/');

		execl(user.shell, name ? name + 1 : user.shell, "-c", cmd, NULL);
		fprintf(stderr, "%s: %s: execle: %s\n", PROGRAM_NAME, user.shell,
		    strerror(errno));
		_exit(errno);
	}
	// Get command status
	else {
		// The parent process calls waitpid() on the child
		if (waitpid(pid, &status, 0) > 0) {
			if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
				// The program terminated normally and executed successfully
				ret = EXIT_SUCCESS;
			} else if (WIFEXITED(status) && WEXITSTATUS(status)) {
				// Either "command not found" (WEXITSTATUS(status) == 127),
				// "permission denied" (not executable) (WEXITSTATUS(status) ==
				// 126) or the program terminated normally, but returned a
				// non-zero status. These exit codes will be handled by the
				// system shell itself, since we're using here execle()
				ret = WEXITSTATUS(status);
			} else { // The program didn't terminate normally
				ret = EXCRASHERR;
			}
		} else { // Waitpid() failed
			fprintf(stderr, "%s: waitpid: %s\n", PROGRAM_NAME,
			    strerror(errno));
			ret = errno;
		}
	}

	flags &= ~RUNNING_CMD_FG;
	if (flags & DELAYED_REFRESH) {
		flags &= ~DELAYED_REFRESH;
		refresh_files_list(0);
	}

	return ret;*/
}

/* Execute a command and return the corresponding exit status. The exit
 * status could be: zero, if everything went fine, or a non-zero value
 * in case of error. The function takes as first arguement an array of
 * strings containing the command name to be executed and its arguments
 * (cmd), an integer (bg) specifying if the command should be
 * backgrounded (1) or not (0), and a flag to control file descriptors */
int
launch_execve(char **cmd, const int bg, const int xflags)
{
	if (!cmd)
		return EXNULLERR;

	/* Reenable SIGCHLD, in case it was disabled. Otherwise, waitpid
	 * won't be able to catch error codes coming from the child. */
	signal(SIGCHLD, SIG_DFL);

	int ret = 0;
	pid_t pid = fork();
	if (pid < 0) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: fork: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	} else if (pid == 0) {
		if (bg == 0) {
			/* If the program runs in the foreground, reenable signals only
			 * for the child, in case they were disabled for the parent */
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
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, cmd[0], strerror(errno));
		_exit(errno);
	}

	/* Get command status (pid > 0) */
	else {
		if (bg == 1) {
			ret = run_in_background(pid);
		} else {
			ret = run_in_foreground(pid);
			if ((flags & DELAYED_REFRESH) && xargs.open != 1) {
				flags &= ~DELAYED_REFRESH;
				reload_dirlist();
			}
		}
	}

	if (bg == 1 && ret == EXIT_SUCCESS && xargs.open != 1)
		reload_dirlist();

	return ret;
}

/* Prevent the user from killing the program via the 'kill',
 * 'pkill' or 'killall' commands, from within CliFM itself.
 * Otherwise, the program will be forcefully terminated without
 * freeing allocated memory */
static inline int
graceful_quit(char **args)
{
	size_t i;
	for (i = 1; i <= args_n; i++) {
		if ((strcmp(args[0], "kill") == 0 && atoi(args[i]) == (int)own_pid)
		|| ((strcmp(args[0], "killall") == 0 || strcmp(args[0], "pkill") == 0)
		&& strcmp(args[i], argv_bk[0]) == 0)) {
			fprintf(stderr, _("%s: To gracefully quit enter 'q'\n"), PROGRAM_NAME);
			return EXIT_FAILURE;
		}
	}

	return (-1);
}

/* Reload the list of available commands in PATH for TAB completion.
 * Why? If this list is not updated, whenever some new program is
 * installed, renamed, or removed from some of the paths in PATH
 * while in CliFM, this latter needs to be restarted in order
 * to be able to recognize the new program for TAB completion
 *
 * Why the RELOADING_BINARIES flag? While reloading binaries we need
 * to momentarilly change the current directory to those dirs in PATH.
 * Now, it might happen that we get a SIGWINCH signal when doing this.
 * The problem is that, upon SIGWICH, CliFM attempts to reload
 * the current list of files, and in order to succesfully do so
 * we need to be in the current directory informed by CliFM
 * So, the RELOADING_BINARIES flag just informs the SIGWHINCH handler
 * (sigwich_handler()) that we are reloading binaries so that it can
 * handle the case appropriately */
static inline void
reload_binaries(void)
{
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
			free(paths[j]);
	}

	path_n = (size_t)get_path_env();
	get_path_programs();
}

static inline int
__export(char *arg)
{
	if (!arg || !*arg)
		return (-1);
	char *p = strchr(arg, '=');
	if (!p || !*(p + 1))
		return (-1);

	errno = 0;

	*p = '\0';
	int ret = setenv(arg, p + 1, 1);
	if (ret == -1)
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s\n", PROGRAM_NAME, strerror(errno));
	*p = '=';

	return errno;
}

static inline void
append_ampersand(char **cmd, size_t *len)
{
	if (fzf_open_with == 1) {
		fzf_open_with = 0;
		strcat(*cmd, " &>/dev/null");
		*len += 12;
	}

	(*cmd)[*len - 3] = ' ';
	(*cmd)[*len - 2] = '&';
	(*cmd)[*len - 1] = '\0';
}

static inline char *
construct_shell_cmd(char **args)
{
	/* Bypass CliFM's parsing, expansions, and checks to be executed
	 * DIRECTLY by the system shell (launch_execle()) */
	char *first = args[0];
	if (*args[0] == ':' || *args[0] == ';')
		first++;

	size_t len = strlen(first) + 3;
	char *cmd = (char *)xnmalloc(len + (bg_proc ? 2 : 0)
			+ (fzf_open_with ? 12 : 0), sizeof(char));
	strcpy(cmd, first);
	cmd[len - 3] = ' ';
	cmd[len - 2] = '\0';

	size_t i;
	for (i = 1; args[i]; i++) {
		/* Dest string (cmd) is NULL terminated, just as the source
		 * string (comm[i]) */
		if (i > 1) {
			cmd[len - 3] = ' ';
			cmd[len - 2] = '\0';
		}
		len += strlen(args[i]) + 1;
		/* LEN holds the previous size of the buffer, plus space, the
		 * ampersand character, and the new src string. The buffer is
		 * thus big enough */
		cmd = (char *)xrealloc(cmd, (len + 3 + (bg_proc ? 2 : 0)
				+ (fzf_open_with ? 12 : 0)) * sizeof(char));
		strcat(cmd, args[i]);
	}

	if (bg_proc) /* If backgrounded */
		append_ampersand(&cmd, &len);
	else
		cmd[len - 3] = '\0';

	return cmd;
}

static inline int
check_shell_cmd_condtions(char **args)
{
	/* Prevent ungraceful exit */
	if ((*args[0] == 'k' || *args[0] == 'p') && (strcmp(args[0], "kill") == 0
	|| strcmp(args[0], "killall") == 0 || strcmp(args[0], "pkill") == 0)) {
		if (graceful_quit(args) != -1)
			return EXIT_FAILURE;
	}

	if (ext_cmd_ok == 0) {
		fprintf(stderr, _("%s: External commands are not allowed. "
			"Run 'ext on' to enable them.\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int
run_shell_cmd(char **args)
{
	/* 'no_log' will be true when running profile or prompt commands */
	if (!no_log)
		log_function(args);

	if (check_shell_cmd_condtions(args) == EXIT_FAILURE)
		return EXIT_FAILURE;

	/* Little export implementation. What it lacks? Command substitution */
	if (*args[0] == 'e' && strcmp(args[0], "export") == 0 && args[1]) {
		int exit_status = __export(args[1]);
		if (exit_status != -1)
			return exit_status;
	}

	char *cmd = construct_shell_cmd(args);

	/* Calling the system shell is vulnerable to command injection, true.
	 * But it is the user here who is directly running the command: this
	 * should not be taken as an untrusted source */
	int exit_status = launch_execle(cmd); /* lgtm [cpp/command-line-injection] */
	free(cmd);

	reload_binaries();

	return exit_status;
}

/* Free everything and exit the program */
static void
_quit(char **args, int exit_status)
{
	if (!args || !args[0])
		return;

	if (*args[0] == 'Q')
		cd_on_quit = 1;
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
		if (autols == 1) reload_dirlist();
		print_reload_msg(_("Max files unset\n"));
		return EXIT_SUCCESS;
	}

	if (*args[1] == '0' && !args[1][1]) {
		max_files = 0;
		if (autols == 1) reload_dirlist();
		print_reload_msg(_("Max files set to %d\n"), max_files);
		return EXIT_SUCCESS;
	}

	long inum = strtol(args[1], NULL, 10);
	if (inum == LONG_MAX || inum == LONG_MIN || inum <= 0) {
		fprintf(stderr, _("%s: %s: Invalid number\n"), PROGRAM_NAME, args[1]);
		return (exit_code = EXIT_FAILURE);
	}

	max_files = (int)inum;
	if (autols == 1) reload_dirlist();
	print_reload_msg(_("Max files set to %d\n"), max_files);

	return EXIT_SUCCESS;
}

static int
unicode_function(char *arg)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(UNICODE_USAGE));
		return (exit_code = EXIT_FAILURE);
	}

	if (IS_HELP(arg)) {
		puts(_(UNICODE_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		printf(_("Unicode is %s\n"), unicode == 1 ? _("enabled") : _("disabled"));
	} else if (*arg == 'o' && strcmp(arg, "on") == 0) {
		unicode = 1;
		puts(_("Unicode enabled"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		unicode = 0;
		puts(_("Unicode disabled"));
	} else {
		fprintf(stderr, "%s\n", _(UNICODE_USAGE));
		exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
dirs_first_function(char *arg)
{
	if (autols == 0)
		return EXIT_SUCCESS;

	if (!arg) {
		fprintf(stderr, "%s\n", _(FF_USAGE));
		return (exit_code = EXIT_FAILURE);
	}

	if (IS_HELP(arg)) {
		puts(_(FF_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		printf(_("Directories first is %s\n"),
			list_dirs_first == 1 ? _("enabled") : _("disabled"));
	}  else if (*arg == 'o' && strcmp(arg, "on") == 0) {
		list_dirs_first = 1;
		if (autols == 1) reload_dirlist();
		print_reload_msg(_("Directories first enabled\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		list_dirs_first = 0;
		if (autols == 1) reload_dirlist();
		print_reload_msg(_("Directories first disabled\n"));
	} else {
		fprintf(stderr, "%s\n", _(FF_USAGE));
		return EXIT_FAILURE;
	}

	return exit_status;
}

static int
filescounter_function(char *arg)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(FC_USAGE));
		return EXIT_FAILURE;
	}

	if (*arg == 'o' && strcmp(arg, "on") == 0) {
		files_counter = 1;
		if (autols == 1) reload_dirlist();
		print_reload_msg(_("Files counter enabled\n"));
		return EXIT_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "off") == 0) {
		files_counter = 0;
		if (autols == 1) reload_dirlist();
		print_reload_msg(_("Files counter disabled\n"));
		return EXIT_SUCCESS;
	}

	if (*arg == 's' && strcmp(arg, "status") == 0) {
		if (files_counter == 1)
			puts(_("The files counter is enabled"));
		else
			puts(_("The files counter is disabled"));
		return EXIT_SUCCESS;
	}

	fprintf(stderr, "%s\n", _(FC_USAGE));
	return EXIT_FAILURE;
}

static int
pager_function(char *arg)
{
	if (!arg) {
		puts(_(PAGER_USAGE));
		return (exit_code = EXIT_FAILURE);
	}

	if (IS_HELP(arg)) {
		puts(_(PAGER_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;
	if (*arg == 's' && strcmp(arg, "status") == 0) {
		printf(_("The files pager is %s\n"),
			pager == 1 ? _("enabled") : _("disabled"));
	} else if (*arg == 'o' && strcmp(arg, "on") == 0) {
		pager = 1;
		if (autols == 1) reload_dirlist();
		print_reload_msg(_("Pager enabled\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		pager = 0;
		if (autols == 1) reload_dirlist();
		print_reload_msg(_("Pager disabled\n"));
	} else {
		fprintf(stderr, "%s\n", _(PAGER_USAGE));
		exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
ext_args_function(char *arg)
{
	if (!arg) {
		puts(_(EXT_USAGE));
		return EXIT_FAILURE;
	}

	if (IS_HELP(arg)) {
		puts(_(EXT_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;
	if (*arg == 's' && strcmp(arg, "status") == 0) {
		printf(_("External commands are %s\n"),
			ext_cmd_ok ? _("allowed") : _("not allowed"));
	} else if (*arg == 'o' && strcmp(arg, "on") == 0) {
		ext_cmd_ok = 1;
		printf(_("External commands allowed\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		ext_cmd_ok = 0;
		printf(_("External commands disallowed\n"));
	} else {
		fprintf(stderr, "%s\n", _(EXT_USAGE));
		exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
autocd_function(char *arg)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(AUTOCD_USAGE));
		return EXIT_FAILURE;
	}

	if (strcmp(arg, "on") == 0) {
		autocd = 1;
		printf(_("Autocd enabled\n"));
	} else if (strcmp(arg, "off") == 0) {
		autocd = 0;
		printf(_("Autocd disabled\n"));
	} else if (strcmp(arg, "status") == 0) {
		printf(_("Autocd is %s\n"), autocd == 1 ? _("enabled") : _("disabled"));
	} else if (IS_HELP(arg)) {
		puts(_(AUTOCD_USAGE));
	} else {
		fprintf(stderr, "%s\n", _(AUTOCD_USAGE));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int
auto_open_function(char *arg)
{
	if (!arg) {
		fprintf(stderr, "%s\n", _(AUTO_OPEN_USAGE));
		return EXIT_FAILURE;
	}

	if (strcmp(arg, "on") == 0) {
		auto_open = 1;
		printf(_("Auto-open enabled\n"));
	} else if (strcmp(arg, "off") == 0) {
		auto_open = 0;
		printf(_("Auto-open disabled\n"));
	} else if (strcmp(arg, "status") == 0) {
		printf(_("Auto-open is %s\n"), auto_open == 1 ? _("enabled") : _("disabled"));
	} else if (IS_HELP(arg)) {
		puts(_(AUTO_OPEN_USAGE));
	} else {
		fprintf(stderr, "%s\n", _(AUTO_OPEN_USAGE));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static int
columns_function(char *arg)
{
	if (!arg || IS_HELP(arg)) {
		puts(_(COLUMNS_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	if (*arg == 'o' && arg[1] == 'n' && !arg[2]) {
		columned = 1;
		if (autols == 1) {
			free_dirlist();
			exit_status = list_dir();
		}
		print_reload_msg(_("Columns enabled\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		columned = 0;
		if (autols == 1) {
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
icons_function(char *arg)
{
#ifdef _NO_ICONS
	UNUSED(arg);
	fprintf(stderr, _("%s: icons: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
	return EXIT_SUCCESS;
#else
	if (!arg || IS_HELP(arg)) {
		puts(_(ICONS_USAGE));
		return EXIT_SUCCESS;
	}

	if (*arg == 'o' && arg[1] == 'n' && !arg[2]) {
		icons = 1;
		int ret = EXIT_SUCCESS;
		if (autols == 1) { free_dirlist(); ret = list_dir(); }
		print_reload_msg("Icons enabled\n");
		return ret;
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		icons = 0;
		int ret = EXIT_SUCCESS;
		if (autols == 1) { free_dirlist(); ret = list_dir(); }
		print_reload_msg("Icons disabled\n");
		return ret;
	} else {
		fprintf(stderr, "%s\n", _(ICONS_USAGE));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
#endif /* _NO_ICONS */
}

static int
msgs_function(char *arg)
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

		if (autols == 1)
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
opener_function(char *arg)
{
	if (!arg) {
		printf("opener: %s\n", opener ? opener : "lira (built-in)");
		return EXIT_SUCCESS;
	}
	if (IS_HELP(arg)) {
		puts(_(OPENER_USAGE));
		return EXIT_SUCCESS;
	}

	if (opener) {
		free(opener);
		opener = (char *)NULL;
	}

	if (strcmp(arg, "default") != 0 && strcmp(arg, "lira") != 0)
		opener = savestring(arg, strlen(arg));
	printf(_("Opener set to '%s'\n"), opener ? opener : "lira (built-in)");

	return EXIT_SUCCESS;
}

static int
lightmode_function(char *arg)
{
	if (!arg || IS_HELP(arg)) {
		fprintf(stderr, "%s\n", _(LM_USAGE));
		return EXIT_SUCCESS;
	}

	if (*arg == 'o' && strcmp(arg, "on") == 0) {
		light_mode = 1;
		if (autols == 1)
			reload_dirlist();
		print_reload_msg(_("Switched to light mode\n"));
	} else if (*arg == 'o' && strcmp(arg, "off") == 0) {
		light_mode = 0;
		if (autols == 1)
			reload_dirlist();
		print_reload_msg(_("Switched back to normal mode\n"));
	} else {
		puts(_(LM_USAGE));
		return EXIT_FAILURE;
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
print_alias(char *name)
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

	fprintf(stderr, _("%s: %s: No such alias\n"), PROGRAM_NAME, name);
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
			fprintf(stderr, "%s\n", _(ALIAS_USAGE));
			return EXIT_FAILURE;
		}
		return alias_import(args[2]);
	}

	if (*args[1] == 'l' && (strcmp(args[1], "ls") == 0
	|| strcmp(args[1], "list") == 0))
		return list_aliases();

	return print_alias(args[1]);
}

static int
_log_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(LOG_USAGE));
		return EXIT_SUCCESS;
	}

	/* I make this check here, and not in the function itself,
	 * because this function is also called by other instances of
	 * the program where no message should be printed */
	if (!config_ok) {
		fprintf(stderr, _("%s: Log function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	return log_function(args);
}

static int
hidden_function(char **comm)
{
	int exit_status = EXIT_SUCCESS;

	if (strcmp(comm[1], "status") == 0) {
		printf(_("Hidden files is %s\n"), show_hidden
			? _("enabled") : _("disabled"));
	} else if (strcmp(comm[1], "off") == 0) {
		show_hidden = 0;
		if (autols == 1) {
			free_dirlist();
			exit_status = list_dir();
		}
		print_reload_msg(_("Hidden files disabled\n"));
	} else if (strcmp(comm[1], "on") == 0) {
		show_hidden = 1;
		if (autols == 1) {
			free_dirlist();
			exit_status = list_dir();
		}
		print_reload_msg(_("Hidden files enabled\n"));
	} else {
		fprintf(stderr, "%s\n", _(HF_USAGE));
	}

	return exit_status;
}

static int
_hidden_function(char **args)
{
	if (!args[1]) {
		puts(_(HF_USAGE));
		return EXIT_FAILURE;
	}

	if (IS_HELP(args[1])) {
		/* The same message is in hidden_function(), and printed
		 * whenever an invalid argument is entered */
		puts(_(HF_USAGE));
		return EXIT_SUCCESS;
	}

	return hidden_function(args);
}

static int
_toggle_exec(char **args)
{
	if (!args[1] || IS_HELP(args[1])) {
		puts(_(TE_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;
	size_t i, n = 0;
	for (i = 1; args[i]; i++) {
		struct stat attr;
		if (strchr(args[i], '\\')) {
			char *tmp = dequote_str(args[i], 0);
			if (tmp) {
				strcpy(args[i], tmp);
				free(tmp);
			}
		}

		if (lstat(args[i], &attr) == -1) {
			fprintf(stderr, "stat: %s: %s\n", args[i], strerror(errno));
			exit_status = EXIT_FAILURE;
			continue;
		}

		if (toggle_exec(args[i], attr.st_mode) == -1)
			exit_status = EXIT_FAILURE;
		else
			n++;
	}

	if (n > 0) {
		if (autols == 1) reload_dirlist();
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
	if (!args[1]) {
		fprintf(stderr, "%s\n", _(PROP_USAGE));
		return EXIT_FAILURE;
	}

	if (IS_HELP(args[1])) {
		puts(_(PROP_USAGE));
		return EXIT_SUCCESS;
	}

	return properties_function(args);
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
	fprintf(stderr, "%s: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	return EXIT_FAILURE;
#endif
}

static int
refresh_function(const int old_exit_code)
{
	if (autols == 1)
		reload_dirlist();

	return old_exit_code;
}

static int
export_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(_(EXPORT_USAGE));
		return EXIT_SUCCESS;
	}

	char *ret = export(args, 1);
	if (ret) {
		printf("Files exported to: %s\n", ret);
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
	welcome_message = 0;

	if (autols == 1) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

/* MODE could be either MEDIA_LIST (mp command) or MEDIA_MOUNT (media command) */
static int
media_function(char *arg, int mode)
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
			fprintf(stderr, _("%s: No pinned file\n"), PROGRAM_NAME);
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
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if (args[0][1] == ';' || args[0][1] == ':') {
		/* If double semi colon or colon (or ";:" or ":;") */
		fprintf(stderr, _("%s: '%s': Syntax error\n"), PROGRAM_NAME, args[0]);
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
	if (autocd && (file_info[i].type == DT_DIR || file_info[i].dir == 1))
		return cd_function(args[0], CD_PRINT_ERROR);

	if (auto_open && (file_info[i].type == DT_REG
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

	if (*args[0] == '/' || (!autocd && !auto_open) || (args[1]
	&& (*args[1] != '&' || args[1][1])))
		return (-1);

	if (*args[0] == 'e' && strcmp(args[0], "edit") == 0)
		return (-1);

	char *deq_str = (char *)NULL;
	if (autocd || auto_open)
		expand_and_deescape(&args[0], &deq_str);

	char *tmp = deq_str ? deq_str : args[0];
	size_t len = strlen(tmp);
	if (tmp[len - 1] == '/') tmp[len - 1] = '\0';

	if (autocd && cdpath_n && !args[1]
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

	if (autocd) {
		ret = cd_function(tmp, CD_PRINT_ERROR);
	} else {
		fprintf(stderr, _("%s: cd: %s: Is a directory\n"), PROGRAM_NAME, tmp);
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

	if (autocd == 1 && cdpath_n > 0 && !args[1]) {
		int ret = cd_function(tmp, CD_NO_PRINT_ERROR);
		if (ret == EXIT_SUCCESS)
			{ free(tmp); return EXIT_SUCCESS; }
	}

	struct stat attr;
	if (stat(tmp, &attr) != 0)
		{ free(tmp); return (-1); }

	if (autocd == 1 && S_ISDIR(attr.st_mode) && !args[1])
		return autocd_dir(tmp);

	/* Regular, non-executable file, or exec file not in PATH
	 * not ./file and not /path/to/file */
	if (auto_open == 1 && S_ISREG(attr.st_mode)
	&& (!(attr.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
	|| (!is_bin_cmd(tmp) && !(*tmp == '.' && *(tmp + 1) == '/') && *tmp != '/' ) ) )
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
	int exit_status = list_dir();
	if (get_sel_files() != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

	return exit_status;
}

static int
lira_function(char **args)
{
#ifndef _NO_LIRA
	if (mime_open(args) == EXIT_SUCCESS)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
#else
	UNUSED(args);
	fprintf(stderr, _("%s: lira: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
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

	if (autocd == 1 && S_ISDIR(a.st_mode))
		return EXIT_FAILURE;

	if (auto_open == 1 && !S_ISDIR(a.st_mode))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int
print_stats(void)
{
	if (light_mode == 1) {
		puts("Running in light mode: files statistics are not available");
		return EXIT_SUCCESS;
	}

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
	fprintf(stderr, _("%s: trash: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
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
	fprintf(stderr, _("%s: trash: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
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
		fprintf(stderr, _("%s: %s: Invalid argument. Try 'fz -h'\n"),
			PROGRAM_NAME, arg);
		return EXIT_FAILURE;
	}

	if (*(arg + 1) == 'n' && !*(arg + 2)) {
		if (full_dir_size == 1) {
			puts(_("Full directory size is already enabled"));
		} else {
			full_dir_size = 1;
			if (autols == 1) reload_dirlist();
			print_reload_msg(_("Full directory size enabled\n"));
		}
		return EXIT_SUCCESS;
	}

	if (*(arg + 1) == 'f' && *(arg + 2) == 'f' && !*(arg + 3)) {
		if (full_dir_size == 0) {
			puts(_("Full directory size is already disabled"));
		} else {
			full_dir_size = 0;
			if (autols == 1) reload_dirlist();
			print_reload_msg(_("Full directory size disabled\n"));
		}
		return EXIT_SUCCESS;
	}

	fprintf(stderr, _("%s: %s: Invalid argument. Try 'fz -h'\n"),
		PROGRAM_NAME, arg);
	return EXIT_FAILURE;
}

static void
set_cp_cmd(char **cmd)
{
	switch(cp_cmd) {
	case CP_ADVCP:
		*cmd = (char *)xrealloc(*cmd, (strlen(__DEF_ADVCP_CMD) + 1) * sizeof(char));
		strcpy(*cmd, __DEF_ADVCP_CMD);
		break;
	case CP_WCP:
		*cmd = (char *)xrealloc(*cmd, (strlen(__DEF_WCP_CMD) + 1) * sizeof(char));
		strcpy(*cmd, __DEF_WCP_CMD);
		break;
	case CP_RSYNC:
		*cmd = (char *)xrealloc(*cmd, (strlen(__DEF_RSYNC_CMD) + 1) * sizeof(char));
		strcpy(*cmd, __DEF_RSYNC_CMD);
		break;
	case CP_CP: /* fallthrough */
	default:
		*cmd = (char *)xrealloc(*cmd, (strlen(__DEF_CP_CMD) + 1) * sizeof(char));
		strcpy(*cmd, __DEF_CP_CMD);
		break;
	}
}

static void
set_mv_cmd(char **cmd)
{
	switch(mv_cmd) {
	case MV_ADVMV:
		*cmd = (char *)xrealloc(*cmd, (strlen(__DEF_ADVMV_CMD) + 1) * sizeof(char));
		strcpy(*cmd, __DEF_ADVMV_CMD);
		break;
	case MV_MV: /* fallthrough */
	default:
		*cmd = (char *)xrealloc(*cmd, (strlen(__DEF_MV_CMD) + 1) * sizeof(char));
		strcpy(*cmd, __DEF_MV_CMD);
		break;
	}
}

/* Let's check we have not left any zombie process behind. This happens
 * whenever we run a process in the background via launch_execve */
static void
check_zombies(void)
{
	int status = 0;
	if (waitpid(-1, &status, WNOHANG) > 0)
		zombies--;
}

/*
static int
bring_to_foreground(char *str)
{
	if (!str || !*str || IS_HELP(str)) {
		puts("Usage: fg PID");
		return EXIT_SUCCESS;
	}

	if (!is_number(str)) {
		fprintf(stderr, "%s: %s: Not a valid PID\n", PROGRAM_NAME, str);
		return EXIT_FAILURE;
	}

	int pid = atoi(str);
	if (kill((pid_t)pid, SIGCONT) == -1) {
		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	}

	int status = 0;
	if (waitpid(pid, &status, 0) <= 0) {
		fprintf(stderr, "%s: waitpid: %s\n", PROGRAM_NAME, strerror(errno));
		return errno;
	}

	return EXIT_SUCCESS;
} */

/* Print the current working directory. Try first our own internal representation
 * (workspaces array). If something went wrong, fallback to getcwd(3) */
static int
print_cwd(void)
{
	if (workspaces && workspaces[cur_ws].path) {
		printf("%s\n", workspaces[cur_ws].path);
		return EXIT_SUCCESS;
	}

	char p[PATH_MAX];
	*p = '\0';
	char *buf = getcwd(p, sizeof(p));
	if (!buf) {
		int err = errno;
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: getcwd: %s\n", PROGRAM_NAME, strerror(err));
		return err;
	}

	printf("%s\n", p);
	return EXIT_SUCCESS;
}

/* Return 1 if STR is a path, zero otherwise */
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
	if ((exit_code = check_auto_first(comm)) != -1)
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

	/*         ############### BACKDIR ##################     */
	else if (*comm[0] == 'b' && comm[0][1] == 'd' && !comm[0][2])
		return (exit_code = backdir(comm[1]));

	/*      ############### OPEN WITH ##################     */
	else if (*comm[0] == 'o' && comm[0][1] == 'w' && !comm[0][2])
		return (exit_code = ow_function(comm));

	/*   ############## DIRECTORY JUMPER ##################     */
	else if (*comm[0] == 'j' && (!comm[0][1] || ((comm[0][1] == 'c'
	|| comm[0][1] == 'p' || comm[0][1] == 'e' || comm[0][1] == 'o'
	|| comm[0][1] == 'l') && !comm[0][2])))
		return (exit_code = dirjump(comm, NO_SUG_JUMP));

	/*       ############### REFRESH ##################     */
	else if (*comm[0] == 'r' && ((comm[0][1] == 'f' && !comm[0][2])
	|| strcmp(comm[0], "refresh") == 0))
		return (exit_code = refresh_function(old_exit_code));

	/*     ############### BOOKMARKS ##################     */
	else if (*comm[0] == 'b' && ((comm[0][1] == 'm' && !comm[0][2])
	|| strcmp(comm[0], "bookmarks") == 0))
		return (exit_code = _bookmarks_function(comm));

	/*       ############### BACK AND FORTH ##################     */
	else if (*comm[0] == 'b' && (!comm[0][1] || strcmp(comm[0], "back") == 0))
		return (exit_code = back_function(comm));

	else if (*comm[0] == 'f' && (!comm[0][1] || strcmp(comm[0], "forth") == 0))
		return (exit_code = forth_function(comm));

	else if ((*comm[0] == 'b' || *comm[0] == 'f')
	&& comm[0][1] == 'h' && !comm[0][2]) {
		print_dirhist(); return EXIT_SUCCESS;
	}

	else if (*comm[0] == 'r' && comm[0][1] == 'r' && !comm[0][2])
		exit_code = bulk_remove(comm[1] ? comm[1] : NULL,
					(comm[1] && comm[2]) ? comm[2] : NULL);

	/*     ################# TAGS ##################     */
	else if (*comm[0] == 't'
	&& (((comm[0][1] == 'a' || comm[0][1] == 'd' || comm[0][1] == 'l'
	|| comm[0][1] == 'm' || comm[0][1] == 'n' || comm[0][1] == 'u'
	|| comm[0][1] == 'y') && !comm[0][2]) || strcmp(comm[0], "tag") == 0))
#ifndef _NO_TAGS
		exit_code = tags_function(comm);
#else
	{
		fprintf(stderr, "%s: tag: %s\n", PROGRAM_NAME, NOT_AVAILABLE);
		return EXIT_FAILURE;
	}
#endif

	/*     ################# NEW FILE ##################     */
	else if (*comm[0] == 'n' && (!comm[0][1] || strcmp(comm[0], "new") == 0))
		exit_code = create_file(comm);

	/*     ############### DUPLICATE FILE ##################     */
	else if (*comm[0] == 'd' && (!comm[0][1] || strcmp(comm[0], "dup") == 0))
		exit_code = dup_file(comm);

#if defined(__HAIKU__)// || defined(__APPLE__)
	else if ((*comm[0] == 'c' || *comm[0] == 'r' || *comm[0] == 'm'
	|| *comm[0] == 't' || *comm[0] == 'u' || *comm[0] == 'l')
	&& (strcmp(comm[0], "cp") == 0 || strcmp(comm[0], "rm") == 0
	|| strcmp(comm[0], "mkdir") == 0 || strcmp(comm[0], "unlink") == 0
	|| strcmp(comm[0], "touch") == 0 || strcmp(comm[0], "ln") == 0
	|| strcmp(comm[0], "chmod") == 0))
		return (exit_code = run_and_refresh(comm));
#endif

	/*     ############### COPY AND MOVE ##################     */
	else if ((*comm[0] == 'c' && (!comm[0][1] || (comm[0][1] == 'p'
	&& !comm[0][2])))

	|| (*comm[0] == 'm' && (!comm[0][1] || (comm[0][1] == 'v'
	&& !comm[0][2])))

	|| (*comm[0] == 'v' && (!comm[0][1] || (comm[0][1] == 'v'
	&& !comm[0][2])))

	|| (*comm[0] == 'p' && strcmp(comm[0], "paste") == 0)) {
		int copy_and_rename = 0;
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

			set_cp_cmd(&comm[0]);
		} else if (*comm[0] == 'm' && !comm[0][1]) {
			if (comm[1] && IS_HELP(comm[1])) {
				puts(_(WRAPPERS_USAGE));
				return EXIT_SUCCESS;
			}
			if (!sel_is_last && comm[1] && !comm[2])
				xrename = 1;
			set_mv_cmd(&comm[0]);
		}

		kbind_busy = 1;
		exit_code = copy_function(comm, copy_and_rename);
		kbind_busy = 0;
	}

	/*         ############### TRASH ##################     */
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
	else if (*comm[0] == 's' && (!comm[0][1] || strcmp(comm[0], "sel") == 0))
		return (exit_code = sel_function(comm));

	else if (*comm[0] == 's' && (strcmp(comm[0], "sb") == 0
	|| strcmp(comm[0], "selbox") == 0)) {
		show_sel_files(); return EXIT_SUCCESS;
	}

	else if (*comm[0] == 'd' && (strcmp(comm[0], "ds") == 0
	|| strcmp(comm[0], "desel") == 0))
		return (exit_code = desel_function(comm));

	/*  ############# SOME SHELL CMD WRAPPERS ###############  */

	else if ((*comm[0] == 'r' || *comm[0] == 'm' || *comm[0] == 'l')
	&& (strcmp(comm[0], "r") == 0 || strcmp(comm[0], "l") == 0
	|| strcmp(comm[0], "md") == 0 || strcmp(comm[0], "le") == 0)) {
		/* This help is only for c, l, le, m, r, t, and md commands */
		if (comm[1] && IS_HELP(comm[1])) {
			puts(_(WRAPPERS_USAGE)); return EXIT_SUCCESS;
		}

		if (*comm[0] == 'l' && !comm[0][1]) {
			comm[0] = (char *)xrealloc(comm[0], 7 * sizeof(char));
#if defined(_BE_POSIX)
			strcpy(comm[0], "ln -s");
#else
			strcpy(comm[0], "ln -sn");
#endif
			/* Make sure the symlink source is always an absolute path */
			if (comm[1] && *comm[1] != '/' && *comm[1] != '~') {
				size_t len = strlen(comm[1]);
				char *tmp = (char *)xnmalloc(len + 1, sizeof(char));
				xstrsncpy(tmp, comm[1], len);
				comm[1] = (char *)xrealloc(comm[1], (len
						+ strlen(workspaces[cur_ws].path) + 2) * sizeof(char));
				sprintf(comm[1], "%s/%s", workspaces[cur_ws].path, tmp);
				free(tmp);
			}
		} else if (*comm[0] == 'r' && !comm[0][1]) {
			exit_code = remove_file(comm);
			goto CHECK_EVENTS;
		} else if (*comm[0] == 'm' && comm[0][1] == 'd' && !comm[0][2]) {
			comm[0] = (char *)xrealloc(comm[0], 9 * sizeof(char));
			strcpy(comm[0], "mkdir -p");
		}

		if (*comm[0] == 'l' && comm[0][1] == 'e' && !comm[0][2]) {
			if (!comm[1]) {
				fprintf(stderr, "%s\n", _(LE_USAGE));
				return (exit_code = EXIT_FAILURE);
			}
			exit_code = edit_link(comm[1]);
			goto CHECK_EVENTS;
		} else if (*comm[0] == 'l' && comm[0][1] == 'n' && !comm[0][2]) {
			if (comm[1] && (strcmp(comm[1], "edit") == 0
			|| strcmp(comm[1], "e") == 0)) {
				if (!comm[2]) {
					fprintf(stderr, "%s\n", _(LE_USAGE));
					return (exit_code = EXIT_FAILURE);
				}
				exit_code = edit_link(comm[2]);
				goto CHECK_EVENTS;
			}
		}

		kbind_busy = 1;
		exit_code = run_and_refresh(comm);
		kbind_busy = 0;
	}

	/*    ############### TOGGLE EXEC ##################     */
	else if (*comm[0] == 't' && comm[0][1] == 'e' && !comm[0][2])
		return (exit_code = _toggle_exec(comm));

	/*    ############### (UN)PIN FILE ##################     */
	else if (*comm[0] == 'p' && strcmp(comm[0], "pin") == 0)
		return (exit_code = pin_function(comm[1]));

	else if (*comm[0] == 'u' && strcmp(comm[0], "unpin") == 0)
		return (exit_code = unpin_dir());

	else if (*comm[0] == 'p' && strcmp(comm[0], "prompt") == 0)
		return (exit_code = prompt_function(comm[1] ? comm[1] : NULL));

	/*    ############### PROPERTIES ##################     */
	else if (*comm[0] == 'p' && (!comm[0][1] || strcmp(comm[0], "pr") == 0
	|| strcmp(comm[0], "pp") == 0 || strcmp(comm[0], "prop") == 0))
		return (exit_code = props_function(comm));

	/*     ############### SEARCH ##################     */
	else if ( (*comm[0] == '/' && is_path(comm[0]) == 0)
	|| (*comm[0] == '/' && !comm[0][1] && comm[1] && *comm[1] == '-' && IS_HELP(comm[1])) )
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

	/*    ########## FILE NAMES CLEANER ############## */
	else if (*comm[0] == 'b' && ((comm[0][1] == 'b' && !comm[0][2])
	|| strcmp(comm[0], "bleach") == 0)) {
		if (comm[1] && IS_HELP(comm[1])) {
			puts(_(BLEACH_USAGE));
			return EXIT_SUCCESS;
		}
#ifndef _NO_BLEACH
		exit_code = bleach_files(comm);
#else
		fprintf(stderr, _("%s: bleach: %s\n"), PROGRAM_NAME, NOT_AVAILABLE);
		return EXIT_FAILURE;
#endif
	}

	/*   ################ ARCHIVER ##################     */
	else if (*comm[0] == 'a' && ((comm[0][1] == 'c' || comm[0][1] == 'd')
	&& !comm[0][2])) {
#ifndef _NO_ARCHIVING
		if (!comm[1] || IS_HELP(comm[1])) {
			puts(_(ARCHIVE_USAGE));
			return EXIT_SUCCESS;
		}

		if (comm[0][1] == 'c')
			exit_code = archiver(comm, 'c');
		else
			exit_code = archiver(comm, 'd');
#else
		fprintf(stderr, _("%s: archiver: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
		return EXIT_FAILURE;
#endif
	}

	/* ##################################################
	 * #                 MINOR FUNCTIONS                #
	 * ##################################################*/

	else if (*comm[0] == 'w' && comm[0][1] == 's' && !comm[0][2])
		return (exit_code = handle_workspaces(comm[1] ? comm[1] : NULL));

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
		return (exit_code = export_function(comm));

	else if (*comm[0] == 'o' && strcmp(comm[0], "opener") == 0)
		return (exit_code = opener_function(comm[1]));

	else if (*comm[0] == 't' && strcmp(comm[0], "tips") == 0) {
		print_tips(1); return EXIT_SUCCESS;
	}

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

	else if (!autols && *comm[0] == 'l' && comm[0][1] == 's' && !comm[0][2])
		return (exit_code = ls_function());

	/* #### PROFILE #### */
	else if (*comm[0] == 'p' && ((comm[0][1] == 'f' && !comm[0][2])
	|| strcmp(comm[0], "prof") == 0 || strcmp(comm[0], "profile") == 0))
		return (exit_code = profile_function(comm));

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
		return (exit_code = ext_args_function(comm[1]));

	/* #### PAGER #### */
	else if (*comm[0] == 'p' && ((comm[0][1] == 'g' && !comm[0][2])
	|| strcmp(comm[0], "pager") == 0))
		return (exit_code = pager_function(comm[1]));

	/* #### FILES COUNTER #### */
	else if (*comm[0] == 'f' && ((comm[0][1] == 'c' && !comm[0][2])
	|| strcmp(comm[0], "filescounter") == 0))
		return (exit_code = filescounter_function(comm[1]));

	/* #### UNICODE #### */
	else if (*comm[0] == 'u' && ((comm[0][1] == 'c' && !comm[0][2])
	|| strcmp(comm[0], "unicode") == 0))
		return (exit_code = unicode_function(comm[1]));

	/* #### DIRECTORIES FIRST #### */
	else if ((*comm[0] == 'f' && comm[0][1] == 'f' && !comm[0][2])
	|| (*comm[0] == 'd' && strcmp(comm[0], "dirs-first") == 0))
		return (exit_code = dirs_first_function(comm[1]));

	/* #### LOG #### */
	else if (*comm[0] == 'l' && strcmp(comm[0], "log") == 0)
		return (exit_code = _log_function(comm));

	/* #### MESSAGES #### */
	else if (*comm[0] == 'm' && (strcmp(comm[0], "msg") == 0
	|| strcmp(comm[0], "messages") == 0))
		return (exit_code = msgs_function(comm[1]));

	/* #### ALIASES #### */
	else if (*comm[0] == 'a' && strcmp(comm[0], "alias") == 0)
		return (exit_code = alias_function(comm));

	/* #### EDIT #### */
	else if (*comm[0] == 'e' && strcmp(comm[0], "edit") == 0)
		return (exit_code = edit_function(comm));

	/* #### HISTORY #### */
	else if (*comm[0] == 'h' && strcmp(comm[0], "history") == 0)
		return (exit_code = history_function(comm));

	/* #### HIDDEN FILES #### */
	else if (*comm[0] == 'h' && ((comm[0][1] == 'f' && !comm[0][2])
	|| strcmp(comm[0], "hidden") == 0))
		return (exit_code = _hidden_function(comm));

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

	else if (strcmp(comm[0], "path") == 0 || strcmp(comm[0], "cwd") == 0) {
		return (exit_code = print_cwd());
	}

	else if ((*comm[0] == '?' && !comm[0][1]) || strcmp(comm[0], "help") == 0) {
		return (exit_code = quick_help(comm[1] ? comm[1] : NULL));
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

		/* #  EXTERNAL/SHELL COMMANDS # */
		if ((exit_code = run_shell_cmd(comm)) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}

CHECK_EVENTS:
	if (autols == 0)
		return exit_code;
#ifdef LINUX_INOTIFY
	if (watch)
		read_inotify();
#elif defined(BSD_KQUEUE)
	if (watch && event_fd >= 0)
		read_kqueue();
#endif
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
		if (exec_cmd(cmd) != 0)
			*err_code = 1;
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
//	flags &= ~RUNNING_SHELL_CMD;

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

		if (int_vars == 1 && strchr(line, '=') && !_ISDIGIT(*line))
			create_usr_var(line);
		else
			run_profile_line(line);
	}

	free(line);
	fclose(fp);
}
