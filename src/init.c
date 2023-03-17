/* init.c -- functions controlling the program initialization */

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

#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>

#if defined(__linux__) || defined(__HAIKU__) || defined(__sun) || defined(__CYGWIN__)
# include <grp.h> /* getgrouplist(3) */
#endif

#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
//#include <readline/readline.h>
#include <readline/tilde.h>
#if defined(__OpenBSD__)
# include <ereadline/readline/history.h> /* history_write_timestamps */
#else
# include <readline/history.h>
#endif /* __OpenBSD__ */

#if defined(__NetBSD__)
# include <ctype.h>
#endif

#include <paths.h>

#include "aux.h"
#include "checks.h"
#include "config.h"
#include "exec.h"
#include "init.h"
#if defined(_NO_PROFILES) || defined(_NO_FZF) || defined(_NO_ICONS) || defined(_NO_TRASH)
# include "messages.h"
#endif
#include "mime.h"
#include "misc.h"
#include "navigation.h"
#include "sort.h"
#include "file_operations.h"
#include "autocmds.h"
#include "sanitize.h"
#include "selection.h"

/* Macros to be able to consult the value of a macro string */
#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)

#define PREVIEW_FILE 1
#define OPEN_FILE    2

/* We need this for get_user_groups() */
#if !defined(NGROUPS_MAX)
# if defined(__linux__)
#  define NGROUPS_MAX 65536
# else
#  define NGROUPS_MAX 1024
# endif /* __linux__ */
#endif /* !NGROUPS_MAX */

void
init_workspaces_opts(void)
{
	size_t i;
	for (i = 0; i < MAX_WS; i++) {
		workspace_opts[i].color_scheme = cur_cscheme;
		workspace_opts[i].files_counter = conf.files_counter;

		workspace_opts[i].filter.str = filter.str
			? savestring(filter.str, strlen(filter.str)) : (char *)NULL;
		workspace_opts[i].filter.rev = filter.rev;
		workspace_opts[i].filter.type = filter.type;
		workspace_opts[i].filter.env = filter.env;

		workspace_opts[i].light_mode = conf.light_mode;
		workspace_opts[i].list_dirs_first = conf.list_dirs_first;
		workspace_opts[i].long_view = conf.long_view;
		workspace_opts[i].max_files = max_files;
		workspace_opts[i].max_name_len = conf.max_name_len;
		workspace_opts[i].only_dirs = conf.only_dirs;
		workspace_opts[i].pager = conf.pager;
		workspace_opts[i].show_hidden = conf.show_hidden;
		workspace_opts[i].sort = conf.sort;
		workspace_opts[i].sort_reverse = conf.sort_reverse;
	}
}

static void
init_shades(void)
{
	date_shades.type = SHADE_TYPE_UNSET;
	size_shades.type = SHADE_TYPE_UNSET;

	int i = NUM_SHADES;
	while (--i >= 0) {
		date_shades.shades[i].attr = 0;
		date_shades.shades[i].R = 0;
		date_shades.shades[i].G = 0;
		date_shades.shades[i].B = 0;

		size_shades.shades[i].attr = 0;
		size_shades.shades[i].R = 0;
		size_shades.shades[i].G = 0;
		size_shades.shades[i].B = 0;
	}
}

void
init_conf_struct(void)
{
	conf.apparent_size = UNSET;
	conf.auto_open = UNSET;
	conf.autocd = UNSET;
	conf.autols = UNSET;
	conf.case_sens_dirjump = UNSET;
	conf.case_sens_path_comp = UNSET;
	conf.case_sens_search = UNSET;
	conf.case_sens_list = UNSET;
	conf.cd_on_quit = UNSET;
	conf.classify = UNSET;
	conf.clear_screen = UNSET;
	conf.cmd_desc_sug = UNSET;
	conf.colorize = UNSET;
	conf.columned = UNSET;
	conf.cp_cmd = UNSET;
	conf.desktop_notifications = UNSET;
	conf.dirhist_map = UNSET;
	conf.disk_usage = UNSET;
//	conf.expand_bookmarks = UNSET;
	conf.ext_cmd_ok = UNSET;
	conf.files_counter = UNSET;
	conf.full_dir_size = UNSET;
	conf.fuzzy_match = UNSET;
	conf.fuzzy_match_algo = UNSET;
	conf.fzf_preview = UNSET;
#ifndef _NO_HIGHLIGHT
	conf.highlight = UNSET;
#else
	conf.highlight = 0;
#endif
#ifndef _NO_ICONS
	conf.icons = UNSET;
#endif
	conf.light_mode = UNSET;
	conf.list_dirs_first = UNSET;
	conf.listing_mode = UNSET;
	conf.log_cmds = UNSET;
	conf.logs_enabled = UNSET;
	conf.long_view = UNSET;
	conf.max_dirhist = UNSET;
	conf.max_hist = UNSET;
	conf.max_log = UNSET;
	conf.max_jump_total_rank = UNSET;
	conf.max_name_len = DEF_MAX_NAME_LEN;
	conf.max_path = UNSET;
	conf.max_printselfiles = UNSET;
	conf.min_jump_rank = JUMP_UNSET; /* UNSET (-1) is a valid value for MinJumpRank */
	conf.min_name_trim = UNSET;
	conf.mv_cmd = UNSET;
	conf.no_eln = UNSET;
	conf.only_dirs = UNSET;
	conf.pager = UNSET;
	conf.private_ws_settings = UNSET;
	conf.purge_jumpdb = UNSET;
	conf.relative_time = UNSET;
	conf.restore_last_path = UNSET;
	conf.rm_force = UNSET;
	conf.search_strategy = UNSET;
	conf.share_selbox = UNSET;
	conf.show_hidden = UNSET;
	conf.sort = UNSET;
	conf.sort_reverse = 0;
	conf.splash_screen = UNSET;
	conf.suggest_filetype_color = UNSET;
	conf.suggestions = UNSET;
	conf.tips = UNSET;
#ifndef _NO_TRASH
	conf.tr_as_rm = UNSET;
#endif
//	conf.unicode = UNSET;
	conf.unicode = DEF_UNICODE;
	conf.warning_prompt = UNSET;
	conf.welcome_message = UNSET;

	conf.encoded_prompt = (char *)NULL;
	conf.fzftab_options = (char *)NULL;
	conf.opener = (char *)NULL;
#ifndef _NO_SUGGESTIONS
	conf.suggestion_strategy = (char *)NULL;
#endif
	conf.term = (char *)NULL;
	conf.time_str = (char *)NULL;
	conf.usr_cscheme = (char *)NULL;
	conf.wprompt_str = (char *)NULL;
	conf.welcome_message_str = (char *)NULL;

	init_shades();
}

void
set_prop_fields(char *line)
{
	if (!line || !*line)
		return;

	prop_fields.counter = 0;
	prop_fields.perm =    0;
	prop_fields.ids =     0;
	prop_fields.time =    0;
	prop_fields.size =    0;
	prop_fields.inode =   0;
	prop_fields.xattr =   0;
	prop_fields.len =     2; /* Two spaces between file name and props string */

	size_t i;
	for (i = 0; i < PROP_FIELDS_SIZE && line[i]; i++) {
		switch(line[i]) {
		case 'f': prop_fields.counter = 1; break;
		case 'd': prop_fields.inode = 1; break;
		case 'p': prop_fields.perm = PERM_SYMBOLIC; break;
		case 'n': prop_fields.perm = PERM_NUMERIC; break;
		case 'i': prop_fields.ids = 1; break;
		case 'a': prop_fields.time = PROP_TIME_ACCESS; break;
		case 'c': prop_fields.time = PROP_TIME_CHANGE; break;
		case 'm': prop_fields.time = PROP_TIME_MOD; break;
		case 's': prop_fields.size = PROP_SIZE_HUMAN; break;
		case 'S': prop_fields.size = PROP_SIZE_BYTES; break;
#if defined(_LINUX_XATTR)
		case 'x': prop_fields.xattr = 1; break;
#endif
		default: break;
		}
	}

	/* How much space needs to be reserved to print enabled fields?
	 * Only fixed values are counted: dinamical values are calculated
	 * and added in place: for these values we only count here the space
	 * that follows each of them, if enabled */
	/* Static lengths */
	if (prop_fields.perm != 0)
		prop_fields.len += ((prop_fields.perm == PERM_NUMERIC ? 4 : 13) + 1);
	if (prop_fields.size != 0)
		prop_fields.len += prop_fields.size == PROP_SIZE_HUMAN ? (8 + 1) : 1;

	/* Dynamic lengths */
	if (prop_fields.counter != 0)
		prop_fields.len++;
	if (prop_fields.inode != 0)
		prop_fields.len++;
	if (prop_fields.ids != 0)
		prop_fields.len++;
	/* The length of the date field is calculated by check_time_str() */
}

int
get_sys_shell(void)
{
	char l[PATH_MAX];
	*l = '\0';
	ssize_t ret = readlinkat(AT_FDCWD, "/bin/sh", l, PATH_MAX);

	if (!*l || ret == -1)
		return SHELL_NONE;

	l[ret] = '\0';

	char *s = (char *)NULL;
	char *p = strrchr(l, '/');
	if (p && *(++p))
		s = p;
	else
		s = l;

	if (*s == 'b' && strcmp(s, "bash") == 0)
		return SHELL_BASH;
	if (*s == 'd' && strcmp(s, "dash") == 0)
		return SHELL_DASH;
	if (*s == 'f' && strcmp(s, "fish") == 0)
		return SHELL_FISH;
	if (*s == 'z' && strcmp(s, "zsh") == 0)
		return SHELL_ZSH;

	return SHELL_NONE;
}

#ifndef _NO_GETTEXT
/* Initialize gettext for translations support */
int
init_gettext(void)
{
	char locale_dir[PATH_MAX];
	snprintf(locale_dir, sizeof(locale_dir), "%s/locale", data_dir
		? data_dir : "/usr/local/share");
	bindtextdomain(PNL, locale_dir);
	textdomain(PNL);

	return EXIT_SUCCESS;

}
#endif /* !_NO_GETTEXT */

int
backup_argv(int argc, char **argv)
{
	argc_bk = argc;
	argv_bk = (char **)xnmalloc((size_t)argc + 1, sizeof(char *));

	/* Let's store the executable base name, excluding the path
	 * This is done mainly to detect and disallow nested instances (see exec.c) */
	char *n = strrchr(argv[0], '/');
	if (n && *n && *(n + 1))
		bin_name = savestring(n + 1, strlen(n + 1));
	else
		bin_name = savestring(argv[0], strlen(argv[0]));

	int i = argc;
	while (--i >= 0)
		argv_bk[i] = savestring(argv[i], strlen(argv[i]));
	argv_bk[argc] = (char *)NULL;

	return EXIT_SUCCESS;
}

int
init_workspaces(void)
{
	workspaces = (struct ws_t *)xnmalloc(MAX_WS, sizeof(struct ws_t));
	int i = MAX_WS;
	while (--i >= 0) {
		workspaces[i].path = (char *)NULL;
		workspaces[i].name = (char *)NULL;
	}

	return EXIT_SUCCESS;
}

