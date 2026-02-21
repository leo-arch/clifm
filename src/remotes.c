/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* remote.c -- functions to manage remotes */

#include "helpers.h"

#include <errno.h>
#include <string.h>

#include "aux.h"
#include "file_operations.h"
#include "history.h"
#include "init.h"
#include "jump.h"
#include "listing.h"
#include "messages.h"
#include "misc.h"
#include "navigation.h"
#include "sanitize.h"
#include "spawn.h"

static int
remotes_list(void)
{
	if (remotes_n == 0) {
		printf(_("%s: No remotes defined. Run 'net edit' to add a remote.\n"),
			PROGRAM_NAME);
		return FUNC_SUCCESS;
	}

	for (size_t i = 0; i < remotes_n; i++) {
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
	return FUNC_SUCCESS;
}

static int
dequote_remote_name(char *name)
{
	char *deq = NULL;
	if (strchr(name, '\\')) {
		deq = unescape_str(name);
		if (deq) {
			xstrsncpy(name, deq, strlen(deq) + 1);
			free(deq);
		} else {
			xerror("net: %s: Error unescaping resource name\n", name);
			return FUNC_FAILURE;
		}
	}

	return FUNC_SUCCESS;
}

/* Get the index of the remote named NAME from the remotes list. Returns
 * this index in case of success or -1 in case of error */
static int
get_remote(char *name)
{
	if (!name || !*name)
		return (-1);

	if (dequote_remote_name(name) == FUNC_FAILURE)
		return (-1);

	size_t i = remotes_n;
	int found = 0;

	for (; i-- > 0;) {
		if (remotes[i].name && *name == *remotes[i].name
		&& strcmp(name, remotes[i].name) == 0) {
			found = 1;
			break;
		}
	}

	if (found == 0) {
		xerror(_("net: '%s': No such remote\n"), name);
		return (-1);
	}

	if (!remotes[i].mount_cmd || !*remotes[i].mount_cmd) {
		xerror(_("net: No mount command specified for '%s'\n"), remotes[i].name);
		return (-1);
	}

	if (!remotes[i].mountpoint || !*remotes[i].mountpoint) {
		xerror(_("net: No mountpoint specified for '%s'\n"), remotes[i].name);
		return (-1);
	}

	return i > INT_MAX ? INT_MAX : (int)i;
}

static int
create_mountpoint(const int i)
{
	const char *cmd[] = {"mkdir", "-p", remotes[i].mountpoint, NULL};

	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS) {
		xerror("net: '%s': %s\n", remotes[i].mountpoint, strerror(errno));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static int
cd_to_mountpoint(const int i)
{
	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(remotes[i].mountpoint,
		strlen(remotes[i].mountpoint));
	add_to_jumpdb(workspaces[cur_ws].path);
	add_to_dirhist(workspaces[cur_ws].path);

	dir_changed = 1;
	reload_dirlist();

	return FUNC_SUCCESS;
}

static int
print_cd_error(const int i)
{
	xerror("net: '%s': %s\n", remotes[i].mountpoint, strerror(errno));
	return FUNC_FAILURE;
}

static int
remotes_mount(char *name)
{
	const int i = get_remote(name);
	if (i == -1)
		return FUNC_FAILURE;

	/* name, mountpoint, and mount_cmd members of the remotes struct are
	 * guaranteed to be non-NULL by get_remote(). */

	if (xargs.secure_cmds == 1
	&& sanitize_cmd(remotes[i].mount_cmd, SNT_NET) != FUNC_SUCCESS)
		return FUNC_FAILURE;

	/* If mountpoint doesn't exist, create it */
	struct stat attr;
	if (stat(remotes[i].mountpoint, &attr) == -1
	&& create_mountpoint(i) == FUNC_FAILURE)
		return FUNC_FAILURE;

	/* Make sure mountpoint is not populated and run the mount command */
	if (count_dir(remotes[i].mountpoint, CPOP) <= 2
	&& launch_execl(remotes[i].mount_cmd) != FUNC_SUCCESS)
		return FUNC_FAILURE;

	if (xchdir(remotes[i].mountpoint, SET_TITLE) == -1)
		return print_cd_error(i);

	int exit_status = FUNC_SUCCESS;
	if (conf.autols == 1) {
		exit_status = cd_to_mountpoint(i);
	} else {
		printf(_("net: '%s': Changed to mountpoint (%s)\n"),
			remotes[i].name, remotes[i].mountpoint);
	}

	remotes[i].mounted = 1;
	return exit_status;
}

static int
remotes_unmount(char *name)
{
	const int i = get_remote(name);
	if (i == -1)
		return FUNC_FAILURE;

	if (remotes[i].mounted == 0) {
		xerror(_("net: '%s': Not mounted\n"), remotes[i].name);
		return FUNC_FAILURE;
	}

	if (!remotes[i].mountpoint || !*remotes[i].mountpoint) {
		xerror(_("net: Error getting mountpoint for '%s'\n"), remotes[i].name);
		return FUNC_FAILURE;
	}

	if (!remotes[i].unmount_cmd || !*remotes[i].unmount_cmd) {
		xerror(_("net: No unmount command for '%s'\n"), remotes[i].name);
		return FUNC_FAILURE;
	}

	if (xargs.secure_cmds == 1
	&& sanitize_cmd(remotes[i].unmount_cmd, SNT_NET) != FUNC_SUCCESS)
		return FUNC_FAILURE;

	/* Get out of mountpoint before unmounting. */
	int reload_files = 0;
	size_t mlen = strlen(remotes[i].mountpoint);
	if (strncmp(remotes[i].mountpoint, workspaces[cur_ws].path, mlen) == 0) {
		if (mlen > 0 && remotes[i].mountpoint[mlen - 1] == '/')
			remotes[i].mountpoint[mlen - 1] = '\0';

		char *p = strrchr(remotes[i].mountpoint, '/');
		if (!p) {
			xerror(_("net: '%s': Error getting parent directory\n"),
				remotes[i].mountpoint);
			return FUNC_FAILURE;
		}

		*p = '\0';
		errno = 0;
		if (xchdir(remotes[i].mountpoint, SET_TITLE) == FUNC_FAILURE) {
			*p = '/';
			xerror("net: '%s': %s\n", remotes[i].mountpoint, strerror(errno));
			return FUNC_FAILURE;
		}

		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(remotes[i].mountpoint,
			strlen(remotes[i].mountpoint));
		*p = '/';

		if (conf.autols == 1)
			reload_files = 1;
	}

	if (launch_execl(remotes[i].unmount_cmd) != FUNC_SUCCESS)
		return FUNC_FAILURE;

	if (reload_files == 1)
		reload_dirlist();

	remotes[i].mounted = 0;
	return FUNC_SUCCESS;
}

static int
remotes_edit(char *app)
{
	if (!remotes_file) {
		xerror(_("net: Remotes file is undefined\n"));
		return FUNC_FAILURE;
	}

	struct stat attr;
	if (stat(remotes_file, &attr) == -1) {
		xerror("net: '%s': %s\n", remotes_file, strerror(errno));
		return errno;
	}

	const time_t mtime_bfr = attr.st_mtime;

	const int ret = open_config_file(app, remotes_file);
	if (ret != FUNC_SUCCESS)
		return ret;

	if (stat(remotes_file, &attr) == -1) {
		xerror("net: '%s': %s\n", remotes_file, strerror(errno));
		return errno;
	}

	if (mtime_bfr != attr.st_mtime) {
		free_remotes(0);
		load_remotes();
		print_reload_msg(NULL, NULL, _("File modified. Remotes reloaded.\n"));
	}

	return FUNC_SUCCESS;
}

int
remotes_function(char **args)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: net: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!args[1] || (*args[1] == 'l' && strcmp(args[1], "list") == 0))
		return remotes_list();

	if (IS_HELP(args[1]))
		{ puts(_(NET_USAGE)); return FUNC_SUCCESS; }

	if (*args[1] == 'e' && strcmp(args[1], "edit") == 0)
		return remotes_edit(args[2]);

	if (*args[1] == 'u' && (!*(args[1] + 1) || strcmp(args[1], "unmount") == 0)) {
		if (!args[2]) {
			fprintf(stderr, "%s\n", _(NET_USAGE));
			return FUNC_FAILURE;
		}
		return remotes_unmount(args[2]);
	}

	if (*args[1] == 'm' && (!*(args[1] + 1) || strcmp(args[1], "mount") == 0)) {
		if (!args[2]) {
			fprintf(stderr, "%s\n", _(NET_USAGE));
			return FUNC_FAILURE;
		}
		return remotes_mount(args[2]);
	}

	return remotes_mount(args[1]);
}

