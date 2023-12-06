/* navigation.c -- functions to control the navigation system */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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
#include <readline/tilde.h>

#include "aux.h"
#include "checks.h"
#include "colors.h" /* Used to get workspace path color */
#include "fuzzy_match.h"
#include "history.h"
#include "jump.h"
#include "listing.h"
#include "misc.h"
#include "navigation.h"
#include "messages.h"
#include "readline.h"

#define BD_CONTINUE 2

/* Builtin version of pwd(1). Print the current working directory.
 * Try first our own internal representation (workspaces array). If something
 * goes wrong, fallback to $PWD/getcwd(3) (via get_cwd()). */
int
pwd_function(const char *arg)
{
	int resolve_links = 0;
	char *pwd = (char *)NULL;

	if (arg && *arg == '-') {
		if (arg[1] == 'P')  {
			resolve_links = 1;
		} else if (IS_HELP(arg)) {
			puts(PWD_DESC);
			return EXIT_SUCCESS;
		} else if (arg[1] != 'L') {
			xerror("pwd: %s: Invalid option\nUsage: pwd [-LP]\n", arg);
			return EXIT_FAILURE;
		}
	}

	if (workspaces && workspaces[cur_ws].path) {
		pwd = workspaces[cur_ws].path;
	} else {
		char p[PATH_MAX]; *p = '\0';
		pwd = get_cwd(p, sizeof(p), 0);
	}

	if (!pwd || !*pwd) {
		xerror(_("%s: Error getting the current working directory\n"),
			PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	if (resolve_links == 0) {
		puts(pwd);
		return EXIT_SUCCESS;
	}

	char p[PATH_MAX]; *p = '\0';
	char *real_path = realpath(pwd, p);
	if (!real_path) {
		xerror("pwd: %s: %s\n", pwd, strerror(errno));
		return errno;
	}

	puts(p);
	return EXIT_SUCCESS;
}

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

/*
static char *
get_workspace_color(const uint8_t num)
{
	if (conf.colorize == 0)
		return df_c;

	switch (num + 1) {
	case 1: return *ws1_c ? ws1_c : DEF_WS1_C;
	case 2: return *ws2_c ? ws2_c : DEF_WS2_C;
	case 3: return *ws3_c ? ws3_c : DEF_WS3_C;
	case 4: return *ws4_c ? ws4_c : DEF_WS4_C;
	case 5: return *ws5_c ? ws5_c : DEF_WS5_C;
	case 6: return *ws6_c ? ws6_c : DEF_WS6_C;
	case 7: return *ws7_c ? ws7_c : DEF_WS7_C;
	case 8: return *ws8_c ? ws8_c : DEF_WS8_C;
	default: return df_c;
	}
} */

static char *
get_workspace_path_color(const uint8_t num)
{
	if (conf.colorize == 0)
		return df_c;

	if (!workspaces[num].path)
		return DEF_DL_C; /* Unset. DL (dividing line) defaults to gray: let's use this */

	struct stat a;
	if (lstat(workspaces[num].path, &a) == -1)
		return uf_c;

	if (check_file_access(a.st_mode, a.st_uid, a.st_gid) == 0)
		return nd_c;

	if (S_ISLNK(a.st_mode)) {
		char p[PATH_MAX];
		*p = '\0';
		char *ret = realpath(workspaces[num].path, p);
		return (ret && *p) ? ln_c : or_c;
	}

	return get_dir_color(workspaces[num].path, a.st_mode, a.st_nlink, -1);
}

static int
list_workspaces(void)
{
	uint8_t i;
	int pad = (int)get_longest_workspace_name();

	for (i = 0; i < MAX_WS; i++) {
		char *path_color = get_workspace_path_color(i);

		if (i == cur_ws)
			printf("%s>%s ", mi_c, df_c);
		else
			fputs("  ", stdout);

//		char *p = get_workspace_color(i);
//		char *ws_color = p ? p : df_c;
		char *ws_color = df_c;

		if (workspaces[i].name) {
			printf("%s%d%s [%s%s%s]: %*s",
				ws_color, i + 1, df_c,
				ws_color, workspaces[i].name, df_c,
				pad - (int)wc_xstrlen(workspaces[i].name), "");
		} else {
			printf("%s%d%s: %*s",
				ws_color, i + 1, df_c,
				pad > 0 ? pad + 3 : 0, "");
		}

		printf("%s%s%s\n",
			path_color,
			workspaces[i].path ? workspaces[i].path : "unset",
			df_c);
	}

	return EXIT_SUCCESS;
}

static int
check_workspace_num(char *str, int *tmp_ws)
{
	int istr = atoi(str);
	if (istr <= 0 || istr > MAX_WS) {
		xerror(_("ws: %d: No such workspace (valid workspaces: "
			"1-%d)\n"), istr, MAX_WS);
		return EXIT_FAILURE;
	}

	*tmp_ws = istr - 1;

	if (*tmp_ws == cur_ws) {
		xerror(_("ws: %d: Is the current workspace\n"), *tmp_ws + 1);
		return EXIT_SUCCESS;
	}

	return 2;
}

static void
save_workspace_opts(const int n)
{
	free(workspace_opts[n].filter.str);
	workspace_opts[n].filter.str = filter.str
		? savestring(filter.str, strlen(filter.str)) : (char *)NULL;
	workspace_opts[n].filter.rev = filter.rev;
	workspace_opts[n].filter.type = filter.type;
	workspace_opts[n].filter.env = filter.env;

	workspace_opts[n].color_scheme = cur_cscheme;
	workspace_opts[n].files_counter = conf.files_counter;
	workspace_opts[n].light_mode = conf.light_mode;
	workspace_opts[n].list_dirs_first = conf.list_dirs_first;
	workspace_opts[n].long_view = conf.long_view;
	workspace_opts[n].max_files = max_files;
	workspace_opts[n].max_name_len = conf.max_name_len;
	workspace_opts[n].only_dirs = conf.only_dirs;
	workspace_opts[n].pager = conf.pager;
	workspace_opts[n].show_hidden = conf.show_hidden;
	workspace_opts[n].sort = conf.sort;
	workspace_opts[n].sort_reverse = conf.sort_reverse;
}

static void
unset_ws_filter(void)
{
	free(filter.str);
	filter.str = (char *)NULL;
	filter.rev = 0;
	filter.type = FILTER_NONE;
	regfree(&regex_exp);
}

static void
set_ws_filter(const int n)
{
	filter.type = workspace_opts[n].filter.type;
	filter.rev = workspace_opts[n].filter.rev;
	filter.env = workspace_opts[n].filter.env;

	free(filter.str);
	regfree(&regex_exp);
	char *p = workspace_opts[n].filter.str;
	filter.str = savestring(p, strlen(p));

	if (filter.type != FILTER_FILE_NAME)
		return;

	if (regcomp(&regex_exp, filter.str, REG_NOSUB | REG_EXTENDED) != EXIT_SUCCESS)
		unset_ws_filter();
}

static void
set_workspace_opts(const int n)
{
	if (workspace_opts[n].color_scheme
	&& workspace_opts[n].color_scheme != cur_cscheme)
		set_colors(workspace_opts[n].color_scheme, 0);

	if (workspace_opts[n].filter.str && *workspace_opts[n].filter.str) {
		set_ws_filter(n);
	} else {
		if (filter.str)
			unset_ws_filter();
	}

	conf.light_mode = workspace_opts[n].light_mode;
	conf.list_dirs_first = workspace_opts[n].list_dirs_first;
	conf.long_view = workspace_opts[n].long_view;
	conf.files_counter = workspace_opts[n].files_counter;
	max_files = workspace_opts[n].max_files;
	conf.max_name_len = workspace_opts[n].max_name_len;
	conf.only_dirs = workspace_opts[n].only_dirs;
	conf.pager = workspace_opts[n].pager;
	conf.show_hidden = workspace_opts[n].show_hidden;
	conf.sort = workspace_opts[n].sort;
	conf.sort_reverse = workspace_opts[n].sort_reverse;
}

static int
switch_workspace(const int tmp_ws)
{
	/* If new workspace has no path yet, copy the path of the current workspace */
	if (!workspaces[tmp_ws].path) {
		workspaces[tmp_ws].path = savestring(workspaces[cur_ws].path,
		    strlen(workspaces[cur_ws].path));
	} else {
		if (access(workspaces[tmp_ws].path, R_OK | X_OK) != EXIT_SUCCESS) {
			xerror("ws: %s: %s\n", workspaces[tmp_ws].path, strerror(errno));
			free(workspaces[tmp_ws].path);
			workspaces[tmp_ws].path = savestring(workspaces[cur_ws].path,
				strlen(workspaces[cur_ws].path));
		}
	}

	if (xchdir(workspaces[tmp_ws].path, SET_TITLE) == -1) {
		xerror("ws: %s: %s\n", workspaces[tmp_ws].path, strerror(errno));
		return EXIT_FAILURE;
	}

	if (conf.private_ws_settings == 1)
		save_workspace_opts(cur_ws);

	prev_ws = cur_ws;
	cur_ws = tmp_ws;
	dir_changed = 1;

	if (conf.colorize == 1 && xargs.eln_use_workspace_color == 1)
		set_eln_color();

	if (conf.private_ws_settings == 1)
		set_workspace_opts(cur_ws);

	if (conf.autols == 1)
		reload_dirlist();

	add_to_dirhist(workspaces[cur_ws].path);
	return EXIT_SUCCESS;
}

/* Return the workspace number corresponding to the workspace name NAME,
 * or -1 if no workspace is named NAME, if error, or if NAME is already
 * the current workspace */
static int
get_workspace_by_name(char *name, const int check_current)
{
	if (!workspaces || !name || !*name)
		return (-1);

	/* CHECK_CURRENT is zero when coming from unset_workspace(), in which
	 * case name is already dequoted */
	char *p = check_current == 1 ? dequote_str(name, 0) : (char *)NULL;
	char *q = p ? p : name;

	int n = MAX_WS;
	while (--n >= 0) {
		if (!workspaces[n].name || *workspaces[n].name != *q
		|| strcmp(workspaces[n].name, q) != 0)
			continue;
		if (n == cur_ws && check_current == 1) {
			xerror(_("ws: %s: Is the current workspace\n"), q);
			free(p);
			return (-1);
		}
		free(p);
		return n;
	}

	xerror(_("ws: %s: No such workspace\n"), q);
	free(p);
	return (-1);
}

static int
unset_workspace(char *str)
{
	int n = -1;

	char *name = dequote_str(str, 0);
	if (!name) {
		xerror("ws: %s: Error dequoting name\n", str);
		return EXIT_FAILURE;
	}

	if (!is_number(name)) {
		if ((n = get_workspace_by_name(name, 0)) == -1)
			goto ERROR;
		n++;
	} else {
		n = atoi(name);
	}

	if (n < 1 || n > MAX_WS) {
		xerror(_("ws: %s: No such workspace (valid workspaces: "
			"1-%d)\n"), name, MAX_WS);
		goto ERROR;
	}

	n--;
	if (n == cur_ws) {
		xerror(_("ws: %s: Is the current workspace\n"), name);
		goto ERROR;
	}

	if (!workspaces[n].path) {
		xerror(_("ws: %s: Already unset\n"), name);
		goto ERROR;
	}

	printf(_("ws: %s: Workspace unset\n"), name);
	free(name);

	free(workspaces[n].path);
	workspaces[n].path = (char *)NULL;

	return EXIT_SUCCESS;

ERROR:
	free(name);
	return EXIT_FAILURE;
}

int
handle_workspaces(char **args)
{
	if (!args[0] || !*args[0])
		return list_workspaces();

	if (IS_HELP(args[0])) {
		puts(_(WS_USAGE));
		return EXIT_SUCCESS;
	}

	if (args[1] && strcmp(args[1], "unset") == 0)
		return unset_workspace(args[0]);

	int tmp_ws = 0;

	if (is_number(args[0])) {
		int ret = check_workspace_num(args[0], &tmp_ws);
		if (ret != 2)
			return ret;
	} else if (*args[0] == '+' && !args[0][1]) {
		if ((cur_ws + 1) >= MAX_WS)
			return EXIT_FAILURE;
		tmp_ws = cur_ws + 1;
	} else if (*args[0] == '-' && !args[0][1]) {
			if ((cur_ws - 1) < 0)
				return EXIT_FAILURE;
			tmp_ws = cur_ws - 1;
	} else {
		tmp_ws = get_workspace_by_name(args[0], 1);
		if (tmp_ws == -1)
			return EXIT_FAILURE;
	}

	return switch_workspace(tmp_ws);
}

/* Return the list of paths in CWD matching STR */
char **
get_bd_matches(const char *str, int *n, const int mode)
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
			p = conf.case_sens_path_comp
				? strstr(cwd, str) : xstrcasestr(cwd, (char *)str);
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
		matches = (char **)xnrealloc(matches, (size_t)(*n + 2), sizeof(char *));
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
grab_bd_input(const int n)
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
		xerror(_("bd: %s: Error dequoting string\n"), str);
		return EXIT_FAILURE;
	}

	if (*dir == '~') {
		char *exp_path = tilde_expand(dir);
		if (!exp_path) {
			xerror(_("bd: %s: Error expanding tilde\n"), dir);
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
help_or_root(const char *str)
{
	if (str && IS_HELP(str)) {
		puts(_(BD_USAGE));
		return EXIT_SUCCESS;
	}

	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1]) {
		puts(_("bd: /: No parent directory"));
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
		xerror(_("bd: %s: No matches found\n"), str);
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

/* Change the current directory.
 *
 * Make sure DIR exists, it is actually a directory and is readable.
 * Only then change directory.
 *
 * CD_FLAG is either SET_TITLE or NO_TITLE. In the latter case we have just a
 * temporary directory change that should not be registered nor informed to
 * the user. For example, when checking trahsed files we change to the Trash
 * dir, check files, and immediately return to the directory we came from).
 *
 * PWD and OLDPWD are updated only if CD_FLAG is SET_TITLE, that is, when the
 * current directory is explicitly changed by the user. The terminal window
 * title is changed accordingly as well, provided cwd_in_title is enabled. */
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

	if (ret == 0 && cd_flag == SET_TITLE) {
		char tmp[PATH_MAX]; *tmp = '\0';
		char *p = get_cwd(tmp, sizeof(tmp), 0);

		/* Do not set OLDPWD if changing to the same directory ("cd ."
		 * and similar commands). */
		if (p && *p && strcmp(p, dir) != 0)
			setenv("OLDPWD", p, 1);

		setenv("PWD", dir, 1);

		if (xargs.cwd_in_title == 1)
			set_term_title(dir);
	}

	return ret;
}

static char *
check_cdpath(const char *name)
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
		if (len > 0 && cdpaths[i][len - 1] == '/')
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
			xerror("%s\n", _("cd: Home directory not found"));
		return ENOENT;
	}

	if (xchdir(user.home, SET_TITLE) != EXIT_SUCCESS) {
		int tmp_err = errno;
		if (cd_flag == CD_PRINT_ERROR)
			xerror("cd: %s: %s\n", user.home, strerror(tmp_err));
		return tmp_err;
	}

	if (workspaces) {
		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(user.home, strlen(user.home));
	}

	return EXIT_SUCCESS;
}

