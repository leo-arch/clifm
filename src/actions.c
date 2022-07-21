/* actions.c -- A few functions for the plugins system */

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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "aux.h"
#include "checks.h"
#include "exec.h"
#include "file_operations.h"
#include "init.h"
#include "mime.h"
#include "misc.h"

/* Get the executable's path of the action ACTION
 * Returns this path on success and NULL on error, in which case STATUS
 * is set to the appropriate error code */
static char *
get_plugin_path(char *action, int *status)
{
	size_t action_len = strlen(action);

	/* Remove terminating new line char */
	if (action_len > 0 && action[action_len - 1] == '\n')
		action[action_len - 1] = '\0';

	char *cmd = (char *)NULL;
	int dir_path = 0;

	if (strchr(action, '/')) {
		cmd = (char *)xnmalloc(action_len + 1, sizeof(char));
		strcpy(cmd, action); /* NOLINT */
		dir_path = 1;
	} else { /* If not a path, PLUGINS_DIR is assumed */
		if (!plugins_dir || !*plugins_dir) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("actions: Plugins directory not defined\n"));
			*status = EXIT_FAILURE;
			return (char *)NULL;
		}
		cmd = (char *)xnmalloc(action_len + strlen(plugins_dir) + 2, sizeof(char));
		sprintf(cmd, "%s/%s", plugins_dir, action); /* NOLINT */
	}

	/* Check if the action file exists and is executable */
	if (access(cmd, X_OK) == -1) {
		/* If not in local dir, check system data dir as well */
		if (data_dir && dir_path == 0) {
			cmd = (char *)xrealloc(cmd, (action_len + strlen(data_dir)
				+ strlen(PNL) + 11) * sizeof(char));
			sprintf(cmd, "%s/%s/plugins/%s", data_dir, PNL, action); /* NOLINT */
			if (access(cmd, X_OK) == -1) {
				_err(ERR_NO_STORE, NOPRINT_PROMPT, "actions: %s: %s\n",	cmd, strerror(errno));
				free(cmd);
				*status = errno;
				return (char *)NULL;
			}
		} else {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "actions: %s: %s\n",	cmd, strerror(errno));
			free(cmd);
			*status = errno;
			return (char *)NULL;
		}
	}

	return cmd;
}

/* The core of this function was taken from NNN's run_selected_plugin
 * function and modified to fit our needs. Thanks NNN! */
int
run_action(char *action, char **args)
{
	if (!action || !*action)
		return EXIT_FAILURE;

		/* #####################################
		 * #    1) CREATE CMD TO BE EXECUTED   #
		 * ##################################### */

	int s = EXIT_SUCCESS;
	char *cmd = get_plugin_path(action, &s);
	if (!cmd)
		return s;

	size_t cmd_len = strlen(cmd);
	args[0] = (char *)xrealloc(args[0], (cmd_len + 1) * sizeof(char));
	xstrsncpy(args[0], cmd, cmd_len);

	free(cmd);

			/* ##############################
			 * #    2) CREATE A PIPE FILE   #
			 * ############################## */

	char *rand_ext = gen_rand_str(6);
	if (!rand_ext)
		return EXIT_FAILURE;

	char fifo_path[PATH_MAX];
	snprintf(fifo_path, sizeof(fifo_path), "%s/.pipe.%s", tmp_dir, rand_ext); /* NOLINT */
	free(rand_ext);

	setenv("CLIFM_BUS", fifo_path, 1);

	if (mkfifo(fifo_path, 0600) != EXIT_SUCCESS) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "actions: %s: %s\n",	fifo_path, strerror(errno));
		unsetenv("CLIFM_BUS");
		return EXIT_FAILURE;
	}

	/* ################################################
	 * #   3) EXEC CMD & LET THE CHILD WRITE TO PIPE  #
	 * ################################################ */

	/* Set terminal title to plugin name */
	if (xargs.cwd_in_title == 1)
		set_term_title(action);

	pid_t pid = fork();

	if (pid == 0) {
		/* Child: write-only end of the pipe */
		int wfd = open(fifo_path, O_WRONLY | O_CLOEXEC);
		if (wfd == -1)
			_exit(EXIT_FAILURE);

		int ret = launch_execve(args, FOREGROUND, E_NOFLAG);
		/* RET will be read later by waitpid(3) to get plugin exit status */
		close(wfd);
		_exit(ret);
	}

		/* ########################################
		 * #    4) LET THE PARENT READ THE PIPE   #
		 * ######################################## */

	/* Parent: read-only end of the pipe */
	int rfd;

	do
		rfd = open(fifo_path, O_RDONLY);
	while (rfd == -1 && errno == EINTR);

	char buf[PATH_MAX];
	*buf = '\0';
	ssize_t buf_len = 0;

	do {
		buf_len = read(rfd, buf, sizeof(buf)); /* flawfinder: ignore */
	} while (buf_len == -1 && errno == EINTR);

	buf[buf_len] = '\0';
	close(rfd);

	/* Wait for the child to finish. Otherwise, the child is left as
	 * zombie process. Store plugin exit status in EXIT_STATUS */
	int status = 0, exit_status = 0;
	if (waitpid(pid, &status, 0) > 0) {
			if (WIFEXITED(status) && !WEXITSTATUS(status)) {
			exit_status = EXIT_SUCCESS;
		} else if (WIFEXITED(status) && WEXITSTATUS(status)) {
			exit_status = WEXITSTATUS(status);
		} else {
			exit_status = EXCRASHERR;
		}
	} else {
		exit_status = errno;
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "actions: waitpid: %s\n", strerror(errno));
	}

	/* If the pipe is empty */
	if (!*buf) {
		unlink(fifo_path);
		if (xargs.cwd_in_title == 1)
			set_term_title(workspaces[cur_ws].path);
		unsetenv("CLIFM_BUS");
		return exit_status;
	}

	if (buf[buf_len - 1] == '\n')
		buf[buf_len - 1] = '\0';

	/* If a valid file */
	struct stat attr;
	if (lstat(buf, &attr) != -1) {
		char *o_cmd[] = {"o", buf, NULL};
		exit_status = open_function(o_cmd);
	} else { /* If not a file, take it as a command*/
		size_t old_args = args_n;
		args_n = 0;

		char **_cmd = parse_input_str(buf);
		if (_cmd) {
			size_t i;
			char **alias_cmd = check_for_alias(_cmd);
			if (alias_cmd) {
				exit_status = exec_cmd(alias_cmd);
				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);
				free(alias_cmd);
			} else {
				exit_status = exec_cmd(_cmd);
				for (i = 0; i <= args_n; i++)
					free(_cmd[i]);
				free(_cmd);
			}
		}

		args_n = old_args;
	}

	/* Remove the pipe file */
	unlink(fifo_path);

	if (xargs.cwd_in_title == 1)
		set_term_title(workspaces[cur_ws].path);

	unsetenv("CLIFM_BUS");
	return exit_status;
}

