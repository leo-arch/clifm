/* remote.c -- functions to manage remotes */

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
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "aux.h"
#include "exec.h"
#include "listing.h"
#include "navigation.h"
#include "history.h"
#include "mime.h"
#include "misc.h"
#include "jump.h"
#include "messages.h"
#include "file_operations.h"

static int
remotes_list(void)
{
	if (!remotes_n) {
		printf(_("%s: No remotes defined\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	size_t i;
	for (i = 0; i < remotes_n; i++) {
		if (!remotes[i].name)
			continue;
		printf(_("Name: %s\n"), remotes[i].name);
		if (remotes[i].desc)
			printf(_(" Comment: %s\n"), remotes[i].desc);
		if (remotes[i].mountpoint)
			printf(_(" Mountpoint: %s\n"), remotes[i].mountpoint);
		if (remotes[i].mount_cmd)
			printf(_(" Mount command: %s\n"), remotes[i].mount_cmd);
		if (remotes[i].unmount_cmd)
			printf(_(" Unmount command: %s\n"), remotes[i].unmount_cmd);
		printf(_(" Auto-unmount: %s\n"), (remotes[i].auto_unmount == 0)
				? _("false") : _("true"));
		printf(_(" Auto-mount: %s\n"), (remotes[i].auto_mount == 0)
				? _("false") : _("true"));
		printf(_(" Mounted: %s\n"), (remotes[i].mounted == 0) ? _("No")
				: _("Yes"));
		if (i < remotes_n - 1)
			puts("");
	}
	return EXIT_SUCCESS;
}

static int
dequote_remote_name(char *name)
{
	char *deq = (char *)NULL;
	if (strchr(name, '\\')) {
		deq = dequote_str(name, 0);
		if (deq) {
			strcpy(name, deq);
			free(deq);
		} else {
			fprintf(stderr, "%s: %s: Error dequoting resource name\n",
					PROGRAM_NAME, name);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

/* Get the index of the remote named NAME from the remotes list. Returns
 * this index incase of success and -1 in case of error */
static int
get_remote(char *name)
{
	if (!name || !*name)
		return (-1);

	if (dequote_remote_name(name) == EXIT_FAILURE)
		return (-1);

	int i = (int)remotes_n,
		found = 0;

	while (--i >= 0) {
		if (*name == *remotes[i].name && strcmp(name, remotes[i].name) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, _("%s: %s: No such remote\n"), PROGRAM_NAME, name);
		return (-1);
	}

	if (!remotes[i].mountpoint) {
		fprintf(stderr, _("%s: No mountpoint specified for '%s'\n"), PROGRAM_NAME,
				remotes[i].name);
		return (-1);
	}

	return i;
}

static int
remotes_mount(char *name)
{
	int i = get_remote(name);
	if (i == -1)
		return EXIT_FAILURE;

	if (!remotes[i].mount_cmd) {
		fprintf(stderr, _("%s: No mount command specified for '%s'\n"),
			PROGRAM_NAME, remotes[i].name);
		return EXIT_FAILURE;
	}

	struct stat attr;
	if (lstat(remotes[i].mountpoint, &attr) == -1) {
		char *cmd[] = {"mkdir", "-p", remotes[i].mountpoint, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			fprintf(stderr, _("%s: %s: %s\n"), PROGRAM_NAME,
					remotes[i].mountpoint, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	if (count_dir(remotes[i].mountpoint, CPOP) <= 2
	&& launch_execle(remotes[i].mount_cmd) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	if (xchdir(remotes[i].mountpoint, SET_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				remotes[i].mountpoint, strerror(errno));
		return EXIT_FAILURE;
	}

	int exit_status = EXIT_SUCCESS;
	if (autols) {
		free(ws[cur_ws].path);
		ws[cur_ws].path = savestring(remotes[i].mountpoint,
							strlen(remotes[i].mountpoint));
		add_to_jumpdb(ws[cur_ws].path);
		add_to_dirhist(ws[cur_ws].path);
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	} else {
		printf(_("%s: %s: Remote mounted on %s\n"), PROGRAM_NAME,
				remotes[i].name, remotes[i].mountpoint);
	}

	remotes[i].mounted = 1;
	return exit_status;
}

static int
remotes_unmount(char *name)
{
	int i = get_remote(name);
	if (i == -1)
		return EXIT_FAILURE;

	if (!remotes[i].unmount_cmd) {
		fprintf(stderr, _("%s: No unmount command found for '%s'\n"),
			PROGRAM_NAME, remotes[i].name);
		return EXIT_FAILURE;
	}

	if (launch_execle(remotes[i].unmount_cmd) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	remotes[i].mounted = 0;
	return EXIT_SUCCESS;
}

static int
remotes_edit(char *app)
{
	if (!remotes_file)
		return EXIT_FAILURE;

	struct stat attr;
	if (stat(remotes_file, &attr) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, remotes_file,
				strerror(errno));
		return EXIT_FAILURE;
	}

	time_t mtime_bfr = (time_t)attr.st_mtime;

	int ret = EXIT_SUCCESS;
	if (app) {
		char *cmd[] = {app, remotes_file, NULL};
		ret = launch_execve(cmd, FOREGROUND, E_NOSTDERR);
	} else {
		ret = open_file(remotes_file);
	}

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	if (stat(remotes_file, &attr) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, remotes_file,
				strerror(errno));
		return EXIT_FAILURE;
	}

	if (mtime_bfr != (time_t)attr.st_mtime) {
		free_remotes(0);
		load_remotes();
	}

	return EXIT_SUCCESS;
}

int
remotes_function(char **args)
{
	if (xargs.stealth_mode == 1) {
		fprintf(stderr, "%s: The net function is disabled in stealth mode\n",
				PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (!args[1])
		return remotes_list();

	if (*args[1] == '-' && strcmp(args[1], "--help") == 0) {
		puts(_(NET_USAGE));
		return EXIT_SUCCESS;
	}

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0) {
		if (args[2])
			return remotes_edit(args[2]);
		return remotes_edit(NULL);
	}

	if (*args[1] == 'u' && (!*(args[1] + 1) || strcmp(args[1], "unmount") == 0)) {
		if (!args[2]) {
			fprintf(stderr, "%s\n", _(NET_USAGE));
			return EXIT_FAILURE;
		}
		return remotes_unmount(args[2]);
	}

	if (*args[1] == 'm' && (!*(args[1] + 1) || strcmp(args[1], "mount") == 0)) {
		if (!args[2]) {
			fprintf(stderr, "%s\n", _(NET_USAGE));
			return EXIT_FAILURE;
		}
		return remotes_mount(args[2]);
	}

	return remotes_mount(args[1]);
}

int
automount_remotes(void)
{
	if (!remotes_n)
		return EXIT_SUCCESS;

	int i = (int)remotes_n,
		exit_status = EXIT_SUCCESS;
	while (--i >= 0) {
		if (remotes[i].name && remotes[i].auto_mount == 1
		&& remotes[i].mountpoint && remotes[i].mount_cmd) {
			struct stat attr;
			if (stat(remotes[i].mountpoint, &attr) == -1) {
				char *cmd[] = {"mkdir", "-p", remotes[i].mountpoint, NULL};
				if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
					continue;
			} else if (count_dir(remotes[i].mountpoint, CPOP) > 2) {
				continue;
			}
			printf(_("%s: Mounting remote...\n"), remotes[i].name);
			if (launch_execle(remotes[i].mount_cmd) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
			else
				remotes[i].mounted = 1;
		}
	}

	return exit_status;
}

int
autounmount_remotes(void)
{
	if (!remotes_n)
		return EXIT_SUCCESS;

	int i = (int)remotes_n,
		exit_status = EXIT_SUCCESS;
	while (--i >= 0) {
		if (remotes[i].name && remotes[i].auto_unmount == 1
		&& remotes[i].mountpoint && remotes[i].unmount_cmd) {
			if (count_dir(remotes[i].mountpoint, CPOP) <= 2)
				continue;
			int dir_change = 0;
			if (*ws[cur_ws].path == *remotes[i].mountpoint
			&& strcmp(remotes[i].mountpoint, ws[cur_ws].path) == 0) {
				xchdir("/", NO_TITLE);
				dir_change = 1;
			}
			printf(_("%s: Unmounting remote...\n"), remotes[i].name);
			if (launch_execle(remotes[i].unmount_cmd) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
			if (dir_change)
				xchdir(ws[cur_ws].path, NO_TITLE);
		}
	}

	return exit_status;
}
