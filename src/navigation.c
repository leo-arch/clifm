/* navigation.c -- functions to control the navigation system */

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
#if defined(__linux__) && defined(_BE_POSIX)
# include "strings.h"
#endif /* __linux__ && _BE_POSIX */

#define BD_CONTINUE 2

static size_t
get_longest_workspace_name(void)
{
	if (!workspaces)
		return 0;

	size_t longest_ws = 0;
	int i = MAX_WS;

	while (--i >= 0) {
		if (!workspaces[i].name)
			continue;
		size_t l = wc_xstrlen(workspaces[i].name);
		if (l > longest_ws)
			longest_ws = l;
	}

	return longest_ws;
}

static int
list_workspaces(void)
{
	int i;
	int pad = (int)get_longest_workspace_name();

	for (i = 0; i < MAX_WS; i++) {
		if (i == cur_ws)
			fputs(mi_c, stdout);
		if (workspaces[i].name) {
			printf("%d [%s]: %*s%s", i + 1, workspaces[i].name,
				pad - (int)wc_xstrlen(workspaces[i].name), "", workspaces[i].path
				? workspaces[i].path : "none");
		} else {
			printf("%d: %*s%s", i + 1, pad > 0 ? pad + 3 : 0, "", workspaces[i].path
				? workspaces[i].path : "none");
		}
		if (i == cur_ws)
			fputs(df_c, stdout);
		putchar('\n');
	}

	return EXIT_SUCCESS;
}

static int
check_workspace_num(char *str, int *tmp_ws)
{
	int istr = atoi(str);
	if (istr <= 0 || istr > MAX_WS) {
		fprintf(stderr, _("%s: ws: %d: No such workspace (valid workspace numbers: 1-%d)\n"),
			PROGRAM_NAME, istr, MAX_WS);
		return EXIT_FAILURE;
	}

	*tmp_ws = istr - 1;

	if (*tmp_ws == cur_ws) {
		fprintf(stderr, _("ws: %d is already the current workspace\n"), *tmp_ws + 1);
		return EXIT_SUCCESS;
	}

	return 2;
}

static int
switch_workspace(int tmp_ws)
{
	/* If new workspace has no path yet, copy the path of the current workspace */
	if (!workspaces[tmp_ws].path) {
		workspaces[tmp_ws].path = savestring(workspaces[cur_ws].path,
		    strlen(workspaces[cur_ws].path));
	} else {
		if (access(workspaces[tmp_ws].path, R_OK | X_OK) != EXIT_SUCCESS) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "ws: %s: %s\n",
				workspaces[tmp_ws].path, strerror(errno));
			free(workspaces[tmp_ws].path);
			workspaces[tmp_ws].path = savestring(workspaces[cur_ws].path,
				strlen(workspaces[cur_ws].path));
		}
	}

	if (xchdir(workspaces[tmp_ws].path, SET_TITLE) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "ws: %s: %s\n", workspaces[tmp_ws].path, strerror(errno));
		return EXIT_FAILURE;
	}

	prev_ws = cur_ws;
	cur_ws = tmp_ws;
	dir_changed = 1;

	if (colorize == 1 && xargs.eln_use_workspace_color == 1)
		set_eln_color();

	if (autols == 1)
		reload_dirlist();

	add_to_dirhist(workspaces[cur_ws].path);
	return EXIT_SUCCESS;
}

/* Return the workspace number corresponding to the workspace name NAME,
 * or -1 if no workspace is named NAME, if error, or if NAME is already
 * the current workspace */
static int
get_workspace_by_name(char *name)
{
	if (!workspaces || !name || !*name)
		return (-1);

	int n = MAX_WS;
	while (--n >= 0) {
		if (!workspaces[n].name || *workspaces[n].name != *name
		|| strcmp(workspaces[n].name, name) != 0)
			continue;
		if (n == cur_ws) {
			fprintf(stderr, _("ws: %s is already the current workspace\n"), name);
			return (-1);
		}
		return n;
	}

	fprintf(stderr, _("ws: %s: No such workspace\n"), name);
	return (-1);
}

