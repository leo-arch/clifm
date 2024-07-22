/* profiles.c -- functions for manipulating user profiles */

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

#ifndef _NO_PROFILES

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/history.h>

#if defined(MAC_OS_X_RENAMEAT_SYS_STDIO_H)
# include <sys/stdio.h> /* renameat(2) */
#endif /* MAC_OS_X_RENAMEAT_SYS_STDIO_H */

#include "aux.h"
#include "bookmarks.h"
#include "checks.h"
#include "config.h"
#include "exec.h" // exec_profile
#include "history.h"
#include "init.h"
#include "listing.h"
#include "messages.h"
#include "misc.h"
#include "navigation.h"
#include "profiles.h"
#include "sort.h"
#include "spawn.h"

/* Get the list of all available profile names and store them in the
 * profile_names global array. Returns zero on success, one otherwise. */
int
get_profile_names(void)
{
	if (!config_dir_gral)
		return FUNC_FAILURE;

	const size_t len = strlen(config_dir_gral) + 10;
	char *pf_dir = xnmalloc(len, sizeof(char));
	snprintf(pf_dir, len, "%s/profiles", config_dir_gral);

	struct dirent **profs = (struct dirent **)NULL;
	int files_n = scandir(pf_dir, &profs, NULL, xalphasort);

	if (files_n == -1) {
		free(pf_dir);
		return FUNC_FAILURE;
	}

	size_t i, pf_n = 0;
#if !defined(_DIRENT_HAVE_D_TYPE)
	struct stat attr;
#endif /* !_DIRENT_HAVE_D_TYPE */

	for (i = 0; i < (size_t)files_n; i++) {
#if !defined(_DIRENT_HAVE_D_TYPE)
		char tmp[PATH_MAX + 1];
		snprintf(tmp, sizeof(tmp), "%s/%s", pf_dir, profs[i]->d_name);
		if (lstat(tmp, &attr) == -1)
			continue;
		if (S_ISDIR(attr.st_mode)
#else
		if (profs[i]->d_type == DT_DIR
#endif /* !_DIRENT_HAVE_D_TYPE */
		    /* Discard ".", "..", and hidden dirs */
		    && *profs[i]->d_name != '.') {
			profile_names = xnrealloc(profile_names, pf_n + 1, sizeof(char *));
			profile_names[pf_n] = savestring(profs[i]->d_name,
			    strlen(profs[i]->d_name));
			pf_n++;
		}

		free(profs[i]);
	}

	free(pf_dir);
	free(profs);

	profile_names = xnrealloc(profile_names, pf_n + 1, sizeof(char *));
	profile_names[pf_n] = (char *)NULL;

	return FUNC_SUCCESS;
}

/* Check if NAME is an existing profile name.
 * If true, returns the index of the profile in the PROFILE_NAMES array.
 * Otherwise, returns -1. */
static int
check_profile(const char *name)
{
	int i;
	for (i = 0; profile_names[i]; i++) {
		if (*name == *profile_names[i] && strcmp(name, profile_names[i]) == 0)
			return i;
	}
	return (-1);
}

/* Switch profile to PROF */
int
profile_set(const char *prof)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: profiles: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!prof)
		return FUNC_FAILURE;

	dir_cmds.first_cmd_in_dir = UNSET;

	/* Check if prof is a valid profile */
	int found = check_profile(prof);
	if (found == -1) {
		xerror(_("pf: '%s': No such profile\nTo add a new "
			"profile enter 'pf add PROFILE'\n"), prof);
		return FUNC_FAILURE;
	}

	/* If changing to the current profile, do nothing */
	if ((!alt_profile && *prof == 'd' && strcmp(prof, "default") == 0)
	|| (alt_profile && *prof == *alt_profile
	&& strcmp(prof, alt_profile) == 0)) {
		printf(_("pf: '%s' is the current profile\n"), prof);
		return FUNC_SUCCESS;
	}

	int i;

	if (conf.restore_last_path == 1)
		save_last_path(NULL);

	if (alt_profile) {
		free(alt_profile);
		alt_profile = (char *)NULL;
	}

	/* Set the new profile value */
	/* Default profile == (alt_profile == NULL) */
	if (*prof != 'd' || strcmp(prof, "default") != 0)
		alt_profile = savestring(prof, strlen(prof));

	i = MAX_WS;
	while (--i >= 0) {
		free(workspaces[i].path);
		workspaces[i].path = (char *)NULL;
		free(workspaces[i].name);
		workspaces[i].name = (char *)NULL;
	}
	cur_ws = UNSET;

	/* Reset everything */
	reload_config();

	i = (int)usrvar_n;
	while (--i >= 0) {
		free(usr_var[i].name);
		free(usr_var[i].value);
	}
	usrvar_n = 0;

	i = (int)actions_n;
	while (--i >= 0) {
		free(usr_actions[i].name);
		free(usr_actions[i].value);
	}
	actions_n = 0;

	exec_profile();

	if (config_ok == 1) {
		/* Shrink the log file if needed */
		truncate_file(msgs_log_file, conf.max_log, 0);
		truncate_file(cmds_log_file, conf.max_log, 0);

		/* Reset history */
		if (access(hist_file, F_OK | W_OK) == 0) {
			clear_history(); /* This is for readline */
			read_history(hist_file);
			history_truncate_file(hist_file, conf.max_hist);
		} else {
			int fd = 0;
			FILE *hist_fp = open_fwrite(hist_file, &fd);
			if (hist_fp) {
				fputs("edit\n", hist_fp);
				fclose(hist_fp);
			} else {
				err('w', PRINT_PROMPT, _("pf: Error opening the "
					"history file\n"));
			}
		}

		get_history(); /* This is only for us */
	}

	free_bookmarks();
	load_bookmarks();
	load_actions();

	/* Reload PATH commands (actions are profile specific) */
	if (bin_commands) {
		for (i = 0; bin_commands[i]; i++)
			free(bin_commands[i]);
		free(bin_commands);
		bin_commands = (char **)NULL;
	}

	if (paths) {
		i = (int)path_n;
		while (--i >= 0)
			free(paths[i].path);
		free(paths);
	}

	path_n = (size_t)get_path_env(1);
	get_path_programs();

	if (conf.restore_last_path == 1)
		get_last_path();

	if (cur_ws == UNSET)
		cur_ws = DEF_CUR_WS;

	if (!workspaces[cur_ws].path) {
		char tmp[PATH_MAX + 1] = "";
		char *cwd = get_cwd(tmp, sizeof(tmp), 0);

		if (!cwd || !*cwd) {
			xerror("pf: %s\n", strerror(errno));
			exit(FUNC_FAILURE);
		}
		workspaces[cur_ws].path = savestring(cwd, strlen(cwd));
	}

	if (xchdir(workspaces[cur_ws].path, SET_TITLE) == -1) {
		xerror("pf: '%s': %s\n", workspaces[cur_ws].path, strerror(errno));
		return FUNC_FAILURE;
	}

	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(_("Switched to profile %s%s%s\n"), BOLD, prof, NC);
	return FUNC_SUCCESS;
}

