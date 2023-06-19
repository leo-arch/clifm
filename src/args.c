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

/* Macros to be able to consult the value of a macro string */
#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)

#ifdef RUN_CMD
# define OPTSTRING "+:aAb:c:C:D:eEfFgGhHiIk:lLmoOp:P:rsStvw:xyz:"
#else
# define OPTSTRING "+:aAb:c:D:eEfFgGhHiIk:lLmoOp:P:rsStvw:xyz:"
#endif /* RUN_CMD */

/* Macros for long options. Let's use values >= 200 to avoid conflicts
 * with short options (a-Z == 65-122). */
#define LOPT_NO_CD_AUTO             200
#define LOPT_NO_OPEN_AUTO           201
#define LOPT_NO_RESTORE_LAST_PATH   202
#define LOPT_NO_TIPS                203
#define LOPT_DISK_USAGE             204
#define LOPT_NO_CLASSIFY            205
#define LOPT_SHARE_SELBOX           206
#define LOPT_RL_VI_MODE             207
#define LOPT_MAX_DIRHIST            208
#define LOPT_SORT_REVERSE           209
#define LOPT_NO_FILES_COUNTER       210
#define LOPT_NO_WELCOME_MESSAGE     211
#define LOPT_NO_CLEAR_SCREEN        212
//#define LOPT_ENABLE_LOGS            213
#define LOPT_MAX_PATH               214
#define LOPT_OPENER                 215
#define LOPT_ONLY_DIRS              217
#define LOPT_LIST_AND_QUIT          218
#define LOPT_COLOR_SCHEME           219
#define LOPT_CD_ON_QUIT             220
#define LOPT_NO_DIR_JUMPER          221
#define LOPT_ICONS                  222
#define LOPT_ICONS_USE_FILE_COLOR   223
#define LOPT_NO_COLUMNS             224
#define LOPT_NO_COLORS              225
#define LOPT_MAX_FILES              226
#define LOPT_TRASH_AS_RM            227
#define LOPT_CASE_SENS_DIRJUMP      228
#define LOPT_CASE_SENS_PATH_COMP    229
#define LOPT_CWD_IN_TITLE           230
#define LOPT_OPEN                   231
#define LOPT_PREVIEW                231 /* Same value as LOPT_OPEN is intended */
#define LOPT_PRINT_SEL              232
#define LOPT_NO_SUGGESTIONS         233
//#define LOPT_AUTOJUMP               234
#define LOPT_NO_HIGHLIGHT           235
#define LOPT_NO_FILE_CAP            236
#define LOPT_NO_FILE_EXT            237
#define LOPT_NO_FOLLOW_SYMLINKS     238
#define LOPT_INT_VARS               240
#define LOPT_STDTAB                 241
#define LOPT_NO_WARNING_PROMPT      242
#define LOPT_MNT_UDISKS2            243
#define LOPT_SECURE_ENV             244
#define LOPT_SECURE_ENV_FULL        245
#define LOPT_SECURE_CMDS            246
#define LOPT_FULL_DIR_SIZE          247
#define LOPT_NO_APPARENT_SIZE       248
#define LOPT_NO_HISTORY             249
#define LOPT_FZYTAB                 250 /* legacy: replaced by fnftab */
#define LOPT_NO_REFRESH_ON_RESIZE   251
#define LOPT_BELL                   252
#define LOPT_FUZZY_MATCHING         253
#define LOPT_SMENUTAB               254
#define LOPT_VIRTUAL_DIR_FULL_PATHS 255
#define LOPT_VIRTUAL_DIR            256
#define LOPT_DESKTOP_NOTIFICATIONS  257
#define LOPT_VT100                  258
#define LOPT_NO_FZFPREVIEW          259
#define LOPT_FZFPREVIEW             260 /* legacy: preview is now default */
#define LOPT_FZFPREVIEW_HIDDEN      261
#define LOPT_SHOTGUN_FILE           262
#define LOPT_FZFTAB                 263
#define LOPT_SI                     264
#define LOPT_DATA_DIR               265
#define LOPT_FUZZY_ALGO             266
#define LOPT_SEL_FILE               267
#define LOPT_NO_TRIM_NAMES          268
#define LOPT_NO_BOLD                269
#define LOPT_FNFTAB                 270