int
handle_workspaces(char *str)
{
	if (!str || !*str)
		return list_workspaces();

	if (IS_HELP(str)) {
		puts(_(WS_USAGE));
		return EXIT_SUCCESS;
	}

	int tmp_ws = 0;

	if (is_number(str)) {
		int ret = check_workspace_num(str, &tmp_ws);
		if (ret != 2)
			return ret;
	} else if (*str == '+' && !str[1]) {
		if ((cur_ws + 1) >= MAX_WS)
			return EXIT_FAILURE;
		tmp_ws = cur_ws + 1;
	} else if (*str == '-' && !str[1]) {
			if ((cur_ws - 1) < 0)
				return EXIT_FAILURE;
			tmp_ws = cur_ws - 1;
	} else {
		tmp_ws = get_workspace_by_name(str);
		if (tmp_ws == -1)
			return EXIT_FAILURE;
	}

	return switch_workspace(tmp_ws);
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
			p = case_sens_path_comp ? strstr(cwd, str) : strcasestr(cwd, (char *)str);
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
				matches[*n] = savestring(ss, strlen(ss));
			else /* Last slash is the first and only char: We have root dir */
				matches[*n] = savestring("/", 1);
			(*n)++;
		} else {
			if (!*workspaces[cur_ws].path) {
				matches[*n] = savestring("/", 1);
			} else {
				matches[*n] = savestring(workspaces[cur_ws].path,
						strlen(workspaces[cur_ws].path));
			}
			(*n)++;
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
	} else {
		if (*n > 0)
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

	return (-1); /* Never reached */
}

static int
backdir_directory(char *dir, const char *str)
{
	if (!dir) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("bd: %s: Error dequoting string\n"), str);
		return EXIT_FAILURE;
	}

	if (*dir == '~') {
		char *exp_path = tilde_expand(dir);
		if (!exp_path) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("bd: %s: Error expanding tilde\n"), dir);
			return EXIT_FAILURE;
		}
		dir = exp_path;
	}

	/* If STR is a directory, just change to it */
	struct stat a;
	if (stat(dir, &a) == 0 && S_ISDIR(a.st_mode))
		return cd_function(dir, CD_PRINT_ERROR);

	return BD_CONTINUE;
}

/* If multiple matches, print a menu to choose from */
static int
backdir_menu(char **matches)
{
	int i;
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
		return cd_function(matches[choice], CD_PRINT_ERROR);

	return EXIT_SUCCESS;
}

