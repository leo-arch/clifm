/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* navigation.c -- functions to control the navigation system */

#include "helpers.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <readline/tilde.h>

#include "aux.h"         /* mem.h, normalize_path, get_cwd */
#include "checks.h"      /* is_number */
#include "colors.h"      /* get_entry_color() */
#include "fuzzy_match.h" /* fuzzy_match, contains_utf8 */
#include "history.h"     /* add_to_dirhist */
#include "jump.h"        /* add_to_jumpdb */
#include "listing.h"     /* reload_dirlist */
#include "messages.h"    /* description and usage messages */
#include "misc.h"        /* xerror */
#include "navigation.h"
#include "readline.h"    /* rl_no_hist */
#include "term.h" 		 /* report_cwd(), set_term_title() */

#define BD_CONTINUE 2

/* Builtin version of pwd(1). Print the current working directory.
 * Try first our own internal representation (workspaces array). If something
 * goes wrong, fallback to $PWD/getcwd(3) (via get_cwd()). */
int
pwd_function(const char *arg)
{
	int resolve_links = 0;
	char *pwd = NULL;

	if (arg && *arg == '-') {
		if (arg[1] == 'P')  {
			resolve_links = 1;
		} else if (IS_HELP(arg)) {
			puts(PWD_DESC);
			return FUNC_SUCCESS;
		} else {
			if (arg[1] != 'L') {
				xerror(_("pwd: '%s': Invalid option\nUsage: pwd [-LP]\n"), arg);
				return FUNC_FAILURE;
			}
		}
	}

	if (workspaces && workspaces[cur_ws].path) {
		pwd = workspaces[cur_ws].path;
	} else {
		char p[PATH_MAX + 1]; *p = '\0';
		pwd = get_cwd(p, sizeof(p), 0);
	}

	if (!pwd || !*pwd) {
		xerror(_("%s: Error getting the current working directory\n"),
			PROGRAM_NAME);
		return FUNC_FAILURE;
	}

	if (resolve_links == 0) {
		puts(pwd);
		return FUNC_SUCCESS;
	}

	char p[PATH_MAX + 1]; *p = '\0';
	const char *real_path = xrealpath(pwd, p);
	if (!real_path) {
		xerror("pwd: '%s': %s\n", pwd, strerror(errno));
		return errno;
	}

	puts(p);
	return FUNC_SUCCESS;
}