/* Link long (--option) and short options (-o) for the getopt_long function. */
static const struct option longopts[] = {
	{"no-hidden", no_argument, 0, 'a'},
	{"show-hidden", no_argument, 0, 'A'},
	{"bookmarks-file", required_argument, 0, 'b'},
	{"config-file", required_argument, 0, 'c'},

#ifdef RUN_CMD
	{"cmd", required_argument, 0, 'C'},
#endif /* RUN_CMD */

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

	/* Only-long options */
//		{"autojump", no_argument, 0, LOPT_NO_AUTOJUMP},
	{"bell", required_argument, 0, LOPT_BELL},
	{"case-sens-dirjump", no_argument, 0, LOPT_CASE_SENS_DIRJUMP},
	{"case-sens-path-comp", no_argument, 0, LOPT_CASE_SENS_PATH_COMP},
	{"cd-on-quit", no_argument, 0, LOPT_CD_ON_QUIT},
	{"color-scheme", required_argument, 0, LOPT_COLOR_SCHEME},
	{"cwd-in-title", no_argument, 0, LOPT_CWD_IN_TITLE},
	{"data-dir", required_argument, 0, LOPT_DATA_DIR},
	{"desktop-notifications", no_argument, 0, LOPT_DESKTOP_NOTIFICATIONS},
	{"disk-usage", no_argument, 0, LOPT_DISK_USAGE},
//		{"enable-logs", no_argument, 0, LOPT_ENABLE_LOGS},
	{"fnftab", no_argument, 0, LOPT_FNFTAB},
	{"full-dir-size", no_argument, 0, LOPT_FULL_DIR_SIZE},
	{"fuzzy-matching", no_argument, 0, LOPT_FUZZY_MATCHING},
	{"fuzzy-algo", required_argument, 0, LOPT_FUZZY_ALGO},
	{"fzfpreview", no_argument, 0, LOPT_FZFPREVIEW}, // legacy: is default now
	{"fzfpreview-hidden", no_argument, 0, LOPT_FZFPREVIEW_HIDDEN},
	{"fzftab", no_argument, 0, LOPT_FZFTAB},
	{"fzytab", no_argument, 0, LOPT_FZYTAB}, // legacy: replaced by fnftab
	{"icons", no_argument, 0, LOPT_ICONS},
	{"icons-use-file-color", no_argument, 0, LOPT_ICONS_USE_FILE_COLOR},
	{"int-vars", no_argument, 0, LOPT_INT_VARS},
	{"list-and-quit", no_argument, 0, LOPT_LIST_AND_QUIT},
	{"max-dirhist", required_argument, 0, LOPT_MAX_DIRHIST},
	{"max-files", required_argument, 0, LOPT_MAX_FILES},
	{"max-path", required_argument, 0, LOPT_MAX_PATH},
	{"mnt-udisks2", no_argument, 0, LOPT_MNT_UDISKS2},
	{"no-apparent-size", no_argument, 0, LOPT_NO_APPARENT_SIZE},
	{"no-bold", no_argument, 0, LOPT_NO_BOLD},
	{"no-cd-auto", no_argument, 0, LOPT_NO_CD_AUTO},
	{"no-classify", no_argument, 0, LOPT_NO_CLASSIFY},
	{"no-clear-screen", no_argument, 0, LOPT_NO_CLEAR_SCREEN},
	{"no-colors", no_argument, 0, LOPT_NO_COLORS},
	{"no-columns", no_argument, 0, LOPT_NO_COLUMNS},
	{"no-dir-jumper", no_argument, 0, LOPT_NO_DIR_JUMPER},
	{"no-file-cap", no_argument, 0, LOPT_NO_FILE_CAP},
	{"no-file-ext", no_argument, 0, LOPT_NO_FILE_EXT},
	{"no-follow-symlinks", no_argument, 0, LOPT_NO_FOLLOW_SYMLINKS},
	{"no-highlight", no_argument, 0, LOPT_NO_HIGHLIGHT},
	{"no-files-counter", no_argument, 0, LOPT_NO_FILES_COUNTER},
	{"no-fzfpreview", no_argument, 0, LOPT_NO_FZFPREVIEW},
	{"no-history", no_argument, 0, LOPT_NO_HISTORY},
	{"no-open-auto", no_argument, 0, LOPT_NO_OPEN_AUTO},
	{"no-refresh-on-resize", no_argument, 0, LOPT_NO_REFRESH_ON_RESIZE},
	{"no-restore-last-path", no_argument, 0, LOPT_NO_RESTORE_LAST_PATH},
	{"no-suggestions", no_argument, 0, LOPT_NO_SUGGESTIONS},
	{"no-tips", no_argument, 0, LOPT_NO_TIPS},
	{"no-trim-names", no_argument, 0, LOPT_NO_TRIM_NAMES},
	{"no-warning-prompt", no_argument, 0, LOPT_NO_WARNING_PROMPT},
	{"no-welcome-message", no_argument, 0, LOPT_NO_WELCOME_MESSAGE},
	{"only-dirs", no_argument, 0, LOPT_ONLY_DIRS},
	{"open", required_argument, 0, LOPT_OPEN},
	{"opener", required_argument, 0, LOPT_OPENER},
	{"preview", required_argument, 0, LOPT_PREVIEW},
	{"print-sel", no_argument, 0, LOPT_PRINT_SEL},
	{"rl-vi-mode", no_argument, 0, LOPT_RL_VI_MODE},
	{"share-selbox", no_argument, 0, LOPT_SHARE_SELBOX},
	{"sort-reverse", no_argument, 0, LOPT_SORT_REVERSE},
	{"trash-as-rm", no_argument, 0, LOPT_TRASH_AS_RM},
	{"secure-cmds", no_argument, 0, LOPT_SECURE_CMDS},
	{"secure-env", no_argument, 0, LOPT_SECURE_ENV},
	{"secure-env-full", no_argument, 0, LOPT_SECURE_ENV_FULL},
	{"sel-file", required_argument, 0, LOPT_SEL_FILE},
	{"shotgun-file", required_argument, 0, LOPT_SHOTGUN_FILE},
	{"si", no_argument, 0, LOPT_SI},
	{"smenutab", no_argument, 0, LOPT_SMENUTAB},
	{"stdtab", no_argument, 0, LOPT_STDTAB},
	{"virtual-dir", required_argument, 0, LOPT_VIRTUAL_DIR},
	{"virtual-dir-full-paths", no_argument, 0, LOPT_VIRTUAL_DIR_FULL_PATHS},
	{"vt100", no_argument, 0, LOPT_VT100},
    {0, 0, 0, 0}
};

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
#endif /* __HAIKU__ */
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
#endif /* !_NO_LIRA */

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
err_arg_required(const char *arg)
{
	fprintf(stderr, _("%s: '%s': Option requires an argument\n"
		"Try '%s --help' for more information.\n"), PROGRAM_NAME,
		arg, PROGRAM_NAME);
	exit(EXIT_FAILURE);
}

