/* actions.c -- a few functions for the plugins systems */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
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
#include <unistd.h>

#include "aux.h"
#include "checks.h"
#include "exec.h"
#include "file_operations.h"
#include "init.h"
#include "mime.h"
#include "misc.h"

/* The core of this function was taken from NNN's run_selected_plugin
 * function and modified to fit our needs. Thanks NNN! */
int
run_action(char *action, char **args)
{
	if (!action)
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;
	char *cmd = (char *)NULL;
	size_t len = 0,
		   action_len = strlen(action);

		/* #####################################
		 * #    1) CREATE CMD TO BE EXECUTED   #
		 * ##################################### */

	/* Remove terminating new line char */
	if (action[action_len - 1] == '\n')
		action[action_len - 1] = '\0';

	if (strchr(action, '/')) {
		len = action_len;
		cmd = savestring(action, len);
	}

	/* If not a path, PLUGINS_DIR is assumed */
	else {
		len = action_len + strlen(PLUGINS_DIR) + 2;
		cmd = (char *)xnmalloc(len, sizeof(char));
		sprintf(cmd, "%s/%s", PLUGINS_DIR, action);
	}

	/* Check if the action file exists and is executable */
	if (access(cmd, F_OK | X_OK) == -1) {
		fprintf(stderr, "actions: %s: %s\n", cmd, strerror(errno));
		free(cmd);
		return EXIT_FAILURE;
	}

	/* Append arguments to command */
	size_t i;
	for (i = 1; args[i]; i++) {
		len += strlen(args[i]) + 2;
		cmd = (char *)xrealloc(cmd, len * sizeof(char));
		strcat(cmd, " ");
		strcat(cmd, args[i]);
	}

			/* ##############################
			 * #    2) CREATE A PIPE FILE   #
			 * ############################## */

	char *rand_ext = gen_rand_str(6);

	if (!rand_ext) {
		free(cmd);
		return EXIT_FAILURE;
	}

	char fifo_path[PATH_MAX];
	sprintf(fifo_path, "%s/.pipe.%s", TMP_DIR, rand_ext);
	free(rand_ext);

	setenv("CLIFM_BUS", fifo_path, 1);

	if (mkfifo(fifo_path, 0600) != EXIT_SUCCESS) {
		free(cmd);
		printf("%s: %s\n", fifo_path, strerror(errno));
		return EXIT_FAILURE;
	}

	/* ################################################
	 * #   3) EXEC CMD & LET THE CHILD WRITE TO PIPE  #
	 * ################################################ */

	/* Set terminal title to plugin name */
	if (xargs.cwd_in_title == 1)
		set_term_title(action);

	if (fork() == EXIT_SUCCESS) {

		/* Child: write-only end of the pipe */
		int wfd = open(fifo_path, O_WRONLY | O_CLOEXEC);

		if (wfd == -1)
			_exit(EXIT_FAILURE);

		launch_execle(cmd);

		close(wfd);
		_exit(EXIT_SUCCESS);
	}

	free(cmd);

		/* ########################################
		 * #    4) LET THE PARENT READ THE PIPE   #
		 * ######################################## */

	/* Parent: read-only end of the pipe */
	int rfd;

	do
		rfd = open(fifo_path, O_RDONLY);
	while (rfd == -1 && errno == EINTR);

	char buf[PATH_MAX] = "";
	ssize_t buf_len = 0;

	do
		buf_len = read(rfd, buf, sizeof(buf));
	while (buf_len == -1 && errno == EINTR);

	close(rfd);

	/* If the pipe is empty */
	if (!*buf) {
		unlink(fifo_path);
		if (xargs.cwd_in_title == 1)
			set_term_title(ws[cur_ws].path);
		return EXIT_SUCCESS;
	}

	if (buf[buf_len - 1] == '\n')
		buf[buf_len - 1] = '\0';

	/* If a valid file */
	struct stat attr;

	if (lstat(buf, &attr) != -1) {
		char *o_cmd[] = {"o", buf, NULL};
		exit_status = open_function(o_cmd);
	}

	/* If not a file, take it as a command*/
	else {
		size_t old_args = args_n;
		args_n = 0;

		char **_cmd = parse_input_str(buf);

		if (_cmd) {

			char **alias_cmd = check_for_alias(_cmd);

			if (alias_cmd) {
				exit_status = exec_cmd(alias_cmd);

				for (i = 0; alias_cmd[i]; i++)
					free(alias_cmd[i]);

				free(alias_cmd);
			}

			else {
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
		set_term_title(ws[cur_ws].path);

	return exit_status;
}

int
edit_actions(void)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: Access to configuration files is not allowed in "
		       "stealth mode\n", PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	/* Get actions file's current modification time */
	struct stat file_attrib;

	if (stat(ACTIONS_FILE, &file_attrib) == -1) {
		fprintf(stderr, "actions: %s: %s\n", ACTIONS_FILE, strerror(errno));
		return EXIT_FAILURE;
	}

	time_t mtime_bfr = (time_t)file_attrib.st_mtime;

	char *cmd[] = {"mm", ACTIONS_FILE, NULL};

	int ret = mime_open(cmd);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* Get modification time after opening the file */
	stat(ACTIONS_FILE, &file_attrib);

	/* If modification times differ, the file was modified after being
	 * opened */
	if (mtime_bfr != (time_t)file_attrib.st_mtime) {

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
	}

	return EXIT_SUCCESS;
}
