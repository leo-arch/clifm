/* bookmarks.c -- bookmarking functions */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2023, L. Abramovich <johndoe.arch@outlook.com>
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
#include <readline/readline.h> // tilde_expand
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "aux.h"
#include "bookmarks.h"
#include "checks.h"
#include "exec.h"
#include "file_operations.h"
#include "init.h"
#include "readline.h"
#include "messages.h"
#include "listing.h"
#include "misc.h"

#define NO_BOOKMARKS "bookmarks: There are no bookmarks\nEnter 'bm edit' \
or press F11 to edit the bookmarks file. You can also enter 'bm add PATH' \
to add a new bookmark\n"
#define PRINT_BM_HEADER 1
#define NO_BM_HEADER    0
#define BM_SCREEN       1 /* The edit function is called from the bookmarks screen */
#define NO_BM_SCREEN    0

void
free_bookmarks(void)
{
	if (bm_n == 0)
		return;

	size_t i;
	for (i = 0; i < bm_n; i++) {
		free(bookmarks[i].shortcut);
		free(bookmarks[i].name);
		free(bookmarks[i].path);
	}

	free(bookmarks);
	bookmarks = (struct bookmarks_t *)NULL;
	bm_n = 0;

	return;
}

void
reload_bookmarks(void)
{
	free_bookmarks();
	load_bookmarks();
}