static void
err_invalid_opt(const char *arg)
{
	fprintf(stderr, _("%s: '%s': Unrecognized option\n"
		"Try '%s --help' for more information.\n"),
		PROGRAM_NAME, arg, PROGRAM_NAME);
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
	 * via the '?' and ':' cases in the switch statement. */
	opterr = optind = 0;
	int optc;
	int open_prev_mode = 0;

	/* Variables to store arguments to options. */
	char *path_value = (char *)NULL,
#ifndef _NO_PROFILES
		*alt_profile_value = (char *)NULL,
#endif /* !_NO_PROFILES */
		*alt_dir_value = (char *)NULL,
		*config_value = (char *)NULL,
		*kbinds_value = (char *)NULL,
		*virtual_dir_value = (char *)NULL,
		*bm_value = (char *)NULL,
		*open_prev_file = (char *)NULL;

	while ((optc = getopt_long(argc, argv, OPTSTRING,
		longopts, (int *)0)) != EOF) {

		switch (optc) {
		/* Short options */
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
#endif /* RUN_CMD */

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

		/* Only-long options */
		case LOPT_BELL: {
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

		case LOPT_CASE_SENS_DIRJUMP:
			xargs.case_sens_dirjump = conf.case_sens_dirjump = 1; break;
		case LOPT_CASE_SENS_PATH_COMP:
			xargs.case_sens_path_comp = conf.case_sens_path_comp = 1; break;
		case LOPT_CD_ON_QUIT: xargs.cd_on_quit = conf.cd_on_quit = 1; break;

		case LOPT_COLOR_SCHEME:
			if (!optarg || !*optarg || *optarg == '-')
				err_arg_required("--color-scheme");

			conf.usr_cscheme = savestring(optarg, strlen(optarg));
			break;

		case LOPT_CWD_IN_TITLE: xargs.cwd_in_title = 1; break;

		case LOPT_DATA_DIR:
			if (!optarg || !*optarg || *optarg == '-')
				err_arg_required("--data-dir");

			get_data_dir_from_path(optarg);
			break;

		case LOPT_DESKTOP_NOTIFICATIONS:
			xargs.desktop_notifications = conf.desktop_notifications = 1;
			break;
		case LOPT_DISK_USAGE: xargs.disk_usage = conf.disk_usage = 1; break;

		case LOPT_FNFTAB:
#ifndef _NO_FZF
			xargs.fnftab = 1; fzftab = 1; tabmode = FNF_TAB; break;
#else
			fprintf(stderr, _("%s: fnf-tab: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* _NO_FZF */

		case LOPT_FULL_DIR_SIZE:
			xargs.full_dir_size = conf.full_dir_size = 1; break;

		case LOPT_FUZZY_ALGO: {
			int a = optarg ? atoi(optarg) : -1;
			if (!optarg || a < 1 || a > FUZZY_ALGO_MAX) {
				fprintf(stderr, "%s: fuzzy-algo: Valid arguments are either 1 "
					"or 2\n", PROGRAM_NAME);
				exit(EXIT_FAILURE);
			}
			xargs.fuzzy_match_algo = conf.fuzzy_match_algo = a;
			}
			break;

		case LOPT_FUZZY_MATCHING:
			xargs.fuzzy_match = conf.fuzzy_match = 1; break;

		case LOPT_FZFPREVIEW: /* fallthrough */
		case LOPT_FZFPREVIEW_HIDDEN:
#ifndef _NO_FZF
			xargs.fzf_preview = 1;
			conf.fzf_preview = optc == LOPT_FZFPREVIEW ? 1 : 2;
			xargs.fzftab = fzftab = 1; tabmode = FZF_TAB;
#else
			fprintf(stderr, _("%s: fzf-preview: %s\n"),
				PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_FZF */
			break;

		case LOPT_FZFTAB:
#ifndef _NO_FZF
			xargs.fzftab = fzftab = 1; tabmode = FZF_TAB; break;
#else
			fprintf(stderr, _("%s: fzf-tab: %s\n"),
				PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_FZF */

		case LOPT_FZYTAB:
			fprintf(stderr, "%s: --fzytab: We have migrated to 'fnf'.\n"
				"Install 'fnf' (https://github.com/leo-arch/fnf) and then "
				"use --fnftab instead\n", PROGRAM_NAME);
			exit(EXIT_FAILURE);

#ifndef _NO_ICONS
		case LOPT_ICONS: xargs.icons = conf.icons = 1; break;
		case LOPT_ICONS_USE_FILE_COLOR:
			xargs.icons = conf.icons = xargs.icons_use_file_color = 1; break;
#else
		case LOPT_ICONS: /* fallthrough */
		case LOPT_ICONS_USE_FILE_COLOR:
			fprintf(stderr, _("%s: icons: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_ICONS */

		case LOPT_INT_VARS: xargs.int_vars = int_vars = 1; break;
		case LOPT_LIST_AND_QUIT: xargs.list_and_quit = 1; break;

		case LOPT_MAX_DIRHIST: {
			if (!is_number(optarg))
				break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0 && opt_int <= INT_MAX)
				xargs.max_dirhist = conf.max_dirhist = opt_int;
		} break;

		case LOPT_MAX_FILES: {
			if (!is_number(optarg))
				break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0 && opt_int <= INT_MAX)
				xargs.max_files = max_files = opt_int;
		} break;

		case LOPT_MAX_PATH: {
			if (!is_number(optarg))
				break;
			int opt_int = atoi(optarg);
			if (opt_int >= 0 && opt_int <= INT_MAX)
				xargs.max_path = conf.max_path = opt_int;
		} break;

		case LOPT_MNT_UDISKS2: xargs.mount_cmd = MNT_UDISKS2; break;
		case LOPT_NO_APPARENT_SIZE:
			xargs.apparent_size = conf.apparent_size = 0; break;
		case LOPT_NO_BOLD: xargs.no_bold = 1; break;
		case LOPT_NO_CD_AUTO: xargs.autocd = conf.autocd = 0; break;
		case LOPT_NO_CLASSIFY: xargs.classify = conf.classify = 0; break;
		case LOPT_NO_CLEAR_SCREEN:
			xargs.clear_screen = conf.clear_screen = 0; break;

		case LOPT_NO_COLORS:
			xargs.colorize = conf.colorize = 0;
#ifndef _NO_HIGHLIGHT
			xargs.highlight = conf.highlight = 0;
#endif /* !_NO_HIGHLIGHT */
			break;

		case LOPT_NO_COLUMNS: xargs.columns = conf.columned = 0; break;
		case LOPT_NO_DIR_JUMPER: xargs.no_dirjump = 1; break;
		case LOPT_NO_FILE_CAP: xargs.check_cap = check_cap = 0; break;
		case LOPT_NO_FILE_EXT: xargs.check_ext = check_ext = 0; break;
		case LOPT_NO_FILES_COUNTER:
			xargs.files_counter = conf.files_counter = 0; break;
		case LOPT_NO_FOLLOW_SYMLINKS:
			xargs.follow_symlinks = follow_symlinks = 0; break;
		case LOPT_NO_FZFPREVIEW:
			xargs.fzf_preview = conf.fzf_preview = 0; break;
		case LOPT_NO_HIGHLIGHT:
#ifndef _NO_HIGHLIGHT
			xargs.highlight = conf.highlight = 0;
#endif /* !_NO_HIGHLIGHT */
			break;

		case LOPT_NO_HISTORY: xargs.history = 0; break;
		case LOPT_NO_OPEN_AUTO: xargs.auto_open = conf.auto_open = 0; break;
		case LOPT_NO_REFRESH_ON_RESIZE: xargs.refresh_on_resize = 0; break;
		case LOPT_NO_RESTORE_LAST_PATH:
			xargs.restore_last_path = conf.restore_last_path = 0; break;

		case LOPT_NO_SUGGESTIONS:
#ifndef _NO_SUGGESTIONS
			xargs.suggestions = conf.suggestions = 0;
#endif /* !_NO_SUGGESTIONS */
			break;

		case LOPT_NO_TIPS: xargs.tips = conf.tips = 0; break;
		case LOPT_NO_TRIM_NAMES: xargs.trim_names = conf.trim_names = 0; break;
		case LOPT_NO_WARNING_PROMPT:
			xargs.warning_prompt = conf.warning_prompt = 0; break;
		case LOPT_NO_WELCOME_MESSAGE:
			xargs.welcome_message = conf.welcome_message = 0; break;

		case LOPT_ONLY_DIRS: xargs.only_dirs = conf.only_dirs = 1; break;

		case LOPT_OPEN: { /* --open or --preview */
			open_prev_file = optarg;
			int n = *argv[optind - 1] == '-' ? 1 : 2;
			if (*(argv[optind - n] + 2) == 'p')
				open_prev_mode = PREVIEW_FILE; /* --preview */
			else
				open_prev_mode = OPEN_FILE; /* --open */
		} break;

		case LOPT_OPENER:
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

		case LOPT_PRINT_SEL: xargs.printsel = conf.print_selfiles = 1; break;
		case LOPT_RL_VI_MODE: xargs.rl_vi_mode = 1; break;
		case LOPT_SECURE_CMDS: xargs.secure_cmds = 1; break;

		case LOPT_SECURE_ENV:
			xargs.secure_env = 1;
			xsecure_env(SECURE_ENV_IMPORT);
			break;

		case LOPT_SECURE_ENV_FULL:
			xargs.secure_env_full = 1;
			xsecure_env(SECURE_ENV_FULL);
			break;

		case LOPT_SEL_FILE: set_custom_selfile(optarg); break;
		case LOPT_SHARE_SELBOX:
			xargs.share_selbox = conf.share_selbox = 1; break;

		case LOPT_SHOTGUN_FILE:
			if (!optarg || !*optarg || *optarg == '-')
				err_arg_required("--shotgun-file");

			alt_preview_file = stat_file(optarg);
			break;

		case LOPT_SI: xargs.si = 1; break;

		case LOPT_SMENUTAB:
#ifndef _NO_FZF
			xargs.smenutab = 1; fzftab = 1; tabmode = SMENU_TAB; break;
#else
			fprintf(stderr, _("%s: smenu-tab: %s\n"),
				PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_FZF */

		case LOPT_SORT_REVERSE:
			xargs.sort_reverse = conf.sort_reverse = 1; break;

		case LOPT_STDTAB:
#ifndef _NO_FZF
			xargs.fzftab = 0;
#endif /* !_NO_FZF */
			fzftab = 0; tabmode = STD_TAB;
			break;

		case LOPT_TRASH_AS_RM:
#ifndef _NO_TRASH
			xargs.trasrm = conf.tr_as_rm = 1; break;
#else
			fprintf(stderr, _("%s: trash: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_TRASH */

		case LOPT_VIRTUAL_DIR:
			if (optarg && *optarg && *optarg == '/') {
				virtual_dir_value = optarg;
			} else {
				fprintf(stderr, "%s: '--virtual-dir': Absolute path "
					"is required as argument\n", PROGRAM_NAME);
				exit(EXIT_FAILURE);
			}
			break;

		case LOPT_VIRTUAL_DIR_FULL_PATHS:
			xargs.virtual_dir_full_paths = 1; break;

		case LOPT_VT100:
			xargs.vt100 = 1;
			xargs.clear_screen = conf.clear_screen = 0;
			fzftab = 0; tabmode = STD_TAB;
			break;

		/* Handle error */
		case ':': err_arg_required(argv[optind - 1]); exit(EXIT_FAILURE);
		case '?': err_invalid_opt(argv[optind - 1]); exit(EXIT_FAILURE);

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
#endif /* !_NO_PROFILES */
}