int
get_home(void)
{
	if (!user.home || access(user.home, W_OK) == -1) {
		/* If no user's home, or if it's not writable, there won't be
		 * any config directory. These flags are used to prevent functions
		 * from trying to access any of these directories */
		home_ok = config_ok = 0;

		_err('e', PRINT_PROMPT, _("%s: Cannot access the home directory. "
			"Bookmarks, commands logs, and commands history are "
			"disabled. Program messages, selected files, and the jump database "
			"won't be persistent. Using default options\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int
init_history(void)
{
	if (!hist_file)
		return EXIT_FAILURE;

	/* Shrink the log file size */
	if (log_file)
		check_file_size(log_file, conf.max_log);

	/* Get history */
	history_comment_char = '#';
	history_write_timestamps = 1;

	struct stat attr;
	if (stat(hist_file, &attr) == 0 && FILE_SIZE != 0) {
		/* If the size condition is not included, and in case of a zero
		 * size file, read_history() produces malloc errors */
		/* Recover history from the history file */
		read_history(hist_file); /* This line adds more leaks to readline */
		/* Limit the size of the history file to max_hist lines */
		history_truncate_file(hist_file, conf.max_hist);
	} else {
	/* If the history file doesn't exist, create it */
		FILE *hist_fp = fopen(hist_file, "w+");
		if (!hist_fp) {
			_err('w', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
			    PROGRAM_NAME, hist_file, strerror(errno));
		} else {
			/* To avoid malloc errors in read_history(), do not
			 * create an empty file */
			fputs("edit\n", hist_fp);
			/* There is no need to run read_history() here, since
			 * the history file is still empty */
			fclose(hist_fp);
		}
	}

	return EXIT_SUCCESS;
}

/* If path was not set (neither in the config file nor via command line nor
 * via the RestoreLastPath option), set the default (CWD), and if CWD is not
 * set, use the user's home directory, and if the home cannot be found either,
 * try the root directory, and if there's no access to the root dir either, exit */
static void
set_cur_workspace(void)
{
	if (workspaces[cur_ws].path)
		return;

	char cwd[PATH_MAX] = "";
	if (getcwd(cwd, sizeof(cwd)) == NULL) { /* avoid compiler warning */ }
	if (!*cwd || strlen(cwd) == 0) {
		if (user.home) {
			workspaces[cur_ws].path = savestring(user.home, user.home_len);
		} else {
			if (access("/", R_OK | X_OK) == -1) {
				_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: /: %s\n", PROGRAM_NAME, strerror(errno));
				exit(EXIT_FAILURE);
			} else {
				workspaces[cur_ws].path = savestring("/", 1);
			}
		}
	} else {
		workspaces[cur_ws].path = savestring(cwd, strlen(cwd));
	}
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
			"\nFalling back to workspace %zu\n"), PROGRAM_NAME, cur_ws, cur_ws + 1);
	}

	prev_ws = cur_ws;
	set_cur_workspace();

	/* Make path the CWD */
	/* If chdir() fails, set path to CWD, list files and print the
	 * error message. If no access to CWD either, exit */
	if (xchdir(workspaces[cur_ws].path, NO_TITLE) == -1) {
		_err('e', PRINT_PROMPT, "%s: chdir: '%s': %s\n", PROGRAM_NAME,
		    workspaces[cur_ws].path, strerror(errno));

		char cwd[PATH_MAX] = "";
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			_err(0, NOPRINT_PROMPT, _("%s: Fatal error! Failure "
				"retrieving current working directory\n"), PROGRAM_NAME);
			exit(EXIT_FAILURE);
		}

		if (workspaces[cur_ws].path)
			free(workspaces[cur_ws].path);
		workspaces[cur_ws].path = savestring(cwd, strlen(cwd));
	}

	/* Set OLDPWD */
	char *pwd = getenv("PWD");
	if (pwd && workspaces[cur_ws].path && strcmp(workspaces[cur_ws].path, pwd) != 0) {
#if _DEBUG_OLDPWD
		_err(ERR_NO_LOG, PRINT_PROMPT, "OLDPWD: %s\n", pwd);
#endif
		setenv("OLDPWD", pwd, 1);
	}

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

	char *data_dirs[] = {
		"/usr/local/share",
		"/usr/share",
		"/opt/local/share",
		"/opt/share",
		"/opt/clifm/share",
		home_local,
#if defined(__HAIKU__)
		"/boot/system/non-packaged/data",
		"/boot/system/data",
#endif
		NULL };

	size_t i;
	struct stat a;

	for (i = 0; data_dirs[i]; i++) {
		char tmp[PATH_MAX];
		snprintf(tmp, sizeof(tmp), "%s/%s/%src", data_dirs[i], PNL, PNL);

		if (stat(tmp, &a) == -1 || !S_ISREG(a.st_mode))
			continue;

		data_dir = savestring(data_dirs[i], strlen(data_dirs[i]));
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

static char *
try_datadir(char *dir)
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
		char *q = (char *)xnmalloc(strlen(dir) + 7, sizeof(char));
		sprintf(q, "%s/share", dir);
		return q;
	}

	/* Try DIR/clifm/clifmrc */
	snprintf(p, sizeof(p), "%s/%s/%src", dir, PNL, PNL);
	if (stat(p, &a) != -1 && S_ISREG(a.st_mode))
		return savestring(dir, strlen(dir));

	return (char *)NULL;
}

static char *
resolve_absolute_path(char *s)
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
resolve_relative_path(char *s)
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
resolve_basename(char *s)
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
 * paths, and finally guessing based on whatever argv[0] provides */
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
		data_dir = savestring(STRINGIZE(CLIFM_DATADIR), strlen(STRINGIZE(CLIFM_DATADIR)));
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

void
check_env_filter(void)
{
	if (filter.str)
		return;

	char *p = getenv("CLIFM_FILTER");
	if (!p)
		return;

	if (*p == '!' || (*p == '\\' && *(p + 1) == '!')) {
		filter.rev = 1;
		if (*p == '\\')
			p++;
		p++;
	} else {
		filter.rev = 0;
	}

	filter.env = 1;
	set_filter_type(*p);
	filter.str = savestring(p, strlen(p));
}

char *
get_date(void)
{
	time_t rawtime = time(NULL);
	struct tm tm;
	localtime_r(&rawtime, &tm);
	size_t date_max = 128;

	char *p = (char *)malloc((date_max + 1) * sizeof(char)), *date;
	if (p) {
		date = p;
		p = (char *)NULL;
	} else {
		return (char *)NULL;
	}

	strftime(date, date_max, "%Y-%m-%dT%T%z", &tm);
	return date;
}

static pid_t
get_own_pid(void)
{
	pid_t pid;

	pid = getpid();

	if (pid < 0)
		return 0;
	return pid;
}

/* Return 1 is secure-env is enabled. Otherwise, return 0
 * Used only at an early stage, where command line options haven't been
 * parsed yet */
static int
is_secure_env(void)
{
	if (!argv_bk)
		return 0;

	size_t i;
	for(i = 0; argv_bk[i]; i++) {
		if (*argv_bk[i] == '-' && (strncmp(argv_bk[i], "--secure-env", 12) == 0))
			return 1;
	}

	return 0;
}

/* Retrieve user groups
 * Return an array with the ID's of groups to which the user belongs
 * NGROUPS is set to the number of groups
 * NOTE: getgroups(3) does not include the user's main group
 * We use getgroups(3) on TERMUX because getgrouplist(3) always returns zero groups */
static gid_t *
get_user_groups(const char *name, const gid_t gid, int *ngroups)
{
	int n = *ngroups;

#if defined(__TERMUX__)
	UNUSED(name); UNUSED(gid);
	gid_t *g = (gid_t *)xnmalloc(NGROUPS_MAX, sizeof(g));
	if ((n = getgroups(NGROUPS_MAX, g)) == -1) {
		_err('e', PRINT_PROMPT, "%s: getgroups: %s\n", PROGRAM_NAME, strerror(errno));
		free(g);
		return (gid_t *)NULL;
	}
	if (NGROUPS_MAX > n) /* Reduce array to actual amount of groups (N) */
		g = (gid_t *)xrealloc(g, (size_t)n * sizeof(g));
#elif defined(__linux__)
	n = 0;
	getgrouplist(name, gid, NULL, &n);
	gid_t *g = (gid_t *)xnmalloc((size_t)n, sizeof(g));
	getgrouplist(name, gid, g, &n);
#else
	n = NGROUPS_MAX;
	gid_t *g = (gid_t *)xnmalloc((size_t)n, sizeof(g));
# if defined(__APPLE__)
	getgrouplist(name, (int)gid, (int *)g, &n);
# else
	getgrouplist(name, gid, g, &n);
# endif /* __APPLE__ */

	if (NGROUPS_MAX > n)
		g = (gid_t *)xrealloc(g, (size_t)n * sizeof(g));
#endif /* __TERMUX__ */

	*ngroups = n;
	return g;
}