/* Change current directory to NEW_PATH */
static int
change_to_path(char *new_path, const int cd_flag)
{
	if (!new_path || !*new_path) {
		xerror("%s\n", _("cd: Path is NULL or empty"));
		return ENOENT;
	}

	if (strchr(new_path, '\\')) {
		char *deq_path = dequote_str(new_path, 0);
		if (deq_path) {
			xstrsncpy(new_path, deq_path, strlen(deq_path) + 1);
			free(deq_path);
		}
	}

	char *p = check_cdpath(new_path);
	errno = 0;
	char *r = p ? p : new_path;
	char *q = normalize_path(r, strlen(r));

	if (!q) {
		if (cd_flag == CD_PRINT_ERROR)
			xerror(_("cd: %s: Error normalizing path\n"), new_path);
		free(p);
		return EXIT_FAILURE;
	}
	free(p);

	if (xchdir(q, SET_TITLE) != EXIT_SUCCESS) {
		int tmp_err = errno;
		if (cd_flag == CD_PRINT_ERROR)
			xerror("cd: %s: %s\n", new_path, strerror(tmp_err));

		free(q);

		/* Most shells return 1 in case of EACCESS/ENOENT error. However, 1, as
		 * a general error code, is not quite informative. Why not to return the
		 * actual error code returned by chdir(3)? Note that POSIX only requires
		 * for cd to return >0 in case of error (see cd(1p)). */
		if (tmp_err == EACCES || tmp_err == ENOENT)
			return EXIT_FAILURE;

		return tmp_err;
	}

	if (workspaces) {
		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(q, strlen(q));
	}

	free(q);

	return EXIT_SUCCESS;
}

