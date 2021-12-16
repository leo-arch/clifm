/* bookmarks.c -- bookmarking functions */

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
#include <unistd.h>
#include <limits.h>

#include "aux.h"
#include "bookmarks.h"
#include "checks.h"
#include "exec.h"
#include "file_operations.h"
#include "init.h"
#include "mime.h"
#include "readline.h"
#include "messages.h"

void
free_bookmarks(void)
{
	if (!bm_n)
		return;

	size_t i;
	for (i = 0; i < bm_n; i++) {
		if (bookmarks[i].shortcut)
			free(bookmarks[i].shortcut);

		if (bookmarks[i].name)
			free(bookmarks[i].name);

		if (bookmarks[i].path)
			free(bookmarks[i].path);
	}

	free(bookmarks);
	bookmarks = (struct bookmarks_t *)NULL;

	for (i = 0; bookmark_names[i]; i++)
		free(bookmark_names[i]);
	free(bookmark_names);
	bookmark_names = (char **)NULL;
	bm_n = 0;
	return;
}

static char **
bm_prompt(void)
{
	char *bm_sel = (char *)NULL;
	printf(_("%s%s\nEnter '%c' to edit your bookmarks or '%c' to quit.\n"),
	    NC, df_c, 'e', 'q');

	while (!bm_sel)
		bm_sel = rl_no_hist(_("Choose a bookmark: "));

	char **comm_bm = get_substr(bm_sel, ' ');
	free(bm_sel);
	return comm_bm;
}

static void
free_del_elements(char **elements)
{
	size_t i;
	for (i = 0; elements[i]; i++)
		free(elements[i]);
	free(elements);
}

static void
free_bms(char **_bms, const size_t _bmn)
{
	size_t i;
	for (i = 0; i < _bmn; i++)
		free(_bms[i]);
	free(_bms);
}

