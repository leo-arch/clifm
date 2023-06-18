/* args.c -- Handle command line arguments */

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h> /* INT_MAX */
#include <pwd.h> /* getpwuid() */
#include <errno.h>
#include <readline/tilde.h> /* tilde_expand() */
#include <string.h>

#include "aux.h"
#include "checks.h"
#include "exec.h"
#include "file_operations.h"
#include "init.h"
#include "mime.h"
#include "misc.h"
#include "navigation.h"
#include "profiles.h"
#include "sanitize.h"
#include "strings.h"

#define PREVIEW_FILE 1
#define OPEN_FILE    2

/* If path was not set (neither in the config file nor via command line nor
 * via the RestoreLastPath option), set the default (CWD), and if CWD is not
 * set, use the user's home directory, and if the home dir cannot be found
 * either, try the root directory, and if we have no access to the root dir
 * either, exit. */
static void
set_cur_workspace(void)
{
	if (workspaces[cur_ws].path)
		return;

	char p[PATH_MAX] = "";
	char *cwd = get_cwd(p, sizeof(p), 0);

	if (cwd && *cwd) {
		workspaces[cur_ws].path = savestring(cwd, strlen(cwd));
		return;
	}

	if (user.home) {
		workspaces[cur_ws].path = savestring(user.home, user.home_len);
		return;
	}

	if (access("/", R_OK | X_OK) != -1) {
		workspaces[cur_ws].path = savestring("/", 1);
		return;
	}

	xerror("%s: /: %s\n", PROGRAM_NAME, strerror(errno));
	exit(EXIT_FAILURE);
}

int
set_start_path(void)
{
	/* Last path is overriden by positional parameters in the command line */
	if (conf.restore_last_path == 1)
		get_last_path();

	if (cur_ws == UNSET)
		cur_ws = DEF_CUR_WS;

	if (cur_ws > MAX_WS - 1) {
		cur_ws = DEF_CUR_WS;
		_err('w', PRINT_PROMPT, _("%s: %zu: Invalid workspace."
			"\nFalling back to workspace %zu\n"),
			PROGRAM_NAME, cur_ws, cur_ws + 1);
	}

	prev_ws = cur_ws;
	set_cur_workspace();

	/* Make path the CWD. */
	int ret = xchdir(workspaces[cur_ws].path, NO_TITLE);

	char tmp[PATH_MAX] = "";
	char *pwd = get_cwd(tmp, sizeof(tmp), 0);

	/* If chdir() fails, set path to PWD, list files and print the
	 * error message. If no access to PWD either, exit. */
	if (ret == -1) {
		_err('e', PRINT_PROMPT, "%s: chdir: '%s': %s\n", PROGRAM_NAME,
		    workspaces[cur_ws].path, strerror(errno));

		if (!pwd || !*pwd) {
			_err(0, NOPRINT_PROMPT, _("%s: Fatal error! Failure "
				"retrieving current working directory\n"), PROGRAM_NAME);
			exit(EXIT_FAILURE);
		}

		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(pwd, strlen(pwd));
	}

	/* Set OLDPWD. */
	if (pwd && *pwd && (!workspaces || !workspaces[cur_ws].path
	|| strcmp(workspaces[cur_ws].path, pwd) != 0))
		setenv("OLDPWD", pwd, 1);

	dir_changed = 1;
	return EXIT_SUCCESS;
}