/* Add a new profile named PROF */
static int
profile_add(const char *prof)
{
	if (!prof)
		return FUNC_FAILURE;

	const int found = check_profile(prof);
	if (found != -1) {
		xerror(_("pf: '%s': Profile already exists\n"), prof);
		return FUNC_FAILURE;
	}

	if (!home_ok) {
		xerror(_("pf: '%s': Cannot create profile: Home directory "
			"not found\n"), prof);
		return FUNC_FAILURE;
	}

	const size_t pnl_len = strlen(PROGRAM_NAME);
	size_t tmp_len = strlen(config_dir_gral) + strlen(prof) + 11;
	/* ### GENERATE PROGRAM'S CONFIG DIRECTORY NAME ### */
	char *nconfig_dir = xnmalloc(tmp_len, sizeof(char));
	snprintf(nconfig_dir, tmp_len, "%s/profiles/%s", config_dir_gral, prof);

	/* #### CREATE THE CONFIG DIR #### */
	char *tmp_cmd[] = {"mkdir", "-p", nconfig_dir, NULL};
	if (launch_execv(tmp_cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS) {
		xerror(_("pf: mkdir: '%s': Error creating "
			"configuration directory\n"), nconfig_dir);
		free(nconfig_dir);
		return FUNC_FAILURE;
	}

	/* If the config dir is fine, generate config file names */
	int exit_status = FUNC_SUCCESS;
	const size_t config_len = strlen(nconfig_dir);

	tmp_len = config_len + pnl_len + 4;
	char *nconfig_file = xnmalloc(tmp_len, sizeof(char));
	snprintf(nconfig_file, tmp_len, "%s/%src", nconfig_dir, PROGRAM_NAME);
	tmp_len = config_len + 15;
	char *nhist_file = xnmalloc(tmp_len, sizeof(char));
	snprintf(nhist_file, tmp_len, "%s/history.clifm", nconfig_dir);
	tmp_len = config_len + 16;
	char *nmime_file = xnmalloc(tmp_len, sizeof(char));
	snprintf(nmime_file, tmp_len, "%s/mimelist.clifm", nconfig_dir);

	/* Create config files */

	/* #### CREATE THE HISTORY FILE #### */
	int fd = 0;
	FILE *hist_fp = open_fwrite(nhist_file, &fd);
	if (!hist_fp) {
		xerror("pf: fopen: %s: %s\n", nhist_file, strerror(errno));
		exit_status = FUNC_FAILURE;
	} else {
		/* To avoid malloc errors in read_history(), do not create
		 * an empty file */
		fputs("edit\n", hist_fp);
		fclose(hist_fp);
	}

	/* #### CREATE THE MIME CONFIG FILE #### */
	if (create_mime_file(nmime_file, 1) != FUNC_SUCCESS)
		exit_status = FUNC_FAILURE;

	/* #### CREATE THE MAIN CONFIG FILE #### */
	if (create_main_config_file(nconfig_file) != FUNC_SUCCESS)
		exit_status = FUNC_FAILURE;

	/* Free stuff */
	free(nconfig_dir);
	free(nconfig_file);
	free(nhist_file);
	free(nmime_file);

	if (exit_status == FUNC_SUCCESS) {
		printf(_("Succesfully created profile %s%s%s\n"), BOLD, prof, df_c);

		size_t i;
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);

		get_profile_names();
	} else {
		xerror(_("pf: 's': Error creating profile\n"), prof);
	}

	return exit_status;
}

