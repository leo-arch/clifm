/* args.c -- Handle command line arguments */

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

#include "helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h> /* getpwuid() */
#include <errno.h>
#include <readline/tilde.h> /* tilde_expand() */
#include <string.h>

#include "aux.h"
#include "checks.h"
#include "config.h" /* set_time_style */
#include "file_operations.h"
#include "init.h"
#include "mime.h"
#include "misc.h"
#include "navigation.h"
#include "profiles.h"
#include "sanitize.h"
#include "spawn.h"
#include "strings.h"

#if defined(_NO_PROFILES) || defined(_NO_FZF) || defined(_NO_ICONS) \
|| defined(_NO_TRASH) || defined(_BE_POSIX) || defined(_NO_LIRA)
# include "messages.h"
#endif /* _NO_PROFILES || _NO_FZF || _NO_ICONS || _NO_TRASH */

#ifndef _NO_LIRA
# define PREVIEW_FILE 1
# define OPEN_FILE    2
#endif /* _NO_LIRA */

/* Macros to be able to consult the value of a macro string */
#ifdef CLIFM_DATADIR
# define STRINGIZE_(x) #x
# define STRINGIZE(x) STRINGIZE_(x)
#endif /* CLIFM_DATADIR */

#ifdef _BE_POSIX
# define OPTSTRING ":aAb:B:c:CdDeEfFgGhHiI:j:J:k:lLmMnNo:O:p:P:qQrRsSt:TuvV:w:WxXyYz:Z"
#else
# ifdef RUN_CMD
#  define OPTSTRING "+:aAb:c:C:D:eEfFgGhHiIk:lLmoOp:P:rsStT:vw:xyz:"
# else
#  define OPTSTRING "+:aAb:c:D:eEfFgGhHiIk:lLmoOp:P:rsStT:vw:xyz:"
# endif /* RUN_CMD */
#endif /* _BE_POSIX */

#ifndef _BE_POSIX
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

#define LOPT_STAT                   271
#define LOPT_STAT_FULL              272
#define LOPT_READONLY               273
#define LOPT_LSCOLORS               274
#define LOPT_PROP_FIELDS            275
#define LOPT_TIME_STYLE             276
#define LOPT_PTIME_STYLE            277
#define LOPT_COLOR_LNK_AS_TARGET    278
#define LOPT_PAGER_VIEW             279

/* Link long (--option) and short options (-o) for the getopt_long function. */
static struct option const longopts[] = {
	{"show-hidden", no_argument, 0, 'a'},
	{"no-hidden", no_argument, 0, 'A'},
	{"bookmarks-file", required_argument, 0, 'b'},
	{"config-file", required_argument, 0, 'c'},

#ifdef RUN_CMD
	{"cmd", required_argument, 0, 'C'},
#endif /* RUN_CMD */

	{"config-dir", required_argument, 0, 'D'},
	{"no-eln", no_argument, 0, 'e'},
	{"eln-use-workspace-color", no_argument, 0, 'E'},
	{"dirs-first", no_argument, 0, 'f'},
	{"no-dirs-first", no_argument, 0, 'F'},
	{"pager", no_argument, 0, 'g'},
	{"no-pager", no_argument, 0, 'G'},
	{"help", no_argument, 0, 'h'},
	{"horizontal-list", no_argument, 0, 'H'},
	{"no-case-sensitive", no_argument, 0, 'i'},
	{"case-sensitive", no_argument, 0, 'I'},
	{"keybindings-file", required_argument, 0, 'k'},
	{"long-view", no_argument, 0, 'l'},
	{"follow-symlinks-long", no_argument, 0, 'L'},
	{"dirhist-map", no_argument, 0, 'm'},
	{"autols", no_argument, 0, 'o'},
	{"no-autols", no_argument, 0, 'O'},
	{"path", required_argument, 0, 'p'},
	{"profile", required_argument, 0, 'P'},
	{"no-refresh-on-emtpy-line", no_argument, 0, 'r'},
	{"splash", no_argument, 0, 's'},
	{"stealth-mode", no_argument, 0, 'S'},
	{"disk-usage-analyzer", no_argument, 0, 't'},
	{"trash-dir", required_argument, 0, 'T'},
	{"version", no_argument, 0, 'v'},
	{"workspace", required_argument, 0, 'w'},
	{"no-ext-cmds", no_argument, 0, 'x'},
	{"light-mode", no_argument, 0, 'y'},
	{"sort", required_argument, 0, 'z'},

	/* Only-long options */
	{"bell", required_argument, 0, LOPT_BELL},
	{"case-sens-dirjump", no_argument, 0, LOPT_CASE_SENS_DIRJUMP},
	{"case-sens-path-comp", no_argument, 0, LOPT_CASE_SENS_PATH_COMP},
	{"cd-on-quit", no_argument, 0, LOPT_CD_ON_QUIT},
	{"color-scheme", required_argument, 0, LOPT_COLOR_SCHEME},
	{"color-links-as-target", no_argument, 0, LOPT_COLOR_LNK_AS_TARGET},
	{"cwd-in-title", no_argument, 0, LOPT_CWD_IN_TITLE},
	{"data-dir", required_argument, 0, LOPT_DATA_DIR},
	{"desktop-notifications", no_argument, 0, LOPT_DESKTOP_NOTIFICATIONS},
	{"disk-usage", no_argument, 0, LOPT_DISK_USAGE},
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
	{"ls", no_argument, 0, LOPT_LIST_AND_QUIT},
	{"lscolors", no_argument, 0, LOPT_LSCOLORS},
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
	{"pager-view", required_argument, 0, LOPT_PAGER_VIEW},
	{"physical-size", no_argument, 0, LOPT_NO_APPARENT_SIZE},
	{"ptime-style", required_argument, 0, LOPT_PTIME_STYLE},
	{"preview", required_argument, 0, LOPT_PREVIEW},
	{"print-sel", no_argument, 0, LOPT_PRINT_SEL},
	{"prop-fields", required_argument, 0, LOPT_PROP_FIELDS},
	{"readonly", no_argument, 0, LOPT_READONLY},
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
	{"stat", required_argument, 0, LOPT_STAT},
	{"stat-full", required_argument, 0, LOPT_STAT_FULL},
	{"stdtab", no_argument, 0, LOPT_STDTAB},
	{"time-style", required_argument, 0, LOPT_TIME_STYLE},
	{"virtual-dir", required_argument, 0, LOPT_VIRTUAL_DIR},
	{"virtual-dir-full-paths", no_argument, 0, LOPT_VIRTUAL_DIR_FULL_PATHS},
	{"vt100", no_argument, 0, LOPT_VT100},
    {0, 0, 0, 0}
};
#endif /* !_BE_POSIX */