/* Get user data from environment variables. Used only in case getpwuid() failed */
static struct user_t
get_user_data_env(void)
{
	struct user_t tmp_user;
	tmp_user.home = (char *)NULL;
	/* If secure-env, do not fallback to environment variables */
	int sec_env = is_secure_env();
	char *t = sec_env == 0 ? getenv("HOME") : (char *)NULL;

	if (t) {
		char *p = realpath(t, NULL);
		char *h = p ? p : t;
		tmp_user.home = savestring(h, strlen(h));
		free(p);
	}

	struct stat a;
	if (!tmp_user.home || !*tmp_user.home || stat(tmp_user.home, &a) == -1
	|| !S_ISDIR(a.st_mode)) {
		free(tmp_user.home);
		fprintf(stderr, "%s: Home directory not found. Exiting.\n", PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	tmp_user.home_len = strlen(tmp_user.home);
	t = sec_env == 0 ? getenv("USER") : (char *)NULL;;
	tmp_user.name = t ? savestring(t, strlen(t)) : (char *)NULL;

	tmp_user.gid = getgid();
	tmp_user.ngroups = 0;
	if (tmp_user.name && tmp_user.gid != (gid_t)-1)
		tmp_user.groups = get_user_groups(tmp_user.name, tmp_user.gid, &tmp_user.ngroups);
	else
		tmp_user.groups = (gid_t *)NULL;

	t = sec_env == 0 ? getenv("SHELL") : (char *)NULL;
	tmp_user.shell = t ? savestring(t, strlen(t)) : (char *)NULL;

	return tmp_user;
}

/* Return the value of the environment variable S, allocated via malloc if
 * ALLOC is 1, or just a pointer to the value if 0 */
char *
xgetenv(const char *s, const int alloc)
{
	if (!s || !*s)
		return (char *)NULL;

	char *p = getenv(s);
	if (p && *p)
		return alloc == 1 ? savestring(p, strlen(p)) : p;

	return (char *)NULL;
}

/* Retrieve user information and store it in a user_t struct for later access */
struct user_t
get_user_data(void)
{
	struct passwd *pw = (struct passwd *)NULL;
	struct user_t tmp_user;

	errno = 0;
	tmp_user.uid = geteuid();
	pw = getpwuid(tmp_user.uid);
	if (!pw) { /* Fallback to environment variables (if not secure-env) */
		_err('e', PRINT_PROMPT, "%s: getpwuid: %s\n", PROGRAM_NAME, strerror(errno));
		return get_user_data_env();
	}

	tmp_user.uid = pw->pw_uid;
	tmp_user.gid = pw->pw_gid;
	tmp_user.ngroups = 0;
	tmp_user.groups = get_user_groups(pw->pw_name, pw->pw_gid, &tmp_user.ngroups);

	struct stat a;
	char *homedir = (char *)NULL;
	if (is_secure_env() == 0) {
		char *q = xgetenv("USER", 1);
		tmp_user.name = q ? q : savestring(pw->pw_name, strlen(pw->pw_name));
		q = xgetenv("SHELL", 1);
		tmp_user.shell = q ? q : savestring(pw->pw_shell, strlen(pw->pw_shell));
		q = xgetenv("HOME", 0);
		homedir = q ? q : pw->pw_dir;

		if (homedir == q && q && (stat(q, &a) == -1 || !S_ISDIR(a.st_mode))) {
			_err('e', PRINT_PROMPT, _("%s: %s: Home directory not found\n"
				"Falling back to %s\n"), PROGRAM_NAME, q, pw->pw_dir);
			homedir = pw->pw_dir;
		}
	} else {
		tmp_user.name = savestring(pw->pw_name, strlen(pw->pw_name));
		tmp_user.shell = savestring(pw->pw_shell, strlen(pw->pw_shell));
		homedir = pw->pw_dir;
	}

	if (homedir == pw->pw_dir && (!homedir
	|| stat(homedir, &a) == -1 || !S_ISDIR(a.st_mode))) {
		fprintf(stderr, _("%s: %s: Invalid home directory in the password "
			"database.\nSomething is really wrong. Exiting.\n"), PROGRAM_NAME,
			homedir ? homedir : "!");
		exit(errno);
	}

	if (!homedir) {
		fprintf(stderr, _("%s: Home directory not found.\n"
			"Something is really wrong. Exiting.\n"), PROGRAM_NAME);
		exit(errno);
	}

	/* Sometimes (FreeBSD for example) the home directory, as returned by the
	 * passwd struct, is a symlink, in which case we want to resolve it
	 * See https://lists.freebsd.org/pipermail/freebsd-arm/2016-July/014404.html */
	char *r = realpath(homedir, NULL);
	if (r) {
		tmp_user.home_len = strlen(r);
		tmp_user.home = savestring(r, tmp_user.home_len);
		free(r);
	} else {
		tmp_user.home_len = strlen(homedir);
		tmp_user.home = savestring(homedir, tmp_user.home_len);
	}

	return tmp_user;
}

#ifndef _DIRENT_HAVE_D_TYPE
static int
check_tag(char *name)
{
	if (!name || !*name)
		return EXIT_FAILURE;

	char dir[PATH_MAX];
	snprintf(dir, PATH_MAX, "%s/%s", tags_dir, name);

	struct stat a;
	if (stat(dir, &a) == -1 || !S_ISDIR(a.st_mode))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
#endif /* _DIRENT_HAVE_D_TYPE */

void
load_tags(void)
{
	if (!tags_dir || !*tags_dir) return;

	struct dirent **t = (struct dirent **)NULL;
	int i, n = scandir(tags_dir, &t, NULL, alphasort);
	if (n == -1) return;

	if (n <= 2) {
		for (i = 0; i < n; i++)
			free(t[i]);
		free(t);
		return;
	}

	tags_n = 0;
	tags = (char **)xnmalloc((size_t)n + 2, sizeof(char **));
	for (i = 0; i < n; i++) {
#ifdef _DIRENT_HAVE_D_TYPE
		if (t[i]->d_type != DT_DIR) {
			free(t[i]);
			continue;
		}
#else
		if (check_tag(t[i]->d_name) == EXIT_FAILURE)
			continue;
#endif /* _DIRENT_HAVE_D_TYPE */
		if (SELFORPARENT(t[i]->d_name)) {
			free(t[i]);
			continue;
		}
		tags[tags_n] = savestring(t[i]->d_name, strlen(t[i]->d_name));
		tags_n++;
		free(t[i]);
	}
	free(t);

	tags[tags_n] = (char *)NULL;
}

/* Reconstruct the jump database from the database file */
void
load_jumpdb(void)
{
	if (xargs.no_dirjump == 1 || !config_ok || !config_dir)
		return;

	char *jump_file = (char *)xnmalloc(config_dir_len + 12, sizeof(char));
	snprintf(jump_file, config_dir_len + 12, "%s/jump.clifm", config_dir);

	int fd;
	FILE *fp = open_fstream_r(jump_file, &fd);
	if (!fp) {
		free(jump_file);
		return;
	}

	char tmp_line[PATH_MAX];
	size_t jump_lines = 0;

	while (fgets(tmp_line, (int)sizeof(tmp_line), fp)) {
		if (*tmp_line != '\n' && *tmp_line >= '0' && *tmp_line <= '9')
			jump_lines++;
	}

	if (!jump_lines) {
		free(jump_file);
		close_fstream(fp, fd);
		return;
	}

	jump_db = (struct jump_t *)xnmalloc(jump_lines + 2, sizeof(struct jump_t));

	fseek(fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!*line || *line == '\n' || *line == '#')
			continue;
		if (*line == '@') {
			if (line[line_len - 1] == '\n')
				line[line_len - 1] = '\0';
			if (is_number(line + 1)) {
				int a = atoi(line + 1);
				jump_total_rank = a == INT_MIN ? 0 : a;
			}
			continue;
		}
		if (*line < '0' || *line > '9')
			continue;

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		char *tmp = strchr(line, ':');
		if (!tmp)
			continue;

		*tmp = '\0';
		if (!*(++tmp))
			continue;

		int visits = 1;

		if (is_number(line)) {
			visits = atoi(line);
			if (visits == INT_MIN)
				visits = 0;
		}

		char *tmpb = strchr(tmp, ':');
		if (!tmpb)
			continue;

		*tmpb = '\0';

		if (!*(++tmpb))
			continue;

		time_t first = 0;

		if (is_number(tmp)) {
			int a = atoi(tmp);
			first = a == INT_MIN ? 0 : (time_t)a;
		}

		char *tmpc = strchr(tmpb, ':');
		if (!tmpc)
			continue;

		*tmpc = '\0';

		if (!*(++tmpc))
			continue;

		/* Purge the database from non-existent directories */
		if (conf.purge_jumpdb == 1 && access(tmpc, F_OK) == -1)
			continue;

		jump_db[jump_n].visits = (size_t)visits;
		jump_db[jump_n].first_visit = first;

		if (is_number(tmpb)) {
			int a = atoi(tmpb);
			jump_db[jump_n].last_visit = a == INT_MIN ? 0 : (time_t)a;
		} else {
			jump_db[jump_n].last_visit = 0; /* UNIX Epoch */
		}

		jump_db[jump_n].keep = 0;
		jump_db[jump_n].rank = 0;
		jump_db[jump_n].len = strlen(tmpc);
		jump_db[jump_n].path = savestring(tmpc, jump_db[jump_n].len);
		jump_n++;
	}

	close_fstream(fp, fd);
	free(line);
	free(jump_file);

	if (!jump_n) {
		free(jump_db);
		jump_db = (struct jump_t *)NULL;
		return;
	}

	jump_db[jump_n].path = (char *)NULL;
	jump_db[jump_n].len = 0;
	jump_db[jump_n].rank = 0;
	jump_db[jump_n].keep = 0;
	jump_db[jump_n].visits = 0;
	jump_db[jump_n].first_visit = -1;
}

int
load_bookmarks(void)
{
	if (create_bm_file() == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (!bm_file)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fstream_r(bm_file, &fd);
	if (!fp)
		return EXIT_FAILURE;

	size_t bm_total = 0;
	char tmp_line[256];
	while (fgets(tmp_line, (int)sizeof(tmp_line), fp)) {
		if (!*tmp_line || *tmp_line == '#' || *tmp_line == '\n')
			continue;
		bm_total++;
	}

	if (bm_total == 0) {
		close_fstream(fp, fd);
		return EXIT_SUCCESS;
	}

	fseek(fp, 0L, SEEK_SET);

	bookmarks = (struct bookmarks_t *)xnmalloc(bm_total + 1,
	    sizeof(struct bookmarks_t));
	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!*line || *line == '\n' || *line == '#')
			continue;
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		/* Neither hotkey nor name, but only a path */
		if (*line == '/') {
			bookmarks[bm_n].shortcut = (char *)NULL;
			bookmarks[bm_n].name = (char *)NULL;
			bookmarks[bm_n].path = savestring(line, strlen(line));
			bm_n++;
			continue;
		}

		if (*line == '[') {
			char *p = line;
			p++;
			char *tmp = strchr(line, ']');
			if (!tmp) {
				bookmarks[bm_n].shortcut = (char *)NULL;
				bookmarks[bm_n].name = (char *)NULL;
				bookmarks[bm_n].path = (char *)NULL;
				bm_n++;
				continue;
			}

			*tmp = '\0';

			bookmarks[bm_n].shortcut = savestring(p, strlen(p));

			tmp++;
			p = tmp;
			tmp = strchr(p, ':');

			if (!tmp) {
				bookmarks[bm_n].name = (char *)NULL;
				if (*p)
					bookmarks[bm_n].path = savestring(p, strlen(p));
				else
					bookmarks[bm_n].path = (char *)NULL;
				bm_n++;
				continue;
			}

			*tmp = '\0';
			bookmarks[bm_n].name = savestring(p, strlen(p));

			if (!*(++tmp)) {
				bookmarks[bm_n].path = (char *)NULL;
				bm_n++;
				continue;
			}

			bookmarks[bm_n].path = savestring(tmp, strlen(tmp));
			bm_n++;
			continue;
		}

		/* No shortcut. Let's try with name */
		bookmarks[bm_n].shortcut = (char *)NULL;
		char *tmp = strchr(line, ':');

		/* No name either */
		if (!tmp) {
			bookmarks[bm_n].name = (char *)NULL;
			bookmarks[bm_n].path = (char *)NULL;
			bm_n++;
			continue;
		}

		*tmp = '\0';
		bookmarks[bm_n].name = savestring(line, strlen(line));

		if (!*(++tmp)) {
			bookmarks[bm_n].path = (char *)NULL;
			bm_n++;
			continue;
		} else {
			bookmarks[bm_n].path = savestring(tmp, strlen(tmp));
			bm_n++;
		}
	}

	free(line);
	close_fstream(fp, fd);

	if (bm_n == 0) {
		free(bookmarks);
		bookmarks = (struct bookmarks_t *)NULL;
		return EXIT_SUCCESS;
	}

	bookmarks[bm_n].name = (char *)NULL;
	bookmarks[bm_n].path = (char *)NULL;
	bookmarks[bm_n].shortcut = (char *)NULL;

	return EXIT_SUCCESS;
}

/* Store actions from the actions file into a struct */
int
load_actions(void)
{
	if (!config_ok || !actions_file)
		return EXIT_FAILURE;

	/* Free the actions struct array */
	if (actions_n) {
		int i = (int)actions_n;
		while (--i >= 0) {
			free(usr_actions[i].name);
			free(usr_actions[i].value);
		}

		free(usr_actions);
		usr_actions = (struct actions_t *)xnmalloc(1, sizeof(struct actions_t));
		actions_n = 0;
	}

	/* Open the actions file */
	int fd;
	FILE *fp = open_fstream_r(actions_file, &fd);
	if (!fp)
		return EXIT_FAILURE;

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!line || !*line || *line == '#' || *line == '\n')
			continue;
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		char *tmp = (char *)NULL;
		tmp = strrchr(line, '=');
		if (!tmp)
			continue;

		/* Now copy left and right value of each action into the actions struct */
		usr_actions = xrealloc(usr_actions, (size_t)(actions_n + 1)
					* sizeof(struct actions_t));
		usr_actions[actions_n].value = savestring(tmp + 1, strlen(tmp + 1));
		*tmp = '\0';
		usr_actions[actions_n].name = savestring(line, strlen(line));
		actions_n++;
	}

	free(line);
	close_fstream(fp, fd);
	return EXIT_SUCCESS;
}

static inline void
reset_remotes_values(const size_t i)
{
	remotes[i].name = (char *)NULL;
	remotes[i].desc = (char *)NULL;
	remotes[i].mountpoint = (char *)NULL;
	remotes[i].mount_cmd = (char *)NULL;
	remotes[i].unmount_cmd = (char *)NULL;
	remotes[i].auto_unmount = 0;
	remotes[i].auto_mount = 0;
	remotes[i].mounted = 0;
}