static int
bookmark_del(char *name)
{
	FILE *bm_fp = NULL;
	bm_fp = fopen(bm_file, "r");
	if (!bm_fp)
		return EXIT_FAILURE;

	size_t i = 0;

	/* Get bookmarks from file */
	size_t line_size = 0;
	char *line = (char *)NULL, **bms = (char **)NULL;
	size_t bmn = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, bm_fp)) > 0) {
		if (!line || !*line || *line == '#' || *line == '\n')
			continue;

		int slash = 0;
		for (i = 0; i < (size_t)line_len; i++) {
			if (line[i] == '/') {
				slash = 1;
				break;
			}
		}

		if (!slash)
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		bms = (char **)xrealloc(bms, (bmn + 1) * sizeof(char *));
		bms[bmn++] = savestring(line, (size_t)line_len);
	}

	free(line);
	line = (char *)NULL;

	if (!bmn) {
		puts(_("bookmarks: There are no bookmarks"));
		fclose(bm_fp);
		return EXIT_SUCCESS;
	}

	char **del_elements = (char **)NULL;
	int cmd_line = -1;
	/* This variable let us know two things: a) bookmark name was
	 * specified in command line; b) the index of this name in the
	 * bookmarks array. It is initialized as -1 since the index name
	 * could be zero */

	if (name) {
		for (i = 0; i < bmn; i++) {
			char *bm_name = strbtw(bms[i], ']', ':');
			if (!bm_name)
				continue;
			if (strcmp(name, bm_name) == 0) {
				free(bm_name);
				cmd_line = (int)i;
				break;
			}
			free(bm_name);
		}
	}

	/* If a valid bookmark name was passed in command line, copy the
	 * corresponding bookmark index (plus 1, as if it were typed in the
	 * bookmarks screen) to the del_elements array */
	if (cmd_line != -1) {
		del_elements = (char **)xnmalloc(2, sizeof(char *));
		del_elements[0] = (char *)xnmalloc((size_t)DIGINUM(cmd_line + 1) + 1, sizeof(char));
		sprintf(del_elements[0], "%d", cmd_line + 1);
		del_elements[1] = (char *)NULL;
	}

	/* If bookmark name was passed but it is not a valid bookmark */
	else if (name) {
		fprintf(stderr, _("bookmarks: %s: No such bookmark\n"), name);
		free_bms(bms, bmn);
		fclose(bm_fp);
		return EXIT_FAILURE;
	}

	/* If not name, list bookmarks and get user input */
	else {
		printf(_("%sBookmarks%s\n\n"), BOLD, df_c);

		for (i = 0; i < bmn; i++)
			printf("%s%zu %s%s%s\n", el_c, i + 1, bm_c, bms[i], df_c);

		/* Get user input */
		printf(_("\n%sEnter '%c' to quit.\n"), df_c, 'q');
		char *input = (char *)NULL;
		while (!input)
			input = rl_no_hist(_("Bookmark(s) to be deleted "
								"(ex: 1 2-6, or *): "));
		del_elements = get_substr(input, ' ');
		free(input);
		input = (char *)NULL;

		if (!del_elements) {
			free_bms(bms, bmn);
			fclose(bm_fp);
			fprintf(stderr, _("bookmarks: Error parsing input\n"));
			return EXIT_FAILURE;
		}
	}

	/* We have input */
	/* If quit */
	/* I inspect all substrings entered by the user for "q" before any
	 * other value to prevent some accidental deletion, like "1 q", or
	 * worst, "* q" */
	for (i = 0; del_elements[i]; i++) {
		int quit = 0;

		if (strcmp(del_elements[i], "q") == 0) {
			quit = 1;
		} else if (is_number(del_elements[i]) && (atoi(del_elements[i]) <= 0
		|| atoi(del_elements[i]) > (int)bmn)) {
			fprintf(stderr, _("bookmarks: %s: No such bookmark\n"),
			    del_elements[i]);
			quit = 1;
		}

		if (quit) {
			free_bms(bms, bmn);
			free_del_elements(del_elements);
			fclose(bm_fp);
			return EXIT_SUCCESS;
		}
	}

	/* If "*", simply remove the bookmarks file */
	/* If there is some "*" in the input line (like "1 5 6-9 *"), it
	 * makes no sense to remove singles bookmarks: Just delete all of
	 * them at once */
	for (i = 0; del_elements[i]; i++) {
		if (strcmp(del_elements[i], "*") == 0) {
			/* Create a backup copy of the bookmarks file, just in case */
			char *bk_file = (char *)NULL;
			bk_file = (char *)xcalloc(strlen(config_dir) + 14,
			    sizeof(char));
			sprintf(bk_file, "%s/bookmarks.bk", config_dir);
			char *tmp_cmd[] = {"cp", bm_file, bk_file, NULL};

			int ret = launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG);
			/* Remove the bookmarks file, free stuff, and exit */
			if (ret == EXIT_SUCCESS) {
				unlink(bm_file);
				printf(_("bookmarks: All bookmarks were deleted\n "
					 "However, a backup copy was created (%s)\n"), bk_file);
				free(bk_file);
				bk_file = (char *)NULL;
			} else {
				printf(_("bookmarks: Error creating backup file. No "
					 "bookmark was deleted\n"));
			}

			free_bms(bms, bmn);
			free_del_elements(del_elements);
			fclose(bm_fp);
			/* Update bookmark names for TAB completion */
			/*          get_bm_names(); */
			free_bookmarks();
			load_bookmarks();

			/* If the argument "*" was specified in command line */
			if (cmd_line != -1)
				fputs(_("All bookmarks succesfully removed\n"), stdout);

			return EXIT_SUCCESS;
		}
	}

	/* Remove single bookmarks */
	/* Open a temporary file */
	char *tmp_file = (char *)NULL;
	tmp_file = (char *)xnmalloc(strlen(config_dir) + 8, sizeof(char));
	sprintf(tmp_file, "%s/bm_tmp", config_dir);

	FILE *tmp_fp = fopen(tmp_file, "w+");
	if (!tmp_fp) {
		free_bms(bms, bmn);
		free_del_elements(del_elements);
		fclose(bm_fp);
		fprintf(stderr, _("bookmarks: Error creating temporary file\n"));

		return EXIT_FAILURE;
	}

	/* Go back to the beginning of the bookmarks file */
	fseek(bm_fp, 0, SEEK_SET);

	/* Dump into the tmp file everything except bookmarks marked for
	 * deletion */

	char *lineb = (char *)NULL;
	while ((line_len = getline(&lineb, &line_size, bm_fp)) > 0) {
		if (lineb[line_len - 1] == '\n')
			lineb[line_len - 1] = '\0';

		int bm_found = 0;
		size_t j;

		for (j = 0; del_elements[j]; j++) {
			if (!is_number(del_elements[j]))
				continue;
			int a = atoi(del_elements[j]);
			if (a == INT_MIN)
				continue;
			if (strcmp(bms[a - 1], lineb) == 0)
				bm_found = 1;
		}

		if (bm_found)
			continue;

		fprintf(tmp_fp, "%s\n", lineb);
	}

	free(lineb);

	/* Free stuff */
	free_del_elements(del_elements);
	free_bms(bms, bmn);

	fclose(bm_fp);
	fclose(tmp_fp);

	/* Remove the old bookmarks file and make the tmp file the new
	 * bookmarks file*/
	unlink(bm_file);
	rename(tmp_file, bm_file);
	free(tmp_file);

	/* Update bookmark names for TAB completion */
	/*  get_bm_names(); */
	free_bookmarks();
	load_bookmarks();

	/* If the bookmark to be removed was specified in command line */
	if (cmd_line != -1)
		printf(_("Successfully removed '%s'\n"), name);

	return EXIT_SUCCESS;
}