static char **
bm_prompt(const int print_header)
{
	char *bm = (char *)NULL;
	if (print_header)
		printf(_("%s%s\nEnter '%c' to edit your bookmarks or '%c' to quit\n"
			"Choose a bookmark (by ELN, shortcut, or name):\n"), NC, df_c, 'e', 'q');

	char bm_str[(MAX_COLOR * 2) + 7];
	snprintf(bm_str, sizeof(bm_str), "\001%s\002>\001%s\002 ", mi_c, tx_c);
	while (!bm)
		bm = rl_no_hist(bm_str);

	flags |= IN_BOOKMARKS_SCREEN;
	char **cmd = split_str(bm, NO_UPDATE_ARGS);
	flags &= ~IN_BOOKMARKS_SCREEN;
	free(bm);

	return cmd;
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
		bms[bmn] = savestring(line, (size_t)line_len);
		bmn++;
	}

	free(line);
	line = (char *)NULL;

	if (bmn == 0) {
		printf(_("%s: No bookmarks\n"), PROGRAM_NAME);
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
		char *p = dequote_str(name, 0);
		if (p) {
			strcpy(name, p);
			free(p);
		}
		for (i = 0; i < bmn; i++) {
			char *bm_name = (char *)NULL;
			if (*bms[i] == '[') {
				bm_name = strbtw(bms[i], ']', ':');
			} else {
				char *s = strchr(bms[i], ':');
				if (s) {
					*s = '\0';
					bm_name = savestring(bms[i], strlen(bms[i]));
					*s = ':';
				}
			}
			if (!bm_name)
				continue;
			if (*name == *bm_name && strcmp(name, bm_name) == 0) {
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
		sprintf(del_elements[0], "%d", cmd_line + 1); /* NOLINT */
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
			input = rl_no_hist(_("Bookmark(s) to be deleted (ex: 1 2-6, or *): "));
		del_elements = get_substr(input, ' ');
		free(input);
		input = (char *)NULL;

		if (!del_elements) {
			free_bms(bms, bmn);
			fclose(bm_fp);
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("bookmarks: Error parsing input\n"));
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

		if (*del_elements[i] == 'q' && !*(del_elements[i] + 1)) {
			quit = 1;
		} else {
			int n = atoi(del_elements[i]);
			if (is_number(del_elements[i]) && (n <= 0 || n > (int)bmn)) {
				fprintf(stderr, _("bookmarks: %s: No such bookmark\n"), del_elements[i]);
				quit = 1;
			}
		}

		if (quit == 1) {
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
			bk_file = (char *)xcalloc(config_dir_len + 14, sizeof(char));
			sprintf(bk_file, "%s/bookmarks.bk", config_dir); /* NOLINT */
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

			reload_bookmarks();

			/* If the argument "*" was specified in command line */
			if (cmd_line != -1)
				fputs(_("bookmarks: All bookmarks succesfully removed\n"), stdout);

			return EXIT_SUCCESS;
		}
	}

	/* Remove single bookmarks */
	/* Open a temporary file */
	char *tmp_file = (char *)NULL;
	tmp_file = (char *)xnmalloc(config_dir_len + 8, sizeof(char));
	sprintf(tmp_file, "%s/bm_tmp", config_dir); /* NOLINT */

	FILE *tmp_fp = fopen(tmp_file, "w+");
	if (!tmp_fp) {
		free_bms(bms, bmn);
		free_del_elements(del_elements);
		fclose(bm_fp);
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("bookmarks: Error creating temporary "
			"file: %s: %s\n"), tmp_file, strerror(errno));
		return errno;
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

	reload_bookmarks();

	/* If the bookmark to be removed was specified in command line */
	if (cmd_line != -1)
		printf(_("bookmarks: Successfully removed %s%s%s\n"), BOLD, name, NC);

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
		tmp_file = (char *)xnmalloc((strlen(workspaces[cur_ws].path) + strlen(file) + 2),
			sizeof(char));
		sprintf(tmp_file, "%s/%s", workspaces[cur_ws].path, file); /* NOLINT */
		file = tmp_file;
		tmp_file = (char *)NULL;
		mod_file = 1;
	}

	/* Check if FILE is an available path */
	FILE *bm_fp = fopen(bm_file, "r");
	if (!bm_fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "bookmarks: fopen: %s: %s\n",
			bm_file, strerror(errno));
		if (mod_file)
			free(file);
		return errno;
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

			if (tmp_line_len > 0 && tmp_line[tmp_line_len - 1] == '\n')
				tmp_line[tmp_line_len - 1] = '\0';

			if (strcmp(tmp_line, file) == 0) {
				fprintf(stderr, _("bookmarks: %s: Path already bookmarked\n"), file);
				dup = 1;
				break;
			}

			tmp_line = (char *)NULL;
		}

		/* Store lines: used later to check for duplicated shortcuts and names */
		bms = (char **)xrealloc(bms, (bmn + 1) * sizeof(char *));
		bms[bmn] = savestring(line, strlen(line));
		bmn++;
	}

	free(line);
	line = (char *)NULL;
	fclose(bm_fp);

	if (dup == 1) {
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		if (mod_file)
			free(file);
		return EXIT_FAILURE;
	}

	/* If path is available */

	char *name = (char *)NULL, *hk = (char *)NULL, *tmp = (char *)NULL;
	char _prompt[(MAX_COLOR * 2) + 17];

	/* Ask for data to construct the bookmark line. Both values could be NULL */
	puts(_("Enter 'q' to quit"));
	puts(_("Bookmark: [shortcut]name:path (Ex: [g]games:~/games)"));
	snprintf(_prompt, sizeof(_prompt), "%c%s%c>%c%s%c Shortcut: ",
		RL_PROMPT_START_IGNORE, mi_c, RL_PROMPT_END_IGNORE,
		RL_PROMPT_START_IGNORE, tx_c, RL_PROMPT_END_IGNORE);
	hk = rl_no_hist(_prompt);

	if (hk && *hk == 'q' && !*(hk + 1)) {
		free(hk);
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		if (mod_file)
			free(file);
		return EXIT_SUCCESS;
	}

	/* Check if hotkey is available */
	if (hk) {
		char *tmp_line = (char *)NULL;

		for (i = 0; i < bmn; i++) {
			tmp_line = strbtw(bms[i], '[', ']');
			if (!tmp_line)
				continue;
			if (strcmp(hk, tmp_line) == 0) {
				fprintf(stderr, _("bookmarks: %s: Shortcut already taken\n"), hk);
				dup = 1;
				free(tmp_line);
				break;
			}

			free(tmp_line);
		}
	}

	if (dup == 1) {
		if (hk)
			free(hk);
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		if (mod_file)
			free(file);
		return EXIT_FAILURE;
	}

	snprintf(_prompt, sizeof(_prompt), "%c%s%c>%c%s%c Name: ",
		RL_PROMPT_START_IGNORE, mi_c, RL_PROMPT_END_IGNORE,
		RL_PROMPT_START_IGNORE, tx_c, RL_PROMPT_END_IGNORE);
	name = rl_no_hist(_prompt);

	if (name && *name == 'q' && !*(name + 1)) {
		free(hk);
		free(name);
		for (i = 0; i < bmn; i++)
			free(bms[i]);
		free(bms);
		if (mod_file)
			free(file);
		return EXIT_SUCCESS;
	}

	if (name) {
		/* Check name is not duplicated */
		char *tmp_line = (char *)NULL;
		for (i = 0; i < bmn; i++) {
			if (*bms[i] == '[') {
				tmp_line = strbtw(bms[i], ']', ':');
			} else {
				char *s = strchr(bms[i], ':');
				if (s) {
					*s = '\0';
					tmp_line = savestring(bms[i], strlen(bms[i]));
					*s = ':';
				}
			}

			if (!tmp_line)
				continue;
			if (*name == *tmp_line && strcmp(name, tmp_line) == 0) {
				fprintf(stderr, _("bookmarks: %s: Name already taken\n"), name);
				dup = 1;
				free(tmp_line);
				break;
			}
			free(tmp_line);
			tmp_line = (char *)NULL;
		}

		if (dup == 1) {
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
			sprintf(tmp, "[%s]%s:%s\n", hk, name, file); /* NOLINT */
			free(hk);
		} else { /* Only name */
			tmp = (char *)xnmalloc(strlen(name) + strlen(file) + 3, sizeof(char));
			sprintf(tmp, "%s:%s\n", name, file); /* NOLINT */
		}

		free(name);
		name = (char *)NULL;
	}

	else if (hk) { /* Only hk */
		tmp = (char *)xnmalloc(strlen(hk) + strlen(file) + 4, sizeof(char));
		sprintf(tmp, "[%s]%s\n", hk, file); /* NOLINT */
		free(hk);
		hk = (char *)NULL;
	} else { /* Neither shortcut nor name: only path */
		tmp = (char *)xnmalloc(strlen(file) + 2, sizeof(char));
		sprintf(tmp, "%s\n", file); /* NOLINT */
	}

	for (i = 0; i < bmn; i++)
		free(bms[i]);
	free(bms);
	bms = (char **)NULL;

	if (!tmp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("bookmarks: Error generating the "
			"bookmark line\n"));
		return EXIT_FAILURE;
	}

	/* Once we have the bookmark line, write it to the bookmarks file */
	bm_fp = fopen(bm_file, "a+");
	if (!bm_fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "bookmarks: fopen: %s: %s\n",
			bm_file, strerror(errno));
		free(tmp);
		return errno;
	}

	if (mod_file)
		free(file);

	if (fseek(bm_fp, 0L, SEEK_END) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "bookmarks: fseek: %s: %s\n",
			bm_file, strerror(errno));
		int err = errno;
		free(tmp);
		fclose(bm_fp);
		return err;
	}

	/* Everything is fine: add the new bookmark to the bookmarks file */
	fprintf(bm_fp, "%s", tmp);
	fclose(bm_fp);
	printf(_("bookmarks: File succesfully bookmarked\n"));
	free(tmp);
	reload_bookmarks(); /* Update bookmarks for TAB completion */

	return EXIT_SUCCESS;
}