/* Load remotes information from REMOTES_FILE */
int
load_remotes(void)
{
	if (!remotes_file || !*remotes_file)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fstream_r(remotes_file, &fd);
	if (!fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s\n", remotes_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t n = 0;
	remotes = (struct remote_t *)xnmalloc(n + 1, sizeof(struct remote_t));
	reset_remotes_values(n);

	size_t line_sz = 0;
	char *line = (char *)NULL;

	while (getline(&line, &line_sz, fp) > 0) {
		if (!*line || *line == '#' || *line == '\n')
			continue;
		if (*line == '[') {
			if (remotes[n].name)
				n++;
			remotes = (struct remote_t *)xrealloc(
					remotes, (n + 2) * sizeof(struct remote_t));
			reset_remotes_values(n);

			char *name = strbtw(line, '[', ']');
			if (!name)
				continue;
			if (!*name) {
				free(name);
				name = (char *)NULL;
				continue;
			}
			remotes[n].name = (char *)xrealloc(remotes[n].name,
							(strlen(name) + 1) * sizeof(char));
			strcpy(remotes[n].name, name);
			free(name);
			name = (char *)NULL;
		}

		if (!remotes[n].name)
			continue;

		char *ret = strchr(line, '=');
		if (!ret)
			continue;
		if (!*(++ret))
			continue;

		size_t ret_len = strlen(ret);
		if (ret_len > 0 && ret[ret_len - 1] == '\n') {
			ret_len--;
			ret[ret_len] = '\0';
		}

		char *deq_str = remove_quotes(ret);
		if (deq_str)
			ret = deq_str;

		if (strncmp(line, "Comment=", 8) == 0) {
			remotes[n].desc = (char *)xrealloc(remotes[n].desc,
							(ret_len + 1) * sizeof(char));
			strcpy(remotes[n].desc, ret);
		} else if (strncmp(line, "Mountpoint=", 11) == 0) {
			char *tmp = (char *)NULL;
			if (*ret == '~')
				tmp = tilde_expand(ret);
			remotes[n].mountpoint = (char *)xrealloc(remotes[n].mountpoint,
								((tmp ? strlen(tmp) : ret_len) + 1)
								* sizeof(char));
			strcpy(remotes[n].mountpoint, tmp ? tmp : ret);
			free(tmp);
			if (count_dir(remotes[n].mountpoint, CPOP) > 2)
				remotes[n].mounted = 1;
		} else if (strncmp(line, "MountCmd=", 9) == 0) {
			int replaced = 0;
			if (remotes[n].mountpoint) {
				char *rep = replace_substr(ret, "%m", remotes[n].mountpoint);
				if (rep) {
					remotes[n].mount_cmd = (char *)xrealloc(
								remotes[n].mount_cmd,
								(strlen(rep) + 1) * sizeof(char));
					strcpy(remotes[n].mount_cmd, rep);
					free(rep);
					replaced = 1;
				}
			}

			if (!replaced) {
				remotes[n].mount_cmd = (char *)xrealloc(remotes[n].mount_cmd,
									(ret_len + 1) * sizeof(char));
				strcpy(remotes[n].mount_cmd, ret);
			}
		} else if (strncmp(line, "UnmountCmd=", 11) == 0) {
			int replaced = 0;
			if (remotes[n].mountpoint) {
				char *rep = replace_substr(ret, "%m", remotes[n].mountpoint);
				if (rep) {
					remotes[n].unmount_cmd = (char *)xrealloc(
							remotes[n].unmount_cmd,
							(strlen(rep) + 1) * sizeof(char));
					strcpy(remotes[n].unmount_cmd, rep);
					free(rep);
					replaced = 1;
				}
			}

			if (!replaced) {
				remotes[n].unmount_cmd = (char *)xrealloc(remotes[n].unmount_cmd,
								(ret_len + 1) * sizeof(char));
				strcpy(remotes[n].unmount_cmd, ret);
			}
		} else if (strncmp(line, "AutoUnmount=", 12) == 0) {
			if (strcmp(ret, "true") == 0)
				remotes[n].auto_unmount = 1;
		} else {
			if (strncmp(line, "AutoMount=", 10) == 0) {
				if (strcmp(ret, "true") == 0)
					remotes[n].auto_mount = 1;
			}
		}
	}

	free(line);
	close_fstream(fp, fd);

	if (remotes[n].name) {
		++n;
		remotes[n].name = (char *)NULL;
	}

	remotes_n = n;
	return EXIT_SUCCESS;
}

static void
unset_prompt_values(const size_t n)
{
	prompts[n].name = (char *)NULL;
	prompts[n].regular = (char *)NULL;
	prompts[n].warning = (char *)NULL;
	prompts[n].notifications = DEF_PROMPT_NOTIF;
	prompts[n].warning_prompt_enabled = DEF_WARNING_PROMPT;
}

static char *
set_prompts_file(void)
{
	if (!config_dir_gral || !*config_dir_gral)
		return (char *)NULL;

	struct stat a;
	char *f = (char *)xnmalloc(strlen(config_dir_gral) + 15, sizeof(char));
	sprintf(f, "%s/prompts.clifm", config_dir_gral);

	if (stat(f, &a) != -1 && S_ISREG(a.st_mode))
		return f;

	if (!data_dir || !*data_dir)
		goto ERROR;

	char t[PATH_MAX];
	snprintf(t, sizeof(t), "%s/%s/prompts.clifm", data_dir, PNL);
	if (stat(t, &a) == -1 || !S_ISREG(a.st_mode))
		goto ERROR;

	char *cmd[] = {"cp", t, f, NULL};
	int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);
	if (ret != EXIT_SUCCESS) {
		free(f);
		return (char *)NULL;
	}
	return f;

ERROR:
	free(f);
	return (char *)NULL;
}

/* Load prompts from PROMPTS_FILE */
int
load_prompts(void)
{
	free_prompts();
	free(prompts_file);
	prompts_file = set_prompts_file();
	if (!prompts_file || !*prompts_file)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fstream_r(prompts_file, &fd);
	if (!fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s\n", prompts_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t n = 0;
	prompts = (struct prompts_t *)xnmalloc(n + 1, sizeof(struct prompts_t));
	unset_prompt_values(n);

	size_t line_sz = 0;
	char *line = (char *)NULL;

	while (getline(&line, &line_sz, fp) > 0) {
		if (!*line || *line == '#' || *line == '\n')
			continue;
		if (*line == '[') {
			if (prompts[n].name)
				n++;
			prompts = (struct prompts_t *)xrealloc(
					prompts, (n + 2) * sizeof(struct prompts_t));
			unset_prompt_values(n);

			char *name = strbtw(line, '[', ']');
			if (!name)
				continue;
			if (!*name) {
				free(name);
				name = (char *)NULL;
				continue;
			}
			prompts[n].name = (char *)xrealloc(prompts[n].name,
							(strlen(name) + 1) * sizeof(char));
			strcpy(prompts[n].name, name);
			free(name);
			name = (char *)NULL;
		}

		if (!prompts[n].name)
			continue;

		char *ret = strchr(line, '=');
		if (!ret)
			continue;
		if (!*(++ret))
			continue;

		size_t ret_len = strlen(ret);
		if (ret_len > 0 && ret[ret_len - 1] == '\n') {
			ret_len--;
			ret[ret_len] = '\0';
		}

		if (*line == 'N' && strncmp(line, "Notifications=", 14) == 0) {
			if (*ret == 't' && strcmp(ret, "true") == 0)
				prompts[n].notifications = 1;
			else if (*ret == 'f' && strcmp(ret, "false") == 0)
				prompts[n].notifications = 0;
			else
				prompts[n].notifications = DEF_PROMPT_NOTIF;
			continue;
		}

		char *deq_str = remove_quotes(ret);
		if (deq_str)
			ret = deq_str;

		if (strncmp(line, "RegularPrompt=", 14) == 0) {
			prompts[n].regular = (char *)xrealloc(prompts[n].regular,
							(ret_len + 1) * sizeof(char));
			strcpy(prompts[n].regular, ret);
			continue;
		}

		if (strncmp(line, "EnableWarningPrompt=", 20) == 0) {
			if (*ret == 't' && strcmp(ret, "true") == 0)
				prompts[n].warning_prompt_enabled = 1;
			else if (*ret == 'f' && strcmp(ret, "false") == 0)
				prompts[n].warning_prompt_enabled = 0;
			else
				prompts[n].warning_prompt_enabled = DEF_WARNING_PROMPT;
			continue;
		}

		if (strncmp(line, "WarningPrompt=", 14) == 0) {
			prompts[n].warning = (char *)xrealloc(prompts[n].warning,
								(ret_len + 1) * sizeof(char));
			strcpy(prompts[n].warning, ret);
		}
	}

	free(line);
	close_fstream(fp, fd);

	if (prompts[n].name) {
		++n;
		prompts[n].name = (char *)NULL;
	}

	prompts_n = n;
	if (conf.encoded_prompt)
		expand_prompt_name(conf.encoded_prompt);
	return EXIT_SUCCESS;
}

static char *
get_home_sec_env(void)
{
	struct passwd *pw = (struct passwd *)NULL;

	uid_t u = geteuid();
	pw = getpwuid(u);
	if (!pw) {
		_err('e', PRINT_PROMPT, "%s: getpwuid: %s\n", PROGRAM_NAME, strerror(errno));
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
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: Could not retrieve the home directory\n", PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	tmp_dir = savestring(P_tmpdir, P_tmpdir_len);

	size_t mime_file_len = 0;

	if (alt_preview_file && preview == 1) {
		mime_file = savestring(alt_preview_file, strlen(alt_preview_file));
	} else {
		mime_file_len = strlen(homedir) + (alt_profile ? strlen(alt_profile) : 7) + 40;
		mime_file = (char *)xnmalloc(mime_file_len, sizeof(char));
		sprintf(mime_file, "%s/.config/clifm/profiles/%s/%s.clifm",
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
	if (preview == 1 && *filename == '~')
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
set_sort(char *arg)
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

static char *
resolve_positional_param(char *file)
{
	struct stat attr;
	int url = 0;
	char *_path = file, *_exp_path = (char *)NULL;

	if (*file == '.') {
		char _tmp[PATH_MAX];
		if (realpath(file, _tmp) == NULL || !*_tmp)
			_err('e', PRINT_PROMPT, _("%s: Error expanding path\n"), PROGRAM_NAME);
		else
			_exp_path = savestring(_tmp, strlen(_tmp));
	} else {
		_exp_path = tilde_expand(file);
	}

	if (!_exp_path) {
		fprintf(stderr, _("%s: Error expanding path\n"), PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	if (IS_FILE_URI(_path)) {
		_path = file + 7;
		if (stat(_path, &attr) == -1) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
				_exp_path, strerror(errno));
			free(_exp_path);
			exit(errno);
		}
	} else if (is_url(_exp_path) == EXIT_SUCCESS) {
		url = 1;
	} else {
		if (stat(_exp_path, &attr) == -1) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
				_exp_path, strerror(errno));
			free(_exp_path);
			exit(errno);
		}

		if (!S_ISDIR(attr.st_mode)) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
				_exp_path, strerror(ENOTDIR));
			free(_exp_path);
			exit(ENOTDIR);
		}
	}

	free(_exp_path);

	if (url == 1 || !S_ISDIR(attr.st_mode))
		open_reg_exit(_path, url, 0);

	xargs.path = 1;
	return _path;
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
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
				file, strerror(errno));
			exit(errno);
		}
		url = 0;
		goto RUN;
	}

	if (is_url(_path) == EXIT_FAILURE) {
		url = 0;
		if (*_path != '~' && stat(_path, &attr) == -1) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
				_path, strerror(errno));
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
			fprintf(stderr, _("%s: %s: Error expanding tilde\n"), PROGRAM_NAME, file);
			exit(EXIT_FAILURE);
		}
	} else {
		p = savestring(file, strlen(file));
	}

	struct stat a;
	if (stat(p, &a) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, p, strerror(errno));
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

/* Evaluate external arguments, if any, and change initial variables to
 * its corresponding value */