static int
try_standard_data_dirs(void)
{
	char home_local[PATH_MAX];
	*home_local = '\0';
	if (user.home && *user.home)
		snprintf(home_local, sizeof(home_local), "%s/.local/share", user.home);

	char *const data_dirs[] = {
		home_local,
		"/usr/local/share",
		"/usr/share",
		"/opt/local/share",
		"/opt/share",
		"/opt/clifm/share",
#if defined(__HAIKU__)
		"/boot/system/non-packaged/data",
		"/boot/system/data",
#endif
		NULL };

	size_t i;
	struct stat a;

	for (i = 0; data_dirs[i]; i++) {
		char tmp[PATH_MAX + 5 + 10];
		snprintf(tmp, sizeof(tmp), "%s/%s/%src", data_dirs[i], PNL, PNL);

		if (stat(tmp, &a) == -1 || !S_ISREG(a.st_mode))
			continue;

		data_dir = savestring(data_dirs[i], strlen(data_dirs[i]));
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

static char *
try_datadir(const char *dir)
{
	if (!dir || !*dir)
		return (char *)NULL;

	/* Remove ending "/bin" from DIR */
	char *r = strrchr(dir, '/');
	if (r && *r && *(++r) && *r == 'b' && strcmp(r, "bin") == 0)
		*(r - 1) = '\0';

	struct stat a;

	char p[PATH_MAX];

	/* Try DIR/share/clifm/clifmrc */
	snprintf(p, sizeof(p), "%s/share/%s/%src", dir, PNL, PNL);
	if (stat(p, &a) != -1 && S_ISREG(a.st_mode)) {
		size_t len = strlen(dir) + 7;
		char *q = (char *)xnmalloc(len, sizeof(char));
		snprintf(q, len, "%s/share", dir);
		return q;
	}

	/* Try DIR/clifm/clifmrc */
	snprintf(p, sizeof(p), "%s/%s/%src", dir, PNL, PNL);
	if (stat(p, &a) != -1 && S_ISREG(a.st_mode))
		return savestring(dir, strlen(dir));

	return (char *)NULL;
}

static char *
resolve_absolute_path(const char *s)
{
	if (!s || !*s)
		return (char *)NULL;

	char *p = strrchr(s, '/'), *t = (char *)NULL;
	if (p && p != s)
		*p = '\0';
	else
		t = "/";

	if (p)
		*p = '/';

	return try_datadir(t ? t : s);
}

static char *
resolve_relative_path(const char *s)
{
	if (!s || !*s)
		return (char *)NULL;

	char *p = realpath(s, NULL);
	if (!p)
		return (char *)NULL;

	char *q = strrchr(p, '/');
	if (q && q != p)
		*q = '\0';

	char *r = try_datadir(p);
	free(p);
	return r;
}

static char *
resolve_basename(const char *s)
{
	if (!s || !*s)
		return (char *)NULL;

	char *p = get_cmd_path(s);
	if (!p)
		return (char *)NULL;

	char *q = strrchr(p, '/');
	if (q && q != p)
		*q = '\0';

	char *r = try_datadir(p);
	free(p);

	return r;
}

static int
get_data_dir_from_path(char *s)
{
	if (!s || !*s)
		return EXIT_FAILURE;

	char *dd = (char *)NULL;
	char *t = *s == '~' ? tilde_expand(s) : s;

	if (*t == '/' && (dd = resolve_absolute_path(t)))
		goto END;

	if (strchr(t, '/') && (dd = resolve_relative_path(t)))
		goto END;

	dd = resolve_basename(t);

END:
	if (t != s)
		free(t);

	if (!dd)
		return EXIT_FAILURE;

	data_dir = dd;
	return EXIT_SUCCESS;
}

/* Get the system data directory (usually /usr/local/share),
 * Try first CLIFM_DATADIR, defined in the Makefile, then a few standard
 * paths, and finally try to guess based on whatever argv[0] provides. */
void
get_data_dir(void)
{
	if (data_dir) /* DATA_DIR was defined via --data-dir */
		return;

#ifdef CLIFM_DATADIR
	struct stat a;
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/%s/%src", STRINGIZE(CLIFM_DATADIR), PNL, PNL);
	if (stat(p, &a) != -1 && S_ISREG(a.st_mode)) {
		data_dir = savestring(STRINGIZE(CLIFM_DATADIR),
			strlen(STRINGIZE(CLIFM_DATADIR)));
		return;
	}
#endif /* CLIFM_DATADIR */

	if (try_standard_data_dirs() == EXIT_SUCCESS)
		return;

	if (get_data_dir_from_path(argv_bk[0]) == EXIT_SUCCESS)
		return;

	_err('w', PRINT_PROMPT, _("%s: No data directory found. Data files, "
		"such as plugins and color schemes, might not be available.\n"),
		PROGRAM_NAME);
}

static char *
get_home_sec_env(void)
{
	struct passwd *pw = (struct passwd *)NULL;

	uid_t u = geteuid();
	pw = getpwuid(u);
	if (!pw) {
		_err('e', PRINT_PROMPT, "%s: getpwuid: %s\n",
			PROGRAM_NAME, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return (pw->pw_dir);
}

/* Opener function: open FILENAME and exit */
static void
open_reg_exit(char *filename, const int url, const int preview)
{
	char *homedir = (xargs.secure_env == 1 || xargs.secure_env_full == 1)
		? get_home_sec_env() : getenv("HOME");
	if (!homedir) {
		xerror("%s: Could not retrieve the home directory\n", PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	tmp_dir = savestring(P_tmpdir, P_tmpdir_len);

	size_t mime_file_len = 0;

	if (alt_preview_file && preview == 1) {
		mime_file = savestring(alt_preview_file, strlen(alt_preview_file));
	} else {
		mime_file_len = strlen(homedir)
			+ (alt_profile ? strlen(alt_profile) : 7) + 40;

		mime_file = (char *)xnmalloc(mime_file_len, sizeof(char));
		snprintf(mime_file, mime_file_len,
			"%s/.config/clifm/profiles/%s/%s.clifm",
			homedir, alt_profile ? alt_profile : "default",
			preview == 1 ? "preview" : "mimelist");
	}

	if (path_n == 0)
		path_n = get_path_env();

#ifndef _NO_LIRA
	if (url == 1 && mime_open_url(filename) == EXIT_SUCCESS)
		exit(EXIT_SUCCESS);
#else
	UNUSED(url);
#endif

	char *p = (char *)NULL;
	if (*filename == '~')
		p = tilde_expand(filename);

	int ret = open_file(p ? p : filename);
	free(p);
	exit(ret);
}

static inline int
set_sort_by_name(const char *name)
{
	size_t i;
	for (i = 0; i <= SORT_TYPES; i++) {
		if (*name == *_sorts[i].name && strcmp(name, _sorts[i].name) == 0)
			return _sorts[i].num;
	}

	return SNAME;
}

static inline void
set_sort(const char *arg)
{
	int n = 0;

	if (!is_number(arg))
		n = set_sort_by_name(arg);
	else
		n = atoi(arg);

	if (n < 0 || n > SORT_TYPES)
		conf.sort = SNAME;
	else
		conf.sort = n;

	xargs.sort = conf.sort;
}

/* Open/preview FILE according to MODE: either PREVIEW_FILE or OPEN_FILE */
static void
open_preview_file(char *file, const int mode)
{
	if (xargs.stealth_mode == 1) {
		fprintf(stderr, "%s: Running in stealth mode. Access to "
			"configuration files is not allowed\n", PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}
	char *_path = file;
	int url = 1, preview = mode == PREVIEW_FILE ? 1 : 0;

	struct stat attr;
	if (IS_FILE_URI(_path)) {
		_path = file + 7;
		if (stat(_path, &attr) == -1) {
			xerror("%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
			exit(errno);
		}
		url = 0;
		goto RUN;
	}

	if (is_url(_path) == EXIT_FAILURE) {
		url = 0;
		if (*_path != '~' && stat(_path, &attr) == -1) {
			xerror("%s: %s: %s\n", PROGRAM_NAME, _path, strerror(errno));
			exit(errno);
		}
	}

RUN:
	xargs.open = preview == 1 ? 0 : 1;
	xargs.preview = preview == 1 ? 1 : 0;
	if (preview == 1)
		clear_term_img();

	open_reg_exit(_path, url, preview);
}

static char *
stat_file(char *file)
{
	if (!file || !*file)
		exit(EXIT_FAILURE);

	char *p = (char *)NULL;
	if (*file == '~') {
		if (!(p = tilde_expand(file))) {
			xerror(_("%s: %s: Error expanding tilde\n"), PROGRAM_NAME, file);
			exit(EXIT_FAILURE);
		}
	} else {
		p = savestring(file, strlen(file));
	}

	struct stat a;
	if (stat(p, &a) == -1) {
		xerror("%s: %s: %s\n", PROGRAM_NAME, p, strerror(errno));
		if (p != file)
			free(p);
		exit(errno);
	}

	return p;
}

static void
err_arg_required(const char *s)
{
	fprintf(stderr, _("%s: '%s': Option requires an argument\n"
		"Try '%s --help' for more information.\n"), PNL, s, PNL);
	exit(EXIT_FAILURE);
}

static void
set_custom_selfile(char *file)
{
	if (!file || !*file || *file == '-')
		err_arg_required("--sel-file");

	if ( (sel_file = normalize_path(file, strlen(file))) ) {
		xargs.sel_file = 1;
		return;
	}

	_err('e', PRINT_PROMPT, _("%s: %s: Error setting custom "
		"selections file\n"), PROGRAM_NAME, file);
}

static void
set_alt_bm_file(char *file)
{
	char *p = (char *)NULL;

	if (*file == '~') {
		p = tilde_expand(file);
		file = p;
	}

	if (access(file, R_OK) == -1) {
		_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
			"Falling back to the default bookmarks file\n"),
		    PROGRAM_NAME, file, strerror(errno));
		free(p);
		return;
	}

	alt_bm_file = savestring(file, strlen(file));
	_err(ERR_NO_LOG, PRINT_PROMPT, _("%s: Loaded alternative "
		"bookmarks file\n"), PROGRAM_NAME);

	free(p);
}

static void
set_alt_config_dir(char *dir)
{
	char *dir_exp = (char *)NULL;

	if (*dir == '~') {
		dir_exp = tilde_expand(dir);
		dir = dir_exp;
	}

	int dir_ok = 1;
	struct stat attr;

	if (stat(dir, &attr) == -1) {
		char *tmp_cmd[] = {"mkdir", "-p", dir, NULL};
		int ret = launch_execve(tmp_cmd, FOREGROUND, E_NOSTDERR);
		if (ret != EXIT_SUCCESS) {
			_err('e', PRINT_PROMPT, _("%s: %s: Cannot create directory "
				"(error %d)\nFalling back to default configuration "
				"directory\n"), PROGRAM_NAME, dir, ret);
			dir_ok = 0;
		}
	}

	if (access(dir, W_OK) == -1) {
		if (dir_ok == 1) {
			_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
				"Falling back to default configuration directory\n"),
				PROGRAM_NAME, dir, strerror(errno));
		}
	} else {
		alt_config_dir = savestring(dir, strlen(dir));
		_err(ERR_NO_LOG, PRINT_PROMPT, _("%s: %s: Using alternative "
			"configuration directory\n"), PROGRAM_NAME, alt_config_dir);
	}

	free(dir_exp);
}

static void
set_alt_kbinds_file(char *file)
{
	char *kbinds_exp = (char *)NULL;
	if (*file == '~') {
		kbinds_exp = tilde_expand(file);
		file = kbinds_exp;
	}

	if (access(file, R_OK) == -1) {
		_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
			"Falling back to the default keybindings file\n"),
		    PROGRAM_NAME, file, strerror(errno));
	} else {
		alt_kbinds_file = savestring(file, strlen(file));
		_err(ERR_NO_LOG, PRINT_PROMPT, _("%s: Loaded alternative "
			"keybindings file\n"), PROGRAM_NAME);
	}

	free(kbinds_exp);
}

static void
set_alt_config_file(char *file)
{
	char *config_exp = (char *)NULL;

	if (*file == '~') {
		config_exp = tilde_expand(file);
		file = config_exp;
	}

	if (access(file, R_OK) == -1) {
		_err('e', PRINT_PROMPT, _("%s: %s: %s\nFalling back to default\n"),
			PROGRAM_NAME, file, strerror(errno));
		xargs.config = -1;
	} else {
		alt_config_file = savestring(file, strlen(file));
		_err(ERR_NO_LOG, PRINT_PROMPT, _("%s: Loaded alternative "
			"configuration file\n"), PROGRAM_NAME);
	}

	free(config_exp);
}

static char *
resolve_path(char *file)
{
	char *_path = (char *)NULL;

	if (*file == '.') {
		_path = normalize_path(file, strlen(file));
		if (!_path) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
			exit(errno);
		}

	} else if (*file == '~') {
		_path = tilde_expand(file);
		if (!_path) {
			fprintf(stderr, "%s: %s: Error expanding tilde\n",
				PROGRAM_NAME, file);
			exit(EXIT_FAILURE);
		}

	} else if (*file == '/') {
		_path = savestring(file, strlen(file));

	} else {
		char tmp[PATH_MAX] = "";
		char *cwd = get_cwd(tmp, sizeof(tmp), 0);

		if (!cwd || !*cwd) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
			exit(errno);
		}

		size_t len = strlen(cwd) + strlen(file) + 2;
		_path = (char *)xnmalloc(len, sizeof(char));
		snprintf(_path, len, "%s/%s", cwd, file);
	}

	return _path;
}