static int
edit_bookmarks(char *cmd, const int flag)
{
	struct stat a;
	if (stat(bm_file, &a) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "bookmarks: %s: %s\n", bm_file, strerror(errno));
		return errno;
	}
	time_t prev = a.st_mtime;

	int ret = EXIT_SUCCESS;
	if (!cmd) {
		open_in_foreground = 1;
		ret = open_file(bm_file);
		open_in_foreground = 0;
	} else {
		char *tmp_cmd[] = {cmd, bm_file, NULL};
		ret = launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG);
	}

	if (ret != EXIT_SUCCESS) {
		if (!cmd)
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("bookmarks: Error opening the bookmarks file\n"));
		return ret;
	}

	stat(bm_file, &a);
	if (prev != a.st_mtime) {
		reload_bookmarks();
		if (flag == NO_BM_SCREEN) {
			reload_dirlist();
			print_reload_msg(_("File modified. Bookmarks reloaded\n"));
		}
	}

	return EXIT_SUCCESS;
}

static size_t
get_largest_shortcut(void)
{
	if (bm_n == 0 || !bookmarks)
		return 0;

	size_t l = 0;
	int i = (int)bm_n;
	while (--i >= 0) {
		if (!bookmarks[i].shortcut || !*bookmarks[i].shortcut)
			continue;
		size_t slen = strlen(bookmarks[i].shortcut);
		if (slen > l)
			l = slen;
	}

	return l;
}

