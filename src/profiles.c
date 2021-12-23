/* profiles.c -- functions for manipulating user profiles */

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
#include <unistd.h>
#include <sys/stat.h>
#include <readline/history.h>

#include "actions.h"
#include "aux.h"
#include "bookmarks.h"
#include "checks.h"
#include "config.h"
#include "exec.h"
#include "history.h"
#include "init.h"
#include "listing.h"
#include "mime.h"
#include "misc.h"
#include "navigation.h"
#include "profiles.h"
#include "sort.h"
#include "messages.h"

int
get_profile_names(void)
{
	if (!config_dir_gral)
		return EXIT_FAILURE;

	char *pf_dir = (char *)xnmalloc(strlen(config_dir_gral) + 10, sizeof(char));
	sprintf(pf_dir, "%s/profiles", config_dir_gral);

	struct dirent **profs = (struct dirent **)NULL;
	int files_n = scandir(pf_dir, &profs, NULL, xalphasort);

	if (files_n == -1) {
		free(pf_dir);
		return EXIT_FAILURE;
	}

	size_t i, pf_n = 0;
#if !defined(_DIRENT_HAVE_D_TYPE)
	struct stat attr;
#endif

	for (i = 0; i < (size_t)files_n; i++) {
#if !defined(_DIRENT_HAVE_D_TYPE)
		char tmp[PATH_MAX];
		snprintf(tmp, PATH_MAX - 1, "%s/%s", pf_dir,profs[i]->d_name);
		if (lstat(tmp, &attr) == -1)
			continue;
		if ((attr.st_mode & S_IFMT) == S_IFDIR
#else
		if (profs[i]->d_type == DT_DIR
#endif
		    /* Discard ".", "..", and hidden dirs */
		    && *profs[i]->d_name != '.') {
			profile_names = (char **)xrealloc(profile_names, (pf_n + 1)
												* sizeof(char *));
			profile_names[pf_n] = savestring(profs[i]->d_name,
			    strlen(profs[i]->d_name));
			pf_n++;
		}

		free(profs[i]);
	}

	free(pf_dir);
	free(profs);

	profile_names = (char **)xrealloc(profile_names, (pf_n + 1) * sizeof(char *));
	profile_names[pf_n] = (char *)NULL;

	return EXIT_SUCCESS;
}

/* Check if NAME is an existing profile name */
static int
check_profile(const char *name)
{
	size_t i;
	for (i = 0; profile_names[i]; i++) {
		if (*name == *profile_names[i] && strcmp(name, profile_names[i]) == 0)
			return 1;
	}
	return 0;
}

/* Switch profile to PROF */
int
profile_set(char *prof)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: The profile function is disabled in stealth mode\n",
		    PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!prof)
		return EXIT_FAILURE;

	/* Check if prof is a valid profile */
	int found = check_profile(prof);
	if (!found) {
		fprintf(stderr, _("%s: %s: No such profile\nTo add a new "
				  "profile enter 'pf add PROFILE'\n"),
		    PROGRAM_NAME, prof);

		return EXIT_FAILURE;
	}

	/* If changing to the current profile, do nothing */
	if ((!alt_profile && *prof == 'd' && strcmp(prof, "default") == 0)
	|| (alt_profile && *prof == *alt_profile && strcmp(prof, alt_profile) == 0)) {

		printf(_("%s: '%s' is the current profile\n"), PROGRAM_NAME,
		    prof);

		return EXIT_SUCCESS;
	}

	if (restore_last_path)
		save_last_path();

	if (alt_profile) {
		free(alt_profile);
		alt_profile = (char *)NULL;
	}

	/* Set the new profile value */
	/* Default profile == (alt_profile == NULL) */
	if (*prof != 'd' || strcmp(prof, "default") != 0)
		alt_profile = savestring(prof, strlen(prof));

	/* Reset everything */
	reload_config();

	/* Check whether we have a working shell */
	if (access(user.shell, X_OK) == -1) {
		_err('w', PRINT_PROMPT, _("%s: %s: System shell not found. Please "
				  "edit the configuration file to specify a working shell.\n"),
				PROGRAM_NAME, user.shell);
	}

	int i = (int)usrvar_n;
	while (--i >= 0) {
		free(usr_var[i].name);
		free(usr_var[i].value);
	}
	usrvar_n = 0;

	i = (int)kbinds_n;
	while (--i >= 0) {
		free(kbinds[i].function);
		free(kbinds[i].key);
	}
	kbinds_n = 0;

	i = (int)actions_n;
	while (--i >= 0) {
		free(usr_actions[i].name);
		free(usr_actions[i].value);
	}
	actions_n = 0;

	/*  my_rl_unbind_functions();
	create_kbinds_file();
	load_keybinds();
	rl_unbind_function_in_map(rl_hidden, rl_get_keymap());
	rl_bind_keyseq(find_key("toggle-hidden"), rl_hidden);
	my_rl_bind_functions(); */

	exec_profile();

	if (msgs_n) {
		i = (int)msgs_n;
		while (--i >= 0)
			free(messages[i]);
	}
	msgs_n = 0;

	if (config_ok) {
		/* Limit the log files size */
		check_file_size(log_file, max_log);
		check_file_size(msg_log_file, max_log);

		/* Reset history */
		if (access(hist_file, F_OK | W_OK) == 0) {
			clear_history(); /* This is for readline */
			read_history(hist_file);
			history_truncate_file(hist_file, max_hist);
		} else {
			FILE *hist_fp = fopen(hist_file, "w");

			if (hist_fp) {
				fputs("edit\n", hist_fp);
				fclose(hist_fp);
			} else {
				_err('w', PRINT_PROMPT, _("%s: Error opening the "
						"history file\n"), PROGRAM_NAME);
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
			free(paths[i]);
	}

	path_n = (size_t)get_path_env();
	get_path_programs();

	i = MAX_WS;
	while (--i >= 0) {
		free(ws[i].path);
		ws[i].path = (char *)NULL;
	}

	cur_ws = UNSET;

	if (restore_last_path)
		get_last_path();

	if (cur_ws == UNSET)
		cur_ws = DEF_CUR_WS;

	if (!ws[cur_ws].path) {
		char cwd[PATH_MAX] = "";
		if (getcwd(cwd, sizeof(cwd)) == NULL) {}
		/* Avoid compiler warning */
		if (!*cwd) {
			fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
			exit(EXIT_FAILURE);
		}
		ws[cur_ws].path = savestring(cwd, strlen(cwd));
	}

	if (xchdir(ws[cur_ws].path, SET_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ws[cur_ws].path,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	int exit_status = EXIT_SUCCESS;

	if (autols) {
		free_dirlist();
		exit_status = list_dir();
	}

	printf("%s->%s Switched to profile %s%s%s\n", mi_c, df_c, "\x1b[1m",
			prof, "\x1b[0m");

	return exit_status;
}

static int
profile_add(char *prof)
{
	if (!prof)
		return EXIT_FAILURE;

	int found = check_profile(prof);
	if (found) {
		fprintf(stderr, _("%s: %s: Profile already exists\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	if (!home_ok) {
		fprintf(stderr, _("%s: %s: Cannot create profile: Home "
				"directory not found\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	size_t pnl_len = strlen(PNL);
	/* ### GENERATE PROGRAM'S CONFIG DIRECTORY NAME ### */
	char *nconfig_dir = (char *)xnmalloc(strlen(config_dir_gral) + strlen(prof) + 11, sizeof(char));
	sprintf(nconfig_dir, "%s/profiles/%s", config_dir_gral, prof);

	/* #### CREATE THE CONFIG DIR #### */
	char *tmp_cmd[] = {"mkdir", "-p", nconfig_dir, NULL};
	int ret = launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG);

	if (ret != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: mkdir: %s: Error creating "
			"configuration directory\n"), PROGRAM_NAME, nconfig_dir);

		free(nconfig_dir);

		return EXIT_FAILURE;
	}

	/* If the config dir is fine, generate config file names */
	int exit_status = EXIT_SUCCESS;
	size_t config_len = strlen(nconfig_dir);

	char *nconfig_file = (char *)xnmalloc(config_len + pnl_len + 4,
	    sizeof(char));
	sprintf(nconfig_file, "%s/%src", nconfig_dir, PNL);
	char *nhist_file = (char *)xnmalloc(config_len + 13, sizeof(char));
	sprintf(nhist_file, "%s/history.cfm", nconfig_dir);
	char *nmime_file = (char *)xnmalloc(config_len + 14, sizeof(char));
	sprintf(nmime_file, "%s/mimelist.cfm", nconfig_dir);

	/* Create config files */

	/* #### CREATE THE HISTORY FILE #### */
	FILE *hist_fp = fopen(nhist_file, "w+");

	if (!hist_fp) {
		fprintf(stderr, "%s: fopen: %s: %s\n", PROGRAM_NAME,
		    nhist_file, strerror(errno));
		exit_status = EXIT_FAILURE;
	} else {
		/* To avoid malloc errors in read_history(), do not create
		 * an empty file */
		fputs("edit\n", hist_fp);
		fclose(hist_fp);
	}

	/* #### CREATE THE MIME CONFIG FILE #### */
	if (create_mime_file(nmime_file, 1) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

	/* #### CREATE THE CONFIG FILE #### */
	if (create_config(nconfig_file) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

	/* Free stuff */
	free(nconfig_dir);
	free(nconfig_file);
	free(nhist_file);
	free(nmime_file);

	if (exit_status == EXIT_SUCCESS) {
		printf(_("%s: '%s': Profile succesfully created\n"), PROGRAM_NAME, prof);

		size_t i;
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);

		get_profile_names();
	} else {
		fprintf(stderr, _("%s: %s: Error creating profile\n"),
		    PROGRAM_NAME, prof);
	}

	return exit_status;
}

static int
profile_del(char *prof)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: The profile function is disabled in stealth mode\n",
		    PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!prof)
		return EXIT_FAILURE;

	/* Check if prof is a valid profile */
	int found = check_profile(prof);
	if (!found) {
		fprintf(stderr, _("%s: %s: No such profile\n"), PROGRAM_NAME, prof);
		return EXIT_FAILURE;
	}

	char *tmp = (char *)xnmalloc(strlen(config_dir_gral) + strlen(prof) + 11,
															sizeof(char));
	sprintf(tmp, "%s/profiles/%s", config_dir_gral, prof);

	char *cmd[] = {"rm", "-r", tmp, NULL};
	int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	free(tmp);

	if (ret == EXIT_SUCCESS) {
		printf(_("%s: '%s': Profile successfully removed\n"), PROGRAM_NAME, prof);
		size_t i;
		for (i = 0; profile_names[i]; i++)
			free(profile_names[i]);

		get_profile_names();
		return EXIT_SUCCESS;
	}

	fprintf(stderr, _("%s: %s: Error removing profile\n"), PROGRAM_NAME, prof);
	return EXIT_FAILURE;
}

int
profile_function(char **comm)
{
	if (xargs.stealth_mode == 1) {
		printf("%s: The profile function is disabled in stealth mode\n",
		    PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	if (comm[1]) {
		if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0)
			puts(_(PROFILES_USAGE));

		/* List profiles */
		else if (comm[1] && (strcmp(comm[1], "ls") == 0
		|| strcmp(comm[1], "list") == 0)) {
			size_t i;

			for (i = 0; profile_names[i]; i++)
				printf("%s\n", profile_names[i]);
		}

		/* Create a new profile */
		else if (strcmp(comm[1], "add") == 0) {
			if (comm[2]) {
				exit_status = profile_add(comm[2]);
			} else {
				fprintf(stderr, "%s\n", PROFILES_USAGE);
				exit_status = EXIT_FAILURE;
			}
		}

		/* Delete a profile */
		else if (*comm[1] == 'd' && strcmp(comm[1], "del") == 0) {
			if (comm[2]) {
				exit_status = profile_del(comm[2]);
			} else {
				fprintf(stderr, "%s\n", PROFILES_USAGE);
				exit_status = EXIT_FAILURE;
			}
		}

		/* Switch to another profile */
		else if (*comm[1] == 's' && strcmp(comm[1], "set") == 0) {
			if (comm[2]) {
				exit_status = profile_set(comm[2]);
			} else {
				fprintf(stderr, "%s\n", PROFILES_USAGE);
				exit_status = EXIT_FAILURE;
			}
		}

		/* None of the above == error */
		else {
			fprintf(stderr, "%s\n", PROFILES_USAGE);
			exit_status = EXIT_FAILURE;
		}
	}

	/* If only "pr" print the current profile name */
	else if (!alt_profile)
		printf("%s: profile: default\n", PROGRAM_NAME);

	else
		printf("%s: profile: '%s'\n", PROGRAM_NAME, alt_profile);

	return exit_status;
}