static char *
resolve_starting_path(char *file)
{
	char *_path = (char *)NULL;

	if (IS_FILE_URI(file)) {
		_path = savestring(file + 7, strlen(file + 7));

	} else if (is_url(file) == EXIT_SUCCESS) {
		open_reg_exit(file, 1, 0);
		exit(EXIT_SUCCESS); /* Never reached */

	} else {
		_path = resolve_path(file);
	}

	struct stat a;
	if (stat(_path, &a) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		free(_path);
		exit(errno);
	}

	if (!S_ISDIR(a.st_mode)) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(ENOTDIR));
		free(_path);
		exit(ENOTDIR);
	}

	xargs.path = 1;
	return _path;
}

static void
_set_starting_path(char *_path)
{
	if (xchdir(_path, SET_TITLE) == 0) {
		if (cur_ws == UNSET)
			cur_ws = DEF_CUR_WS;

		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(_path, strlen(_path));

	} else { /* Error changing directory */
		if (xargs.list_and_quit == 1) {
			xerror("%s: %s: %s\n", PROGRAM_NAME, _path, strerror(errno));
			exit(EXIT_FAILURE);
		}

		_err('w', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			_path, strerror(errno));
	}
}

/* Evaluate command line arguments, if any, and change initial variables to
 * its corresponding value. */