static void
print_bookmarks(void)
{
	printf(_("%sBookmarks Manager%s\n\n"), BOLD, df_c);

	struct stat attr;
	int eln_pad = DIGINUM(bm_n); /* Longest shortcut name to properly pad output */

	size_t ls = get_largest_shortcut();

	/* Print bookmarks, taking into account shortcut, name, and path */
	for (size_t i = 0; i < bm_n; i++) {
		if (!bookmarks[i].path || !*bookmarks[i].path) continue;
		int is_dir = 0, sc_ok = 0, name_ok = 0, non_existent = 0;
		char *p = *bookmarks[i].path == '~' ? tilde_expand(bookmarks[i].path) : (char *)NULL;
		int path_ok = stat(p ? p : bookmarks[i].path, &attr);
		free(p);

		if (bookmarks[i].shortcut) sc_ok = 1;
		if (bookmarks[i].name) name_ok = 1;

		if (path_ok == -1) {
			non_existent = 1;
		} else {
			switch ((attr.st_mode & S_IFMT)) {
			case S_IFDIR: is_dir = 1; break;
			case S_IFREG: break;
			default: non_existent = 1; break;
			}
		}

		int sc_pad = (int)ls;
		if (sc_ok == 0 && ls > 0)
			sc_pad += 2; // No shortcut. Let's count '[' and ']'
		if (sc_ok == 1)
			sc_pad -= (int)strlen(bookmarks[i].shortcut);
		if (sc_pad < 0)
			sc_pad = 0;

		printf("%s%s%-*zu%s%c%s%c%s%c%s%-*s %s%s%s\n",
			NC, el_c, eln_pad, i + 1, df_c, ls > 0 ? ' ' : 0,
		    BOLD, sc_ok == 1 ? '[' : 0, sc_ok ? bookmarks[i].shortcut : "",
		    sc_ok == 1 ? ']' : 0, df_c, sc_pad, "",
		    non_existent ? (conf.colorize ? uf_c : "\x1b[0m\x1b[4m")
		    : (!is_dir ? fi_c : (name_ok ? bm_c : di_c)),
		    name_ok ? bookmarks[i].name : bookmarks[i].path, df_c);
	}
}

static int
_edit_bookmarks(char **arg)
{
	edit_bookmarks(arg[1], BM_SCREEN);

	size_t i;
	for (i = 0; arg[i]; i++)
		free(arg[i]);
	free(arg);

	char *tmp_cmd[] = {"bm", NULL};
	bookmarks_function(tmp_cmd);
	return EXIT_SUCCESS;
}

/* Return a pointer to the bookmark path (in the bookmarks array)
 * corresponding to ARG (either an ELN or a string). Otherwise return NULL
 * The return value of this function must not be free'd */
static char *
get_bm_path(char *arg)
{
	/* If an ELN */
	if (is_number(arg)) {
		int num = atoi(arg);
		if (num <= 0 || (size_t)num > bm_n) {
			fprintf(stderr, _("%s: No such ELN\n"), arg);
			return (char *)NULL;
		}
		return bookmarks[num - 1].path;
	}

	/* If string, check shortcuts and names */
	size_t i;
	for (i = 0; i < bm_n; i++) {
		if ((bookmarks[i].shortcut && *arg == *bookmarks[i].shortcut
		&& strcmp(arg, bookmarks[i].shortcut) == 0)
		|| (bookmarks[i].name && *arg == *bookmarks[i].name
		&& strcmp(arg, bookmarks[i].name) == 0)) {
			if (bookmarks[i].path)
				return bookmarks[i].path;

			fprintf(stderr, _("%s: Invalid bookmark\n"), arg);
			return (char *)NULL;
		}
	}

	fprintf(stderr, _("%s: No such bookmark\n"), arg);
	return (char *)NULL;
}

static void
free_bm_input(char ***p)
{
	size_t i;
	for (i = 0; (*p)[i]; i++)
		free((*p)[i]);
	free(*p);
	*p = (char **)NULL;
}

/* This function takes care of the bookmarks screen */
int
open_bookmark(void)
{
	if (bm_n == 0) { printf(_(NO_BOOKMARKS)); return EXIT_SUCCESS; }
	if (conf.clear_screen) CLEAR;

	int exit_status = EXIT_SUCCESS, header_printed = 0,	is_dir = 0;

	print_bookmarks();
	char **arg = (char **)NULL;
	while (!arg) {
		arg = bm_prompt(header_printed == 1 ? NO_BM_HEADER : PRINT_BM_HEADER);
		header_printed = 1;
		if (!arg) continue;

		if (*arg[0] == 'e' && (!arg[0][1] || strcmp(arg[0], "edit") == 0))
			return _edit_bookmarks(arg);

		if (*arg[0] == 'q' && (!arg[0][1] || strcmp(arg[0], "quit") == 0))
			break;

		char *tmp_path = get_bm_path(arg[0]);
		if (!tmp_path) { free_bm_input(&arg); continue;	}

		char *ep = *tmp_path == '~' ? tilde_expand(tmp_path) : (char *)NULL;
		char *p = ep ? ep : tmp_path;

		struct stat a; /* If not a dir, refresh files list */
		if (stat(p, &a) != -1 && S_ISDIR(a.st_mode))
			is_dir = 1;

		char *tmp_cmd[] = {"o", p, arg[1] ? arg[1] : NULL, NULL};
		exit_status = open_function(tmp_cmd);
		free(ep);
		if (exit_status != EXIT_SUCCESS)
			{ free_bm_input(&arg); continue; }
		break;
	}

	free_bm_input(&arg);
	if (conf.autols == 1 && is_dir == 0) reload_dirlist();
	return EXIT_SUCCESS;
}