static int
bookmark_add(char *file)
{
	if (!file)
		return EXIT_FAILURE;

	int mod_file = 0;
	/* If not absolute path, prepend current path to file */
	if (*file != '/') {
		char *tmp_file = (char *)NULL;
		tmp_file = (char *)xnmalloc((strlen(ws[cur_ws].path) + strlen(file) + 2), sizeof(char));
		sprintf(tmp_file, "%s/%s", ws[cur_ws].path, file);
		file = tmp_file;
		tmp_file = (char *)NULL;
		mod_file = 1;
	}

	/* Check if FILE is an available path */

	FILE *bm_fp = fopen(bm_file, "r");
	if (!bm_fp) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		if (mod_file)
			free(file);

		return EXIT_FAILURE;
	}

	int dup = 0;
	char **bms = (char **)NULL;
	size_t line_size = 0, i, bmn = 0;
	char *line = (char *)NULL;

	while (getline(&line, &line_size, bm_fp) > 0) {
		if (!line || !*line || *line == '#' || *line == '\n')
			continue;

		char *tmp_line = (char *)NULL;
		tmp_line = strchr(line, '/');
		if (tmp_line) {
			size_t tmp_line_len = strlen(tmp_line);

			if (tmp_line_len && tmp_line[tmp_line_len - 1] == '\n')
				tmp_line[tmp_line_len - 1] = '\0';

			if (strcmp(tmp_line, file) == 0) {
				fprintf(stderr, _("bookmarks: %s: Path already "
						  "bookmarked\n"),
				    file);
				dup = 1;
				break;
			}

			tmp_line = (char *)NULL;
		}

		/* Store lines: used later to check hotkeys */
		bms = (char **)xrealloc(bms, (bmn + 1) * sizeof(char *));
		bms[bmn++] = savestring(line, strlen(line));
	}

	free(line);
	line = (char *)NULL;
	fclose(bm_fp);

	if (dup) {
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		if (mod_file)
			free(file);
		return EXIT_FAILURE;
	}

	/* If path is available */

	char *name = (char *)NULL, *hk = (char *)NULL, *tmp = (char *)NULL;

	/* Ask for data to construct the bookmark line. Both values could be
	 * NULL */
	puts(_("Bookmark line example: [sc]name:path"));
	hk = rl_no_hist("Shortcut: ");

	/* Check if hotkey is available */
	if (hk) {
		char *tmp_line = (char *)NULL;

		for (i = 0; i < bmn; i++) {
			tmp_line = strbtw(bms[i], '[', ']');
			if (tmp_line) {
				if (strcmp(hk, tmp_line) == 0) {
					fprintf(stderr, _("bookmarks: %s: This shortcut is "
							  "already in use\n"), hk);

					dup = 1;
					free(tmp_line);
					break;
				}

				free(tmp_line);
			}
		}
	}

	if (dup) {
		if (hk)
			free(hk);
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		if (mod_file)
			free(file);
		return EXIT_FAILURE;
	}

	name = rl_no_hist("Name: ");

	if (name) {
		/* Check name is not duplicated */
		char *tmp_line = (char *)NULL;
		for (i = 0; i < bmn; i++) {
			tmp_line = strbtw(bms[i], ']', ':');
			if (tmp_line) {
				if (strcmp(name, tmp_line) == 0) {
					fprintf(stderr, _("bookmarks: %s: This name is "
							  "already in use\n"), name);
					dup = 1;
					free(tmp_line);
					break;
				}
				free(tmp_line);
			}
		}

		if (dup) {
			free(name);
			if (hk)
				free(hk);
			for (i = 0; i < bmn; i++)
				free(bms[i]);
			free(bms);
			if (mod_file)
				free(file);
			return EXIT_FAILURE;
		}

		/* Generate the bookmark line */
		if (hk) { /* name AND hk */
			tmp = (char *)xcalloc(strlen(hk) + strlen(name) + strlen(file) + 5, sizeof(char));
			sprintf(tmp, "[%s]%s:%s\n", hk, name, file);
			free(hk);
		} else { /* Only name */
			tmp = (char *)xnmalloc(strlen(name) + strlen(file) + 3,
			    sizeof(char));
			sprintf(tmp, "%s:%s\n", name, file);
		}

		free(name);
		name = (char *)NULL;
	}

	else if (hk) { /* Only hk */
		tmp = (char *)xnmalloc(strlen(hk) + strlen(file) + 4,
		    sizeof(char));
		sprintf(tmp, "[%s]%s\n", hk, file);
		free(hk);
		hk = (char *)NULL;
	} else { /* Neither shortcut nor name: only path */
		tmp = (char *)xnmalloc(strlen(file) + 2, sizeof(char));
		sprintf(tmp, "%s\n", file);
	}

	for (i = 0; i < bmn; i++)
		free(bms[i]);
	free(bms);
	bms = (char **)NULL;

	if (!tmp) {
		fprintf(stderr, _("bookmarks: Error generating the bookmark line\n"));
		return EXIT_FAILURE;
	}

	/* Once we have the bookmark line, write it to the bookmarks file */

	bm_fp = fopen(bm_file, "a+");
	if (!bm_fp) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		free(tmp);
		return EXIT_FAILURE;
	}

	if (mod_file)
		free(file);

	if (fseek(bm_fp, 0L, SEEK_END) == -1) {
		fprintf(stderr, _("bookmarks: Error opening the bookmarks file\n"));
		free(tmp);
		fclose(bm_fp);
		return EXIT_FAILURE;
	}

	/* Everything is fine: add the new bookmark to the bookmarks file */
	fprintf(bm_fp, "%s", tmp);
	fclose(bm_fp);
	printf(_("File succesfully bookmarked\n"));
	free(tmp);
	/* Update bookmark names for TAB completion */
	/*  get_bm_names();  */
	free_bookmarks();
	load_bookmarks();
	return EXIT_SUCCESS;
}

