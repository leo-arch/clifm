/* navigation.c -- functions to control the navigation system */

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

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <readline/readline.h>

#include "aux.h"
#include "checks.h"
#include "history.h"
#include "jump.h"
#include "listing.h"
#include "misc.h"
#include "navigation.h"
#include "messages.h"
#include "readline.h"

int
handle_workspaces(char *str)
{
	if (!str || !*str) {
		int i;
		for (i = 0; i < MAX_WS; i++) {
			if (i == cur_ws) {
				printf("%s%d: %s%s\n", mi_c, i + 1, workspaces[i].path, df_c);
			} else {
				printf("%d: %s\n", i + 1, workspaces[i].path
				? workspaces[i].path : "none");
			}
		}
		return EXIT_SUCCESS;
	}

	if (*str == '-' && strcmp(str, "--help") == 0) {
		puts(_(WS_USAGE));
		return EXIT_SUCCESS;
	}

	int tmp_ws = 0;

	if (is_number(str)) {
		int istr = atoi(str);
		if (istr <= 0 || istr > MAX_WS) {
			fprintf(stderr, _("%s: %d: Invalid workspace number\n"),
			    PROGRAM_NAME, istr);
			return EXIT_FAILURE;
		}

		tmp_ws = istr - 1;

		if (tmp_ws == cur_ws) {
			fprintf(stderr, _("%s: %d is already the current workspace\n"),
					PROGRAM_NAME, tmp_ws + 1);
			return EXIT_SUCCESS;
		}
	} else if (*str == '+' && !str[1]) {
		if ((cur_ws + 1) < MAX_WS)
			tmp_ws = cur_ws + 1;
		else
			return EXIT_FAILURE;
	} else {
		if (*str == '-' && !str[1]) {
			if ((cur_ws - 1) >= 0)
				tmp_ws = cur_ws - 1;
			else
				return EXIT_FAILURE;
		}
	}

	/* If new workspace has no path yet, copy the path of the current
	 * workspace */
	if (!workspaces[tmp_ws].path) {
		workspaces[tmp_ws].path = savestring(workspaces[cur_ws].path,
		    strlen(workspaces[cur_ws].path));
	} else {
		if (access(workspaces[tmp_ws].path, R_OK | X_OK) != EXIT_SUCCESS) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				workspaces[tmp_ws].path, strerror(errno));
			free(workspaces[tmp_ws].path);
			workspaces[tmp_ws].path = savestring(workspaces[cur_ws].path,
				strlen(workspaces[cur_ws].path));
		}
	}

	if (xchdir(workspaces[tmp_ws].path, SET_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, workspaces[tmp_ws].path,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	cur_ws = tmp_ws;
	int exit_status = EXIT_SUCCESS;

	dir_changed = 1;
	if (autols) {
		free_dirlist();
		exit_status = list_dir();
	}

	add_to_dirhist(workspaces[cur_ws].path);
	return exit_status;
}

/* Return the list of paths in CWD matching STR */
char **
get_bd_matches(const char *str, int *n, int mode)
{
	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
		return (char **)NULL;

	char *cwd = workspaces[cur_ws].path;
	char **matches = (char **)NULL;

	if (mode == BD_TAB) {
		/* matches will be passed to readline for TAB completion, so
		 * that we need to reserve the first slot to hold the query
		 * string */
		*n = 1;
		matches = (char **)xnmalloc(2, sizeof(char *));
	}

	while(1) {
		char *p = (char *)NULL;
		if (str && *str) { /* Non-empty query string */
			p = case_sens_path_comp ? strstr(cwd, str) : strcasestr(cwd, str);
			if (!p)
				break;
		}
		char *q = strchr(p ? p : cwd, '/');
		if (!q) {
			if (!*(++cwd))
				break;
			continue;
		}
		*q = '\0';
		matches = (char **)xrealloc(matches, (size_t)(*n + 2) * sizeof(char *));
		if (mode == BD_TAB) {
			/* Print only the path base name */
			char *ss = strrchr(workspaces[cur_ws].path, '/');
			if (ss && *(++ss))
				matches[(*n)++] = savestring(ss, strlen(ss));
			else /* Last slash is the first and only char: We have root dir */
				matches[(*n)++] = savestring("/", 1);
		} else {
			if (!*workspaces[cur_ws].path) {
				matches[(*n)++] = savestring("/", 1);
			} else {
				matches[(*n)++] = savestring(workspaces[cur_ws].path,
						strlen(workspaces[cur_ws].path));
			}
		}
		*q = '/';
		cwd = q + 1;

		if (!*cwd)
			break;
	}

	if (mode == BD_TAB) {
		if (*n == 1) { /* No matches */
			free(matches);
			return (char **)NULL;
		} else if (*n == 2) { /* One match */
			char *p = escape_str(matches[1]);
			if (!p) {
				free(matches);
				return (char **)NULL;
			}
			matches[0] = savestring(p, strlen(p));
			free(matches[1]);
			matches[1] = (char *)NULL;
			free(p);
		} else { /* Multiple matches */
			matches[0] = savestring(str, strlen(str));
			matches[*n] = (char *)NULL;
		}
	} else if (*n > 0) {
		matches[*n] = (char *)NULL;
	}

	return matches;
}

static int
grab_bd_input(int n)
{
	char *input = (char *)NULL;
	putchar('\n');
	while (!input) {
		input = rl_no_hist("Choose a directory ('q' to quit): ");
		if (!input)
			continue;
		if (!*input) {
			free(input);
			input = (char *)NULL;
			continue;
		}
		if (is_number(input)) {
			int a = atoi(input);
			if (a > 0 && a <= n) {
				free(input);
				return a - 1;
			} else {
				free(input);
				input = (char *)NULL;
				continue;
			}
		} else if (*input == 'q' && !*(input + 1)) {
			free(input);
			return (-1);
		} else {
			free(input);
			input = (char *)NULL;
			continue;
		}
	}

	return (-1); // Never reached
}

/* Change to parent directory matching STR */
int
backdir(const char* str)
{
	if (str && *str == '-' && strcmp(str, "--help") == 0) {
		puts(_(BD_USAGE));
		return EXIT_SUCCESS;
	}

	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1]) {
		printf(_("%s: /: No parent directory\n"), PROGRAM_NAME);
		return EXIT_SUCCESS;
	}

	char *deq_str = (char *)NULL;
	if (str) {
		deq_str = dequote_str((char *)str, 0);
		if (!deq_str) {
			fprintf(stderr, _("%s: %s: Error dequoting string\n"),
				PROGRAM_NAME, str);
			return EXIT_FAILURE;
		}

		if (*deq_str == '~') {
			char *exp_path = tilde_expand(deq_str);
			if (!exp_path) {
				fprintf(stderr, _("%s: %s: Error expanding tilde\n"),
					PROGRAM_NAME, deq_str);
				free(deq_str);
				return EXIT_FAILURE;
			}
			free(deq_str);
			deq_str = exp_path;
		}

		/* If STR is a directory, just change to it */
		struct stat a;
		if (stat(deq_str, &a) == 0 && S_ISDIR(a.st_mode)) {
			int ret = cd_function(deq_str, CD_PRINT_ERROR);
			free(deq_str);
			return ret;
		}
	}

	if (!workspaces[cur_ws].path) {
		free(deq_str);
		return EXIT_FAILURE;
	}

	int n = 0;
	char **matches = get_bd_matches(deq_str ? deq_str : str, &n, BD_NO_TAB);
	free(deq_str);

	if (n == 0) {
		fprintf(stderr, _("%s: %s: No matches found\n"), PROGRAM_NAME, str);
		return EXIT_FAILURE;
	}

	int exit_status = EXIT_SUCCESS, i;
	if (n == 1) {
		/* Just one match: change to it */
		exit_status = cd_function(matches[0], CD_PRINT_ERROR);
	} else if (n > 1) {
		/* If multiple matches, print a menu to choose from */
		for (i = 0; matches[i]; i++) {
			char *sl = strrchr(matches[i], '/');
			int flag = 0;
			if (sl && *(sl + 1)) {
				*sl = '\0';
				sl++;
				flag = 1;
			}
			printf("%s%d%s %s%s%s\n", el_c, i + 1, df_c, di_c, sl ? sl : "/", df_c);
			if (flag) {
				sl--;
				*sl = '/';
			}
		}
		int choice = grab_bd_input(i);
		if (choice != -1)
			exit_status = cd_function(matches[choice], CD_PRINT_ERROR);
	}

	i = n;
	while (--i >= 0)
		free(matches[i]);
	free(matches);

	return exit_status;
}