void
external_arguments(int argc, char **argv)
{
	/* Disable automatic error messages to be able to handle them ourselves
	 * via the '?' case in the switch */
	opterr = optind = 0;

	/* Link long (--option) and short options (-o) for the getopt_long function */
	static struct option longopts[] = {
		{"no-hidden", no_argument, 0, 'a'},
		{"show-hidden", no_argument, 0, 'A'},
		{"bookmarks-file", required_argument, 0, 'b'},
		{"config-file", required_argument, 0, 'c'},
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
		 * with short options (a-Z == 65-122) */
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
		{"enable-logs", no_argument, 0, 213},
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
		{"apparent-size", no_argument, 0, 248},
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
	    {0, 0, 0, 0}
	};

	int optc;
	/* Variables to store arguments to options */
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
	int open_prev_mode = 0;

	while ((optc = getopt_long(argc, argv,
		    "+aAb:c:D:eEfFgGhHiIk:lLmoOp:P:rsStUuvw:Wxyz:", longopts, (int *)0)) != EOF) {
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
		case 213: xargs.logs = conf.logs_enabled = 1;	break;

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

		case 232: xargs.printsel = 1; break;
		case 233:
#ifndef _NO_SUGGESTIONS
			xargs.suggestions = conf.suggestions = 0;
#endif /* !_NO_SUGGESTIONS */
			break;
//		case 234: xargs.autojump = autojump = 0; break;
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
		case 248: xargs.apparent_size = conf.apparent_size = 1; break;
		case 249: xargs.history = 0; break;
		case 250:
#ifndef _NO_FZF
			xargs.fzytab = 1; fzftab = 1; tabmode = FZY_TAB; break;
#else
			fprintf(stderr, _("%s: fzy-tab: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* _NO_FZF */
		case 251: xargs.refresh_on_resize = 0; break;
		case 252: {
			int a = atoi(optarg);
			if (!is_number(optarg) || a < 0 || a > 3) {
				fprintf(stderr, "%s: bell: valid options are 0:none, 1:audible, "
					"2:visible (requires readline >= 8.1), 3:flash. Defaults to 'visible', "
					"and, if not possible, 'none'.\n", PROGRAM_NAME);
				exit(EXIT_FAILURE);
			}
			xargs.bell_style = a; break;
			}
		case 253: xargs.fuzzy_match = conf.fuzzy_match = 1; break;
		case 254:
#ifndef _NO_FZF
			xargs.smenutab = 1; fzftab = 1; tabmode = SMENU_TAB; break;
#else
			fprintf(stderr, _("%s: smenu-tab: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
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

		case 257: xargs.desktop_notifications = conf.desktop_notifications = 1; break;
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
			fprintf(stderr, _("%s: fzf-preview: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
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
			fprintf(stderr, _("%s: fzf-tab: %s\n"), PROGRAM_NAME, _(NOT_AVAILABLE));
			exit(EXIT_FAILURE);
#endif /* !_NO_FZF */

		case 264:
#ifdef __linux__
			xargs.si = 1; break;
#else
			fprintf(stderr, _("%s: si: This option is available on Linux "
				"only\n"), PROGRAM_NAME);
			exit(EXIT_FAILURE);
#endif /* __linux__ */

		case 265:
			if (!optarg || !*optarg || *optarg == '-')
				err_arg_required("--data-dir");

			get_data_dir_from_path(optarg);
			break;

		case 266: {
			int a = optarg ? atoi(optarg) : -1;
			if (!optarg || a < 1 || a > FUZZY_ALGO_MAX) {
				fprintf(stderr, "%s: fuzzy-algo: Valid arguments are 1 "
					"or 2\n", PROGRAM_NAME);
				exit(EXIT_FAILURE);
			}
			xargs.fuzzy_match_algo = conf.fuzzy_match_algo = a;
			}
			break;

		case 267: set_custom_selfile(optarg); break;

		case 'a': conf.show_hidden = xargs.hidden = 0; break;
		case 'A': conf.show_hidden = xargs.hidden = 1; break;
		case 'b': xargs.bm_file = 1; bm_value = optarg; break;
		case 'c': xargs.config = 1; config_value = optarg; break;
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

	/* Positional parameters. If a directory, use it as CliFM's starting path */
	int i = optind;
	if (argv[i])
		path_value = resolve_positional_param(argv[i]);

	if (bm_value) {
		char *bm_exp = (char *)NULL;

		if (*bm_value == '~') {
			bm_exp = tilde_expand(bm_value);
			bm_value = bm_exp;
		}

		if (access(bm_value, R_OK) == -1) {
			_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
				"Falling back to the default bookmarks file\n"),
			    PROGRAM_NAME, bm_value, strerror(errno));
		} else {
			alt_bm_file = savestring(bm_value, strlen(bm_value));
			_err('n', PRINT_PROMPT, _("%s: Loaded alternative "
				"bookmarks file\n"), PROGRAM_NAME);
		}
	}

	if (virtual_dir_value) {
		stdin_tmp_dir = savestring(virtual_dir_value, strlen(virtual_dir_value));
		setenv("CLIFM_VIRTUAL_DIR", stdin_tmp_dir, 1);
	}

	if (alt_dir_value) {
		char *dir_exp = (char *)NULL;

		if (*alt_dir_value == '~') {
			dir_exp = tilde_expand(alt_dir_value);
			alt_dir_value = dir_exp;
		}

		int dir_ok = 1;
		struct stat attr;
		if (stat(alt_dir_value, &attr) == -1) {
			char *tmp_cmd[] = {"mkdir", "-p", alt_dir_value, NULL};
			int ret = launch_execve(tmp_cmd, FOREGROUND, E_NOSTDERR);
			if (ret != EXIT_SUCCESS) {
				_err('e', PRINT_PROMPT, _("%s: %s: Cannot create directory "
					"(error %d)\nFalling back to default configuration directory\n"),
					PROGRAM_NAME, alt_dir_value, ret);
				dir_ok = 0;
			}
		}

		if (access(alt_dir_value, W_OK) == -1) {
			if (dir_ok) {
				_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
					"Falling back to default configuration directory\n"),
					PROGRAM_NAME, alt_dir_value, strerror(errno));
			}
		} else {
			alt_config_dir = savestring(alt_dir_value, strlen(alt_dir_value));
			_err(ERR_NO_LOG, PRINT_PROMPT, _("%s: %s: Using alternative "
				"configuration directory\n"), PROGRAM_NAME, alt_config_dir);
		}

		free(dir_exp);
	}

	if (kbinds_value) {
		char *kbinds_exp = (char *)NULL;
		if (*kbinds_value == '~') {
			kbinds_exp = tilde_expand(kbinds_value);
			kbinds_value = kbinds_exp;
		}

		if (access(kbinds_value, R_OK) == -1) {
			_err('e', PRINT_PROMPT, _("%s: %s: %s\n"
				"Falling back to the default keybindings file\n"),
			    PROGRAM_NAME, kbinds_value, strerror(errno));
		} else {
			alt_kbinds_file = savestring(kbinds_value, strlen(kbinds_value));
			_err('n', PRINT_PROMPT, _("%s: Loaded alternative "
				"keybindings file\n"), PROGRAM_NAME);
		}

		free(kbinds_exp);
	}

	if (xargs.config && config_value) {
		char *config_exp = (char *)NULL;

		if (*config_value == '~') {
			config_exp = tilde_expand(config_value);
			config_value = config_exp;
		}

		if (access(config_value, R_OK) == -1) {
			_err('e', PRINT_PROMPT, _("%s: %s: %s\nFalling back to default\n"),
				PROGRAM_NAME, config_value, strerror(errno));
			xargs.config = -1;
		} else {
			alt_config_file = savestring(config_value, strlen(config_value));
			_err('n', PRINT_PROMPT, _("%s: Loaded alternative "
				"configuration file\n"), PROGRAM_NAME);
		}

		free(config_exp);
	}

	if (path_value) {
		char *path_exp = (char *)NULL;
		char path_tmp[PATH_MAX];

		if (*path_value == '~') {
			path_exp = tilde_expand(path_value);
			xstrsncpy(path_tmp, path_exp, PATH_MAX);
		} else if (*path_value != '/') {
			if (*path_value == '.') {
				char _tmp[PATH_MAX];
				*_tmp = '\0';
				char *p = realpath(path_value, _tmp);
				if (!p) {
					_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
						path_value, strerror(errno));
					exit(errno);
				}
				xstrsncpy(path_tmp, p, PATH_MAX);
			} else {
				snprintf(path_tmp, PATH_MAX - 1, "%s/%s", getenv("PWD"), path_value);
			}
		} else {
			xstrsncpy(path_tmp, path_value, PATH_MAX);
		}

		if (xchdir(path_tmp, SET_TITLE) == 0) {
			if (cur_ws == UNSET)
				cur_ws = DEF_CUR_WS;
			if (workspaces[cur_ws].path)
				free(workspaces[cur_ws].path);

			workspaces[cur_ws].path = savestring(path_tmp, strlen(path_tmp));
		} else { /* Error changing directory */
			if (xargs.list_and_quit == 1) {
				_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
					path_tmp, strerror(errno));
				exit(EXIT_FAILURE);
			}

			_err('w', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			    path_tmp, strerror(errno));
		}

		free(path_exp);
	}

#ifndef _NO_PROFILES
	if (alt_profile_value) {
		free(alt_profile);
		alt_profile = savestring(alt_profile_value, strlen(alt_profile_value));
	}
#endif
}

void
unset_xargs(void)
{
	xargs.apparent_size = UNSET;
	xargs.auto_open = UNSET;
	xargs.autocd = UNSET;
	xargs.autojump = UNSET;
	xargs.autols= UNSET;
	xargs.bell_style = UNSET;
	xargs.bm_file = UNSET;
	xargs.case_sens_dirjump = UNSET;
	xargs.case_sens_list = UNSET;
	xargs.case_sens_path_comp = UNSET;
	xargs.check_cap = UNSET;
	xargs.check_ext = UNSET;
	xargs.cd_on_quit = UNSET;
	xargs.classify = UNSET;
	xargs.clear_screen = UNSET;
	xargs.color_scheme = UNSET;
	xargs.colorize = UNSET;
	xargs.columns = UNSET;
	xargs.config = UNSET;
//	xargs.control_d_exits = UNSET;
	xargs.cwd_in_title = UNSET;
	xargs.desktop_notifications = UNSET;
	xargs.dirmap = UNSET;
	xargs.disk_usage = UNSET;
	xargs.disk_usage_analyzer = UNSET;
	xargs.eln_use_workspace_color = UNSET;
//	xargs.expand_bookmarks = UNSET;
	xargs.ext = UNSET;
	xargs.dirs_first = UNSET;
	xargs.files_counter = UNSET;
	xargs.follow_symlinks = UNSET;
	xargs.full_dir_size = UNSET;
	xargs.fuzzy_match = UNSET;
	xargs.fuzzy_match_algo = UNSET;
	xargs.fzf_preview = UNSET;
#ifndef _NO_FZF
	xargs.fzftab = UNSET;
	xargs.fzytab = UNSET;
	xargs.smenutab = UNSET;
#endif
	xargs.hidden = UNSET;
#ifndef _NO_HIGHLIGHT
	xargs.highlight = UNSET;
#endif
	xargs.history = UNSET;
	xargs.horizontal_list = UNSET;
#ifndef _NO_ICONS
	xargs.icons = UNSET;
	xargs.icons_use_file_color = UNSET;
#endif
	xargs.int_vars = UNSET;
	xargs.light = UNSET;
	xargs.list_and_quit = UNSET;
	xargs.logs = UNSET;
	xargs.longview = UNSET;
	xargs.max_dirhist = UNSET;
	xargs.max_path = UNSET;
	xargs.mount_cmd = UNSET;
	xargs.no_dirjump = UNSET;
	xargs.noeln = UNSET;
	xargs.only_dirs = UNSET;
	xargs.open = UNSET;
	xargs.pager = UNSET;
	xargs.path = UNSET;
	xargs.printsel = UNSET;
	xargs.refresh_on_empty_line = UNSET;
	xargs.refresh_on_resize = UNSET;
	xargs.restore_last_path = UNSET;
	xargs.rl_vi_mode = UNSET;
	xargs.secure_env_full = UNSET;
	xargs.secure_env = UNSET;
	xargs.secure_cmds = UNSET;
	xargs.sel_file = UNSET;
	xargs.share_selbox = UNSET;
	xargs.si = UNSET;
	xargs.sort = UNSET;
	xargs.sort_reverse = UNSET;
	xargs.splash = UNSET;
	xargs.stealth_mode = UNSET;

#ifndef _NO_SUGGESTIONS
	xargs.suggestions = UNSET;
#endif

	xargs.tips = UNSET;
#ifndef _NO_TRASH
	xargs.trasrm = UNSET;
#endif
	xargs.virtual_dir_full_paths = UNSET;
	xargs.vt100 = UNSET;
	xargs.welcome_message = UNSET;
	xargs.warning_prompt = UNSET;
}

/* Keep track of attributes of the shell. Make sure the shell is running
 * interactively as the foreground job before proceeding.
 * Based on https://www.gnu.org/software/libc/manual/html_node/Initializing-the-Shell.html#Initializing-the-Shell
 * */
void
init_shell(void)
{
	if (!isatty(STDIN_FILENO)) { /* Shell is not interactive */
		exit_code = handle_stdin();
		return;
	}

	if (getenv("CLIFM_OWN_CHILD")) {
		set_signals_to_ignore();
		own_pid = get_own_pid();
		tcgetattr(STDIN_FILENO, &shell_tmodes);
		return;
	}

	own_pid = get_own_pid();
	pid_t shell_pgid = 0;

	/* Loop until we are in the foreground */
	while (tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp()))
		kill(- shell_pgid, SIGTTIN);

	/* Ignore interactive and job-control signals */
	set_signals_to_ignore();
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	/* Put ourselves in our own process group */
	shell_pgid = getpid();
	setpgid(shell_pgid, shell_pgid);
/*	if (setpgid(shell_pgid, shell_pgid) < 0) {
		// This fails with EPERM when running as 'term -e clifm'
		_err(0, NOPRINT_PROMPT, "%s: setpgid: %s\n", PROGRAM_NAME, strerror(errno));
		exit(errno);
	} */

	/* Grab control of the terminal */
	tcsetpgrp(STDIN_FILENO, shell_pgid);

	/* Save default terminal attributes for shell */
	tcgetattr(STDIN_FILENO, &shell_tmodes);
}