int
automount_remotes(void)
{
	if (remotes_n == 0)
		return FUNC_SUCCESS;

	int exit_status = FUNC_SUCCESS;

	for (size_t i = remotes_n; i-- > 0;) {
		if (remotes[i].name && remotes[i].auto_mount == 1
		&& remotes[i].mountpoint && remotes[i].mount_cmd) {

			if (xargs.secure_cmds == 1
			&& sanitize_cmd(remotes[i].mount_cmd, SNT_NET) != FUNC_SUCCESS)
				continue;

			struct stat attr;
			if (stat(remotes[i].mountpoint, &attr) == -1) {
				const char *cmd[] = {"mkdir", "-p", remotes[i].mountpoint, NULL};
				if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
					continue;
			} else {
				if (count_dir(remotes[i].mountpoint, CPOP) > 2)
					continue;
			}

			int ret = 0;
			printf(_("%s: net: %s: Mounting remote...\n"), PROGRAM_NAME,
				remotes[i].name);
			if ((ret = launch_execl(remotes[i].mount_cmd)) != FUNC_SUCCESS) {
				err('w', PRINT_PROMPT, _("net: '%s': Mount command failed with "
					"error code %d\n"), remotes[i].name, ret);
				exit_status = FUNC_FAILURE;
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
		return FUNC_SUCCESS;

	int exit_status = FUNC_SUCCESS;

	for (size_t i = remotes_n; i-- > 0;) {
		if (remotes[i].name && remotes[i].auto_unmount == 1
		&& remotes[i].mountpoint && remotes[i].unmount_cmd) {

			if (xargs.secure_cmds == 1
			&& sanitize_cmd(remotes[i].unmount_cmd, SNT_NET) != FUNC_SUCCESS)
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
			if ((ret = launch_execl(remotes[i].unmount_cmd)) != FUNC_SUCCESS) {
				xerror(_("%s: net: %s: Unmount command failed with "
					"error code %d\n"), PROGRAM_NAME, remotes[i].name, ret);
				exit_status = FUNC_FAILURE;
			}

			if (dir_change == 1)
				xchdir(workspaces[cur_ws].path, NO_TITLE);
		}
	}

	return exit_status;
}