/* Open a bookmark by shortcut, bm name, or (if expand_bookmarks) bm path */
static int
bm_open(char **cmd)
{
	char *p = dequote_str(cmd[1], 0);
	if (!p) p = cmd[1];

	size_t i;
	for (i = 0; i < bm_n; i++) {
		if ((bookmarks[i].shortcut && *p == *bookmarks[i].shortcut
		&& strcmp(p, bookmarks[i].shortcut) == 0)

		|| (bookmarks[i].name && *p == *bookmarks[i].name
		&& strcmp(p, bookmarks[i].name) == 0)) {

/*		|| (conf.expand_bookmarks && bookmarks[i].path
		&& *p == *bookmarks[i].path
		&& strcmp(p, bookmarks[i].path) == 0)) { */

			if (!bookmarks[i].path) {
				fprintf(stderr, _("%s: Invalid bookmark\n"), p);
				if (p != cmd[1]) free(p);
				return EXIT_FAILURE;
			}

			if (p != cmd[1]) free(p);

			char *et = *bookmarks[i].path == '~'
				? tilde_expand(bookmarks[i].path) : (char *)NULL;
			char *tmp_cmd[] = {"o", et ? et : bookmarks[i].path, cmd[2], NULL};
			int ret = open_function(tmp_cmd);
			free(et);
			return ret;
		}
	}

	fprintf(stderr, _("%s: No such bookmark\n"), p);
	if (p != cmd[1]) free(p);
	return EXIT_FAILURE;
}

static int
check_bm_path(char *file)
{
	if (bm_n == 0 || !bookmarks)
		return 1;

	char *p = normalize_path(file, strlen(file));
	char *f = p ? p : file;

	int i = (int)bm_n;
	while (--i >= 0) {
		if (!bookmarks[i].path)
			continue;
		if (*f == *bookmarks[i].path && strcmp(f, bookmarks[i].path) == 0) {
			fprintf(stderr, "bookmarks: %s: Path already "
				"bookmarked as '%s'\n", f, bookmarks[i].name
				? bookmarks[i].name : "unnamed");
			free(p);
			return 0;
		}
	}

	free(p);
	return 1;
}

static int
name_is_reserved_keyword(const char *name)
{
	if (!name || !*name)
		return 0;

	if ( ((*name == 'q' || *name == 'e' || *name == 'd'
	|| *name == 'a') && !*(name + 1))
	|| strcmp(name, "quit") == 0 || strcmp(name, "edit") == 0
	|| strcmp(name, "del") == 0 || strcmp(name, "add") == 0) {
		fprintf(stderr, _("bookmarks: '%s': Reserved bookmark keyword\n"), name);
		return 1;
	}

	return 0;
}

static int
check_bm_name(const char *name)
{
	if (bm_n == 0 || !bookmarks)
		return 1;

	if (name_is_reserved_keyword(name) == 1)
		return 0;

	int i = (int)bm_n;
	while (--i >= 0) {
		if (!bookmarks[i].name)
			continue;
		if (*name == *bookmarks[i].name
		&& strcmp(name, bookmarks[i].name) == 0) {
			fprintf(stderr, _("bookmarks: %s: Name already taken\n"), name);
			return 0;
		}
	}

	return 1;
}

static int
check_bm_shortcut(const char *shortcut)
{
	if (bm_n == 0 || !bookmarks)
		return 1;

	if (name_is_reserved_keyword(shortcut) == 1)
		return 0;

	int i = (int)bm_n;
	while (--i >= 0) {
		if (!bookmarks[i].shortcut)
			continue;
		if (*shortcut == *bookmarks[i].shortcut
		&& strcmp(shortcut, bookmarks[i].shortcut) == 0) {
			fprintf(stderr, _("bookmarks: %s: Shortcut already taken\n"), shortcut);
			return 0;
		}
	}

	return 1;
}