/* Get current entries in the Selection Box, if any */
int
get_sel_files(void)
{
	if (!selfile_ok || !config_ok || !sel_file)
		return EXIT_FAILURE;

	size_t selnbk = sel_n;
	/* First, clear the sel array, in case it was already used */
	if (sel_n > 0) {
		int i = (int)sel_n;
		while (--i >= 0)
			free(sel_elements[i].name);
	}
	sel_n = 0;

	/* Open the tmp sel file and load its contents into the sel array */
	int fd;
	FILE *fp = open_fstream_r(sel_file, &fd);
	/*  sel_elements = xcalloc(1, sizeof(char *)); */
	if (!fp)
		return EXIT_FAILURE;

	struct stat a;
	/* Since this file contains only paths, PATH_MAX should be enough */
	char line[PATH_MAX];
	while (fgets(line, (int)sizeof(line), fp) != NULL) {
		size_t len = strlen(line);
		if (len == 0) continue;

		if (line[len - 1] == '\n') {
			len--;
			line[len] = '\0';
		}

		/* Remove the ending slash: fstatat() won't take a symlink to dir as
		 * a symlink (but as a dir), if the file name ends with a slash */
		if (len > 1 && line[len - 1] == '/') {
			len--;
			line[len] = '\0';
		}

		if (!*line || *line == '#' || len == 0)
			continue;

		if (fstatat(AT_FDCWD, line, &a, AT_SYMLINK_NOFOLLOW) == -1)
			continue;

		sel_elements = (struct sel_t *)xrealloc(sel_elements, (sel_n + 2) * sizeof(struct sel_t));
		sel_elements[sel_n].name = savestring(line, len);
		sel_elements[sel_n].size = (off_t)UNSET;
		/* Store device and inode number to identify later selected files
		 * and mark them in the files list */
		sel_devino = (struct devino_t *)xrealloc(sel_devino, (sel_n + 1) * sizeof(struct devino_t));
		sel_devino[sel_n].ino = a.st_ino;
		sel_devino[sel_n].dev = a.st_dev;
		sel_n++;
		sel_elements[sel_n].name = (char *)NULL;
		sel_elements[sel_n].size = (off_t)UNSET;
	}

	close_fstream(fp, fd);
	/* If previous and current amount of sel files don't match (mostly
	 * because some selected files were removed), recreate the selections
	 * file to reflect current state */
	if (selnbk != sel_n)
		save_sel();

	return EXIT_SUCCESS;
}

/* Store each path in CDPATH env variable into an array (CDPATHS)
 * Returns the number of paths found or zero if none */
size_t
get_cdpath(void)
{
	char *p = getenv("CDPATH");
	if (!p || !*p)
		return 0;

	char *t = savestring(p, strlen(p));

	/* Get each path in CDPATH */
	size_t i, n = 0, len = 0;
	for (i = 0; t[i]; i++) {
		/* Store path in CDPATH in a tmp buffer */
		char buf[PATH_MAX];
		while (t[i] && t[i] != ':') {
			buf[len] = t[i];
			len++;
			i++;
		}
		buf[len] = '\0';

		/* Make room in cdpaths for a new path */
		cdpaths = (char **)xrealloc(cdpaths, (n + 2) * sizeof(char *));

		/* Dump the buffer into the global cdpaths array */
		cdpaths[n] = savestring(buf, len);
		n++;

		len = 0;
		if (!t[i])
			break;
	}

	cdpaths[n] = (char *)NULL;

	free(t);
	return n;
}

static size_t
count_chars(const char *s, const char c)
{
	if (!s || !*s)
		return 0;

	size_t n = 0;
	while (*s) {
		if (*s == c)
			n++;
		s++;
	}

	return n;
}

/* Store all paths in the PATH environment variable into a globally
 * declared array (paths)
 * Returns the number of stored paths  */
size_t
get_path_env(void)
{
	char *ptr = (char *)NULL;
	int malloced_ptr = 0;

	/* If running on a sanitized environment, or PATH cannot be retrieved for
	 * whatever reason, get PATH value from a secure source */
	if (xargs.secure_cmds == 1 || xargs.secure_env == 1
	|| xargs.secure_env_full == 1 || !(ptr = getenv("PATH")) || !*ptr) {
		malloced_ptr = 1;
#ifdef _PATH_STDPATH
		ptr = savestring(_PATH_STDPATH, strlen(_PATH_STDPATH));
#else
		size_t s = confstr(_CS_PATH, NULL, 0); /* Get value's size */
		char *p = (char *)xnmalloc(s, sizeof(char)); /* Allocate space */
		confstr(_CS_PATH, p, s);               /* Get value */
		ptr = p;
#endif
	}

	if (!ptr)
		return 0;

	if (!*ptr) {
		if (malloced_ptr == 1)
			free(ptr);
		return 0;
	}

	char *path_tmp = malloced_ptr == 1 ? ptr : savestring(ptr, strlen(ptr));

	size_t c = count_chars(path_tmp, ':') + 1;
	paths = (char **)xnmalloc(c + 1, sizeof(char *));

	/* Get each path in PATH */
	size_t n = 0;
	char *p = path_tmp, *q = p;
	while (1) {
		if (*q && *q != ':' && *(++q))
			continue;

		char d = *q;
		*q = '\0';
		if (*p && (q - p) > 0) {
			paths[n] = savestring(p, (size_t)(q - p));
			n++;
		}

		if (!d || n == c)
			break;

		p = ++q;
	}

	paths[n] = (char *)NULL;
	free(path_tmp);

	return n;
}

static inline int
validate_line(char *line, char **p)
{
	char *s = line;

	if (!*s || !strchr(s, '/'))
		return (-1);
	if (!strchr(s, ':'))
		return (-1);

	size_t len = strlen(s);
	if (len > 0 && s[len - 1] == '\n')
		s[len - 1] = '\0';

	int cur = 0;
	if (*s == '*') {
		if (!*(++s)) {
			*p = s;
			return (-1);
		}
		cur = 1;
	}

	*p = s;
	return cur;
}

/* Set PATH to last visited directory and CUR_WS to last used
 * workspace */
int
get_last_path(void)
{
	if (!config_dir)
		return EXIT_FAILURE;

	char *last_file = (char *)xnmalloc(config_dir_len + 7, sizeof(char));
	sprintf(last_file, "%s/.last", config_dir);

	int fd;
	FILE *fp = open_fstream_r(last_file, &fd);
	if (!fp) {
		free(last_file);
		return EXIT_FAILURE;
	}

	char line[PATH_MAX + 2] = "";
	while (fgets(line, (int)sizeof(line), fp)) {
		char *p = (char *)NULL;
		int cur = validate_line(line, &p);
		if (cur == -1)
			continue;
		int ws_n = *p - '0';

		if (cur && cur_ws == UNSET)
			cur_ws = ws_n;

		if (ws_n >= 0 && ws_n < MAX_WS && !workspaces[ws_n].path)
			workspaces[ws_n].path = savestring(p + 2, strnlen(p + 2, sizeof(line) - 2));
	}

	close_fstream(fp, fd);
	free(last_file);
	return EXIT_SUCCESS;
}

/* Restore pinned dir from file */
int
load_pinned_dir(void)
{
	if (!config_ok || !config_dir)
		return EXIT_FAILURE;

	char *pin_file = (char *)xnmalloc(config_dir_len + 6, sizeof(char));
	sprintf(pin_file, "%s/.pin", config_dir);

	int fd;
	FILE *fp = open_fstream_r(pin_file, &fd);
	if (!fp) {
		free(pin_file);
		return EXIT_FAILURE;
	}

	char line[PATH_MAX] = "";
	if (fgets(line, (int)sizeof(line), fp) == NULL) {
		free(pin_file);
		close_fstream(fp, fd);
		return EXIT_FAILURE;
	}

	if (!*line || !strchr(line, '/')) {
		free(pin_file);
		close_fstream(fp, fd);
		return EXIT_FAILURE;
	}

	if (pinned_dir) {
		free(pinned_dir);
		pinned_dir = (char *)NULL;
	}

	pinned_dir = savestring(line, strlen(line));
	close_fstream(fp, fd);
	free(pin_file);
	return EXIT_SUCCESS;
}

#if defined(__CYGWIN__)
static int check_cmd_ext(const char *s)
{
	if (!s || !*s)
		return 1;

	switch(TOUPPER(*s)) {
	case 'B': // bat
		return (TOUPPER(s[1]) == 'A' && TOUPPER(s[2]) == 'T' && !s[3]) ? 0 : 1;
	case 'C': // cmd
		return (TOUPPER(s[1]) == 'M' && TOUPPER(s[2]) == 'D' && !s[3]) ? 0 : 1;
	case 'E': // exe
		return (TOUPPER(s[1]) == 'X' && TOUPPER(s[2]) == 'E' && !s[3]) ? 0 : 1;
	case 'J': // js, jse
		return (TOUPPER(s[1]) == 'S' && (!s[2] || (TOUPPER(s[2]) == 'E'
		&& !s[3]) ) ) ? 0 : 1;
	case 'M': // msc
		return (TOUPPER(s[1]) == 'S' && TOUPPER(s[2]) == 'C' && !s[3]) ? 0 : 1;
	case 'V': // vbs, vbe
		return (TOUPPER(s[1]) == 'B' && (TOUPPER(s[2]) == 'S'
		|| TOUPPER(s[2]) == 'E') && !s[3]) ? 0 : 1;
	case 'W': // wsf, wsh
		return (TOUPPER(s[1]) == 'S' && (TOUPPER(s[2]) == 'F'
		|| TOUPPER(s[2]) == 'H') && !s[3]) ? 0 : 1;
	default: return 1;
	}
}

/* Keep only files with executable extension.
 * Returns 1 if the file NAME must be excluded or 0 otherwise */
static int
cygwin_exclude_file(char *name)
{
	if (!name || !*name)
		return 1;

	char *p = strrchr(name, '.');
	if (!p || !*(p + 1) || p == name)
		return 0;

	*p = '\0';
	return check_cmd_ext(p + 1);
}
#endif /* __CYGWIN__ */

/* Get the list of files in PATH, plus CliFM internal commands, and send
 * them into an array to be read by my readline custom auto-complete
 * function (my_rl_completion) */
void
get_path_programs(void)
{
	int i, l = 0, total_cmd = 0;
	int *cmd_n = (int *)0;
	struct dirent ***commands_bin = (struct dirent ***)NULL;

	if (conf.ext_cmd_ok == 1) {
		char cwd[PATH_MAX];
		if (getcwd(cwd, sizeof(cwd)) == NULL) {/* Avoid compiler warning */}

		commands_bin = (struct dirent ***)xnmalloc(path_n, sizeof(struct dirent));
		cmd_n = (int *)xnmalloc(path_n, sizeof(int));

		i = (int)path_n;
		while (--i >= 0) {
			if (!paths[i] || !*paths[i] || xchdir(paths[i], NO_TITLE) == -1) {
				cmd_n[i] = 0;
				continue;
			}

			cmd_n[i] = scandir(paths[i], &commands_bin[i],
#if defined(__CYGWIN__)
					NULL, xalphasort);
#else
					conf.light_mode ? NULL : skip_nonexec, xalphasort);
#endif /* __CYGWIN__ */
			/* If paths[i] directory does not exist, scandir returns -1.
			 * Fedora, for example, adds $HOME/bin and $HOME/.local/bin to
			 * PATH disregarding if they exist or not. If paths[i] dir is
			 * empty do not use it either */
			if (cmd_n[i] > 0)
				total_cmd += cmd_n[i];
		}
		xchdir(cwd, NO_TITLE);
	}

	/* Add internal commands */
	for (internal_cmds_n = 0; internal_cmds[internal_cmds_n].name; internal_cmds_n++);

	bin_commands = (char **)xnmalloc((size_t)total_cmd + internal_cmds_n +
			     aliases_n + actions_n + 2, sizeof(char *));

	i = (int)internal_cmds_n;
	while (--i >= 0) {
		bin_commands[l] = savestring(internal_cmds[i].name, internal_cmds[i].len);
		l++;
	}

	/* Now add aliases, if any */
	if (aliases_n > 0) {
		i = (int)aliases_n;
		while (--i >= 0) {
			bin_commands[l] = savestring(aliases[i].name, strlen(aliases[i].name));
			l++;
		}
	}

	/* And user defined actions too, if any */
	if (actions_n > 0) {
		i = (int)actions_n;
		while (--i >= 0) {
			bin_commands[l] = savestring(usr_actions[i].name, strlen(usr_actions[i].name));
			l++;
		}
	}

	if (conf.ext_cmd_ok == 1 && total_cmd > 0) {
		/* And finally, add commands in PATH */
		i = (int)path_n;
		while (--i >= 0) {
			if (cmd_n[i] <= 0)
				continue;

			int j = cmd_n[i];
			while (--j >= 0) {
				if (SELFORPARENT(commands_bin[i][j]->d_name)) {
					free(commands_bin[i][j]);
					continue;
				}
#if defined(__CYGWIN__)
				if (cygwin_exclude_file(commands_bin[i][j]->d_name) == 1) {
					free(commands_bin[i][j]);
					continue;
				}
#endif /* __CYGWIN__ */
				bin_commands[l] = savestring(commands_bin[i][j]->d_name,
					strlen(commands_bin[i][j]->d_name));
				l++;
				free(commands_bin[i][j]);
			}

			free(commands_bin[i]);
		}

//		free(commands_bin);
//		free(cmd_n);
	}

	free(commands_bin);
	free(cmd_n);
	path_progsn = (size_t)l;
	bin_commands[l] = (char *)NULL;
}

