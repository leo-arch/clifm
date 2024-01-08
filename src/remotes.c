/* remote.c -- functions to manage remotes */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
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

#include "aux.h"
#include "exec.h"
#include "file_operations.h"
#include "history.h"
#include "init.h"
#include "jump.h"
#include "listing.h"
#include "messages.h"
#include "misc.h"
#include "navigation.h"
#include "sanitize.h"

static int
remotes_list(void)
{
	if (remotes_n == 0) {
		printf(_("%s: No remotes defined. Run 'net edit' to add one.\n"),
			PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	size_t i;
	for (i = 0; i < remotes_n; i++) {
		if (!remotes[i].name)
			continue;
		printf(_("Name: %s%s%s\n"), BOLD, remotes[i].name, df_c);
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
		printf(_(" Mounted: %s%s%s\n"), BOLD, (remotes[i].mounted == 0)
			? _("No") : _("Yes"), df_c);
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
		deq = unescape_str(name, 0);
		if (deq) {
			xstrsncpy(name, deq, strlen(deq) + 1);
			free(deq);
		} else {
			xerror("net: %s: Error unescaping resource name\n", name);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

/* Get the index of the remote named NAME from the remotes list. Returns
 * this index in case of success or -1 in case of error */
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

	if (found == 0) {
		xerror(_("net: '%s': No such remote\n"), name);
		return (-1);
	}

	if (!remotes[i].mountpoint) {
		xerror(_("net: No mountpoint specified for '%s'\n"), remotes[i].name);
		return (-1);
	}

	return i;
}

static inline int
create_mountpoint(const int i)
{
	char *cmd[] = {"mkdir", "-p", remotes[i].mountpoint, NULL};

	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
		xerror("net: '%s': %s\n", remotes[i].mountpoint, strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static inline int
cd_to_mountpoint(const int i)
{
	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(remotes[i].mountpoint,
		strlen(remotes[i].mountpoint));
	add_to_jumpdb(workspaces[cur_ws].path);
	add_to_dirhist(workspaces[cur_ws].path);

	free_dirlist();
	if (list_dir() != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static inline int
print_cd_error(const int i)
{
	xerror("net: '%s': %s\n", remotes[i].mountpoint, strerror(errno));
	return EXIT_FAILURE;
}

static inline int
print_no_mount_cmd_error(const int i)
{
	xerror(_("net: No mount command specified for '%s'\n"), remotes[i].name);
	return EXIT_FAILURE;
}

static int
remotes_mount(char *name)
{
	int i = get_remote(name);
	if (i == -1)
		return EXIT_FAILURE;

	if (!remotes[i].mount_cmd)
		return print_no_mount_cmd_error(i);

	if (xargs.secure_cmds == 1
	&& sanitize_cmd(remotes[i].mount_cmd, SNT_NET) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* If mountpoint doesn't exist, create it */
	struct stat attr;
	if (stat(remotes[i].mountpoint, &attr) == -1
	&& create_mountpoint(i) == EXIT_FAILURE)
		return EXIT_FAILURE;

	/* Make sure mountpoint is not populated and run the mount command */
	if (count_dir(remotes[i].mountpoint, CPOP) <= 2
	&& launch_execl(remotes[i].mount_cmd) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	if (xchdir(remotes[i].mountpoint, SET_TITLE) == -1)
		return print_cd_error(i);

	int exit_status = EXIT_SUCCESS;
	if (conf.autols == 1) {
		exit_status = cd_to_mountpoint(i);
	} else {
		printf(_("%s: '%s': Remote mounted on %s\n"), PROGRAM_NAME,
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

	if (remotes[i].mounted == 0) {
		xerror(_("net: '%s': Not mounted\n"), remotes[i].name);
		return EXIT_FAILURE;
	}

	if (!remotes[i].mountpoint) {
		xerror(_("net: Error getting mountpoint for '%s'\n"), remotes[i].name);
		return EXIT_FAILURE;
	}

	if (!remotes[i].unmount_cmd) {
		xerror(_("net: No unmount command for '%s'\n"), remotes[i].name);
		return EXIT_FAILURE;
	}

	if (xargs.secure_cmds == 1
	&& sanitize_cmd(remotes[i].unmount_cmd, SNT_NET) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* Get out of mountpoint before unmounting */
	size_t mlen = strlen(remotes[i].mountpoint);
	if (strncmp(remotes[i].mountpoint, workspaces[cur_ws].path, mlen) == 0) {
		if (mlen > 0 && remotes[i].mountpoint[mlen - 1] == '/') {
			mlen--;
			remotes[i].mountpoint[mlen] = '\0';
		}

		char *p = strrchr(remotes[i].mountpoint, '/');
		if (!p) {
			xerror(_("net: '%s': Error getting parent directory\n"),
				remotes[i].mountpoint);
			return EXIT_FAILURE;
		}

		*p = '\0';
		errno = 0;
		if (xchdir(remotes[i].mountpoint, SET_TITLE) == EXIT_FAILURE) {
			*p = '/';
			xerror("net: '%s': %s\n", remotes[i].mountpoint, strerror(errno));
			return EXIT_FAILURE;
		}

		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(remotes[i].mountpoint,
			strlen(remotes[i].mountpoint));
		*p = '/';

		if (conf.autols == 1)
			reload_dirlist();
	}

	if (launch_execl(remotes[i].unmount_cmd) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	remotes[i].mounted = 0;
	return EXIT_SUCCESS;
}

static int
remotes_edit(char *app)
{
	if (!remotes_file) {
		xerror(_("net: Remotes file is undefined\n"));
		return EXIT_FAILURE;
	}

	struct stat attr;
	if (stat(remotes_file, &attr) == -1) {
		xerror("net: '%s': %s\n", remotes_file, strerror(errno));
		return EXIT_FAILURE;
	}

	time_t mtime_bfr = (time_t)attr.st_mtime;

	int ret = EXIT_SUCCESS;
	if (app && *app) {
		char *cmd[] = {app, remotes_file, NULL};
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	} else {
		open_in_foreground = 1;
		ret = open_file(remotes_file);
		open_in_foreground = 0;
	}

	if (ret != EXIT_SUCCESS)
		return ret;

	if (stat(remotes_file, &attr) == -1) {
		xerror("net: '%s': %s\n", remotes_file, strerror(errno));
		return errno;
	}

	if (mtime_bfr != (time_t)attr.st_mtime) {
		free_remotes(0);
		load_remotes();
		print_reload_msg(_("File modified. Remotes reloaded\n"));
	}

	return EXIT_SUCCESS;
}

int
remotes_function(char **args)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: net: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return EXIT_SUCCESS;
	}

	if (!args[1] || (*args[1] == 'l' && strcmp(args[1], "list") == 0))
		return remotes_list();

	if (IS_HELP(args[1]))
		{ puts(_(NET_USAGE)); return EXIT_SUCCESS; }

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return remotes_edit(args[2]);

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
	if (remotes_n == 0)
		return EXIT_SUCCESS;

	int i = (int)remotes_n;
	int exit_status = EXIT_SUCCESS;

	while (--i >= 0) {
		if (remotes[i].name && remotes[i].auto_mount == 1
		&& remotes[i].mountpoint && remotes[i].mount_cmd) {

			if (xargs.secure_cmds == 1
			&& sanitize_cmd(remotes[i].mount_cmd, SNT_NET) != EXIT_SUCCESS)
				continue;

			struct stat attr;
			if (stat(remotes[i].mountpoint, &attr) == -1) {
				char *cmd[] = {"mkdir", "-p", remotes[i].mountpoint, NULL};
				if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
					continue;
			} else {
				if (count_dir(remotes[i].mountpoint, CPOP) > 2)
					continue;
			}

			int ret = 0;
			printf(_("%s: net: %s: Mounting remote...\n"), PROGRAM_NAME,
				remotes[i].name);
			if ((ret = launch_execl(remotes[i].mount_cmd)) != EXIT_SUCCESS) {
				err('w', PRINT_PROMPT, _("net: '%s': Mount command failed with "
					"error code %d\n"), remotes[i].name, ret);
				exit_status = EXIT_FAILURE;
			} else {
				remotes[i].mounted = 1;
			}
		}
	}

	return exit_status;
}

int
autounmount_remotes(void)
{
	if (remotes_n == 0)
		return EXIT_SUCCESS;

	int i = (int)remotes_n, exit_status = EXIT_SUCCESS;

	while (--i >= 0) {
		if (remotes[i].name && remotes[i].auto_unmount == 1
		&& remotes[i].mountpoint && remotes[i].unmount_cmd) {

			if (xargs.secure_cmds == 1
			&& sanitize_cmd(remotes[i].unmount_cmd, SNT_NET) != EXIT_SUCCESS)
				continue;

			if (count_dir(remotes[i].mountpoint, CPOP) <= 2)
				continue;
			int dir_change = 0;

			if (*workspaces[cur_ws].path == *remotes[i].mountpoint
			&& strcmp(remotes[i].mountpoint, workspaces[cur_ws].path) == 0) {
				xchdir("/", NO_TITLE);
				dir_change = 1;
			}

			int ret = 0;
			printf(_("%s: net: %s: Unmounting remote...\n"), PROGRAM_NAME,
				remotes[i].name);
			if ((ret = launch_execl(remotes[i].unmount_cmd)) != EXIT_SUCCESS) {
				xerror(_("%s: net: %s: Unmount command failed with "
					"error code %d\n"), PROGRAM_NAME, remotes[i].name, ret);
				exit_status = EXIT_FAILURE;
			}

			if (dir_change == 1)
				xchdir(workspaces[cur_ws].path, NO_TITLE);
		}
	}

	return exit_status;
}