__attribute__ ((noreturn))
static void
err_arg_required(const char *arg)
{
#ifdef _BE_POSIX
	char *optname = "-h";
#else
	char *optname = "--help";
#endif /* _BE_POSIX */
	fprintf(stderr, _("%s: '%s': Option requires an argument\n"
		"Try '%s %s' for more information.\n"), PROGRAM_NAME,
		arg, PROGRAM_NAME, optname);
	exit(EXIT_FAILURE);
}

#ifndef _BE_POSIX
__attribute__ ((noreturn))
static void
err_invalid_opt(const char *arg)
{
	fprintf(stderr, _("%s: '%s': Unrecognized option\n"
		"Try '%s --help' for more information.\n"),
		PROGRAM_NAME, arg, PROGRAM_NAME);
	exit(EXIT_FAILURE);
}
#endif

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

	char p[PATH_MAX + 1] = "";
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

	xerror("%s: '/': %s\n", PROGRAM_NAME, strerror(errno));
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
		err('w', PRINT_PROMPT, _("%s: '%zu': Invalid workspace."
			"\nFalling back to workspace %zu\n"),
			PROGRAM_NAME, cur_ws, cur_ws + 1);
	}

	prev_ws = cur_ws;
	set_cur_workspace();

	/* Make path the CWD. */
	const int ret = xchdir(workspaces[cur_ws].path, NO_TITLE);

	char tmp[PATH_MAX + 1] = "";
	char *pwd = get_cwd(tmp, sizeof(tmp), 0);

	/* If chdir() fails, set path to PWD, list files and print the
	 * error message. If no access to PWD either, exit. */
	if (ret == -1) {
		err('e', PRINT_PROMPT, "%s: chdir: '%s': %s\n", PROGRAM_NAME,
		    workspaces[cur_ws].path, strerror(errno));

		if (!pwd || !*pwd) {
			err(0, NOPRINT_PROMPT, _("%s: Fatal error! Failure "
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
	return FUNC_SUCCESS;
}

static int
try_standard_data_dirs(void)
{
	char home_local[PATH_MAX + 1];
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
		snprintf(tmp, sizeof(tmp), "%s/%s/%src", data_dirs[i],
			PROGRAM_NAME, PROGRAM_NAME);

		if (stat(tmp, &a) == -1 || !S_ISREG(a.st_mode))
			continue;

		data_dir = savestring(data_dirs[i], strlen(data_dirs[i]));
		return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
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

	char p[PATH_MAX + 1];

	/* Try DIR/share/clifm/clifmrc */
	snprintf(p, sizeof(p), "%s/share/%s/%src", dir, PROGRAM_NAME, PROGRAM_NAME);
	if (stat(p, &a) != -1 && S_ISREG(a.st_mode)) {
		const size_t len = strlen(dir) + 7;
		char *q = xnmalloc(len, sizeof(char));
		snprintf(q, len, "%s/share", dir);
		return q;
	}

	/* Try DIR/clifm/clifmrc */
	snprintf(p, sizeof(p), "%s/%s/%src", dir, PROGRAM_NAME, PROGRAM_NAME);
	if (stat(p, &a) != -1 && S_ISREG(a.st_mode))
		return savestring(dir, strlen(dir));

	return (char *)NULL;
}

static char *
resolve_absolute_path(const char *s)
{
	if (!s || !*s)
		return (char *)NULL;

	char *p = strrchr(s, '/');
	char *t = (char *)NULL;

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
get_data_dir_from_path(char *arg)
{
	if (!arg || !*arg)
		return FUNC_FAILURE;

	char *datadir = (char *)NULL;
	char *name = *arg == '~' ? tilde_expand(arg) : arg;

	if (*name == '/' && (datadir = resolve_absolute_path(name)))
		goto END;

	if (strchr(name, '/') && (datadir = resolve_relative_path(name)))
		goto END;

	datadir = resolve_basename(name);

END:
	if (name != arg)
		free(name);

	if (!datadir)
		return FUNC_FAILURE;

	data_dir = datadir;
	return FUNC_SUCCESS;
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
	char p[PATH_MAX + 1];
	snprintf(p, sizeof(p), "%s/%s/%src", STRINGIZE(CLIFM_DATADIR),
		PROGRAM_NAME, PROGRAM_NAME);

	if (stat(p, &a) != -1 && S_ISREG(a.st_mode)) {
		data_dir = savestring(STRINGIZE(CLIFM_DATADIR),
			strlen(STRINGIZE(CLIFM_DATADIR)));
		return;
	}
#endif /* CLIFM_DATADIR */

	if (try_standard_data_dirs() == FUNC_SUCCESS)
		return;

	if (get_data_dir_from_path(argv_bk[0]) == FUNC_SUCCESS)
		return;

	err('w', PRINT_PROMPT, _("%s: No data directory found. Data files, "
		"such as plugins and color schemes, may not be available.\n"),
		PROGRAM_NAME);
}

static char *
get_home_sec_env(void)
{
	struct passwd *pw = (struct passwd *)NULL;

	uid_t u = geteuid();
	pw = getpwuid(u);
	if (!pw) {
		err('e', PRINT_PROMPT, "%s: getpwuid: %s\n",
			PROGRAM_NAME, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return (pw->pw_dir);
}

/* Opener function: open FILENAME and exit */
__attribute__ ((noreturn))
static void
open_reg_exit(char *filename, const int url, const int preview)
{
	char *homedir = (xargs.secure_env == 1 || xargs.secure_env_full == 1)
		? get_home_sec_env() : getenv("HOME");
	if (!homedir) {
		xerror(_("%s: Cannot retrieve the home directory\n"), PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	tmp_dir = savestring(P_tmpdir, P_tmpdir_len);

	if (alt_preview_file && preview == 1) {
		mime_file = savestring(alt_preview_file, strlen(alt_preview_file));
	} else {
		const size_t mime_file_len = strlen(homedir)
			+ (alt_profile ? strlen(alt_profile) : 7) + 40;

		mime_file = xnmalloc(mime_file_len, sizeof(char));
		snprintf(mime_file, mime_file_len,
			"%s/.config/clifm/profiles/%s/%s.clifm",
			homedir, alt_profile ? alt_profile : "default",
			preview == 1 ? "preview" : "mimelist");
	}

	if (path_n == 0)
		path_n = get_path_env(0);

#ifndef _NO_LIRA
	if (url == 1 && mime_open_url(filename) == FUNC_SUCCESS)
		exit(EXIT_SUCCESS);
#else
	UNUSED(url);
#endif /* !_NO_LIRA */

	char *p = (char *)NULL;
	if (*filename == '~')
		p = tilde_expand(filename);

	const int ret = open_file(p ? p : filename);
	free(p);
	exit(ret);
}

static int
set_sort_by_name(const char *name)
{
	size_t i;
	for (i = 0; i <= SORT_TYPES; i++) {
		if (*name == *sort_methods[i].name
		&& strcmp(name, sort_methods[i].name) == 0)
			return sort_methods[i].num;
	}

	fprintf(stderr, _("%s: --sort: '%s': Invalid value\n"
		"Valid values: atime, btime, ctime, mtime, extension, group, "
		"inode, name,\n              none, owner, size, version, "
		"blocks, links.\n"), PROGRAM_NAME, name);
	exit(EXIT_FAILURE);
}

static void
set_sort(const char *arg)
{
	int n = 0;

	if (!is_number(arg))
		n = set_sort_by_name(arg);
	else
		n = atoi(arg);

	if (n < 0 || n > SORT_TYPES) {
		fprintf(stderr, _("%s: --sort: '%s': Valid values are 0-%d\n"),
			PROGRAM_NAME, arg, SORT_TYPES);
		exit(EXIT_FAILURE);
	}

	xargs.sort = conf.sort = n;
}

#ifndef _NO_LIRA
/* Open/preview FILE according to MODE: either PREVIEW_FILE or OPEN_FILE */
__attribute__ ((noreturn))
static void
open_preview_file(char *file, const int mode)
{
	if (xargs.stealth_mode == 1) {
		fprintf(stderr, _("%s: Running in stealth mode. Access to "
			"configuration files is not allowed\n"), PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	char *fpath = file;
	int url = 1;
	const int preview = mode == PREVIEW_FILE ? 1 : 0;

	struct stat attr;
	if (IS_FILE_URI(fpath)) {
		fpath = file + 7;
		if (stat(fpath, &attr) == -1) {
			xerror("%s: '%s': %s\n", PROGRAM_NAME, file, strerror(errno));
			exit(errno);
		}
		url = 0;
		goto RUN;
	}

	if (is_url(fpath) == FUNC_FAILURE) {
		url = 0;
		if (*fpath != '~' && stat(fpath, &attr) == -1) {
			xerror("%s: '%s': %s\n", PROGRAM_NAME, fpath, strerror(errno));
			exit(errno);
		}
	}

RUN:
	xargs.open = preview == 1 ? 0 : 1;
	xargs.preview = preview == 1 ? 1 : 0;
	if (preview == 1)
		clear_term_img();

	open_reg_exit(fpath, url, preview);
}
#endif /* !_NO_LIRA */

static void
set_alt_bm_file(char *file)
{
	if (!file || !*file || *file == '-')
		err_arg_required("-b"); /* noreturn */

	char *p = (char *)NULL;

	if (*file == '~') {
		p = tilde_expand(file);
		file = p;
	}

	alt_bm_file = savestring(file, strlen(file));
	free(p);
}

#ifndef _BE_POSIX
static void
set_vt100(void)
{
	xargs.vt100 = 1;
	xargs.clear_screen = conf.clear_screen = 0;
	fzftab = 0; tabmode = STD_TAB;
}

__attribute__ ((noreturn))
static void
set_fzytab(void)
{
	fprintf(stderr, _("%s: --fzytab: We have migrated to 'fnf'.\n"
		"Install 'fnf' (https://github.com/leo-arch/fnf) and then "
		"use --fnftab instead\n"), PROGRAM_NAME);
	exit(EXIT_FAILURE);
}

static void
set_fzfpreview(const int optc)
{
#if !defined(_NO_FZF) && !defined(_NO_LIRA)
	xargs.fzf_preview = 1;
	conf.fzf_preview = optc == LOPT_FZFPREVIEW ? 1 : 2;
	xargs.fzftab = fzftab = 1; tabmode = FZF_TAB;
#else
	UNUSED(optc);
	fprintf(stderr, "%s: --fzf-preview: %s\n",
		PROGRAM_NAME, _(NOT_AVAILABLE));
	exit(EXIT_FAILURE);
#endif /* !_NO_FZF && !_NO_LIRA */
}

static void
set_datadir(char *opt)
{
	if (!opt || !*opt || *opt == '-')
		err_arg_required("--data-dir"); /* noreturn */

	get_data_dir_from_path(opt);
}

static void
set_fuzzy_algo(const char *opt)
{
	int a = opt ? atoi(opt) : -1;

	if (a < 1 || a > FUZZY_ALGO_MAX) {
		fprintf(stderr, _("%s: '%s': Invalid fuzzy algorithm. Valid "
			"values are either 1 or 2.\n"), PROGRAM_NAME, opt ? opt : "NULL");
		exit(EXIT_FAILURE);
	}

	xargs.fuzzy_match_algo = conf.fuzzy_match_algo = a;
}

static void
set_bell_style(const char *opt)
{
	int a = atoi(opt);

	if (!is_number(opt) || a < 0 || a > 3) {
		fprintf(stderr, _("%s: '%s': Invalid bell style. Valid values "
			"are 0:none, 1:audible, 2:visible (requires readline >= 8.1), "
			"3:flash. Defaults to 'visible', and, if not possible, 'none'.\n"),
			PROGRAM_NAME, opt);
		exit(EXIT_FAILURE);
	}

	xargs.bell_style = a;
}

static void
set_alt_config_dir(char *dir)
{
	if (!dir || !*dir || *dir == '-')
		err_arg_required("--config-dir"); /* noreturn */

	char *dir_exp = (char *)NULL;

	if (*dir == '~') {
		dir_exp = tilde_expand(dir);
		dir = dir_exp;
	}

	int dir_ok = 1;
	struct stat attr;

	if (stat(dir, &attr) == -1) {
		char *tmp_cmd[] = {"mkdir", "-p", "--", dir, NULL};
		const int ret = launch_execv(tmp_cmd, FOREGROUND, E_NOSTDERR);
		if (ret != FUNC_SUCCESS) {
			err('e', PRINT_PROMPT, _("%s: Cannot create directory '%s' "
				"(error %d)\nFalling back to the default configuration "
				"directory\n"), PROGRAM_NAME, dir, ret);
			dir_ok = 0;
		}
	}

	if (access(dir, W_OK) == -1) {
		if (dir_ok == 1) {
			err('e', PRINT_PROMPT, _("%s: '%s': %s\n"
				"Falling back to the default configuration directory\n"),
				PROGRAM_NAME, dir, strerror(errno));
		}
	} else {
		alt_config_dir = savestring(dir, strlen(dir));
		err(ERR_NO_LOG, PRINT_PROMPT, _("%s: '%s': Using alternative "
			"configuration directory\n"), PROGRAM_NAME, alt_config_dir);
	}

	free(dir_exp);
}

static void
set_custom_selfile(char *file)
{
	if (!file || !*file || *file == '-')
		err_arg_required("--sel-file"); /* noreturn */

	if ( (sel_file = normalize_path(file, strlen(file))) ) {
		xargs.sel_file = 1;
		return;
	}

	err('e', PRINT_PROMPT, _("%s: '%s': Error setting custom "
		"selections file\n"), PROGRAM_NAME, file);
}

#ifndef _NO_LIRA
static char *
stat_file(char *file)
{
	if (!file || !*file)
		exit(EXIT_FAILURE);

	char *p = (char *)NULL;
	if (*file == '~') {
		if (!(p = tilde_expand(file))) {
			xerror(_("%s: '%s': Error expanding tilde\n"), PROGRAM_NAME, file);
			exit(EXIT_FAILURE);
		}
	} else {
		p = savestring(file, strlen(file));
	}

	struct stat a;
	if (stat(p, &a) == -1) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, p, strerror(errno));
		if (p != file)
			free(p);
		exit(errno);
	}

	return p;
}

static void
set_shotgun_file(char *opt)
{
	if (!opt || !*opt || *opt == '-')
		err_arg_required("--shotgun-file"); /* noreturn */

	alt_preview_file = stat_file(opt);
}
#endif /* !_NO_LIRA */
#endif /* !_BE_POSIX */

static void
set_alt_trash_dir(char *file)
{
	if (!file || !*file || *file == '-')
#ifndef _BE_POSIX
		err_arg_required("-T"); /* noreturn */
#else
		err_arg_required("-i"); /* noreturn */
#endif

	char *trash_exp = (char *)NULL;

	if (*file == '~') {
		trash_exp = tilde_expand(file);
		file = trash_exp;
	}

	alt_trash_dir = savestring(file, strlen(file));
	free(trash_exp);
}

static void
set_alt_kbinds_file(char *file)
{
	if (!file || !*file || *file == '-')
		err_arg_required("-k"); /* noreturn */

	char *kbinds_exp = (char *)NULL;

	if (*file == '~') {
		kbinds_exp = tilde_expand(file);
		file = kbinds_exp;
	}

	alt_kbinds_file = savestring(file, strlen(file));
	free(kbinds_exp);
}

static void
set_alt_config_file(char *file)
{
	if (!file || !*file || *file == '-')
		err_arg_required("-c"); /* noreturn */

	char *config_exp = (char *)NULL;

	if (*file == '~') {
		config_exp = tilde_expand(file);
		file = config_exp;
	}

	alt_config_file = savestring(file, strlen(file));
	free(config_exp);
}

static char *
resolve_path(char *file)
{
	char *_path = (char *)NULL;

	if (*file == '.') {
		_path = normalize_path(file, strlen(file));
		if (!_path) {
			fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, file, strerror(errno));
			exit(errno);
		}

	} else if (*file == '~') {
		_path = tilde_expand(file);
		if (!_path) {
			fprintf(stderr, _("%s: '%s': Error expanding tilde\n"),
				PROGRAM_NAME, file);
			exit(EXIT_FAILURE);
		}

	} else if (*file == '/') {
		_path = savestring(file, strlen(file));

	} else {
		char tmp[PATH_MAX + 1] = "";
		char *cwd = get_cwd(tmp, sizeof(tmp), 0);

		if (!cwd || !*cwd) {
			fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, file, strerror(errno));
			exit(errno);
		}

		const size_t len = strlen(cwd) + strlen(file) + 2;
		_path = xnmalloc(len, sizeof(char));
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
	} else if (is_url(file) == FUNC_SUCCESS) {
		open_reg_exit(file, 1, 0); /* noreturn */
	} else {
		_path = resolve_path(file);
	}

	struct stat a;
	if (stat(_path, &a) == -1) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, file, strerror(errno));
		free(_path);
		exit(errno);
	}

	if (!S_ISDIR(a.st_mode)) {
		fprintf(stderr, "%s: '%s': %s\n", PROGRAM_NAME, file, strerror(ENOTDIR));
		free(_path);
		exit(ENOTDIR);
	}

	xargs.path = 1;
	return _path;
}

static void
set_starting_path(char *_path)
{
	if (xchdir(_path, SET_TITLE) == 0) {
		if (cur_ws == UNSET)
			cur_ws = DEF_CUR_WS;

		free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(_path, strlen(_path));

	} else { /* Error changing directory */
		if (xargs.list_and_quit == 1) {
			xerror("%s: '%s': %s\n", PROGRAM_NAME, _path, strerror(errno));
			exit(EXIT_FAILURE);
		}

		err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			_path, strerror(errno));
	}
}

static void
set_opener(const char *str)
{
	if (!str || !*str)
		return;

	if (*str != '~') {
		conf.opener = savestring(optarg, strlen(optarg));
		return;
	}

	char *ep = tilde_expand(optarg);
	if (ep) {
		conf.opener = savestring(ep, strlen(ep));
		free(ep);
	} else {
		err('w', PRINT_PROMPT, _("%s: Error expanding tilde. "
			"Using default opener.\n"), PROGRAM_NAME);
	}
}

static void
set_alt_profile(const char *name)
{
#ifndef _NO_PROFILES
	free(alt_profile);

	if (validate_profile_name(name) == FUNC_SUCCESS) {
		alt_profile = savestring(name, strlen(name));
		return;
	}

	fprintf(stderr, _("%s: '%s': Invalid profile name\n"),
		PROGRAM_NAME, name);
	exit(EXIT_FAILURE);
#else
	UNUSED(name);
	fprintf(stderr, "%s: profiles: %s\n", PROGRAM_NAME, NOT_AVAILABLE);
	exit(EXIT_FAILURE);
#endif /* !_NO_PROFILES */
}

static void
set_virtual_dir(const char *str, const char *optname)
{
	if (!str || *str != '/') {
		fprintf(stderr, _("%s: '%s': Absolute path is required "
			"as argument\n"), PROGRAM_NAME, optname);
		exit(EXIT_FAILURE);
	}

	stdin_tmp_dir = savestring(optarg, strlen(optarg));
	setenv("CLIFM_VIRTUAL_DIR", stdin_tmp_dir, 1);
}

static void
set_max_value(const char *opt, int *xval, int *intval)
{
	if (!is_number(opt))
		return;

	int opt_int = atoi(opt);
	if (opt_int >= 0)
		*xval = *intval = opt_int;
}

static void
set_workspace(const char *opt)
{
	if (!is_number(opt))
		goto ERROR;

	int opt_int = atoi(opt);
	if (opt_int >= 0 && opt_int <= MAX_WS) {
		cur_ws = opt_int - 1;
		return;
	}

ERROR:
	fprintf(stderr, _("%s: '%s': Invalid workspace. Valid "
		"values are 1-8.\n"), PROGRAM_NAME, opt);
	exit(EXIT_FAILURE);
}

static void
set_color_scheme(const char *opt, const char *optname)
{
	if (!opt || !*opt || *opt == '-')
		err_arg_required(optname); /* noreturn */

	conf.usr_cscheme = savestring(opt, strlen(opt));
}

static void
set_no_colors(void)
{
	xargs.colorize = conf.colorize = 0;
#ifndef _NO_HIGHLIGHT
	xargs.highlight = conf.highlight = 0;
#endif /* !_NO_HIGHLIGHT */
}

static void
set_fnftab(void)
{
#ifndef _NO_FZF
	xargs.fnftab = 1; fzftab = 1; tabmode = FNF_TAB;
#else
	fprintf(stderr, "%s: fnf-tab: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	exit(EXIT_FAILURE);
#endif /* _NO_FZF */
}

static void
set_fzftab(void)
{
#ifndef _NO_FZF
	xargs.fzftab = fzftab = 1; tabmode = FZF_TAB;
#else
	fprintf(stderr, "%s: fzf-tab: %s\n",
		PROGRAM_NAME, _(NOT_AVAILABLE));
	exit(EXIT_FAILURE);
#endif /* !_NO_FZF */
}

static void
set_smenutab(void)
{
#ifndef _NO_FZF
	xargs.smenutab = 1; fzftab = 1; tabmode = SMENU_TAB;
#else
	fprintf(stderr, "%s: smenu-tab: %s\n",
		PROGRAM_NAME, _(NOT_AVAILABLE));
	exit(EXIT_FAILURE);
#endif /* !_NO_FZF */
}

static void
set_stdtab(void)
{
#ifndef _NO_FZF
	xargs.fzftab = 0;
#endif /* !_NO_FZF */
	fzftab = 0; tabmode = STD_TAB;
}

static void
set_no_suggestions(void)
{
#ifndef _NO_SUGGESTIONS
	xargs.suggestions = conf.suggestions = 0;
#endif /* !_NO_SUGGESTIONS */
	return;
}

static void
set_trash_as_rm(void)
{
#ifndef _NO_TRASH
	xargs.trasrm = conf.tr_as_rm = 1;
#else
	fprintf(stderr, "%s: trash: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
	exit(EXIT_FAILURE);
#endif /* !_NO_TRASH */
}

static void
set_stat(const int optc, const char *optval)
{
	if (!optval || !*optval || *optval == '-')
#ifndef _BE_POSIX
		err_arg_required(optc == LOPT_STAT ? "--stat" : "--stat-full");

	xargs.stat = (optc == LOPT_STAT ? SIMPLE_STAT : FULL_STAT);
#else
		err_arg_required(optc == 'j' ? "-j" : "-J");

	xargs.stat = (optc == 'j' ? SIMPLE_STAT : FULL_STAT);
#endif /* !_BE_POSIX */

	xargs.restore_last_path = conf.restore_last_path = 0;
}

#ifndef _BE_POSIX
static void
xset_time_style(char *optval, const int ptime)
{
	if (!optval || !*optval || *optval == '-')
		err_arg_required(ptime == 0 ? "--time-style" : "--ptime-style");

	if (ptime == 1) {
		xargs.ptime_style = 1;
		set_time_style(optval, &conf.ptime_str, 1);
	} else {
		xargs.time_style = 1;
		set_time_style(optval, &conf.time_str, 0);
	}
}

static void
xset_prop_fields(const char *optval)
{
	/* --prop-fields accepts a single dash (-) as argument. */
	if (!optval || !*optval || (*optval == '-' && optval[1]))
		err_arg_required("--prop-fields");

	xargs.prop_fields_str = 1;
	xstrsncpy(prop_fields_str, optval, sizeof(prop_fields_str));
	set_prop_fields(prop_fields_str);

#ifndef ST_BTIME
	if (prop_fields.time == PROP_TIME_BIRTH) {
		fprintf(stderr, _("%s: --prop-fields: 'b': Birth time is not "
			"available on this platform\n"), PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}
#endif /* !ST_BTIME*/
}
#endif /* !_BE_POSIX */

__attribute__ ((noreturn))
static void
print_version(void)
{
#ifdef _BE_POSIX
	char *posix = "-POSIX";
#else
	char *posix = "";
#endif /* _BE_POSIX */
	printf("%s%s\n", VERSION, posix);
	exit(EXIT_SUCCESS);
}

#ifdef _BE_POSIX
static void
set_tab_mode(const char *opt)
{
	if (!opt || !*opt || opt[1])
		return;

	const int n = *opt - '0';

	switch (n) {
	case 0:	set_stdtab(); break;
	case 1:	set_fzftab(); break;
	case 2: set_smenutab(); break;
	case 3: set_fnftab(); break;
	default:
		fprintf(stderr, _("%s: '%s': Valid values are 0-3\n"), PROGRAM_NAME, opt);
		exit(EXIT_FAILURE);
	}
}
#endif /* _BE_POSIX */

static void
xset_pager_view(char *arg)
{
	if (!arg || !*arg || *arg == '-')
		err_arg_required("--pager-view");

	if (*arg == 'a' && strcmp(arg, "auto") == 0) {
		xargs.pager_view = conf.pager_view = PAGER_AUTO;
	} else if (*arg == 'l' && strcmp(arg, "long") == 0) {
		xargs.pager_view = conf.pager_view = PAGER_LONG;
	} else if (*arg == 's' && strcmp(arg, "short") == 0) {
		xargs.pager_view = conf.pager_view = PAGER_SHORT;
	} else {
		fprintf(stderr, _("%s: --pager-view: '%s': Invalid value.\n"
			"Valid values are 'auto', 'long', and 'short'\n"),
			PROGRAM_NAME, arg);
		exit(EXIT_FAILURE);
	}
}

/* Evaluate command line arguments, if any, and change initial variables to
 * their corresponding values. */
#ifdef _BE_POSIX
/* POSIX version: getopt(3) is used (instead of getopt_long(3)) and only
 * short options are provided. */
void
parse_cmdline_args(const int argc, char **argv)
{
	opterr = optind = 0;
	int optc;
#ifndef _NO_LIRA
	int open_prev_mode = 0;
	char *open_prev_file = (char *)NULL;
#endif /* !_NO_LIRA */

	while ((optc = getopt(argc, argv, OPTSTRING)) != EOF) {
		switch (optc) {
		case 'a': conf.show_hidden = xargs.hidden = 1; break;
		case 'A': conf.show_hidden = xargs.hidden = 0; break;
		case 'b': xargs.bm_file = 1; set_alt_bm_file(optarg); break;
		case 'B': set_tab_mode(optarg); break;
		case 'c': xargs.config = 1; set_alt_config_file(optarg); break;
		case 'C': xargs.clear_screen = conf.clear_screen = 0; break;
		case 'd': xargs.disk_usage = conf.disk_usage = 1; break;
		case 'D': xargs.only_dirs = conf.only_dirs = 1; break;
		case 'e': xargs.auto_open = conf.auto_open = 0; break;
		case 'E': xargs.autocd = conf.autocd = 0; break;
		case 'f': xargs.full_dir_size = conf.full_dir_size = 1; break;
		case 'F': xargs.files_counter = conf.files_counter = 0; break;
		case 'g': xargs.si = 1; break;
		case 'G': xargs.apparent_size = conf.apparent_size = 0; break;
		case 'h': help_function(); break; /* noreturn */
		case 'H':
#ifndef _NO_HIGHLIGHT
			xargs.highlight = conf.highlight = 0;
#endif /* !_NO_HIGHLIGHT */
			break;
		case 'i':
#ifndef _NO_ICONS
			xargs.icons = conf.icons = 1; break;
#else
			fprintf(stderr, "%s: icons: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_ICONS */
		case 'I': set_alt_trash_dir(optarg); break;
		case 'j': set_stat(optc, optarg); break;
		case 'J': set_stat(optc, optarg); break;
		case 'k': set_alt_kbinds_file(optarg); break;
		case 'l': conf.long_view = xargs.longview = 1; break;
		case 'L':
			xargs.follow_symlinks_long = conf.follow_symlinks_long = 1; break;
		case 'm': xargs.fuzzy_match = conf.fuzzy_match = 1; break;
		case 'M': set_no_colors(); break;
		case 'n': xargs.history = 0; break;
		case 'N': xargs.no_bold = 1; break;
		case 'o': set_opener(optarg); break;
		case 'O':
#ifdef _NO_LIRA
			fprintf(stderr, "%s: open: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#else
			open_prev_file = optarg;
			open_prev_mode = OPEN_FILE;
			break;
#endif /* _NO_LIRA */
		case 'p': set_alt_profile(optarg); break;
		case 'P':
#ifdef _NO_LIRA
			fprintf(stderr, "%s: preview: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#else
			open_prev_file = optarg;
			open_prev_mode = PREVIEW_FILE;
			break;
#endif /* _NO_LIRA */
		case 'q': xargs.list_and_quit = 1; break;
		case 'Q': xargs.cd_on_quit = conf.cd_on_quit = 1; break;
		case 'r': set_trash_as_rm(); break;
		case 'R': xargs.classify = conf.classify = 0; break;
		case 's': xargs.stealth_mode = 1; break;
		case 'S': set_no_suggestions(); break;
		case 't': set_color_scheme(optarg, "-t"); break;
		case 'T': xargs.trim_names = conf.trim_names = 0; break;
		case 'u': xargs.disk_usage_analyzer = 1; break;
		case 'v': print_version(); /* noreturn */
		case 'V': set_virtual_dir(optarg, "-V"); break;
		case 'w': set_workspace(optarg); break;
		case 'W': xargs.printsel = conf.print_selfiles = 1; break;
		case 'x':
			xargs.secure_env = 1;
			xsecure_env(SECURE_ENV_IMPORT);
			break;
		case 'X':
			xargs.secure_env_full = 1;
			xsecure_env(SECURE_ENV_FULL);
			break;
		case 'y': conf.light_mode = xargs.light = 1; break;
		case 'Y':
			xargs.secure_cmds = xargs.secure_env = 1;
			xsecure_env(SECURE_ENV_IMPORT);
			break;
		case 'z': set_sort(optarg); break;
		case 'Z':
			set_max_value(optarg, &xargs.max_files, &max_files);
			break;

		/* Handle error */
		case ':':
			fprintf(stderr, _("%s: Option '-%c' requires an argument.\n"
				"Try '%s -h' for more information.\n"),
				PROGRAM_NAME, optopt, PROGRAM_NAME);
			exit(EXIT_FAILURE);
		case '?':
			fprintf(stderr, _("%s: Unrecognized option: '-%c'\n"
				"Try '%s -h' for more information.\n"),
				PROGRAM_NAME, optopt, PROGRAM_NAME);
			exit(EXIT_FAILURE);

		default: break;
		}
	}

#ifndef _NO_LIRA
	if (open_prev_mode != 0)
		open_preview_file(open_prev_file, open_prev_mode); /* noreturn */
#endif /* !_NO_LIRA */

	if (argv[optind]) { /* Starting path passed as positional parameter */
		char *spath = resolve_starting_path(argv[optind]);
		if (spath) {
			set_starting_path(spath);
			free(spath);
		}
	} else {
		if (xargs.list_and_quit == 1) {
			conf.restore_last_path = 0;
			set_start_path();
		}
	}
}

#else
/* Non-POSIX version: getopt_long(3) is used and long options are
* supported. */
void
parse_cmdline_args(const int argc, char **argv)
{
	/* Disable automatic error messages to be able to handle them ourselves
	 * via the '?' and ':' cases in the switch statement. */
	opterr = optind = 0;

	int optc;
	char *path_value = (char *)NULL;
#ifndef _NO_LIRA
	int open_prev_mode = 0;
	char *open_prev_file = (char *)NULL;
#endif /* _NO_LIRA */

	while ((optc = getopt_long(argc, argv, OPTSTRING,
		longopts, (int *)0)) != EOF) {

		switch (optc) {
		/* Short options */
		case 'a': conf.show_hidden = xargs.hidden = 1; break;
		case 'A': conf.show_hidden = xargs.hidden = 0; break;
		case 'b': xargs.bm_file = 1; set_alt_bm_file(optarg); break;
		case 'c': xargs.config = 1; set_alt_config_file(optarg); break;

#ifdef RUN_CMD
		case 'C':
			if (!optarg || !*optarg || *optarg == '-')
				err_arg_required("--cmd"); /* noreturn */

			cmd_line_cmd = savestring(optarg, strlen(optarg));
			break;
#endif /* RUN_CMD */

		case 'D': set_alt_config_dir(optarg); break;
		case 'e': xargs.noeln = conf.no_eln = 1; break;
		case 'E': xargs.eln_use_workspace_color = 1; break;
		case 'f': conf.list_dirs_first = xargs.dirs_first = 1; break;
		case 'F': conf.list_dirs_first = xargs.dirs_first = 0; break;
		case 'g': conf.pager = xargs.pager = 1; break;
		case 'G': conf.pager = xargs.pager = 0; break;
		case 'h': help_function(); break; /* noreturn */
		case 'H': xargs.horizontal_list = 1; conf.listing_mode = HORLIST; break;
		case 'i': conf.case_sens_list = xargs.case_sens_list = 0; break;
		case 'I': conf.case_sens_list = xargs.case_sens_list = 1; break;
		case 'k': set_alt_kbinds_file(optarg); break;
		case 'l': conf.long_view = xargs.longview = 1; break;
		case 'L':
			xargs.follow_symlinks_long = conf.follow_symlinks_long = 1; break;
		case 'm': conf.dirhist_map = xargs.dirmap = 1; break;
		case 'o': conf.autols = xargs.autols = 1; break;
		case 'O': conf.autols = xargs.autols = 0; break;
		case 'p': path_value = optarg; xargs.path = 1; break;
		case 'P': set_alt_profile(optarg); break;
		case 'r': xargs.refresh_on_empty_line = 0; break;
		case 's': conf.splash_screen = xargs.splash = 1; break;
		case 'S': xargs.stealth_mode = 1; break;
		case 't': xargs.disk_usage_analyzer = 1; break;
		case 'T': set_alt_trash_dir(optarg); break;
		case 'v': print_version(); /* noreturn */
		case 'w': set_workspace(optarg); break;
		case 'x': conf.ext_cmd_ok = xargs.ext = 0; break;
		case 'y': conf.light_mode = xargs.light = 1; break;
		case 'z': set_sort(optarg); break;

		/* Only-long options */
		case LOPT_BELL:
			set_bell_style(optarg); break;
		case LOPT_CASE_SENS_DIRJUMP:
			xargs.case_sens_dirjump = conf.case_sens_dirjump = 1; break;
		case LOPT_CASE_SENS_PATH_COMP:
			xargs.case_sens_path_comp = conf.case_sens_path_comp = 1; break;
		case LOPT_CD_ON_QUIT:
			xargs.cd_on_quit = conf.cd_on_quit = 1; break;
		case LOPT_COLOR_SCHEME:
			set_color_scheme(optarg, "--color-scheme"); break;
		case LOPT_COLOR_LNK_AS_TARGET:
			xargs.color_lnk_as_target = conf.color_lnk_as_target = 1; break;
		case LOPT_CWD_IN_TITLE:
			xargs.cwd_in_title = 1; break;
		case LOPT_DATA_DIR:
			set_datadir(optarg); break;
		case LOPT_DESKTOP_NOTIFICATIONS:
			xargs.desktop_notifications = conf.desktop_notifications = 1;
			break;
		case LOPT_DISK_USAGE:
			xargs.disk_usage = conf.disk_usage = 1; break;
		case LOPT_FNFTAB:
			set_fnftab(); break;
		case LOPT_FULL_DIR_SIZE:
			xargs.full_dir_size = conf.full_dir_size = 1; break;
		case LOPT_FUZZY_ALGO:
			set_fuzzy_algo(optarg); break;
		case LOPT_FUZZY_MATCHING:
			xargs.fuzzy_match = conf.fuzzy_match = 1; break;
		case LOPT_FZFPREVIEW: /* fallthrough */
		case LOPT_FZFPREVIEW_HIDDEN:
			set_fzfpreview(optc); break;
		case LOPT_FZFTAB:
			set_fzftab(); break;
		case LOPT_FZYTAB:
			set_fzytab(); /* noreturn */

#ifndef _NO_ICONS
		case LOPT_ICONS:
			xargs.icons = conf.icons = 1; break;
		case LOPT_ICONS_USE_FILE_COLOR:
			xargs.icons = conf.icons = xargs.icons_use_file_color = 1; break;
#else
		case LOPT_ICONS: /* fallthrough */
		case LOPT_ICONS_USE_FILE_COLOR:
			fprintf(stderr, "%s: icons: %s\n", PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
			break;
#endif /* !_NO_ICONS */

		case LOPT_INT_VARS:
			xargs.int_vars = int_vars = 1; break;
		case LOPT_LIST_AND_QUIT:
			xargs.list_and_quit = 1;
			xargs.no_dirjump = 1;
			xargs.restore_last_path = conf.restore_last_path = 0;
			break;
		case LOPT_LSCOLORS: xargs.lscolors = 1; break;
		case LOPT_MAX_DIRHIST:
			set_max_value(optarg, &xargs.max_dirhist, &conf.max_dirhist);
			break;
		case LOPT_MAX_FILES:
			set_max_value(optarg, &xargs.max_files, &max_files);
			break;
		case LOPT_MAX_PATH:
			set_max_value(optarg, &xargs.max_path, &conf.max_path);
			break;
		case LOPT_MNT_UDISKS2:
			xargs.mount_cmd = MNT_UDISKS2; break;
		case LOPT_NO_APPARENT_SIZE:
			xargs.apparent_size = conf.apparent_size = 0; break;
		case LOPT_NO_BOLD:
			xargs.no_bold = 1; break;
		case LOPT_NO_CD_AUTO:
			xargs.autocd = conf.autocd = 0; break;
		case LOPT_NO_CLASSIFY:
			xargs.classify = conf.classify = 0; break;
		case LOPT_NO_CLEAR_SCREEN:
			xargs.clear_screen = conf.clear_screen = 0; break;
		case LOPT_NO_COLORS:
			set_no_colors(); break;
		case LOPT_NO_COLUMNS:
			xargs.columns = conf.columned = 0; break;
		case LOPT_NO_DIR_JUMPER:
			xargs.no_dirjump = 1; break;
		case LOPT_NO_FILE_CAP:
			xargs.check_cap = check_cap = 0; break;
		case LOPT_NO_FILE_EXT:
			xargs.check_ext = check_ext = 0; break;
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

		case LOPT_NO_HISTORY:
			xargs.history = 0; break;
		case LOPT_NO_OPEN_AUTO:
			xargs.auto_open = conf.auto_open = 0; break;
		case LOPT_NO_REFRESH_ON_RESIZE:
			xargs.refresh_on_resize = 0; break;
		case LOPT_NO_RESTORE_LAST_PATH:
			xargs.restore_last_path = conf.restore_last_path = 0; break;
		case LOPT_NO_SUGGESTIONS:
			set_no_suggestions(); break;
		case LOPT_NO_TIPS:
			xargs.tips = conf.tips = 0; break;
		case LOPT_NO_TRIM_NAMES:
			xargs.trim_names = conf.trim_names = 0; break;
		case LOPT_NO_WARNING_PROMPT:
			xargs.warning_prompt = conf.warning_prompt = 0; break;
		case LOPT_NO_WELCOME_MESSAGE:
			xargs.welcome_message = conf.welcome_message = 0; break;
		case LOPT_ONLY_DIRS:
			xargs.only_dirs = conf.only_dirs = 1; break;

		case LOPT_OPEN: /* --open or --preview */
#ifdef _NO_LIRA
			fprintf(stderr, "%s: --open/--preview: %s\n",
				PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#else
		{
			open_prev_file = optarg;
			int n = *argv[optind - 1] == '-' ? 1 : 2;
			if (*(argv[optind - n] + 2) == 'p')
				open_prev_mode = PREVIEW_FILE; /* --preview */
			else
				open_prev_mode = OPEN_FILE; /* --open */
		} break;
#endif /* _NO_LIRA */

		case LOPT_OPENER:
			set_opener(optarg); break;
		case LOPT_PAGER_VIEW: xset_pager_view(optarg); break;
		case LOPT_PRINT_SEL:
			xargs.printsel = conf.print_selfiles = 1; break;
		case LOPT_PROP_FIELDS: xset_prop_fields(optarg); break;
		case LOPT_READONLY: xargs.readonly = conf.readonly = 1; break;
		case LOPT_RL_VI_MODE:
			xargs.rl_vi_mode = 1; break;
		case LOPT_SECURE_CMDS:
			xargs.secure_cmds = xargs.secure_env = 1;
			xsecure_env(SECURE_ENV_IMPORT);
			break;

		case LOPT_SECURE_ENV:
			xargs.secure_env = 1;
			xsecure_env(SECURE_ENV_IMPORT);
			break;

		case LOPT_SECURE_ENV_FULL:
			xargs.secure_env_full = 1;
			xsecure_env(SECURE_ENV_FULL);
			break;

		case LOPT_SEL_FILE:
			set_custom_selfile(optarg); break;
		case LOPT_SHARE_SELBOX:
			xargs.share_selbox = conf.share_selbox = 1; break;
		case LOPT_SHOTGUN_FILE:
#ifdef _NO_LIRA
			fprintf(stderr, "%s: --shotgun-file: %s\n",
				PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#else
			set_shotgun_file(optarg); break;
#endif /* !_NO_LIRA */
		case LOPT_SI:
			xargs.si = 1; break;
		case LOPT_SMENUTAB:
			set_smenutab(); break;
		case LOPT_SORT_REVERSE:
			xargs.sort_reverse = conf.sort_reverse = 1; break;
		case LOPT_STAT: /* fallthrough */
		case LOPT_STAT_FULL:
			set_stat(optc, optarg); break;
		case LOPT_STDTAB:
			set_stdtab(); break;
		case LOPT_PTIME_STYLE: xset_time_style(optarg, 1); break;
		case LOPT_TIME_STYLE: xset_time_style(optarg, 0); break;
		case LOPT_TRASH_AS_RM:
			set_trash_as_rm(); break;
		case LOPT_VIRTUAL_DIR:
			set_virtual_dir(optarg, "--virtual-dir"); break;
		case LOPT_VIRTUAL_DIR_FULL_PATHS:
			xargs.virtual_dir_full_paths = 1; break;
		case LOPT_VT100:
			set_vt100(); break;

		/* Handle error */
		case ':':
			err_arg_required(argv[optind - 1]); /* noreturn */
		case '?':
			err_invalid_opt(argv[optind - 1]); /* noreturn */

		default: break;
		}
	}

#ifndef _NO_LIRA
	if (open_prev_mode != 0)
		open_preview_file(open_prev_file, open_prev_mode); /* noreturn */
#endif /* !_NO_LIRA */

	char *spath = (char *)NULL;
	if (xargs.stat == 0 && argv[optind]) { /* Starting path passed as positional parameter */
		spath = resolve_starting_path(argv[optind]);
	} else {
		if (path_value) /* Starting path passed via -p */
			spath = resolve_starting_path(path_value);
	}

	if (spath) {
		set_starting_path(spath);
		free(spath);
	} else {
		if (xargs.list_and_quit == 1) {
			/* Starting path not specified in the command line, let's use
			 * the current directory. */
			conf.restore_last_path = 0;
			set_start_path();
		}
	}
}
#endif /* _BE_POSIX */