static inline void
free_aliases(void)
{
	int i = (int)aliases_n;

	while (--i >= 0) {
		free(aliases[i].name);
		free(aliases[i].cmd);
	}

	free(aliases);
	aliases = (struct alias_t *)xnmalloc(1, sizeof(struct alias_t));
	aliases_n = 0;
}

static inline void
write_alias(char *s, char *p)
{
	aliases = (struct alias_t *)xrealloc(aliases, (aliases_n + 2)
				* sizeof(struct alias_t));
	aliases[aliases_n].name = savestring(s, strlen(s));
	int add = 0;
	if (*p == '\'') {
		aliases[aliases_n].cmd = strbtw(p, '\'', '\'');
		aliases_n++;
		add = 1;
	} else {
		if (*p == '"') {
			aliases[aliases_n].cmd = strbtw(p, '"', '"');
			aliases_n++;
			add = 1;
		}
	}

	if (add == 1) {
		aliases[aliases_n].name = (char *)NULL;
		aliases[aliases_n].cmd = (char *)NULL;
	}
}

static inline int
alias_exists(char *s)
{
	int i = (int)aliases_n;

	while (--i >= 0) {
		if (!aliases[i].name)
			continue;
		if (*s == *aliases[i].name && strcmp(s, aliases[i].name) == 0)
			return 1;
	}

	return 0;
}

void
get_aliases(void)
{
	if (!config_ok || !config_file)
		return;

	int fd;
	FILE *fp = open_fstream_r(config_file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: alias: '%s': %s\n",
		    PROGRAM_NAME, config_file, strerror(errno));
		return;
	}

	if (aliases_n)
		free_aliases();

	char *line = (char *)NULL;
	size_t line_size = 0;

	while (getline(&line, &line_size, fp) > 0) {
		if (*line == 'a' && strncmp(line, "alias ", 6) == 0) {
			char *s = strchr(line, ' ');
			if (!s || !*(++s))
				continue;
			char *p = strchr(s, '=');
			if (!p || !*(p + 1))
				continue;
			*p = '\0';
			p++;

			if (alias_exists(s) == 1) continue;
			write_alias(s, p);
		}
	}

	free(line);
	close_fstream(fp, fd);
}

static inline void
write_dirhist(char *line, ssize_t len)
{
	if (!line || !*line || *line == '\n')
		return;

	if (len > 0 && line[len - 1] == '\n') {
		line[len - 1] = '\0';
		len--;
	}

	old_pwd[dirhist_total_index] = (char *)xnmalloc((size_t)len + 1,
		sizeof(char));
	strcpy(old_pwd[dirhist_total_index], line);
	dirhist_total_index++;
}

int
load_dirhist(void)
{
	if (!config_ok || !dirhist_file)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fstream_r(dirhist_file, &fd);
	if (!fp)
		return EXIT_FAILURE;

	size_t dirs = 0;

	char tmp_line[PATH_MAX];
	while (fgets(tmp_line, (int)sizeof(tmp_line), fp))
		dirs++;

	if (!dirs) {
		close_fstream(fp, fd);
		return EXIT_SUCCESS;
	}

	old_pwd = (char **)xnmalloc(dirs + 2, sizeof(char *));

	fseek(fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;
	dirhist_total_index = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0)
		write_dirhist(line, line_len);

	close_fstream(fp, fd);
	old_pwd[dirhist_total_index] = (char *)NULL;
	free(line);
	dirhist_cur_index = dirhist_total_index - 1;
	return EXIT_SUCCESS;
}

static inline void
free_prompt_cmds(void)
{
	size_t i;
	for (i = 0; i < prompt_cmds_n; i++)
		free(prompt_cmds[i]);
	free(prompt_cmds);
	prompt_cmds = (char **)NULL;
	prompt_cmds_n = 0;
}

void
get_prompt_cmds(void)
{
	if (!config_ok || !config_file)
		return;

	int fd;
	FILE *fp = open_fstream_r(config_file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: prompt: '%s': %s\n",
		    PROGRAM_NAME, config_file, strerror(errno));
		return;
	}

	if (prompt_cmds_n)
		free_prompt_cmds();

	char *line = (char *)NULL;
	size_t line_size = 0;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (*line != 'p' || strncmp(line, "promptcmd ", 10) != 0)
			continue;
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';
		if (!(*line + 10))
			continue;
		prompt_cmds = (char **)xrealloc(prompt_cmds,
		    (prompt_cmds_n + 1) * sizeof(char *));
		prompt_cmds[prompt_cmds_n] = savestring(
		    line + 10, (size_t)line_len - 10);
		prompt_cmds_n++;
	}

	free(line);
	close_fstream(fp, fd);
}

/* Get the length of the current time format
 * We need this to construct the time string in case of invalid timestamp (0),
 * and to calculate the space left to print file names in long view */

/* GCC (not clang though) complains about tfmt being not a string literal.
 * Let's silence this warning until we find a better approach.
 * This shouldn't be a problem, however, since the use of non string literals
 * for the format string is documented in the strftime manpage itself. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void
check_time_str(void)
{
	if (prop_fields.time == 0)
		return;

	if (conf.relative_time == 1) {
		prop_fields.len += 7;
		return;
	}

	/* Get length of the current time format */
	struct tm tm;
	time_t t = time(NULL);
	localtime_r(&t, &tm);
	char tim[MAX_TIME_STR];
	char *tfmt = conf.time_str ? conf.time_str : DEF_TIME_STYLE_OLDER;
	size_t l = strftime(tim, sizeof(tim), tfmt, &tm);

	/* Construct the invalid time format string (used when we get an
	 * invalid file timestamp) */
	if (l > MAX_TIME_STR)
		l = MAX_TIME_STR;

	size_t i;
	*invalid_time_str = '-';
	for (i = 1; i < l; i++)
		invalid_time_str[i] = ' ';
	invalid_time_str[i] = '\0';

	/* Append the time string length to the properties total length, so that
	 * we can better calculate how much space left we have to print file names */
	prop_fields.len += (int)(l + 1);
}
#pragma GCC diagnostic pop

/* If some option was not set, set it to the default value */
void
check_options(void)
{
	if (conf.relative_time == UNSET)
		conf.relative_time = DEF_RELATIVE_TIME;

	if (conf.cmd_desc_sug == UNSET)
		conf.cmd_desc_sug = DEF_CMD_DESC_SUG;

	if (conf.purge_jumpdb == UNSET)
		conf.purge_jumpdb = DEF_PURGE_JUMPDB;

	if (conf.fuzzy_match == UNSET) {
		conf.fuzzy_match = xargs.fuzzy_match == UNSET
			? DEF_FUZZY_MATCH : xargs.fuzzy_match;
	}

	if (conf.fuzzy_match_algo == UNSET) {
		conf.fuzzy_match_algo = xargs.fuzzy_match_algo == UNSET
			? DEF_FUZZY_MATCH_ALGO : xargs.fuzzy_match_algo;
	}

	if (conf.private_ws_settings == UNSET)
		conf.private_ws_settings = DEF_PRIVATE_WS_SETTINGS;

	if (conf.rm_force == UNSET)
		conf.rm_force = DEF_RM_FORCE;

	if (conf.desktop_notifications == UNSET) {
		if (xargs.desktop_notifications == UNSET)
			conf.desktop_notifications = DEF_DESKTOP_NOTIFICATIONS;
		else
			conf.desktop_notifications = xargs.desktop_notifications;
	}

	if (!*prop_fields_str) {
		xstrsncpy(prop_fields_str, DEF_PROP_FIELDS, PROP_FIELDS_SIZE);
		set_prop_fields(prop_fields_str);
	}
	check_time_str();

	if (conf.search_strategy == UNSET)
		conf.search_strategy = DEF_SEARCH_STRATEGY;

	if (xargs.eln_use_workspace_color == UNSET)
		xargs.eln_use_workspace_color = DEF_ELN_USE_WORKSPACE_COLOR;

	if (xargs.refresh_on_empty_line == UNSET)
		xargs.refresh_on_empty_line = DEF_REFRESH_ON_EMPTY_LINE;

	if (print_removed_files == UNSET)
		print_removed_files = DEF_PRINT_REMOVED_FILES;

	if (xargs.bell_style == UNSET)
		bell = DEF_BELL_STYLE;
	else
		bell = xargs.bell_style;

	if (xargs.refresh_on_resize == UNSET)
		xargs.refresh_on_resize = DEF_REFRESH_ON_RESIZE;

	if (xargs.si == UNSET)
		xargs.si = DEF_SI;

	if (hist_status == UNSET) {
		if (xargs.history == UNSET)
			hist_status = DEF_HIST_STATUS;
		else
			hist_status = xargs.history;
	}

/*	if (!conf.usr_cscheme) {
		if (term_caps.color >= 256)
			conf.usr_cscheme = savestring("default-256", 11);
		else
			conf.usr_cscheme = savestring("default", 7);
	} */

	if (!conf.wprompt_str) {
		if (conf.colorize == 1)
			conf.wprompt_str = savestring(DEF_WPROMPT_STR, strlen(DEF_WPROMPT_STR));
		else
			conf.wprompt_str = savestring(DEF_WPROMPT_STR_NO_COLOR,
				strlen(DEF_WPROMPT_STR_NO_COLOR));
	}

	/* Do no override command line options */
	if (xargs.cwd_in_title == UNSET)
		xargs.cwd_in_title = DEF_CWD_IN_TITLE;

	if (xargs.secure_cmds == UNSET)
		xargs.secure_cmds = DEF_SECURE_CMDS;

	if (xargs.secure_env == UNSET)
		xargs.secure_env = DEF_SECURE_ENV;

	if (xargs.secure_env_full == UNSET)
		xargs.secure_env_full = DEF_SECURE_ENV_FULL;

	if (conf.cp_cmd == UNSET)
		conf.cp_cmd = DEF_CP_CMD;

	if (check_cap == UNSET)
		check_cap = DEF_CHECK_CAP;

	if (check_ext == UNSET)
		check_ext = DEF_CHECK_EXT;

	if (follow_symlinks == UNSET)
		follow_symlinks = DEF_FOLLOW_SYMLINKS;

	if (conf.mv_cmd == UNSET)
		conf.mv_cmd = DEF_MV_CMD;

	if (conf.min_name_trim == UNSET)
		conf.min_name_trim = DEF_MIN_NAME_TRIM;

	if (conf.min_jump_rank == JUMP_UNSET)
		conf.min_jump_rank = DEF_MIN_JUMP_RANK;

	if (conf.max_jump_total_rank == UNSET)
		conf.max_jump_total_rank = DEF_MAX_JUMP_TOTAL_RANK;

	if (conf.no_eln == UNSET) {
		if (xargs.noeln == UNSET)
			conf.no_eln = DEF_NOELN;
		else
			conf.no_eln = xargs.noeln;
	}

	if (prompt_notif == UNSET)
		prompt_notif = DEF_PROMPT_NOTIF;

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == UNSET) {
		if (xargs.highlight == UNSET)
			conf.highlight = DEF_HIGHLIGHT;
		else
			conf.highlight = xargs.highlight;
	}
#endif /* !_NO_HIGHLIGHT */

	if (conf.apparent_size == UNSET) {
		if (xargs.apparent_size == UNSET)
			conf.apparent_size = DEF_APPARENT_SIZE;
		else
			conf.apparent_size = xargs.apparent_size;
	}

	if (conf.full_dir_size == UNSET) {
		if (xargs.full_dir_size == UNSET)
			conf.full_dir_size = DEF_FULL_DIR_SIZE;
		else
			conf.full_dir_size = xargs.full_dir_size;
	}

	if (conf.warning_prompt == UNSET) {
		if (xargs.warning_prompt == UNSET)
			conf.warning_prompt = DEF_WARNING_PROMPT;
		else
			conf.warning_prompt = xargs.warning_prompt;
	}

	if (conf.listing_mode == UNSET) {
		if (xargs.horizontal_list == UNSET)
			conf.listing_mode = DEF_LISTING_MODE;
		else
			conf.listing_mode = xargs.horizontal_list ? 1 : 0;
	}