/* Return the list of paths in CWD matching STR. */
char **
get_bd_matches(const char *str, int *n, const int mode)
{
	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
		return NULL;

	char *cwd = workspaces[cur_ws].path;
	char **matches = NULL;

	if (mode == BD_TAB) {
		/* matches will be passed to readline for tab completion, so that
		 * we need to reserve the first slot to hold the query string. */
		*n = 1;
		matches = xnmalloc(2, sizeof(char *));
	}

	while (1) {
		char *p = NULL;
		if (str && *str) { /* Non-empty query string. */
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

		matches = xnrealloc(matches, (size_t)*n + 2, sizeof(char *));
		if (mode == BD_TAB) {
			/* Print only the path base name. */
			const char *ss = strrchr(workspaces[cur_ws].path, '/');
			if (ss && *(++ss))
				matches[*n] = savestring(ss, strlen(ss));
			else /* Last slash is the first and only char: we have the root dir. */
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
		if (*n == 1) { /* No matches. */
			free(matches);
			return NULL;
		} else if (*n == 2) { /* One match. */
			char *p = escape_str(matches[1]);
			if (!p) {
				free(matches[1]);
				free(matches);
				return NULL;
			}
			matches[0] = p;
			free(matches[1]);
			matches[1] = NULL;
		} else { /* Multiple matches. */
			matches[0] = savestring(str, strlen(str));
			matches[*n] = NULL;
		}
	} else {
		if (*n > 0)
			matches[*n] = NULL;
	}

	return matches;
}

static int
grab_bd_input(const int n)
{
	char *input = NULL;
	putchar('\n');

	while (!input) {
		input = rl_no_hist(_("Select a directory ('q' to quit): "), 0);
		if (!input || !*input) {
			free(input);
			input = NULL;
			continue;
		}

		if (is_number(input)) {
			const int a = xatoi(input);
			if (a > 0 && a <= n) {
				free(input);
				return a - 1;
			} else {
				free(input);
				input = NULL;
				continue;
			}

		} else if (*input == 'q' && !input[1]) {
			free(input);
			return (-1);

		} else {
			free(input);
			input = NULL;
			continue;
		}
	}

	return (-1); /* Never reached. */
}

static int
backdir_directory(char *dir, const char *str)
{
	if (!dir) {
		xerror(_("bd: '%s': Error unescaping string\n"), str);
		return FUNC_FAILURE;
	}

	if (*dir == '~') {
		char *exp_path = tilde_expand(dir);
		if (!exp_path) {
			xerror(_("bd: '%s': Error expanding tilde\n"), dir);
			return FUNC_FAILURE;
		}
		dir = exp_path;
	}

	/* If STR is a directory, just change to it. */
	struct stat a;
	if (stat(dir, &a) == 0 && S_ISDIR(a.st_mode))
		return cd_function(dir, CD_PRINT_ERROR);

	return BD_CONTINUE;
}

/* If multiple matches, print a menu to select from. */
static int
backdir_menu(char **matches)
{
	int i;
	for (i = 0; matches[i]; i++) {
		char *sl = strrchr(matches[i], '/');
		int flag = 0;
		if (sl && sl[1]) {
			*sl = '\0';
			sl++;
			flag = 1;
		}
		printf("%s%d%s %s%s%s\n", el_c, i + 1, df_c, di_c, sl ? sl : "/", df_c);
		if (flag == 1) {
			sl--;
			*sl = '/';
		}
	}

	const int choice = grab_bd_input(i);
	if (choice != -1)
		return cd_function(matches[choice], CD_PRINT_ERROR);

	return FUNC_SUCCESS;
}

static int
help_or_root(const char *str)
{
	if (str && IS_HELP(str)) {
		puts(_(BD_USAGE));
		return FUNC_SUCCESS;
	}

	if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1]) {
		puts(_("bd: '/': No parent directory"));
		return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
}

/* Change to parent directory matching STR. */
int
backdir(char *str)
{
	if (help_or_root(str) == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	char *deq_str = str ? unescape_str(str, 0) : NULL;
	if (str) {
		const int ret = backdir_directory(deq_str, str);
		if (ret != BD_CONTINUE) {
			free(deq_str);
			return ret;
		}
	}

	if (!workspaces[cur_ws].path) {
		free(deq_str);
		return FUNC_FAILURE;
	}

	int n = 0;
	char **matches = get_bd_matches(deq_str ? deq_str : str, &n, BD_NO_TAB);
	free(deq_str);

	if (n == 0) {
		xerror(_("bd: %s: No matches found\n"), str);
		return FUNC_FAILURE;
	}

	int exit_status = FUNC_SUCCESS;
	if (n == 1) /* Just one match: change to it. */
		exit_status = cd_function(matches[0], CD_PRINT_ERROR);
	else /* Multiple matches: print a menu to select from. */
		exit_status = backdir_menu(matches);

	int i = n;
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

	const int ret = chdir(dir);

	if (ret == 0 && cd_flag == SET_TITLE) {
		char tmp[PATH_MAX + 1]; *tmp = '\0';
		const char *p = get_cwd(tmp, sizeof(tmp), 0);

		/* Do not set OLDPWD if changing to the same directory ("cd ."
		 * and similar commands). */
		if (p && *p && strcmp(p, dir) != 0)
			setenv("OLDPWD", p, 1);

		setenv("PWD", dir, 1);

		if (xargs.vt100 != 1) { /* --vt100 */
			if (xargs.report_cwd != 0) /* --no-report-cwd */
				report_cwd(dir); /* OSC-7 escape sequence */

			set_term_title(dir); /* OSC-2 escape sequence */
		}
	}

	return ret;
}

static char *
check_cdpath(const char *name)
{
	if (!name || !*name)
		return NULL;

	if (*name == '/' || (*name == '.' && name[1] == '/')
	|| (*name == '.' && name[1] == '.' && name[2] == '/'))
		return NULL;

	const size_t namelen = strlen(name);
	char *p = NULL;
	struct stat a;

	for (size_t i = 0; cdpaths[i]; i++) {
		const size_t len = strlen(cdpaths[i]);
		const size_t tmp_len = len + namelen + 2;
		char *tmp = xnmalloc(tmp_len, sizeof(char));

		if (len > 0 && cdpaths[i][len - 1] == '/')
			snprintf(tmp, tmp_len, "%s%s", cdpaths[i], name);
		else
			snprintf(tmp, tmp_len, "%s/%s", cdpaths[i], name);

		char *exp_path = NULL;
		if (*tmp == '~')
			exp_path = tilde_expand(tmp);

		const char *dir = exp_path ? exp_path : tmp;
		if (stat(dir, &a) != -1 && S_ISDIR(a.st_mode)) {
			p = savestring(dir, strlen(dir));
			free(tmp);
			free(exp_path);
			break;
		}

		free(tmp);
		free(exp_path);
	}

	/* Print message (post_listing(), in listing.c) to let the user
	 * know they changed to a dir in CDPATH. */
	if (p)
		is_cdpath = 1;

	return p;
}

/* Change the current directory to the home directory. */
static int
change_to_home_dir(const int cd_flag)
{
	if (!user.home) {
		if (cd_flag == CD_PRINT_ERROR)
			xerror("%s\n", _("cd: Home directory not found"));
		return ENOENT;
	}

	if (xchdir(user.home, SET_TITLE) != FUNC_SUCCESS) {
		if (cd_flag == CD_PRINT_ERROR)
			xerror("cd: '%s': %s\n", user.home, strerror(errno));
		return errno;
	}

	if (workspaces) {
		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(user.home, user.home_len);
	}

	return FUNC_SUCCESS;
}

/* Change current directory to NEW_PATH. */
static int
change_to_path(char *new_path, const int cd_flag)
{
	if (!new_path || !*new_path) {
		xerror("%s\n", _("cd: Path is NULL or empty"));
		return EINVAL;
	}

	if (strchr(new_path, '\\')) {
		char *deq_path = unescape_str(new_path, 0);
		if (deq_path) {
			/* deq_path is guaranteed to be shorter than new_path. */
			xstrsncpy(new_path, deq_path, strlen(deq_path) + 1);
			free(deq_path);
		}
	}

	char *cdpath_path = cdpath_n > 0 ? check_cdpath(new_path) : NULL;
	errno = 0;
	char *tmp = cdpath_path ? cdpath_path : new_path;
	char *dest_dir = normalize_path(tmp, strlen(tmp));
	free(cdpath_path);

	if (!dest_dir) {
		if (cd_flag == CD_PRINT_ERROR)
			xerror(_("cd: '%s': Error normalizing path\n"), new_path);
		return FUNC_FAILURE;
	}

	if (xchdir(dest_dir, SET_TITLE) != FUNC_SUCCESS) {
		if (cd_flag == CD_PRINT_ERROR)
			xerror("cd: '%s': %s\n", new_path, strerror(errno));

		free(dest_dir);

		/* Most shells return 1 in case of EACCESS/ENOENT error. However, 1, as
		 * a general error code, is not quite informative. Why not to return the
		 * actual error code returned by chdir(3)? Note that POSIX only requires
		 * for cd to return >0 in case of error (see cd(1p)). */
		return (errno == EACCES || errno == ENOENT) ? 1 : errno;
	}

	if (workspaces) {
		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(dest_dir, strlen(dest_dir));
	}

	free(dest_dir);

	return FUNC_SUCCESS;
}

/* Implementation of the shell 'cd -' command. */
static int
change_to_previous_dir(void)
{
	static int state = 0;
	char *cmd[] = { state == 0 ? "b" : "f", NULL };

	state = !state;
	return *cmd[0] == 'b' ? back_function(cmd) : forth_function(cmd);
}

static inline int
skip_directory(const char *dir)
{
	return (conf.dirhistignore_regex && *conf.dirhistignore_regex
		&& regexec(&regex_dirhist, dir, 0, NULL, 0) == FUNC_SUCCESS);
}

/* Change current directory to NEW_PATH, or to HOME if new_path is NULL.
 * Errors are printed only if CD_FLAG is set to CD_PRINT_ERROR (1) */
int
cd_function(char *new_path, const int cd_flag)
{
	/* If no argument, change to home. */
	int ret = FUNC_SUCCESS;

	if (!new_path || !*new_path) {
		if ((ret = change_to_home_dir(cd_flag)) != FUNC_SUCCESS)
			return ret;

	} else if (*new_path == '-' && !new_path[1]) {
		return change_to_previous_dir();

	} else {
		if ((ret = change_to_path(new_path, cd_flag)) != FUNC_SUCCESS)
			return ret;
	}

	const int skip = skip_directory(workspaces[cur_ws].path);

	if (skip == 0)
		add_to_dirhist(workspaces[cur_ws].path);

	dir_changed = 1;
	if (conf.autols == 1)
		reload_dirlist();

	if (skip == 0)
		add_to_jumpdb(workspaces[cur_ws].path);

	return ret;
}

/* Return a pointer to the first occurrence in the string STR of a byte that
 * is not C. Otherwise, if only C is found, NULL is returned. */
static const char *
xstrcpbrk(const char *restrict str, const char c)
{
	if (!str || !*str)
		return NULL;

	while (*str) {
		if (*str != c)
			return str;
		str++;
	}

	return NULL;
}

/* Convert "... n" into "../.. n"
 * and "../.. n" into the corresponding path. */
char *
fastback(const char *str)
{
	if (!str || !*str || xstrcpbrk(str, '.'))
		return NULL;

	/* At this point we know STR contains only dots. */
	const size_t dots = strlen(str);

	if (dots < 2)
		return NULL;

	if (dots == 2) {
		char dir[] = "..";
		char *ret = normalize_path(dir, 2);
		return ret;
	}

	char dir[PATH_MAX + 1];
	dir[0] = '.';
	dir[1] = '.';

	/* Replace each dot after ".." by "/.." */
	size_t i, c = 2;
	for (i = 2; c < dots && i + 3 < sizeof(dir);) {
		dir[i] = '/'; i++;
		dir[i] = '.'; i++;
		dir[i] = '.'; i++;
		c++;
	}

	dir[i] = '\0';

	return normalize_path(dir, i);
}

void
print_dirhist(char *query)
{
	const int n = DIGINUM(dirhist_total_index);
	const size_t len = (query && conf.fuzzy_match == 1) ? strlen(query) : 0;
	const int fuzzy_str_type = (len > 0 && contains_utf8(query) == 1)
		? FUZZY_FILES_UTF8 : FUZZY_FILES_ASCII;
	struct stat a;
	char pointer[(MAX_COLOR * 2) + 8];

	snprintf(pointer, sizeof(pointer), "%s%s%s", mi_c, SET_MISC_PTR, df_c);

	for (int i = 0; i < dirhist_total_index; i++) {
		if (!old_pwd[i] || *old_pwd[i] == KEY_ESC)
			continue;

		if (query && (conf.fuzzy_match == 1
		? fuzzy_match(query, old_pwd[i], len, fuzzy_str_type) == 0
		: !strstr(old_pwd[i], query) ) )
			continue;

		const char *color = lstat(old_pwd[i], &a) == 0 ?
			get_entry_color(old_pwd[i], &a) : uf_c;

		printf("%s %s%-*d%s %s%s%s\n", i == dirhist_cur_index ? pointer : " ",
			el_c, n, i + 1, df_c, color, old_pwd[i], df_c);
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

	printf(_("%s: Directory history cleared\n"), PROGRAM_NAME);

	return FUNC_SUCCESS;
}

/* Change to the specified directory number (N) in the directory
 * history list. */
static int
change_to_dirhist_num(int n)
{
	if (n <= 0 || n > dirhist_total_index) {
		xerror(_("history: %d: No such ELN\n"), n);
		return FUNC_FAILURE;
	}

	n--;
	if (!old_pwd[n] || *old_pwd[n] == KEY_ESC) {
		xerror("%s\n", _("history: Invalid history entry"));
		return FUNC_FAILURE;
	}

	int ret = xchdir(old_pwd[n], SET_TITLE);
	if (ret != 0) {
		xerror("history: '%s': %s\n", old_pwd[n], strerror(errno));
		return FUNC_FAILURE;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(old_pwd[n], strlen(old_pwd[n]));

	dirhist_cur_index = n;
	ret = FUNC_SUCCESS;

	if (conf.autols == 1)
		reload_dirlist();

	return ret;
}

static int
surf_hist(char **args)
{
	if (*args[1] == 'h' && (!args[1][1] || strcmp(args[1], "hist") == 0)) {
		print_dirhist(NULL);
		return FUNC_SUCCESS;
	}

	if (*args[1] == 'c' && strcmp(args[1], "clear") == 0)
		return clear_dirhist();

	if (*args[1] != '!' || is_number(args[1] + 1) != 1) {
		fprintf(stderr, "%s\n", _(DIRHIST_USAGE));
		return FUNC_FAILURE;
	}

	const int n = xatoi(args[1] + 1);
	return change_to_dirhist_num(n);
}

/* Set the path of the current workspace to NEW_PATH. */
static int
set_path(const char *new_path)
{
	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(new_path, strlen(new_path));
	if (!workspaces[cur_ws].path)
		return FUNC_FAILURE;

	add_to_jumpdb(workspaces[cur_ws].path);

	dir_changed = 1;
	if (conf.autols == 1)
		reload_dirlist();

	return FUNC_SUCCESS;
}

/* Go back one entry in dirhist. */
int
back_function(char **args)
{
	if (!args)
		return FUNC_FAILURE;

	if (args[1]) {
		if (!IS_HELP(args[1]))
			return surf_hist(args);
		puts(_(BACK_USAGE));
		return FUNC_SUCCESS;
	}

	/* Find the previous non-consecutive equal and valid entry. */
	int i = dirhist_cur_index;
	while (--i >= 0) {
		if (old_pwd[i] && *old_pwd[i] != KEY_ESC && (!workspaces[cur_ws].path
		|| strcmp(workspaces[cur_ws].path, old_pwd[i]) != 0))
			break;
	}

	if (i < 0)
		return FUNC_SUCCESS;

	dirhist_cur_index = i;

	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == FUNC_SUCCESS)
		return set_path(old_pwd[dirhist_cur_index]);

	xerror("cd: '%s': %s\n", old_pwd[dirhist_cur_index], strerror(errno));

	/* Invalidate this entry. */
	*old_pwd[dirhist_cur_index] = KEY_ESC;
	if (dirhist_cur_index > 0)
		dirhist_cur_index--;

	return FUNC_FAILURE;
}

/* Go forth one entry in dirhist. */
int
forth_function(char **args)
{
	if (!args)
		return FUNC_FAILURE;

	if (args[1]) {
		if (!IS_HELP(args[1]))
			return surf_hist(args);
		puts(_(FORTH_USAGE));
		return FUNC_SUCCESS;
	}

	/* Find the next valid entry. */
	int i = dirhist_cur_index;
	while (++i <= dirhist_total_index) {
		if (old_pwd[i] && (*old_pwd[i] != KEY_ESC && (!workspaces[cur_ws].path
		|| strcmp(workspaces[cur_ws].path, old_pwd[i]) != 0)))
			break;
	}

	if (i > dirhist_total_index)
		return FUNC_SUCCESS;

	dirhist_cur_index = i;


	if (xchdir(old_pwd[dirhist_cur_index], SET_TITLE) == FUNC_SUCCESS)
		return set_path(old_pwd[dirhist_cur_index]);

	xerror("cd: '%s': %s\n", old_pwd[dirhist_cur_index], strerror(errno));

	/* Invalidate this entry. */
	*old_pwd[dirhist_cur_index] = KEY_ESC;
	if (dirhist_cur_index < dirhist_total_index
	&& old_pwd[dirhist_cur_index + 1])
		dirhist_cur_index++;

	return FUNC_FAILURE;
}