/* Make sure DIR exists, it is actually a directory and is readable.
 * Only then change directory */
int
xchdir(const char *dir, const int set_title)
{
	if (!dir || !*dir)
		return -1;

	DIR *dirp = opendir(dir);

	if (!dirp)
		return -1;

	closedir(dirp);

	int ret;
	ret = chdir(dir);

	if (set_title && ret == 0 && xargs.cwd_in_title == 1)
		set_term_title(dir);

	return ret;
}

static char *
check_cdpath(char *name)
{
	if (cdpath_n == 0 || !name || !*name)
		return (char *)NULL;

	if (*name == '/' || (*name == '.' && name[1] == '/')
	|| (*name == '.' && name[1] == '.' && name[2] == '/'))
		return (char *)NULL;

	size_t i;
	char t[PATH_MAX];
	char *p = (char *)NULL;
	struct stat attr;
	for (i = 0; cdpaths[i]; i++) {
		size_t len = strlen(cdpaths[i]);
		if (cdpaths[i][len - 1] == '/')
			snprintf(t, PATH_MAX, "%s%s", cdpaths[i], name);
		else
			snprintf(t, PATH_MAX, "%s/%s", cdpaths[i], name);
		if (stat(t, &attr) != -1 && (attr.st_mode & S_IFMT) == S_IFDIR) {
			p = (char *)xnmalloc(strlen(t) + 1, sizeof(char));
			strcpy(p, t);
			break;
		}
	}

	return p;
}