/* Delete the profile PROF */
static int
profile_del(const char *prof)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: profiles: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!prof)
		return FUNC_FAILURE;

	/* Check if prof is a valid profile */
	const int found = check_profile(prof);
	if (found == -1) {
		xerror(_("pf: '%s': No such profile\n"), prof);
		return FUNC_FAILURE;
	}

	const size_t len = strlen(config_dir_gral) + strlen(prof) + 11;
	char *tmp = xnmalloc(len, sizeof(char));
	snprintf(tmp, len, "%s/profiles/%s", config_dir_gral, prof);

	char *cmd[] = {"rm", "-r", "--", tmp, NULL};
	const int ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	free(tmp);

	if (ret != FUNC_SUCCESS) {
		xerror(_("pf: '%s': Error removing profile\n"), prof);
		return ret;
	}

	printf(_("Successfully removed profile %s%s%s\n"), BOLD, prof, df_c);
	size_t i;
	for (i = 0; profile_names[i]; i++)
		free(profile_names[i]);

	get_profile_names();
	return FUNC_SUCCESS;
}

static int
print_profiles(void)
{
	size_t i;
	char *cur_prof = alt_profile ? alt_profile : "default";

	for (i = 0; profile_names[i]; i++) {
		if (*cur_prof == *profile_names[i]
		&& strcmp(cur_prof, profile_names[i]) == 0)
			printf("%s>%s %s\n", mi_c, df_c, profile_names[i]);
		else
			printf("  %s\n", profile_names[i]);
	}

	return FUNC_SUCCESS;
}

static int
print_current_profile(void)
{
	printf("Current profile: %s%s%s\n", BOLD, alt_profile
		? alt_profile : "default", df_c);
	return FUNC_SUCCESS;
}

int
validate_profile_name(const char *name)
{
	if (!name || !*name || *name == '.' || *name == '~' || *name == '$'
	|| strchr(name, '/'))
		return FUNC_FAILURE;

	return FUNC_SUCCESS;
}

static int
create_profile(const char *name)
{
	if (name) {
		if (validate_profile_name(name) == FUNC_SUCCESS)
			return profile_add(name);

		xerror(_("pf: '%s': Invalid profile name\n"), name);
		return FUNC_FAILURE;
	}

	fprintf(stderr, "%s\n", PROFILES_USAGE);
	return FUNC_FAILURE;
}

static int
delete_profile(const char *name)
{
	if (name)
		return profile_del(name);

	fprintf(stderr, "%s\n", PROFILES_USAGE);
	return FUNC_FAILURE;
}

static int
switch_profile(const char *name)
{
	if (name)
		return profile_set(name);

	fprintf(stderr, "%s\n", PROFILES_USAGE);
	return FUNC_FAILURE;
}