static int
help_or_root(char *str)
{
	if (str && IS_HELP(str)) {
		puts(_(BD_USAGE));
		return EXIT_SUCCESS;
	}

	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1]) {
		printf(_("bd: /: No parent directory\n"));
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

/* Change to parent directory matching STR */
int
backdir(char* str)
{
	if (help_or_root(str) == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	char *deq_str = str ? dequote_str(str, 0) : (char *)NULL;
	if (str) {
		int ret = backdir_directory(deq_str, str);
		if (ret != BD_CONTINUE) {
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
		fprintf(stderr, _("bd: %s: No matches found\n"), str);
		return EXIT_FAILURE;
	}

	int exit_status = EXIT_SUCCESS, i = n;
	if (n == 1) /* Just one match: change to it */
		exit_status = cd_function(matches[0], CD_PRINT_ERROR);
	else if (n > 1) /* Multiple matches: print a menu to choose from */
		exit_status = backdir_menu(matches);

	while (--i >= 0)
		free(matches[i]);
	free(matches);
	return exit_status;
}

/* Make sure DIR exists, it is actually a directory and is readable.
 * Only then change directory */
int
xchdir(char *dir, const int cd_flag)
{
	if (!dir || !*dir) {
		errno = ENOENT;
		return (-1);
	}

	DIR *dirp = opendir(dir);
	if (!dirp)
		return (-1);

	closedir(dirp);

	int ret = chdir(dir);

	if (cd_flag == SET_TITLE && ret == 0 && xargs.cwd_in_title == 1)
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
	char tmp[PATH_MAX];
	char *p = (char *)NULL;
	struct stat a;
	for (i = 0; cdpaths[i]; i++) {
		size_t len = strlen(cdpaths[i]);
		if (cdpaths[i][len - 1] == '/')
			snprintf(tmp, sizeof(tmp), "%s%s", cdpaths[i], name);
		else
			snprintf(tmp, sizeof(tmp), "%s/%s", cdpaths[i], name);

		char *exp_path = (char *)NULL;
		if (*tmp == '~')
			exp_path = tilde_expand(tmp);

		char *dir = exp_path ? exp_path : tmp;
		if (stat(dir, &a) != -1 && S_ISDIR(a.st_mode)) {
			p = savestring(dir, strlen(dir));
			free(exp_path);
			break;
		}
		free(exp_path);
	}

	return p;
}

/* Change the current directory to the home directory */
static int
go_home(const int cd_flag)
{
	if (!user.home) {
		if (cd_flag == CD_PRINT_ERROR)
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("cd: Home directory not found\n"));
		return ENOENT;
	}

	if (xchdir(user.home, SET_TITLE) != EXIT_SUCCESS) {
		if (cd_flag == CD_PRINT_ERROR)
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "cd: %s: %s\n", user.home, strerror(errno));
		return errno;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(user.home, strlen(user.home));

	return EXIT_SUCCESS;
}

/* Change current directory to NEW_PATH */
static int
change_to_path(char *new_path, const int cd_flag)
{
	if (strchr(new_path, '\\')) {
		char *deq_path = dequote_str(new_path, 0);
		if (deq_path) {
			strcpy(new_path, deq_path);
			free(deq_path);
		}
	}

	char *p = check_cdpath(new_path);
	errno = 0;
	char *q = realpath(p ? p : new_path, NULL);
	if (!q) {
		if (cd_flag == CD_PRINT_ERROR) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "cd: %s: %s\n", p ? p : new_path, strerror(errno));
		}
		free(p);
		return errno;
	}
	free(p);

	if (xchdir(q, SET_TITLE) != EXIT_SUCCESS) {
		if (cd_flag == CD_PRINT_ERROR)
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "cd: %s: %s\n", q, strerror(errno));
		free(q);
		return errno;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(q, strlen(q));
	free(q);

	return EXIT_SUCCESS;
}

/* Change current directory to NEW_PATH, or to HOME if new_path is NULL.
 * Errors are printed only if CD_FLAG is set to CD_PRINT_ERROR (1) */
int
cd_function(char *new_path, const int cd_flag)
{
	/* If no argument, change to home */
	int ret = EXIT_SUCCESS;
	if (!new_path || !*new_path) {
		if ((ret = go_home(cd_flag)) != EXIT_SUCCESS)
			return ret;
	} else {
		if ((ret = change_to_path(new_path, cd_flag)) != EXIT_SUCCESS)
			return ret;
	}

	add_to_dirhist(workspaces[cur_ws].path);

	dir_changed = 1;
	if (autols == 1) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			ret = EXIT_FAILURE;
	}

	add_to_jumpdb(workspaces[cur_ws].path);
	return ret;
}

/* Convert ... n into ../.. n */
char *
fastback(char *str)
{
	if (!str || !*str)
		return (char *)NULL;

	char *p = str;
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
	int n = DIGINUM(dirhist_total_index), i;

	for (i = 0; i < dirhist_total_index; i++) {
		if (!old_pwd[i] || *old_pwd[i] == _ESC)
			continue;
		if (i == dirhist_cur_index)
			printf(" %s%-*d%s %s%s%s\n", el_c, n, i + 1, df_c, mi_c, old_pwd[i], df_c);
		else
			printf(" %s%-*d%s %s%s%s\n", el_c, n, i + 1, df_c, di_c, old_pwd[i], df_c);
	}
}

static int
clear_dirhist(void)
{
	int i = dirhist_total_index;

	while (--i >= 0)
		free(old_pwd[i]);
	dirhist_cur_index = dirhist_total_index = 0;
	add_to_dirhist(workspaces[cur_ws].path);

	printf("%s: Directory history cleared\n", PROGRAM_NAME);

	return EXIT_SUCCESS;
}