/* Change CliFM working directory to NEW_PATH */
int
cd_function(char *new_path, const int print_error)
{
	/* If no argument, change to home */
	if (!new_path || !*new_path) {
		if (!user.home) {
			if (print_error) {
				fprintf(stderr, _("%s: cd: Home directory not found\n"),
					PROGRAM_NAME);
			}
			return EXIT_FAILURE;
		}

		if (xchdir(user.home, SET_TITLE) != EXIT_SUCCESS) {
			if (print_error) {
				fprintf(stderr, "%s: cd: %s: %s\n", PROGRAM_NAME,
					user.home, strerror(errno));
			}
			return EXIT_FAILURE;
		}

		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(user.home, strlen(user.home));
	}

	/* If we have some argument, dequote it, resolve it with realpath(),
	 * cd into the resolved path, and set the path variable to this
	 * latter */
	else {
		if (strchr(new_path, '\\')) {
			char *deq_path = dequote_str(new_path, 0);
			if (deq_path) {
				strcpy(new_path, deq_path);
				free(deq_path);
			}
		}

		char *p = check_cdpath(new_path);
		char *q = realpath(p ? p : new_path, NULL);
		if (!q) {
			if (print_error) {
				fprintf(stderr, "%s: cd: %s: %s\n", PROGRAM_NAME,
					p ? p : new_path, strerror(errno));
			}
			free(p);
			return EXIT_FAILURE;
		}
		free(p);

		if (xchdir(q, SET_TITLE) != EXIT_SUCCESS) {
			if (print_error) {
				fprintf(stderr, "%s: cd: %s: %s\n", PROGRAM_NAME,
					q, strerror(errno));
			}
			free(q);
			return EXIT_FAILURE;
		}

		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(q, strlen(q));
		free(q);
	}

	int exit_status = EXIT_SUCCESS;
	add_to_dirhist(workspaces[cur_ws].path);

	dir_changed = 1;
	if (autols) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	add_to_jumpdb(workspaces[cur_ws].path);
	return exit_status;
}

/* Convert ... n into ../.. n */
char *
fastback(const char *str)
{
	if (!str || !*str)
		return (char *)NULL;

	char *p = (char *)str;
	size_t dots = 0;

	char *rem = (char *)NULL;
	while (*p) {
		if (*p != '.') {
			rem = p;
			break;
		}
		dots++;
		p++;
	}

	if (dots <= 2)
		return (char *)NULL;

	char *q = (char *)NULL;
	if (rem)
		q = (char *)xnmalloc((dots * 3 + strlen(rem) + 2), sizeof(char));
	else
		q = (char *)xnmalloc((dots * 3), sizeof(char));

	q[0] = '.';
	q[1] = '.';

	size_t i, c = 2;
	for (i = 2; c < dots;) {
		q[i] = '/'; i++;
		q[i] = '.'; i++;
		q[i] = '.'; i++;
		c++;
	}

	q[i] = '\0';

	if (rem) {
		if (*rem != '/') {
			q[i] = '/';
			q[i + 1] = '\0';
		}
		strcat(q, rem);
	}

	return q;
}

void
print_dirhist(void)
{
	int i;
	for (i = 0; i < dirhist_total_index; i++) {
		if (!old_pwd[i] || *old_pwd[i] == _ESC)
			continue;
		if (i == dirhist_cur_index)
			printf("  %d  %s%s%s\n", i + 1, dh_c, old_pwd[i], df_c);
		else
			printf("  %d  %s\n", i + 1, old_pwd[i]);
	}
}