void
parse_cmdline_args(const int argc, char **argv)
{
	/* Disable automatic error messages to be able to handle them ourselves
	 * via the '?' case in the switch. */
	opterr = optind = 0;

	/* Link long (--option) and short options (-o) for the getopt_long function. */
	static const struct option longopts[] = {
		{"no-hidden", no_argument, 0, 'a'},
		{"show-hidden", no_argument, 0, 'A'},
		{"bookmarks-file", required_argument, 0, 'b'},
		{"config-file", required_argument, 0, 'c'},

#ifdef RUN_CMD
		{"cmd", required_argument, 0, 'C'},
#endif

		{"config-dir", required_argument, 0, 'D'},
		{"no-eln", no_argument, 0, 'e'},
		{"eln-use-workspace-color", no_argument, 0, 'E'},
		{"no-dirs-first", no_argument, 0, 'f'},
		{"dirs-first", no_argument, 0, 'F'},
		{"pager", no_argument, 0, 'g'},
		{"no-pager", no_argument, 0, 'G'},
		{"help", no_argument, 0, 'h'},
		{"horizontal-list", no_argument, 0, 'H'},
		{"no-case-sensitive", no_argument, 0, 'i'},
		{"case-sensitive", no_argument, 0, 'I'},
		{"keybindings-file", required_argument, 0, 'k'},
		{"no-long-view", no_argument, 0, 'l'},
		{"long-view", no_argument, 0, 'L'},
		{"dirhist-map", no_argument, 0, 'm'},
		{"no-autols", no_argument, 0, 'o'},
		{"autols", no_argument, 0, 'O'},
		{"path", required_argument, 0, 'p'},
		{"profile", required_argument, 0, 'P'},
		{"no-refresh-on-emtpy-line", no_argument, 0, 'r'},
		{"splash", no_argument, 0, 's'},
		{"stealth-mode", no_argument, 0, 'S'},
		{"disk-usage-analyzer", no_argument, 0, 't'},
		{"version", no_argument, 0, 'v'},
		{"workspace", required_argument, 0, 'w'},
		{"no-ext-cmds", no_argument, 0, 'x'},
		{"light-mode", no_argument, 0, 'y'},
		{"sort", required_argument, 0, 'z'},

		/* Only long options. Let's use values >= 200 to avoid conflicts
		 * with short options (a-Z == 65-122). */
		{"no-cd-auto", no_argument, 0, 200},
		{"no-open-auto", no_argument, 0, 201},
		{"no-restore-last-path", no_argument, 0, 202},
		{"no-tips", no_argument, 0, 203},
		{"disk-usage", no_argument, 0, 204},
		{"no-classify", no_argument, 0, 205},
		{"share-selbox", no_argument, 0, 206},
		{"rl-vi-mode", no_argument, 0, 207},
		{"max-dirhist", required_argument, 0, 208},
		{"sort-reverse", no_argument, 0, 209},
		{"no-files-counter", no_argument, 0, 210},
		{"no-welcome-message", no_argument, 0, 211},
		{"no-clear-screen", no_argument, 0, 212},
//		{"enable-logs", no_argument, 0, 213},
		{"max-path", required_argument, 0, 214},
		{"opener", required_argument, 0, 215},
		{"only-dirs", no_argument, 0, 217},
		{"list-and-quit", no_argument, 0, 218},
		{"color-scheme", required_argument, 0, 219},
		{"cd-on-quit", no_argument, 0, 220},
		{"no-dir-jumper", no_argument, 0, 221},
		{"icons", no_argument, 0, 222},
		{"icons-use-file-color", no_argument, 0, 223},
		{"no-columns", no_argument, 0, 224},
		{"no-colors", no_argument, 0, 225},
		{"max-files", required_argument, 0, 226},
		{"trash-as-rm", no_argument, 0, 227},
		{"case-sens-dirjump", no_argument, 0, 228},
		{"case-sens-path-comp", no_argument, 0, 229},
		{"cwd-in-title", no_argument, 0, 230},
		{"open", required_argument, 0, 231},
		{"preview", required_argument, 0, 231},
		{"print-sel", no_argument, 0, 232},
		{"no-suggestions", no_argument, 0, 233},
//		{"autojump", no_argument, 0, 234},
		{"no-highlight", no_argument, 0, 235},
		{"no-file-cap", no_argument, 0, 236},
		{"no-file-ext", no_argument, 0, 237},
		{"no-follow-symlinks", no_argument, 0, 238},
		{"int-vars", no_argument, 0, 240},
		{"stdtab", no_argument, 0, 241},
		{"no-warning-prompt", no_argument, 0, 242},
		{"mnt-udisks2", no_argument, 0, 243},
		{"secure-env", no_argument, 0, 244},
		{"secure-env-full", no_argument, 0, 245},
		{"secure-cmds", no_argument, 0, 246},
		{"full-dir-size", no_argument, 0, 247},
		{"no-apparent-size", no_argument, 0, 248},
		{"no-history", no_argument, 0, 249},
		{"fzytab", no_argument, 0, 250},
		{"no-refresh-on-resize", no_argument, 0, 251},
		{"bell", required_argument, 0, 252},
		{"fuzzy-matching", no_argument, 0, 253},
		{"smenutab", no_argument, 0, 254},
		{"virtual-dir-full-paths", no_argument, 0, 255},
		{"virtual-dir", required_argument, 0, 256},
		{"desktop-notifications", no_argument, 0, 257},
		{"vt100", no_argument, 0, 258},
		{"no-fzfpreview", no_argument, 0, 259},
		{"fzfpreview", no_argument, 0, 260}, // legacy
		{"fzfpreview-hidden", no_argument, 0, 261},
		{"shotgun-file", required_argument, 0, 262},
		{"fzftab", no_argument, 0, 263},
		{"si", no_argument, 0, 264},
		{"data-dir", required_argument, 0, 265},
		{"fuzzy-algo", required_argument, 0, 266},
		{"sel-file", required_argument, 0, 267},
		{"no-trim-names", no_argument, 0, 268},
		{"no-bold", no_argument, 0, 269},
		{"fnftab", no_argument, 0, 270},
	    {0, 0, 0, 0}
	};

	int optc;
	int open_prev_mode = 0;

	/* Variables to store arguments to options. */
	char *path_value = (char *)NULL,
#ifndef _NO_PROFILES
		*alt_profile_value = (char *)NULL,
#endif
		*alt_dir_value = (char *)NULL,
		*config_value = (char *)NULL,
		*kbinds_value = (char *)NULL,
		*virtual_dir_value = (char *)NULL,
		*bm_value = (char *)NULL,
		*open_prev_file = (char *)NULL;

	while ((optc = getopt_long(argc, argv,
#ifdef RUN_CMD
		    "+aAb:c:C:D:eEfFgGhHiIk:lLmoOp:P:rsStUuvw:Wxyz:",
#else
		    "+aAb:c:D:eEfFgGhHiIk:lLmoOp:P:rsStUuvw:Wxyz:",
#endif
			longopts, (int *)0)) != EOF) {
		/* ':' and '::' in the short options string means 'required' and
		 * 'optional argument' respectivelly. Thus, 'p' and 'P' require
		 * an argument here. The plus char (+) tells getopt to stop
		 * processing at the first non-option (and non-argument) */
		switch (optc) {

		case 200: xargs.autocd = conf.autocd = 0; break;
		case 201: xargs.auto_open = conf.auto_open = 0; break;
		case 202: xargs.restore_last_path = conf.restore_last_path = 0; break;
		case 203: xargs.tips = conf.tips = 0; break;
		case 204: xargs.disk_usage = conf.disk_usage = 1; break;
		case 205: xargs.classify = conf.classify = 0; break;
		case 206: xargs.share_selbox = conf.share_selbox = 1; break;
		case 207: xargs.rl_vi_mode = 1; break;

		case 208: {
			if (!is_number(optarg))
				break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0 && opt_int <= INT_MAX)
				xargs.max_dirhist = conf.max_dirhist = opt_int;
		} break;

		case 209: xargs.sort_reverse = conf.sort_reverse = 1; break;
		case 210: xargs.files_counter = conf.files_counter = 0; break;
		case 211: xargs.welcome_message = conf.welcome_message = 0; break;
		case 212: xargs.clear_screen = conf.clear_screen = 0; break;

		case 214: {
			if (!is_number(optarg))
				break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0 && opt_int <= INT_MAX)
				xargs.max_path = conf.max_path = opt_int;
		} break;

		case 215:
			if (*optarg == '~') {
				char *ep = tilde_expand(optarg);
				if (ep) {
					conf.opener = savestring(ep, strlen(ep));
					free(ep);
				} else {
					_err('w', PRINT_PROMPT, _("%s: Error expanding tilde. "
						"Using default opener\n"), PROGRAM_NAME);
				}
			} else {
				conf.opener = savestring(optarg, strlen(optarg));
			}
			break;

		case 217: xargs.only_dirs = conf.only_dirs = 1; break;
		case 218: xargs.list_and_quit = 1; break;

		case 219:
			if (!optarg || !*optarg || *optarg == '-')
				err_arg_required("--color-scheme");

			conf.usr_cscheme = savestring(optarg, strlen(optarg));
			break;

		case 220: xargs.cd_on_quit = conf.cd_on_quit = 1; break;
		case 221: xargs.no_dirjump = 1; break;
#ifndef _NO_ICONS
		case 222: xargs.icons = conf.icons = 1; break;
		case 223: xargs.icons = conf.icons = xargs.icons_use_file_color = 1; break;
#else
		case 222: /* fallthrough */
		case 223:
			fprintf(stderr, _("%s: icons: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_ICONS */
		case 224: xargs.columns = conf.columned = 0; break;
		case 225:
			xargs.colorize = conf.colorize = 0;
#ifndef _NO_HIGHLIGHT
			xargs.highlight = conf.highlight = 0;
#endif /* _NO_HIGHLIGHT */
			break;

		case 226:
			if (!is_number(optarg))
				break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0 && opt_int <= INT_MAX)
				xargs.max_files = max_files = opt_int;
			break;

		case 227:
#ifndef _NO_TRASH
			xargs.trasrm = conf.tr_as_rm = 1; break;
#else
			fprintf(stderr, _("%s: trash: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_TRASH */
		case 228: xargs.case_sens_dirjump = conf.case_sens_dirjump = 1; break;
		case 229: xargs.case_sens_path_comp = conf.case_sens_path_comp = 1; break;
		case 230: xargs.cwd_in_title = 1; break;

		case 231: { /* --open or --preview */
			open_prev_file = optarg;
			int n = *argv[optind - 1] == '-' ? 1 : 2;
			if (*(argv[optind - n] + 2) == 'p')
				open_prev_mode = PREVIEW_FILE; /* --preview */
			else
				open_prev_mode = OPEN_FILE; /* --open */
		} break;

		case 232: xargs.printsel = conf.print_selfiles = 1; break;
		case 233:
#ifndef _NO_SUGGESTIONS
			xargs.suggestions = conf.suggestions = 0;
#endif /* !_NO_SUGGESTIONS */
			break;
		case 235:
#ifndef _NO_HIGHLIGHT
			xargs.highlight = conf.highlight = 0;
#endif /* !_NO_HIGHLIGHT */
			break;
		case 236: xargs.check_cap = check_cap = 0; break;
		case 237: xargs.check_ext = check_ext = 0; break;
		case 238: xargs.follow_symlinks = follow_symlinks = 0; break;
		case 240: xargs.int_vars = int_vars = 1; break;
		case 241:
#ifndef _NO_FZF
			xargs.fzftab = 0;
#endif /* !_NO_FZF */
			fzftab = 0; tabmode = STD_TAB;
			break;
		case 242: xargs.warning_prompt = conf.warning_prompt = 0; break;
		case 243: xargs.mount_cmd = MNT_UDISKS2; break;
		case 244:
			xargs.secure_env = 1;
			xsecure_env(SECURE_ENV_IMPORT);
			break;
		case 245:
			xargs.secure_env_full = 1;
			xsecure_env(SECURE_ENV_FULL);
			break;
		case 246: xargs.secure_cmds = 1; break;
		case 247: xargs.full_dir_size = conf.full_dir_size = 1; break;
		case 248: xargs.apparent_size = conf.apparent_size = 0; break;
		case 249: xargs.history = 0; break;
		case 250:
			fprintf(stderr, "%s: --fzytab: We have migrated to 'fnf'.\n"
				"Install 'fnf' (https://github.com/leo-arch/fnf) and then "
				"use --fnftab instead\n", PROGRAM_NAME);
			exit(EXIT_FAILURE);
		case 251: xargs.refresh_on_resize = 0; break;
		case 252: {
			int a = atoi(optarg);
			if (!is_number(optarg) || a < 0 || a > 3) {
				fprintf(stderr, "%s: bell: valid options are 0:none, 1:audible, "
					"2:visible (requires readline >= 8.1), 3:flash. Defaults "
					"to 'visible', and, if not possible, 'none'.\n",
					PROGRAM_NAME);
				exit(EXIT_FAILURE);
			}
			xargs.bell_style = a; break;
			}
		case 253: xargs.fuzzy_match = conf.fuzzy_match = 1; break;
		case 254:
#ifndef _NO_FZF
			xargs.smenutab = 1; fzftab = 1; tabmode = SMENU_TAB; break;
#else
			fprintf(stderr, _("%s: smenu-tab: %s\n"),
				PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_FZF */
		case 255: xargs.virtual_dir_full_paths = 1; break;
		case 256:
			if (optarg && *optarg && *optarg == '/') {
				virtual_dir_value = optarg;
			} else {
				fprintf(stderr, "%s: '--virtual-dir': Absolute path "
					"is required as argument\n", PROGRAM_NAME);
				exit(EXIT_FAILURE);
			}
			break;

		case 257: xargs.desktop_notifications = conf.desktop_notifications = 1;
			break;
		case 258:
			xargs.vt100 = 1;
			xargs.clear_screen = conf.clear_screen = 0;
			fzftab = 0; tabmode = STD_TAB;
			break;

		case 259: xargs.fzf_preview = conf.fzf_preview = 0; break;

		case 260: /* fallthrough */ /* --fzfpreview */
		case 261: /* --fzfpreview-hidden */
#ifndef _NO_FZF
			xargs.fzf_preview = 1;
			conf.fzf_preview = optc == 260 ? 1 : 2;
			xargs.fzftab = fzftab = 1; tabmode = FZF_TAB;
#else
			fprintf(stderr, _("%s: fzf-preview: %s\n"),
				PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_FZF */
			break;

		case 262:
			if (!optarg || !*optarg || *optarg == '-')
				err_arg_required("--shotgun-file");

			alt_preview_file = stat_file(optarg);
			break;

		case 263:
#ifndef _NO_FZF
			xargs.fzftab = fzftab = 1; tabmode = FZF_TAB; break;
#else
			fprintf(stderr, _("%s: fzf-tab: %s\n"),
				PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_FZF */

		case 264: xargs.si = 1; break;

		case 265:
			if (!optarg || !*optarg || *optarg == '-')
				err_arg_required("--data-dir");

			get_data_dir_from_path(optarg);
			break;

		case 266: {
			int a = optarg ? atoi(optarg) : -1;
			if (!optarg || a < 1 || a > FUZZY_ALGO_MAX) {
				fprintf(stderr, "%s: fuzzy-algo: Valid arguments are either 1 "
					"or 2\n", PROGRAM_NAME);
				exit(EXIT_FAILURE);
			}
			xargs.fuzzy_match_algo = conf.fuzzy_match_algo = a;
			}
			break;

		case 267: set_custom_selfile(optarg); break;
		case 268: xargs.trim_names = conf.trim_names = 0; break;
		case 269: xargs.no_bold = 1; break;

		case 270:
#ifndef _NO_FZF
			xargs.fnftab = 1; fzftab = 1; tabmode = FNF_TAB; break;
#else
			fprintf(stderr, _("%s: fnf-tab: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* _NO_FZF */

		case 'a': conf.show_hidden = xargs.hidden = 0; break;
		case 'A': conf.show_hidden = xargs.hidden = 1; break;
		case 'b': xargs.bm_file = 1; bm_value = optarg; break;
		case 'c': xargs.config = 1; config_value = optarg; break;

#ifdef RUN_CMD
		case 'C':
			if (!optarg || !*optarg || *optarg == '-') {
				err_arg_required("--cmd");
				exit(EXIT_FAILURE);
			}
			cmd_line_cmd = savestring(optarg, strlen(optarg));
			break;
#endif

		case 'D': alt_dir_value = optarg; break;
		case 'e': xargs.noeln = conf.no_eln = 1; break;
		case 'E': xargs.eln_use_workspace_color = 1; break;
		case 'f': conf.list_dirs_first = xargs.dirs_first = 0; break;
		case 'F': conf.list_dirs_first = xargs.dirs_first = 1; break;
		case 'g': conf.pager = xargs.pager = 1; break;
		case 'G': conf.pager = xargs.pager = 0; break;
		case 'h': help_function(); exit(EXIT_SUCCESS);
		case 'H': xargs.horizontal_list = 1; conf.listing_mode = HORLIST; break;
		case 'i': conf.case_sens_list = xargs.case_sens_list = 0; break;
		case 'I': conf.case_sens_list = xargs.case_sens_list = 1; break;
		case 'k': kbinds_value = optarg; break;
		case 'l': conf.long_view = xargs.longview = 0; break;
		case 'L': conf.long_view = xargs.longview = 1; break;
		case 'm': conf.dirhist_map = xargs.dirmap = 1; break;
		case 'o': conf.autols = xargs.autols = 0; break;
		case 'O': conf.autols = xargs.autols = 1; break;
		case 'p': path_value = optarg; xargs.path = 1; break;
		case 'P':
#ifndef _NO_PROFILES
			alt_profile_value = optarg; break;
#else
			fprintf(stderr, "%s: profiles: %s\n", PROGRAM_NAME, NOT_AVAILABLE);
			exit(EXIT_FAILURE);
#endif /* !_NO_PROFILES */
		case 'r': xargs.refresh_on_empty_line = 0; break;
		case 's': conf.splash_screen = xargs.splash = 1; break;
		case 'S': xargs.stealth_mode = 1; break;
		case 't': xargs.disk_usage_analyzer = 1; break;
		case 'v': printf("%s\n", VERSION); exit(EXIT_SUCCESS);

		case 'w': {
			if (!is_number(optarg))
				break;
			int iopt = atoi(optarg);
			if (iopt >= 0 && iopt <= MAX_WS)
				cur_ws = iopt - 1;
		} break;

		case 'x': conf.ext_cmd_ok = xargs.ext = 0; break;
		case 'y': conf.light_mode = xargs.light = 1; break;
		case 'z': set_sort(optarg); break;

		case '?': /* If some unrecognized option was found... */
			/* Options that require an argument */
			/* Short options */
			switch (optopt) {
			case 'b': /* fallthrough */
			case 'c': /* fallthrough */
#ifdef RUN_CMD
			case 'C': /* fallthrough */
#endif
			case 'D': /* fallthrough */
			case 'k': /* fallthrough */
			case 'p': /* fallthrough */
			case 'P': /* fallthrough */
			case 'w': /* fallthrough */
			case 'z': /* fallthrough */
			/* Long options */
			case 208: /* fallthrough */
			case 214: /* fallthrough */
			case 215: /* fallthrough */
			case 219: /* fallthrough */
			case 226: /* fallthrough */
			case 231: /* fallthrough */
			case 252: /* fallthrough */
			case 256: /* fallthrough */
			case 262: /* fallthrough */
			case 265: /* fallthrough */
			case 266: /* fallthrough */
			case 267: err_arg_required(argv[optind - 1]); break;

			default: break;
			}

			fprintf(stderr, _("%s: '%s': Unrecognized option\n"
				"Try '%s --help' for more information.\n"),
				PROGRAM_NAME, argv[optind - 1], PNL);
			exit(EXIT_FAILURE);
		default: break;
		}
	}

	if (open_prev_mode != 0) {
		open_preview_file(open_prev_file, open_prev_mode);
		exit(EXIT_SUCCESS); /* Never reached */
	}

	char *_path = (char *)NULL;
	if (argv[optind]) /* Starting path passed as positional parameter */
		_path = resolve_starting_path(argv[optind]);
	else if (path_value) /* Starting path passed via -p */
		_path = resolve_starting_path(path_value);

	if (_path) {
		_set_starting_path(_path);
		free(_path);
	}

	if (bm_value)
		set_alt_bm_file(bm_value);

	if (virtual_dir_value) {
		stdin_tmp_dir = savestring(virtual_dir_value, strlen(virtual_dir_value));
		setenv("CLIFM_VIRTUAL_DIR", stdin_tmp_dir, 1);
	}

	if (alt_dir_value)
		set_alt_config_dir(alt_dir_value);

	if (kbinds_value)
		set_alt_kbinds_file(kbinds_value);

	if (xargs.config == 1 && config_value)
		set_alt_config_file(config_value);

#ifndef _NO_PROFILES
	if (alt_profile_value) {
		free(alt_profile);
		if (validate_profile_name(alt_profile_value) == EXIT_SUCCESS) {
			alt_profile = savestring(alt_profile_value, strlen(alt_profile_value));
		} else {
			fprintf(stderr, _("%s: %s: Invalid profile name\n"), PROGRAM_NAME,
				alt_profile_value);
			exit(EXIT_FAILURE);
		}
	}
#endif
}