static int
edit_actions(char *app)
{
	if (xargs.stealth_mode == 1) {
		printf("actions: Access to configuration files is not allowed in stealth mode\n");
		return EXIT_SUCCESS;
	}

	if (!actions_file)
		return EXIT_FAILURE;

	/* Get actions file's current modification time */
	struct stat attr;
	if (stat(actions_file, &attr) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "actions: %s: %s\n",	actions_file, strerror(errno));
		return EXIT_FAILURE;
	}

	time_t mtime_bfr = (time_t)attr.st_mtime;

	int ret = EXIT_SUCCESS;

	if (app && *app) {
		char *cmd[] = {app, actions_file, NULL};
		ret = launch_execve(cmd, FOREGROUND, E_NOSTDERR);
	} else {
		open_in_foreground = 1;
		ret = open_file(actions_file);
		open_in_foreground = 0;
	}

	if (ret != EXIT_SUCCESS)
		return ret;

	/* Get modification time after opening the file */
	stat(actions_file, &attr);

	if (mtime_bfr == (time_t)attr.st_mtime)
		return EXIT_SUCCESS;

	/* If modification times differ, the file was modified after being opened */
	/* Reload the array of available actions */
	if (load_actions() != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* Reload PATH commands as well to add new action(s) */
	if (bin_commands) {
		size_t i;
		for (i = 0; bin_commands[i]; i++)
			free(bin_commands[i]);
		free(bin_commands);
		bin_commands = (char **)NULL;
	}

	if (paths) {
		size_t i;
		for (i = 0; i < path_n; i++)
			free(paths[i]);
	}

	path_n = (size_t)get_path_env();
	get_path_programs();

	print_reload_msg("File modified. Actions reloaded\n");
	return EXIT_SUCCESS;
}

static size_t
get_largest_action_name(void)
{
	int i = (int)actions_n;
	size_t largest = 0;
	while (--i >= 0) {
		size_t len = strlen(usr_actions[i].name);
		if (len > largest)
			largest = len;
	}

	return largest;
}

int
actions_function(char **args)
{
	if (!args[1]) {
		if (actions_n > 0) {
			/* Just list available actions */
			printf(_("To run a plugin just enter its action name\n"
				"Example: enter '-' to run the fzfnav plugin\n"));
			size_t i, largest = get_largest_action_name();
			for (i = 0; i < actions_n; i++) {
				printf("%-*s %s->%s %s\n", (int)largest, usr_actions[i].name,
				    mi_c, df_c, usr_actions[i].value);
			}
			return EXIT_SUCCESS;
		} else {
			printf(_("actions: No actions defined. Use the 'actions edit' "
				"command to add new actions\n"));
			return EXIT_FAILURE;
		}

	} else if (strcmp(args[1], "edit") == 0) {
		return edit_actions(args[2] ? args[2] : NULL);

	} else if (IS_HELP(args[1])) {
		puts(_(ACTIONS_USAGE));
		return EXIT_SUCCESS;

	} else {
		fprintf(stderr, "%s\n", _(ACTIONS_USAGE));
		return EXIT_FAILURE;
	}
}