static int
surf_hist(char **comm)
{
	if (*comm[1] == 'h' && (strcmp(comm[1], "h") == 0
	|| strcmp(comm[1], "hist") == 0)) {
		print_dirhist();
		return EXIT_SUCCESS;
	}

	if (*comm[1] == 'c' && strcmp(comm[1], "clear") == 0) {
		int i = dirhist_total_index;
		while (--i >= 0)
			free(old_pwd[i]);
		dirhist_cur_index = dirhist_total_index = 0;
		add_to_dirhist(workspaces[cur_ws].path);
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_FAILURE;

	if (*comm[1] == '!' && is_number(comm[1] + 1) == 1) {
		/* Go the the specified directory (first arg) in the directory
		 * history list */
		int atoi_comm = atoi(comm[1] + 1);
		if (atoi_comm > 0 && atoi_comm <= dirhist_total_index) {
			if (!old_pwd[atoi_comm - 1] || *old_pwd[atoi_comm - 1] == _ESC) {
				fprintf(stderr, _("%s: Invalid history entry\n"), PROGRAM_NAME);
				exit_status = EXIT_FAILURE;
				return exit_status;
			}
			int ret = xchdir(old_pwd[atoi_comm - 1], SET_TITLE);
			if (ret == 0) {
				free(workspaces[cur_ws].path);
				workspaces[cur_ws].path = (char *)xnmalloc(strlen(
						old_pwd[atoi_comm - 1]) + 1, sizeof(char));
				strcpy(workspaces[cur_ws].path, old_pwd[atoi_comm - 1]);

				dirhist_cur_index = atoi_comm - 1;

				exit_status = EXIT_SUCCESS;

				if (autols) {
					free_dirlist();
					exit_status = list_dir();
				}
			} else {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				    old_pwd[atoi_comm - 1], strerror(errno));
			}
		} else {
			fprintf(stderr, _("history: %s: No such ELN\n"), comm[1] + 1);
		}
	} else {
		fprintf(stderr, "%s\n", _(DIRHIST_USAGE));
	}

	return exit_status;
}

/* Set the path of the current workspace to NEW_PATH */
static int
set_path(const char *new_path)
{
	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(new_path, strlen(new_path));
	if (!workspaces[cur_ws].path)
		return EXIT_FAILURE;

	add_to_jumpdb(workspaces[cur_ws].path);
	int exit_status = EXIT_SUCCESS;

	dir_changed = 1;
	if (autols) {
		free_dirlist();
		exit_status = list_dir();
	}

	return exit_status;
}

/* Go back one entry in dirhist */
int
back_function(char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1]) {
		if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
			puts(_(BACK_USAGE));
			return EXIT_SUCCESS;
		}
		return surf_hist(comm);
	}

	/* If just 'back', with no arguments */

	/* If first path in current dirhist was reached, do nothing */
	if (dirhist_cur_index <= 0)
		return EXIT_SUCCESS;

	dirhist_cur_index--;

	if (!old_pwd[dirhist_cur_index] || *old_pwd[dirhist_cur_index] == _ESC) {
		if (dirhist_cur_index)
			dirhist_cur_index--;
		else
			return EXIT_FAILURE;
	}

	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == EXIT_SUCCESS)
		return set_path(old_pwd[dirhist_cur_index]);

	fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
	    old_pwd[dirhist_cur_index], strerror(errno));
	/* Invalidate this entry */
	*old_pwd[dirhist_cur_index] = _ESC;
	if (dirhist_cur_index)
		dirhist_cur_index--;

	return EXIT_FAILURE;
}

/* Go forth one entry in dirhist */
int
forth_function(char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1]) {
		if (*comm[1] == '-' && strcmp(comm[1], "--help") == 0) {
			puts(_(FORTH_USAGE));
			return EXIT_SUCCESS;
		}
		return surf_hist(comm);
	}

	/* If just 'forth', with no arguments */
	/* If last path in dirhist was reached, do nothing */
	if (dirhist_cur_index + 1 >= dirhist_total_index)
		return EXIT_SUCCESS;

	dirhist_cur_index++;

	if (!old_pwd[dirhist_cur_index] || *old_pwd[dirhist_cur_index] == _ESC) {
		if (dirhist_cur_index < dirhist_total_index
		&& old_pwd[dirhist_cur_index + 1])
			dirhist_cur_index++;
		else
			return EXIT_FAILURE;
	}

	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == EXIT_SUCCESS)
		return set_path(old_pwd[dirhist_cur_index]);

	fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
	    old_pwd[dirhist_cur_index], strerror(errno));
	/* Invalidate this entry */
	*old_pwd[dirhist_cur_index] = _ESC;
	if (dirhist_cur_index < dirhist_total_index
	&& old_pwd[dirhist_cur_index + 1])
		dirhist_cur_index++;

	return EXIT_FAILURE;
}