static int
skip_directory(const char *dir)
{
	return (conf.dirhistignore_regex && *conf.dirhistignore_regex
		&& regexec(&regex_dirhist, dir, 0, NULL, 0) == EXIT_SUCCESS);
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
	} else if (*new_path == '-' && !new_path[1]) {
		/* Implementation of the shell 'cd -' command */
		static int state = 0;
		char *c[] = { state == 0 ? "b" : "f", NULL };
		if (state == 0) {
			state = 1;
			return back_function(c);
		} else {
			state = 0;
			return forth_function(c);
		}
	} else {
		if ((ret = change_to_path(new_path, cd_flag)) != EXIT_SUCCESS)
			return ret;
	}

	const int skip = skip_directory(workspaces[cur_ws].path);

	if (skip == 0)
		add_to_dirhist(workspaces[cur_ws].path);

	dir_changed = 1;
	if (conf.autols == 1) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			ret = EXIT_FAILURE;
	}

	if (skip == 0)
		add_to_jumpdb(workspaces[cur_ws].path);

	return ret;
}

/* Return a pointer to the first occurrence in the string STR of a byte that
 * is not C. Otherwise, if only C is found, NULL is returned. */
static char *
xstrcpbrk(char *str, const char c)
{
	if (!str || !*str)
		return (char *)NULL;

	while (*str) {
		if (*str != c)
			return str;
		str++;
	}

	return (char *)NULL;
}