static int
add_bookmark_not_interactive(char *file, char *name, char *shortcut)
{
	/* FILE and NAME are guarranteed to be non-NULL
	 * FILE is already dequoted */

	if (check_bm_path(file) == 0)
		return EXIT_FAILURE;

	char *p = dequote_str(name, 0);
	char *n = p ? p : name;

	if (check_bm_name(n) == 0) {
		free(p);
		return EXIT_FAILURE;
	}

	char *q = (char *)NULL;
	char *s = (char *)NULL;
	if (shortcut) {
		q = dequote_str(shortcut, 0);
		s = q ? q : shortcut;

		if (check_bm_shortcut(s) == 0) {
			free(p);
			free(q);
			return EXIT_FAILURE;
		}
	}

	/* Once we have path, name and (optioanlly) shortcut, write it to the bookmarks file */
	FILE *bm_fp = fopen(bm_file, "a+");
	if (!bm_fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "bookmarks: fopen: %s: %s\n",
			bm_file, strerror(errno));
		free(p);
		free(q);
		return errno;
	}

	if (fseek(bm_fp, 0L, SEEK_END) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "bookmarks: fseek: %s: %s\n",
			bm_file, strerror(errno));
		int err = errno;
		free(p);
		free(q);
		fclose(bm_fp);
		return err;
	}

	char *np = normalize_path(file, strlen(file));

	/* Everything is fine: add the new bookmark to the bookmarks file */
	if (s)
		fprintf(bm_fp, "[%s]%s:%s\n", s, n, np ? np : file);
	else
		fprintf(bm_fp, "%s:%s\n", n, np ? np : file);

	fclose(bm_fp);

	printf(_("File succesfully bookmarked\n"));
	if (s)
		printf("[%s]%s %s->%s %s\n", s, n, mi_c, tx_c, np ? np : file);
	else
		printf("%s %s->%s %s\n", n, mi_c, tx_c, np ? np : file);

	free(np);
	free(p);
	free(q);

	reload_bookmarks(); /* Update bookmarks for TAB completion */

	return EXIT_SUCCESS;
}

static int
add_bookmark(char **cmd)
{
	if (!cmd || !cmd[0]) {
		puts(_(BOOKMARKS_USAGE));
		return EXIT_SUCCESS;
	}

	char *p = dequote_str(cmd[0], 0);
	if (!p) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("bookmarks: %s: Error dequoting file name\n"), cmd[0]);
		return EXIT_FAILURE;
	}

	if (access(p, F_OK) != 0) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "bookmarks: %s: %s\n", p, strerror(errno));
		free(p);
		return errno;
	}

	int ret = EXIT_SUCCESS;

	if (cmd[1]) {
		ret = add_bookmark_not_interactive(p, cmd[1], cmd[2]);
		free(p);
		return ret;
	}

	ret = bookmark_add(p);
	free(p);
	return ret;
}

/* Handle bookmarks: run the corresponding function according to CMD */
int
bookmarks_function(char **cmd)
{
	if (xargs.stealth_mode == 1) {
		printf(_("%s: bookamrks: %s\n"), PROGRAM_NAME, STEALTH_DISABLED);
		return EXIT_SUCCESS;
	}

	if (config_ok == 0) {
		fprintf(stderr, _("%s: Bookmarks function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* If no arguments, load the bookmarks screen */
	if (!cmd[1])
		return open_bookmark();

	/* Add a bookmark */
	if (*cmd[1] == 'a' && (!cmd[1][1] || strcmp(cmd[1], "add") == 0))
		return add_bookmark(cmd + 2);

	/* Delete bookmarks */
	if (*cmd[1] == 'd' && (!cmd[1][1] || strcmp(cmd[1], "del") == 0))
		return bookmark_del(cmd[2]);

	/* Edit */
	if (*cmd[1] == 'e' && (!cmd[1][1] || strcmp(cmd[1], "edit") == 0))
		return edit_bookmarks(cmd[2], NO_BM_SCREEN);

	if (*cmd[1] == 'r' && (!cmd[1][1] || strcmp(cmd[1], "reload") == 0)) {
		reload_bookmarks();
		return EXIT_SUCCESS;
	}

	/* Shortcut, bm name, or (if expand_bookmarks) bm path */
	return bm_open(cmd);
}