int
edit_bookmarks(char *cmd)
{
	int exit_status = EXIT_SUCCESS;

	if (!cmd) {
		open_in_foreground = 1;
		exit_status = open_file(bm_file);
		open_in_foreground = 0;
	} else {
		char *tmp_cmd[] = {cmd, bm_file, NULL};
		if (launch_execve(tmp_cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	if (exit_status == EXIT_FAILURE)
		fprintf(stderr, _("%s: Cannot open the bookmarks file"), PROGRAM_NAME);

	return exit_status;
}

int
open_bookmark(void)
{
	/* If no bookmarks */
	if (bm_n == 0) {
		printf(_("Bookmarks: There are no bookmarks\nEnter 'bm edit' "
			 "or press F11 to edit the bookmarks file. You can "
			 "also enter 'bm add PATH' to add a new bookmark\n"));
		return EXIT_SUCCESS;
	}

	/* We have bookmarks... */
	struct stat file_attrib;

	if (clear_screen)
		CLEAR;

	printf(_("%sBookmarks Manager%s\n\n"), BOLD, df_c);

	/* Print bookmarks taking into account the existence of shortcut,
	 * name, and path for each bookmark */
	size_t i, eln = 0;

	for (i = 0; i < bm_n; i++) {
		if (!bookmarks[i].path || !*bookmarks[i].path)
			continue;
		eln++;
		int is_dir = 0, sc_ok = 0, name_ok = 0, non_existent = 0;
		int path_ok = stat(bookmarks[i].path, &file_attrib);

		if (bookmarks[i].shortcut)
			sc_ok = 1;

		if (bookmarks[i].name)
			name_ok = 1;

		if (path_ok == -1) {
			non_existent = 1;
		} else {
			switch ((file_attrib.st_mode & S_IFMT)) {
			case S_IFDIR: is_dir = 1; break;
			case S_IFREG: break;
			default: non_existent = 1; break;
			}
		}

		printf("%s%zu%s %s%c%s%c%s %s%s%s\n", el_c, eln, df_c,
		    BOLD, sc_ok ? '[' : 0, sc_ok ? bookmarks[i].shortcut : "",
		    sc_ok ? ']' : 0, df_c, non_existent ? GRAY : (is_dir ? bm_c : fi_c),
		    name_ok ? bookmarks[i].name : bookmarks[i].path, df_c);
	}

	/* User selection. Display the prompt */
	char **arg = bm_prompt();
	if (!arg || !*arg)
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;

	/* Case "edit" */
	if (*arg[0] == 'e' && (!arg[0][1] || strcmp(arg[0], "edit") == 0)) {
		stat(bm_file, &file_attrib);
		time_t mtime_bfr = (time_t)file_attrib.st_mtime;

		edit_bookmarks(arg[1] ? arg[1] : NULL);

		stat(bm_file, &file_attrib);
		if (mtime_bfr != (time_t)file_attrib.st_mtime) {
			free_bookmarks();
			load_bookmarks();
		}

		for (i = 0; arg[i]; i++)
			free(arg[i]);
		free(arg);

		arg = (char **)NULL;

		char *tmp_cmd[] = {"bm", NULL};
		bookmarks_function(tmp_cmd);
		return EXIT_SUCCESS;
	}

	/* Case "quit" */
	if (*arg[0] == 'q' && (!arg[0][1] || strcmp(arg[0], "quit") == 0))
		goto FREE_AND_EXIT;

	char *tmp_path = (char *)NULL;

	/* Get the corresponding bookmark path */
	/* If an ELN */
	if (is_number(arg[0])) {
		int num = atoi(arg[0]);
		if (num <= 0 || (size_t)num > bm_n) {
			fprintf(stderr, _("Bookmarks: %s: No such ELN\n"), arg[0]);
			exit_status = EXIT_FAILURE;
			goto FREE_AND_EXIT;
		} else {
			tmp_path = bookmarks[num - 1].path;
		}
	} else {
	/* If string, check shortcuts and names */
		for (i = 0; i < bm_n; i++) {
			if ((bookmarks[i].shortcut && *arg[0] == *bookmarks[i].shortcut
			&& strcmp(arg[0], bookmarks[i].shortcut) == 0)
			|| (bookmarks[i].name && *arg[0] == *bookmarks[i].name
			&& strcmp(arg[0], bookmarks[i].name) == 0)) {

				if (bookmarks[i].path) {
					char *tmp_cmd[] = {"o", bookmarks[i].path,
					    arg[1] ? arg[1] : NULL,
					    NULL};

					exit_status = open_function(tmp_cmd);
					goto FREE_AND_EXIT;
				}

				fprintf(stderr, _("%s: %s: Invalid bookmark\n"),
				    PROGRAM_NAME, arg[0]);
				exit_status = EXIT_FAILURE;
				goto FREE_AND_EXIT;
			}
		}
	}

	if (!tmp_path) {
		fprintf(stderr, _("Bookmarks: %s: No such bookmark\n"), arg[0]);
		exit_status = EXIT_FAILURE;
		goto FREE_AND_EXIT;
	}

	char *tmp_cmd[] = {"o", tmp_path, arg[1] ? arg[1] : NULL, NULL};
	exit_status = open_function(tmp_cmd);
	goto FREE_AND_EXIT;

FREE_AND_EXIT : {
	for (i = 0; arg[i]; i++)
		free(arg[i]);
	free(arg);
	arg = (char **)NULL;
	return exit_status;
}
}

int
bookmarks_function(char **cmd)
{
	if (xargs.stealth_mode == 1) {
		printf(_("%s: Access to configuration files is not allowed in "
			 "stealth mode\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	if (!config_ok) {
		fprintf(stderr, _("Bookmarks function disabled\n"));
		return EXIT_FAILURE;
	}

	/* If the bookmarks file doesn't exist, create it. NOTE: This file
	 * should be created at startup (by get_bm_names()), but we check
	 * it again here just in case it was meanwhile deleted for some
	 * reason */

	/* If no arguments */
	if (!cmd[1])
		return open_bookmark();

	/* Check arguments */

	/* Add a bookmark */
	if (*cmd[1] == 'a' && (!cmd[1][1] || strcmp(cmd[1], "add") == 0)) {
		if (!cmd[2]) {
			puts(_(BOOKMARKS_USAGE));
			return EXIT_SUCCESS;
		}

		if (access(cmd[2], F_OK) != 0) {
			fprintf(stderr, _("Bookmarks: %s: %s\n"), cmd[2],
			    strerror(errno));
			return EXIT_FAILURE;
		}

		return bookmark_add(cmd[2]);
	}

	/* Delete bookmarks */
	if (*cmd[1] == 'd' && (!cmd[1][1] || strcmp(cmd[1], "del") == 0))
		return bookmark_del(cmd[2] ? cmd[2] : NULL);

	/* Edit */
	if (*cmd[1] == 'e' && (!cmd[1][1] || strcmp(cmd[1], "edit") == 0))
		return edit_bookmarks(cmd[2] ? cmd[2] : NULL);

	/* Shortcut, bm name, or (if expand_bookmarks) bm path */
	size_t i;
	for (i = 0; i < bm_n; i++) {
		if ((bookmarks[i].shortcut && *cmd[1] == *bookmarks[i].shortcut
			&& strcmp(cmd[1], bookmarks[i].shortcut) == 0)

			|| (bookmarks[i].name && *cmd[1] == *bookmarks[i].name
			&& strcmp(cmd[1], bookmarks[i].name) == 0)

			|| (expand_bookmarks && bookmarks[i].path
			&& *cmd[1] == *bookmarks[i].path
			&& strcmp(cmd[1], bookmarks[i].path) == 0)) {

			if (bookmarks[i].path) {
				char *tmp_cmd[] = {"o", bookmarks[i].path,
				    cmd[2] ? cmd[2] : NULL, NULL};
				return open_function(tmp_cmd);
			}

			fprintf(stderr, _("Bookmarks: %s: Invalid bookmark\n"),
			    cmd[1]);
			return EXIT_FAILURE;
		}
	}

	fprintf(stderr, _("Bookmarks: %s: No such bookmark\n"), cmd[1]);
	return EXIT_FAILURE;
}