/* Change to the specified directory  number (N) in the directory
 * history list */
static int
change_to_num(int n)
{
	if (n <= 0 || n > dirhist_total_index) {
		fprintf(stderr, _("history: %d: No such ELN\n"), n);
		return EXIT_FAILURE;
	}

	n--;
	if (!old_pwd[n] || *old_pwd[n] == _ESC) {
		fprintf(stderr, _("history: Invalid history entry\n"));
		return EXIT_FAILURE;
	}

	int ret = xchdir(old_pwd[n], SET_TITLE);
	if (ret != 0) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "history: %s: %s\n",	old_pwd[n], strerror(errno));
		return EXIT_FAILURE;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(old_pwd[n], strlen(old_pwd[n]));

	dirhist_cur_index = n;
	ret = EXIT_SUCCESS;

	if (autols == 1) {
		free_dirlist();
		ret = list_dir();
	}

	return ret;
}

static int
surf_hist(char **comm)
{
	if (*comm[1] == 'h' && (!comm[1][1] || strcmp(comm[1], "hist") == 0)) {
		print_dirhist();
		return EXIT_SUCCESS;
	}

	if (*comm[1] == 'c' && strcmp(comm[1], "clear") == 0)
		return clear_dirhist();

	if (*comm[1] != '!' || is_number(comm[1] + 1) != 1) {
		fprintf(stderr, "%s\n", _(DIRHIST_USAGE));
		return EXIT_FAILURE;
	}

	int n = atoi(comm[1] + 1);
	return change_to_num(n);
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
	if (autols == 1) {
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
		if (!IS_HELP(comm[1]))
			return surf_hist(comm);
		puts(_(BACK_USAGE));
		return EXIT_SUCCESS;
	}

	/* If just 'back', with no arguments */
	/* If first path in current dirhist was reached, do nothing */
	if (dirhist_cur_index <= 0)
		return EXIT_SUCCESS;

	dirhist_cur_index--;

	if (!old_pwd[dirhist_cur_index] || *old_pwd[dirhist_cur_index] == _ESC) {
		if (dirhist_cur_index <= 0)
			return EXIT_FAILURE;
		dirhist_cur_index--;
	}

	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == EXIT_SUCCESS)
		return set_path(old_pwd[dirhist_cur_index]);
	_err(ERR_NO_STORE, NOPRINT_PROMPT, "cd: %s: %s\n",
	    old_pwd[dirhist_cur_index], strerror(errno));

	/* Invalidate this entry */
	*old_pwd[dirhist_cur_index] = _ESC;
	if (dirhist_cur_index > 0)
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
		if (!IS_HELP(comm[1]))
			return surf_hist(comm);
		puts(_(FORTH_USAGE));
		return EXIT_SUCCESS;
	}

	/* If just 'forth', with no arguments */
	/* If last path in dirhist was reached, do nothing */
	if (dirhist_cur_index + 1 >= dirhist_total_index)
		return EXIT_SUCCESS;

	dirhist_cur_index++;

	if (!old_pwd[dirhist_cur_index] || *old_pwd[dirhist_cur_index] == _ESC) {
/*		if (dirhist_cur_index >= dirhist_total_index
		|| !old_pwd[dirhist_cur_index + 1]) */
		if (!old_pwd[dirhist_cur_index + 1])
			return EXIT_FAILURE;
		dirhist_cur_index++;
	}

	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == EXIT_SUCCESS)
		return set_path(old_pwd[dirhist_cur_index]);
	_err(ERR_NO_STORE, NOPRINT_PROMPT, "cd: %s: %s\n", old_pwd[dirhist_cur_index], strerror(errno));

	/* Invalidate this entry */
	*old_pwd[dirhist_cur_index] = _ESC;
	if (dirhist_cur_index < dirhist_total_index
	&& old_pwd[dirhist_cur_index + 1])
		dirhist_cur_index++;

	return EXIT_FAILURE;
}