/* Rename profile named ARG[0] to ARG[1].
 * The corresponding profile file is renamed, just as the corresponding
 * entry in the PROFILE_NAMES array. */
static int
rename_profile(char **args)
{
	if (!args || !args[0] || !args[1]) {
		fprintf(stderr, "%s\n", PROFILES_USAGE);
		return FUNC_FAILURE;
	}

	if (!config_dir_gral) {
		xerror(_("pf: Configuration directory is not set\n"));
		return FUNC_FAILURE;
	}

	if (validate_profile_name(args[1]) != FUNC_SUCCESS) {
		xerror(_("pf: '%s': Invalid profile name\n"), args[1]);
		return FUNC_FAILURE;
	}

	if (*args[0] == *args[1] && strcmp(args[0], args[1]) == 0) {
		puts(_("pf: Nothing to do. Source and destination names are the same."));
		return FUNC_FAILURE;
	}

	char *p = unescape_str(args[0], 0);
	if (p) {
		free(args[0]);
		args[0] = p;
	}

	int src_pf_index = check_profile(args[0]);
	if (src_pf_index == -1) {
		xerror(_("pf: '%s': No such profile\n"), args[0]);
		return FUNC_FAILURE;
	}

	if (strcmp(args[0], alt_profile ? alt_profile : "default") == 0) {
		xerror(_("pf: '%s': Is the current profile\n"), args[0]);
		return FUNC_FAILURE;
	}

	char src_pf_name[PATH_MAX + NAME_MAX + 11];
	snprintf(src_pf_name, sizeof(src_pf_name), "%s/profiles/%s",
		config_dir_gral, args[0]);

	DIR *dir = opendir(src_pf_name);
	if (!dir) {
		xerror("pf: '%s': %s\n", src_pf_name, strerror(errno));
		return errno;
	}

	int fd = dirfd(dir);

	p = unescape_str(args[1], 0);
	if (p) {
		free(args[1]);
		args[1] = p;
	}

	char dst_pf_name[PATH_MAX + NAME_MAX + 11];
	snprintf(dst_pf_name, sizeof(dst_pf_name), "%s/profiles/%s",
		config_dir_gral, args[1]);

	const int ret = renameat(fd, src_pf_name, XAT_FDCWD, dst_pf_name);
	closedir(dir);

	if (ret == -1) {
		xerror(_("pf: Cannot rename profile '%s': %s\n"),
			args[0], strerror(errno));
		return errno;
	}

	const size_t len = strlen(args[1]);
	profile_names[src_pf_index] = xnrealloc(profile_names[src_pf_index],
		len + 1, sizeof(char));
	xstrsncpy(profile_names[src_pf_index], args[1], len + 1);

	printf(_("pf: '%s': Profile successfully renamed to %s%s%s\n"),
		args[0], BOLD, args[1], df_c);

	return FUNC_SUCCESS;
}

/* Main profiles function. Call the operation specified by the first
 * argument option: list, add, set, or delete */
int
profile_function(char **args)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: profiles: %s\n", PROGRAM_NAME, STEALTH_DISABLED);
		return FUNC_SUCCESS;
	}

	if (!args[1])
		return print_current_profile();

	if (IS_HELP(args[1])) {
		puts(_(PROFILES_USAGE));
		return FUNC_SUCCESS;
	}

	/* List profiles */
	if (*args[1] == 'l' && (strcmp(args[1], "ls") == 0
	|| strcmp(args[1], "list") == 0))
		return print_profiles();

	if (args[2]) {
		char *p = unescape_str(args[2], 0);
		if (p) {
			free(args[2]);
			args[2] = p;
		}
	}

	/* Create a new profile */
	if (*args[1] == 'a' && strcmp(args[1], "add") == 0)
		return create_profile(args[2]);

	/* Delete a profile */
	if (*args[1] == 'd' && strcmp(args[1], "del") == 0)
		return delete_profile(args[2]);

	/* Rename a profile */
	if (*args[1] == 'r' && strcmp(args[1], "rename") == 0)
		return rename_profile(args + 2);

	/* Switch to another profile */
	if (*args[1] == 's' && strcmp(args[1], "set") == 0)
		return switch_profile(args[2]);

	fprintf(stderr, "%s\n", PROFILES_USAGE);
	return FUNC_FAILURE;
}

#else
void *_skip_me_prof;
#endif /* !_NO_PROFILES */