#ifndef _NO_FZF
	if (fzftab == UNSET) {
		if (xargs.fzftab == UNSET) {
			/* This flag will be true only when reloading the config file,
			 * because the check for the fzf binary is made at startup AFTER
			 * reading the config file (check_third_party_cmds() in checks.c) */
			if (finder_flags & FZF_BIN_OK)
				fzftab = 1;
		} else {
			fzftab = xargs.fzftab;
		}

		if (xargs.fzytab == 1)
			tabmode = FZY_TAB;
		else if (xargs.fzftab == 1)
			tabmode = FZF_TAB;
		else if (xargs.smenutab == 1)
			tabmode = SMENU_TAB;
		else
			tabmode = STD_TAB;
	}

	if (!conf.fzftab_options) {
		if (conf.colorize == 1 || !getenv("FZF_DEFAULT_OPTS")) {
			char *pp = conf.colorize == 1 ? DEF_FZFTAB_OPTIONS : DEF_FZFTAB_OPTIONS_NO_COLOR;
			conf.fzftab_options = savestring(pp, strlen(pp));
		} else {
			conf.fzftab_options = savestring("", 1);
		}
	}

	set_fzf_preview_border_type();

	smenutab_options_env = (xargs.secure_env_full != 1 && tabmode == SMENU_TAB)
		? getenv("CLIFM_SMENU_OPTIONS") : (char *)NULL;

	if (smenutab_options_env && sanitize_cmd(smenutab_options_env, SNT_BLACKLIST) != 0) {
		_err('w', PRINT_PROMPT, "%s: CLIFM_SMENU_OPTIONS contains unsafe "
			"characters (<>|;&$`). Falling back to default values.\n", PROGRAM_NAME);
		smenutab_options_env = (char *)NULL;
	}

#else
	tabmode = STD_TAB;
#endif /* _NO_FZF */

	if (xargs.stealth_mode == 1) {
		xargs.fzf_preview = conf.fzf_preview = 0;
	} else if (conf.fzf_preview == UNSET) {
		if (xargs.fzf_preview == UNSET)
			conf.fzf_preview = DEF_FZF_PREVIEW;
		else
			conf.fzf_preview = xargs.fzf_preview;
	}

#ifndef _NO_ICONS
	if (conf.icons == UNSET) {
		if (xargs.icons == UNSET)
			conf.icons = DEF_ICONS;
		else
			conf.icons = xargs.icons;
	}
#endif /* _NO_ICONS */

#ifndef _NO_SUGGESTIONS
	if (conf.suggestions == UNSET) {
		if (xargs.suggestions == UNSET)
			conf.suggestions = DEF_SUGGESTIONS;
		else
			conf.suggestions = xargs.suggestions;
	}

	if (!conf.suggestion_strategy)
		conf.suggestion_strategy = savestring(DEF_SUG_STRATEGY, SUG_STRATS);

	if (conf.suggest_filetype_color == UNSET)
		conf.suggest_filetype_color = DEF_SUG_FILETYPE_COLOR;
#endif /* _NO_SUGGESTIONS */

	if (int_vars == UNSET) {
		if (xargs.int_vars == UNSET)
			int_vars = DEF_INT_VARS;
		else
			int_vars = xargs.int_vars;
	}

	if (conf.print_selfiles == UNSET) {
		if (xargs.printsel == UNSET)
			conf.print_selfiles = DEF_PRINTSEL;
		else
			conf.print_selfiles = xargs.printsel;
	}

	if (conf.max_printselfiles == UNSET)
		conf.max_printselfiles = DEF_MAX_PRINTSEL;

	if (conf.case_sens_list == UNSET) {
		if (xargs.case_sens_list == UNSET)
			conf.case_sens_list = DEF_CASE_SENS_LIST;
		else
			conf.case_sens_list = xargs.case_sens_list;
	}

	if (conf.case_sens_search == UNSET)
		conf.case_sens_search = DEF_CASE_SENS_SEARCH;

	if (conf.case_sens_dirjump == UNSET) {
		if (xargs.case_sens_dirjump == UNSET)
			conf.case_sens_dirjump = DEF_CASE_SENS_DIRJUMP;
		else
			conf.case_sens_dirjump = xargs.case_sens_dirjump;
	}

	if (conf.case_sens_path_comp == UNSET) {
		if (xargs.case_sens_path_comp == UNSET)
			conf.case_sens_path_comp = DEF_CASE_SENS_PATH_COMP;
		else
			conf.case_sens_path_comp = xargs.case_sens_path_comp;
	}

#ifndef _NO_TRASH
	if (conf.tr_as_rm == UNSET) {
		if (xargs.trasrm == UNSET)
			conf.tr_as_rm = DEF_TRASRM;
		else
			conf.tr_as_rm = xargs.trasrm;
	}
#endif /* _NO_TRASH */

	if (conf.only_dirs == UNSET) {
		if (xargs.only_dirs == UNSET)
			conf.only_dirs = DEF_ONLY_DIRS;
		else
			conf.only_dirs = xargs.only_dirs;
	}

	if (conf.splash_screen == UNSET) {
		if (xargs.splash == UNSET)
			conf.splash_screen = DEF_SPLASH_SCREEN;
		else
			conf.splash_screen = xargs.splash;
	}

	if (conf.welcome_message == UNSET) {
		if (xargs.welcome_message == UNSET)
			conf.welcome_message = DEF_WELCOME_MESSAGE;
		else
			conf.welcome_message = xargs.welcome_message;
	}

	if (conf.show_hidden == UNSET) {
		if (xargs.hidden == UNSET)
			conf.show_hidden = DEF_SHOW_HIDDEN;
		else
			conf.show_hidden = xargs.hidden;
	}

	if (conf.files_counter == UNSET) {
		if (xargs.files_counter == UNSET)
			conf.files_counter = DEF_FILES_COUNTER;
		else
			conf.files_counter = xargs.files_counter;
	}

	if (conf.long_view == UNSET) {
		if (xargs.longview == UNSET)
			conf.long_view = DEF_LONG_VIEW;
		else
			conf.long_view = xargs.longview;
	}

	if (conf.ext_cmd_ok == UNSET) {
		if (xargs.ext == UNSET)
			conf.ext_cmd_ok = DEF_EXT_CMD_OK;
		else
			conf.ext_cmd_ok = xargs.ext;
	}

	if (conf.pager == UNSET) {
		if (xargs.pager == UNSET)
			conf.pager = DEF_PAGER;
		else
			conf.pager = xargs.pager;
	}

	if (conf.max_dirhist == UNSET) {
		if (xargs.max_dirhist == UNSET)
			conf.max_dirhist = DEF_MAX_DIRHIST;
		else
			conf.max_dirhist = xargs.max_dirhist;
	}

	if (conf.clear_screen == UNSET) {
		if (xargs.clear_screen == UNSET)
			conf.clear_screen = DEF_CLEAR_SCREEN;
		else
			conf.clear_screen = xargs.clear_screen;
	}

	if (conf.list_dirs_first == UNSET) {
		if (xargs.dirs_first == UNSET)
			conf.list_dirs_first = DEF_LIST_DIRS_FIRST;
		else
			conf.list_dirs_first = xargs.dirs_first;
	}

	if (conf.autols == UNSET) {
		if (xargs.autols == UNSET)
			conf.autols = DEF_AUTOLS;
		else
			conf.autols = xargs.autols;
	}

	if (conf.unicode == UNSET)
		conf.unicode = DEF_UNICODE;

	if (conf.max_path == UNSET) {
		if (xargs.max_path == UNSET)
			conf.max_path = DEF_MAX_PATH;
		else
			conf.max_path = xargs.max_path;
	}

	if (conf.logs_enabled == UNSET) {
		if (xargs.logs == UNSET)
			conf.logs_enabled = DEF_LOGS_ENABLED;
		else
			conf.logs_enabled = xargs.logs;
	}

	if (conf.log_cmds == UNSET)
		conf.log_cmds = DEF_LOG_CMDS;

	if (conf.light_mode == UNSET) {
		if (xargs.light == UNSET)
			conf.light_mode = DEF_LIGHT_MODE;
		else
			conf.light_mode = xargs.light;
	}

	if (conf.classify == UNSET) {
		if (xargs.classify == UNSET)
			conf.classify = DEF_CLASSIFY;
		else
			conf.classify = xargs.classify;
	}

	if (conf.share_selbox == UNSET) {
		if (xargs.share_selbox == UNSET)
			conf.share_selbox = DEF_SHARE_SELBOX;
		else
			conf.share_selbox = xargs.share_selbox;
	}

	if (conf.sort == UNSET) {
		if (xargs.sort == UNSET)
			conf.sort = DEF_SORT;
		else
			conf.sort = xargs.sort;
	}

	if (conf.sort_reverse == UNSET) {
		if (xargs.sort_reverse == UNSET)
			conf.sort_reverse = DEF_SORT_REVERSE;
		else
			conf.sort_reverse = xargs.sort_reverse;
	}

	if (conf.tips == UNSET) {
		if (xargs.tips == UNSET)
			conf.tips = DEF_TIPS;
		else
			conf.tips = xargs.tips;
	}

	if (conf.autocd == UNSET) {
		if (xargs.autocd == UNSET)
			conf.autocd = DEF_AUTOCD;
		else
			conf.autocd = xargs.autocd;
	}

	if (conf.auto_open == UNSET) {
		if (xargs.auto_open == UNSET)
			conf.auto_open = DEF_AUTO_OPEN;
		else
			conf.auto_open = xargs.auto_open;
	}

	if (autojump == UNSET) {
		if (xargs.autojump == UNSET)
			autojump = DEF_AUTOJUMP;
		else
			autojump = xargs.autojump;
		if (autojump == 1)
			conf.autocd = 1;
	}

	if (conf.cd_on_quit == UNSET) {
		if (xargs.cd_on_quit == UNSET)
			conf.cd_on_quit = DEF_CD_ON_QUIT;
		else
			conf.cd_on_quit = xargs.cd_on_quit;
	}

	if (conf.dirhist_map == UNSET) {
		if (xargs.dirmap == UNSET)
			conf.dirhist_map = DEF_DIRHIST_MAP;
		else
			conf.dirhist_map = xargs.dirmap;
	}

	if (conf.disk_usage == UNSET) {
		if (xargs.disk_usage == UNSET)
			conf.disk_usage = DEF_DISK_USAGE;
		else
			conf.disk_usage = xargs.disk_usage;
	}

	if (conf.restore_last_path == UNSET) {
		if (xargs.restore_last_path == UNSET)
			conf.restore_last_path = DEF_RESTORE_LAST_PATH;
		else
			conf.restore_last_path = xargs.restore_last_path;
	}

	if (conf.max_hist == UNSET)
		conf.max_hist = DEF_MAX_HIST;

	if (conf.max_log == UNSET)
		conf.max_log = DEF_MAX_LOG;

	if (!user.shell) {
		struct user_t tmp_user = get_user_data();
		user.shell = tmp_user.shell;

		/* We don't need these values of the user struct: free them */
		free(tmp_user.name);
		free(tmp_user.home);
		free(tmp_user.groups);

		if (!user.shell)
			user.shell = savestring(FALLBACK_SHELL, strlen(FALLBACK_SHELL));
	}

	if (!conf.term)
		conf.term = savestring(DEF_TERM_CMD, strlen(DEF_TERM_CMD));

	if (!conf.encoded_prompt || !*conf.encoded_prompt) {
		char *t = conf.colorize == 1 ? DEFAULT_PROMPT : DEFAULT_PROMPT_NO_COLOR;
		conf.encoded_prompt = savestring(t, strlen(t));
	}

	if ((xargs.stealth_mode == 1 || home_ok == 0 ||
	config_ok == 0 || !config_file) && !*div_line)
		xstrsncpy(div_line, DEF_DIV_LINE, sizeof(div_line) - 1);

	if (xargs.stealth_mode == 1 && !conf.opener) {
		/* Since in stealth mode we have no access to the config file, we cannot
		 * use Lira, since it relays on a file. Set it thus to FALLBACK_OPENER,
		 * if not already set via command line */
		conf.opener = savestring(FALLBACK_OPENER, strlen(FALLBACK_OPENER));
	}

#ifndef _NO_SUGGESTIONS
	if (term_caps.suggestions == 0)
		xargs.suggestions = conf.suggestions = 0;
#endif
	if (term_caps.color == 0)
		xargs.colorize = conf.colorize = 0;
	if (term_caps.pager == 0)
		xargs.pager = conf.pager = 0;

	reset_opts();
}