/* Convert "... n" into "../.. n"
 * and "../.. n" into the corresponding path. */
char *
fastback(char *str)
{
	if (!str || !*str || xstrcpbrk(str, '.'))
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

	if (dots <= 2) {
		if (dots == 2)
			return normalize_path("..", 2);
//		if (dots == 1)
//			return normalize_path(".", 1);
		return (char *)NULL;
	}

	char q[PATH_MAX];
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
			i++;
		}

		xstrncat(q, strlen(q), rem, sizeof(q));
	}

	return normalize_path(q, i);
}

void
print_dirhist(char *query)
{
	int n = DIGINUM(dirhist_total_index), i;
	size_t len = (query && conf.fuzzy_match == 1) ? strlen(query) : 0;
	int fuzzy_str_type = (len > 0 && contains_utf8(query) == 1)
		? FUZZY_FILES_UTF8 : FUZZY_FILES_ASCII;

	for (i = 0; i < dirhist_total_index; i++) {
		if (!old_pwd[i] || *old_pwd[i] == KEY_ESC)
			continue;

		if (query && (conf.fuzzy_match == 1
		? fuzzy_match(query, old_pwd[i], len, fuzzy_str_type) == 0
		: !strstr(old_pwd[i], query) ) )
			continue;

		if (i == dirhist_cur_index)
			printf(" %s%-*d%s %s%s%s\n", el_c, n, i + 1, df_c, mi_c,
				old_pwd[i], df_c);
		else
			printf(" %s%-*d%s %s%s%s\n", el_c, n, i + 1, df_c, di_c,
				old_pwd[i], df_c);
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

/* Change to the specified directory number (N) in the directory
 * history list */
static int
change_to_dirhist_num(int n)
{
	if (n <= 0 || n > dirhist_total_index) {
		xerror(_("history: %d: No such ELN\n"), n);
		return EXIT_FAILURE;
	}

	n--;
	if (!old_pwd[n] || *old_pwd[n] == KEY_ESC) {
		xerror("%s\n", _("history: Invalid history entry"));
		return EXIT_FAILURE;
	}

	int ret = xchdir(old_pwd[n], SET_TITLE);
	if (ret != 0) {
		xerror("history: %s: %s\n", old_pwd[n], strerror(errno));
		return EXIT_FAILURE;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(old_pwd[n], strlen(old_pwd[n]));

	dirhist_cur_index = n;
	ret = EXIT_SUCCESS;

	if (conf.autols == 1) {
		free_dirlist();
		ret = list_dir();
	}

	return ret;
}

static int
surf_hist(char **args)
{
	if (*args[1] == 'h' && (!args[1][1] || strcmp(args[1], "hist") == 0)) {
		print_dirhist(NULL);
		return EXIT_SUCCESS;
	}

	if (*args[1] == 'c' && strcmp(args[1], "clear") == 0)
		return clear_dirhist();

	if (*args[1] != '!' || is_number(args[1] + 1) != 1) {
		fprintf(stderr, "%s\n", _(DIRHIST_USAGE));
		return EXIT_FAILURE;
	}

	int n = atoi(args[1] + 1);
	return change_to_dirhist_num(n);
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
	if (conf.autols == 1) {
		free_dirlist();
		exit_status = list_dir();
	}

	return exit_status;
}

/* Go back one entry in dirhist */
int
back_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (args[1]) {
		if (!IS_HELP(args[1]))
			return surf_hist(args);
		puts(_(BACK_USAGE));
		return EXIT_SUCCESS;
	}

	/* Find the previous non-consecutive equal and valid entry */
	int i = dirhist_cur_index;
	while (--i >= 0) {
		if (old_pwd[i] && *old_pwd[i] != KEY_ESC && (!workspaces[cur_ws].path
		|| strcmp(workspaces[cur_ws].path, old_pwd[i]) != 0))
			break;
	}

	if (i < 0)
		return EXIT_SUCCESS;

	dirhist_cur_index = i;

	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == EXIT_SUCCESS)
		return set_path(old_pwd[dirhist_cur_index]);

	xerror("cd: %s: %s\n", old_pwd[dirhist_cur_index], strerror(errno));

	/* Invalidate this entry */
	*old_pwd[dirhist_cur_index] = KEY_ESC;
	if (dirhist_cur_index > 0)
		dirhist_cur_index--;

	return EXIT_FAILURE;
}

/* Go forth one entry in dirhist */
int
forth_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (args[1]) {
		if (!IS_HELP(args[1]))
			return surf_hist(args);
		puts(_(FORTH_USAGE));
		return EXIT_SUCCESS;
	}

	/* Find the next valid entry */
	int i = dirhist_cur_index;
	while (++i <= dirhist_total_index) {
		if (old_pwd[i] && (*old_pwd[i] != KEY_ESC && (!workspaces[cur_ws].path
		|| strcmp(workspaces[cur_ws].path, old_pwd[i]) != 0)))
			break;
	}

	if (i > dirhist_total_index)
		return EXIT_SUCCESS;

	dirhist_cur_index = i;


	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == EXIT_SUCCESS)
		return set_path(old_pwd[dirhist_cur_index]);

	xerror("cd: %s: %s\n", old_pwd[dirhist_cur_index], strerror(errno));

	/* Invalidate this entry */
	*old_pwd[dirhist_cur_index] = KEY_ESC;
	if (dirhist_cur_index < dirhist_total_index
	&& old_pwd[dirhist_cur_index + 1])
		dirhist_cur_index++;

	return EXIT_FAILURE;
}
