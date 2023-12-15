/* init.c -- functions controlling the program initialization */

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

#include <limits.h> /* INT_MIN */
#include <errno.h>

#if defined(__linux__) || defined(__HAIKU__) || defined(__sun) \
|| defined(__CYGWIN__)
# include <grp.h> /* getgrouplist(3) */
#endif /* __linux__ || __HAIKU__ || __sun || __CYGWIN__ */

#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <readline/tilde.h> /* tilde_expand() */

#if defined(__OpenBSD__)
# include <ereadline/readline/history.h> /* history_write_timestamps */
#else
# include <readline/history.h>
#endif /* __OpenBSD__ */

#if defined(__NetBSD__)
# include <ctype.h>
#endif /* __NetBSD__ */

#ifndef _BE_POSIX
# include <paths.h> /* _PATH_STDPATH */
#endif /* _BE_POSIX */

#ifdef LINUX_FSINFO
# include <mntent.h> /* xxxmntent functions, used by get_ext_mountpoints() */
#endif /* LINUX_FSINFO */

#include "aux.h"
#include "checks.h" /* truncate_file(), is_number() */
#include "config.h"
#include "exec.h"
#include "jump.h" /* add_to_jumpdb() */
#include "misc.h"
#include "navigation.h"
#include "sort.h"
#include "autocmds.h"
#include "sanitize.h"
#include "selection.h"

/* We need this for get_user_groups() */
#if !defined(NGROUPS_MAX)
# if defined(__linux__)
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 4)
#   define NGROUPS_MAX 65536
#  else
#   define NGROUPS_MAX 32
#  endif /* linux >= 2.6.4 */
# else
#  define NGROUPS_MAX 1024
# endif /* __linux__ */
#endif /* !NGROUPS_MAX */

#ifdef LINUX_FSINFO
void
get_ext_mountpoints(void)
{
	if (ext_mnt) {
		int i;
		for (i = 0; ext_mnt[i].mnt_point; i++)
			free(ext_mnt[i].mnt_point);
		free(ext_mnt);
		ext_mnt = (struct ext_mnt_t *)NULL;
	}

	FILE *fp = setmntent(_PATH_MOUNTED, "r");
	if (!fp)
		return;

	size_t n = 0;
	struct mntent *ent;
	while ((ent = getmntent(fp)) != NULL) {
		char *t = ent->mnt_type;
		if (*t != 'e' || t[1] != 'x' || t[2] != 't' || !t[3] || t[4])
			continue;

		ext_mnt = (struct ext_mnt_t *)xnrealloc(ext_mnt,
			n + 2, sizeof(struct ext_mnt_t));
		ext_mnt[n].mnt_point = savestring(ent->mnt_dir, strlen(ent->mnt_dir));

		switch (t[3]) {
		case '2': ext_mnt[n].type = EXT2_FSTYPE; break;
		case '3': ext_mnt[n].type = EXT3_FSTYPE; break;
		case '4': ext_mnt[n].type = EXT4_FSTYPE; break;
		default: ext_mnt[n].type = -1; break;
		}

		n++;
	}

	if (n > 0) {
		ext_mnt[n].mnt_point = (char *)NULL;
		ext_mnt[n].type = -1;
	}

	endmntent(fp);
}
#endif /* LINUX_FSINFO */

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

	conf.color_lnk_as_target = DEF_COLOR_LNK_AS_TARGET;

	conf.columned = UNSET;
	conf.cp_cmd = UNSET;
	conf.desktop_notifications = UNSET;
	conf.dirhist_map = UNSET;
	conf.disk_usage = UNSET;
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
#endif /* !_NO_HIGHLIGHT */
#ifndef _NO_ICONS
	conf.icons = UNSET;
#else
	conf.icons = 0;
#endif /* !_NO_ICONS */
	conf.light_mode = UNSET;
	conf.list_dirs_first = UNSET;
	conf.listing_mode = UNSET;
	conf.log_cmds = UNSET;
	conf.log_msgs = UNSET;
	conf.long_view = UNSET;
	conf.max_dirhist = UNSET;
	conf.max_hist = UNSET;
	conf.max_log = UNSET;
	conf.max_jump_total_rank = UNSET;

	conf.max_name_len = DEF_MAX_NAME_LEN;

	conf.max_name_len_bk = 0;

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

	conf.quoting_style = DEF_QUOTING_STYLE;

	conf.readonly = DEF_READONLY;
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

	conf.trim_names = UNSET;

#ifndef _NO_TRASH
	conf.tr_as_rm = UNSET;
#endif /* !_NO_TRASH */
	conf.unicode = DEF_UNICODE;
	conf.warning_prompt = UNSET;
	conf.welcome_message = UNSET;

	conf.encoded_prompt = (char *)NULL;
	conf.fzftab_options = (char *)NULL;
	conf.histignore_regex = (char *)NULL;
	conf.opener = (char *)NULL;
#ifndef _NO_SUGGESTIONS
	conf.suggestion_strategy = (char *)NULL;
#endif /* !_NO_SUGGESTIONS */
	conf.term = (char *)NULL;
	conf.time_str = (char *)NULL;
	conf.ptime_str = (char *)NULL;
	conf.usr_cscheme = (char *)NULL;
	conf.wprompt_str = (char *)NULL;
	conf.welcome_message_str = (char *)NULL;

	init_shades();
}

void
set_prop_fields(const char *line)
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
		switch (line[i]) {
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
#if defined(LINUX_FILE_XATTRS)
		case 'x': prop_fields.xattr = 1; break;
#endif /* LINUX_FILE_XATTRS */
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
	if (!user.shell)
		return SHELL_POSIX;

	char tmp[PATH_MAX];
	*tmp = '\0';
	char *ret = realpath(user.shell, tmp);
	if (!ret || !*tmp)
		return SHELL_POSIX;

	ret = strrchr(tmp, '/');
	if (!ret || !*(ret + 1))
		return SHELL_POSIX;

	char *s = ret + 1;
	user.shell_basename = savestring(s, strlen(s));

	if (*s == 'b' && strcmp(s, "bash") == 0)
		return SHELL_BASH;
	if (*s == 'd' && strcmp(s, "dash") == 0)
		return SHELL_DASH;
	if (*s == 'f' && strcmp(s, "fish") == 0)
		return SHELL_FISH;
	if (*s == 'z' && strcmp(s, "zsh") == 0)
		return SHELL_ZSH;

	return SHELL_POSIX;
}

#ifndef _NO_GETTEXT
/* Initialize gettext for translations support */
int
init_gettext(void)
{
	char locale_dir[PATH_MAX];
	snprintf(locale_dir, sizeof(locale_dir), "%s/locale", data_dir
		? data_dir : "/usr/local/share");
	bindtextdomain(PROGRAM_NAME, locale_dir);
	textdomain(PROGRAM_NAME);

	return EXIT_SUCCESS;

}
#endif /* !_NO_GETTEXT */

int
backup_argv(const int argc, char **argv)
{
	argc_bk = argc;
	argv_bk = argv;

	/* Let's store the executable base name, excluding the path.
	 * Done to detect and disallow nested instances (see exec.c). */
	char *n = strrchr(argv[0], '/');
	if (n && n[1])
		bin_name = savestring(n + 1, strlen(n + 1));
	else
		bin_name = savestring(argv[0], strlen(argv[0]));

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
		 * from trying to access any of these directories. */
		home_ok = config_ok = 0;

		err('e', PRINT_PROMPT, _("%s: Cannot access the home directory. "
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
	/* Shrink the log and the directory history files */
	if (msgs_log_file)
		truncate_file(msgs_log_file, conf.max_log, 0);
	if (cmds_log_file)
		truncate_file(cmds_log_file, conf.max_log, 0);

	if (!hist_file)
		return EXIT_FAILURE;

	/* Get history */
	history_comment_char = '#';
	history_write_timestamps = 1;

	struct stat attr;
	if (stat(hist_file, &attr) == 0 && FILE_SIZE(attr) != 0) {
		/* If the size condition is not included, and in case of a zero
		 * size file, read_history() produces malloc errors */
		/* Recover history from the history file */
		read_history(hist_file); /* This line adds more leaks to readline */
		/* Limit the size of the history file to max_hist lines */
		history_truncate_file(hist_file, conf.max_hist);
	} else {
	/* If the history file doesn't exist, create it */
		int fd = 0;
		FILE *hist_fp = open_fwrite(hist_file, &fd);
		if (!hist_fp) {
			err('w', PRINT_PROMPT, "%s: fopen: '%s': %s\n",
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

static pid_t
get_own_pid(void)
{
	pid_t pid;

	pid = getpid();

	if (pid < 0)
		return 0;
	return pid;
}

/* Return 1 is secure-env is enabled. Otherwise, return 0.
 * Used only at an early stage, where command line options haven't been
 * parsed yet. */
static int
is_secure_env(void)
{
	if (!argv_bk)
		return 0;

	size_t i;
	for (i = 0; argv_bk[i]; i++) {
		if (*argv_bk[i] == '-'
#ifndef _BE_POSIX
		&& (strncmp(argv_bk[i], "--secure-", 9) == 0))
#else
		&& strcspn(argv_bk[i], "xXY") != strlen(argv_bk[i]))
#endif /* !_BE_POSIX */
			return 1;
	}

	return 0;
}

/* Retrieve user groups.
 * Return an array with the ID's of groups to which the user belongs.
 * NGROUPS is set to the number of groups.
 * NOTE: getgroups(2) does not include the user's main group.
 * We use getgroups(2) on TERMUX because getgrouplist(3) always returns
 * zero groups. */
static gid_t *
get_user_groups(const char *name, const gid_t gid, int *ngroups)
{
	int n = *ngroups;

#if defined(__TERMUX__) || defined(_BE_POSIX)
	UNUSED(name); UNUSED(gid);
	gid_t *g = (gid_t *)xnmalloc(NGROUPS_MAX, sizeof(g));
	if ((n = getgroups(NGROUPS_MAX, g)) == -1) {
		err('e', PRINT_PROMPT, "%s: getgroups: %s\n",
			PROGRAM_NAME, strerror(errno));
		free(g);
		return (gid_t *)NULL;
	}
	if (NGROUPS_MAX > n) /* Reduce array to actual amount of groups (N) */
		g = (gid_t *)xnrealloc(g, (size_t)n, sizeof(g));

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
		g = (gid_t *)xnrealloc(g, (size_t)n, sizeof(g));
#endif /* __TERMUX__ */

	*ngroups = n;
	return g;
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

/* The user specified a custom shell via the CLIFM_SHELL environment variable.
 * Since this will be used to run shell commands, it's better to make sure
 * we have a valid shell, that is, one of the shells specified in
 * '/etc/shells'.
 * Return EXIT_SUCCESS if found or EXIT_FAILURE if not. */
static int
check_etc_shells(const char *file, char *shells_file, int *tmp_errno)
{
	int fd;
	FILE *fp = open_fread(shells_file, &fd);
	if (!fp) {
		*tmp_errno = errno;
		return EXIT_FAILURE;
	}

	int ret = EXIT_FAILURE;
	char line[PATH_MAX];

	while (fgets(line, (int)sizeof(line), fp)) {
		if (*line != '/')
			continue;

		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[len - 1] = '\0';

		if (strcmp(line, file) == 0) {
			ret = EXIT_SUCCESS;
			break;
		}
	}

	fclose(fp);
	return ret;
}

static void
validate_custom_shell(char **file)
{
	int tmp_errno = 0;
	errno = 0;

#ifdef _PATH_SHELLS
	char *shells_file = _PATH_SHELLS;
#else
	char *shells_file = "/etc/shells";
#endif /* _PATH_SHELLS */

#ifdef _PATH_BSHELL
	char *def_shell = _PATH_BSHELL;
#else
	char *def_shell = "/bin/sh";
#endif

	if (*file && check_etc_shells(*file, shells_file,
	&tmp_errno) == EXIT_SUCCESS)
		return;

	/* check_etc_shells() sets errno to a positive value only if /etc/shells
	 * couldn't be found/accessed. */
	if (tmp_errno == 0) {
		err('w', PRINT_PROMPT, _("%s: '%s': Invalid shell. Falling back to "
			"'%s'.\nCheck '%s' for a list of valid shells.\n"),
			PROGRAM_NAME, *file ? *file : "NULL", def_shell, shells_file);
	} else {
		err('w', PRINT_PROMPT, _("%s: '%s': %s.\nCannot validate shell. "
			"Falling back to '%s'.\n"), PROGRAM_NAME, shells_file,
			strerror(tmp_errno), def_shell);
	}

	free(*file);
	*file = savestring(def_shell, strlen(def_shell));
}

/* Get user data from environment variables. Used only in case getpwuid() failed */
static struct user_t
get_user_data_env(void)
{
	struct user_t tmp_user = {0};

	/* If secure-env, do not fallback to environment variables */
	int sec_env = is_secure_env();
	char *t = sec_env == 0 ? xgetenv("HOME", 0) : (char *)NULL;

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
		xerror("%s: Home directory not found. Exiting.\n", PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	tmp_user.home_len = strlen(tmp_user.home);
	t = sec_env == 0 ? xgetenv("USER", 0) : (char *)NULL;
	tmp_user.name = t ? savestring(t, strlen(t)) : (char *)NULL;

	tmp_user.gid = getgid();
	tmp_user.ngroups = 0;
	if (tmp_user.name && tmp_user.gid != (gid_t)-1)
		tmp_user.groups = get_user_groups(tmp_user.name, tmp_user.gid, &tmp_user.ngroups);
	else
		tmp_user.groups = (gid_t *)NULL;

	char *p = xgetenv("CLIFM_SHELL", 0);
	t = sec_env == 0 ? (p ? p : xgetenv("SHELL", 0)) : (char *)NULL;
	tmp_user.shell = t ? savestring(t, strlen(t)) : (char *)NULL;

	tmp_user.shell_basename = (char *)NULL;
	if (p && t == p) /* CLIFM_SHELL */
		validate_custom_shell(&tmp_user.shell);

	return tmp_user;
}

/* Retrieve user information and store it in a user_t struct for later access */
struct user_t
get_user_data(void)
{
	struct passwd *pw = (struct passwd *)NULL;
	struct user_t tmp_user = {0};

	errno = 0;
	tmp_user.uid = geteuid();
	pw = getpwuid(tmp_user.uid);
	if (!pw) { /* Fallback to environment variables (if not secure-env) */
		err('e', PRINT_PROMPT, "%s: getpwuid: %s\n",
			PROGRAM_NAME, strerror(errno));
		return get_user_data_env();
	}

	tmp_user.uid = pw->pw_uid;
	tmp_user.gid = pw->pw_gid;
	tmp_user.ngroups = 0;
	tmp_user.groups = get_user_groups(pw->pw_name, pw->pw_gid, &tmp_user.ngroups);

	int is_custom_shell = 0;
	struct stat a;
	char *homedir = (char *)NULL;

	if (is_secure_env() == 0) {
		char *p = xgetenv("USER", 1);
		tmp_user.name = p ? p : savestring(pw->pw_name, strlen(pw->pw_name));

		char *custom_shell = xgetenv("CLIFM_SHELL", 1);
		p = custom_shell ? custom_shell : xgetenv("SHELL", 1);
		if (custom_shell)
			is_custom_shell = 1;

		tmp_user.shell = p ? p : savestring(pw->pw_shell, strlen(pw->pw_shell));

		p = xgetenv("HOME", 0);
		homedir = p ? p : pw->pw_dir;

		if (homedir == p && p && (stat(p, &a) == -1 || !S_ISDIR(a.st_mode))) {
			err('e', PRINT_PROMPT, _("%s: '%s': Home directory not found\n"
				"Falling back to '%s'\n"), PROGRAM_NAME, p, pw->pw_dir);
			homedir = pw->pw_dir;
		}
	} else {
		tmp_user.name = savestring(pw->pw_name, strlen(pw->pw_name));
		tmp_user.shell = savestring(pw->pw_shell, strlen(pw->pw_shell));
		homedir = pw->pw_dir;
	}

	tmp_user.shell_basename = (char *)NULL;
	if (is_custom_shell == 1)
		validate_custom_shell(&tmp_user.shell);

	if (homedir == pw->pw_dir && (!homedir
	|| stat(homedir, &a) == -1 || !S_ISDIR(a.st_mode))) {
		xerror(_("%s: '%s': Invalid home directory in the password "
			"database.\nSomething is really wrong. Exiting.\n"), PROGRAM_NAME,
			homedir ? homedir : "!");
		exit(errno);
	}

	if (!homedir) {
		xerror(_("%s: Home directory not found.\n"
			"Something is really wrong. Exiting.\n"), PROGRAM_NAME);
		exit(errno);
	}

	/* Sometimes (FreeBSD for example) the home directory, as returned by the
	 * passwd struct, is a symlink, in which case we want to resolve it.
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
check_tag(const char *name)
{
	if (!name || !*name)
		return EXIT_FAILURE;

	char dir[PATH_MAX];
	snprintf(dir, sizeof(dir), "%s/%s", tags_dir, name);

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

/* Make sure no entry in the directory history is absent in the jump database.
 *
 * Why do we need this?
 * Jump entries are stored IN MEMORY and written to disk ONLY AT EXIT, where
 * the ENTIRE database is rewritten with values taken from this memory, which
 * is private to the current instance. So, whatever jump entry was added from a
 * second instance (provided this second instance was closed before the first
 * one) will be lost.
 *
 * One of the drawbacks of this approach is that IT ONLY RETRIEVES NEW ENTRIES
 * BUT WON'T UPDATE EXISTING ONES with values coming from the second instance:
 * they're lost.
 *
*/
static void
sync_jumpdb_with_dirhist(void)
{
	if (!old_pwd)
		return;

	int i = dirhist_total_index;
	size_t j = 0;

	while (--i >= 0) {
		if (!old_pwd[i] || !*old_pwd[i])
			continue;

		size_t old_pwd_len = strlen(old_pwd[i]);

		int found = 0;
		for (j = 0; j < jump_n; j++) {
			if (!jump_db[j].path || !*jump_db[j].path
			|| old_pwd[i][1] != jump_db[j].path[1]
			|| old_pwd_len != jump_db[j].len
			|| old_pwd[i][old_pwd_len - 1] != jump_db[j].path[jump_db[j].len - 1])
				continue;

			if (strcmp(old_pwd[i] + 1, jump_db[j].path + 1) == 0) {
				found = 1;
				break;
			}
		}

		if (found == 0)
			add_to_jumpdb(old_pwd[i]);
	}
}

/* Reconstruct the jump database from the database file. */
void
load_jumpdb(void)
{
	if (xargs.no_dirjump == 1 || config_ok == 0 || !config_dir)
		return;

	char *jump_file = (char *)xnmalloc(config_dir_len + 12, sizeof(char));
	snprintf(jump_file, config_dir_len + 12, "%s/jump.clifm", config_dir);

	int fd;
	FILE *fp = open_fread(jump_file, &fd);
	if (!fp) {
		free(jump_file);
		return;
	}

	char tmp_line[PATH_MAX + 32];
	size_t jump_lines = 0;

	while (fgets(tmp_line, (int)sizeof(tmp_line), fp)) {
		if (*tmp_line >= '0' && *tmp_line <= '9')
			jump_lines++;
	}

	if (jump_lines == 0) {
		free(jump_file);
		fclose(fp);
		return;
	}

	jump_db = (struct jump_t *)xnmalloc(jump_lines + 2, sizeof(struct jump_t));

	fseek(fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;

	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (*line < '0')
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

	fclose(fp);
	free(line);
	free(jump_file);

	if (jump_n == 0) {
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

	sync_jumpdb_with_dirhist();
}

static char *
save_bm_path(char *file)
{
	if (!file || !*file)
		return (char *)NULL;

	char *p = *file != '/' ? normalize_path(file, strlen(file)) : (char *)NULL;

	return p ? p : savestring(file, strlen(file));
}

/* Load bookmarks from the bookmarks file */
int
load_bookmarks(void)
{
	if (create_bm_file() == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (!bm_file)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fread(bm_file, &fd);
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
		fclose(fp);
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
				bookmarks[bm_n].path = save_bm_path(p);
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

			bookmarks[bm_n].path = save_bm_path(tmp);
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
			bookmarks[bm_n].path = save_bm_path(tmp);
			bm_n++;
		}
	}

	free(line);
	fclose(fp);

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

/* Load actions from the actions file */
int
load_actions(void)
{
	if (config_ok == 0 || !actions_file)
		return EXIT_FAILURE;

	/* Free the actions struct array */
	if (actions_n > 0) {
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
	FILE *fp = open_fread(actions_file, &fd);
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
		usr_actions = xnrealloc(usr_actions, (size_t)(actions_n + 1),
			sizeof(struct actions_t));
		usr_actions[actions_n].value = savestring(tmp + 1, strlen(tmp + 1));
		*tmp = '\0';
		usr_actions[actions_n].name = savestring(line, strlen(line));
		actions_n++;
	}

	free(line);
	fclose(fp);
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

/* Load remotes information from REMOTES_FILE. */
int
load_remotes(void)
{
	if (!remotes_file || !*remotes_file || config_ok == 0)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fread(remotes_file, &fd);
	if (!fp) {
		xerror("'%s': %s\n", remotes_file, strerror(errno));
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
			remotes = (struct remote_t *)xnrealloc(
				remotes, n + 2, sizeof(struct remote_t));
			reset_remotes_values(n);

			char *name = strbtw(line, '[', ']');
			if (!name)
				continue;
			if (!*name) {
				free(name);
				name = (char *)NULL;
				continue;
			}
			size_t name_len = strlen(name);
			remotes[n].name = (char *)xnrealloc(remotes[n].name,
				name_len + 1, sizeof(char));
			xstrsncpy(remotes[n].name, name, name_len + 1);
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
			remotes[n].desc = (char *)xnrealloc(remotes[n].desc,
				ret_len + 1, sizeof(char));
			xstrsncpy(remotes[n].desc, ret, ret_len + 1);

		} else if (strncmp(line, "Mountpoint=", 11) == 0) {
			char *tmp = (char *)NULL;
			if (*ret == '~')
				tmp = tilde_expand(ret);
			size_t mnt_len = tmp ? strlen(tmp) : ret_len;
			remotes[n].mountpoint = (char *)xnrealloc(remotes[n].mountpoint,
				mnt_len + 1, sizeof(char));
			xstrsncpy(remotes[n].mountpoint, tmp ? tmp : ret, mnt_len + 1);
			free(tmp);
			if (count_dir(remotes[n].mountpoint, CPOP) > 2)
				remotes[n].mounted = 1;

		} else if (strncmp(line, "MountCmd=", 9) == 0) {
			int replaced = 0;
			if (remotes[n].mountpoint) {
				char *rep = replace_substr(ret, "%m", remotes[n].mountpoint);
				if (rep) {
					size_t rep_len = strlen(rep);
					remotes[n].mount_cmd = (char *)xnrealloc(
						remotes[n].mount_cmd, rep_len + 1, sizeof(char));
					xstrsncpy(remotes[n].mount_cmd, rep, rep_len + 1);
					free(rep);
					replaced = 1;
				}
			}

			if (!replaced) {
				remotes[n].mount_cmd = (char *)xnrealloc(remotes[n].mount_cmd,
					ret_len + 1, sizeof(char));
				xstrsncpy(remotes[n].mount_cmd, ret, ret_len + 1);
			}

		} else if (strncmp(line, "UnmountCmd=", 11) == 0) {
			int replaced = 0;
			if (remotes[n].mountpoint) {
				char *rep = replace_substr(ret, "%m", remotes[n].mountpoint);
				if (rep) {
					size_t rep_len = strlen(rep);
					remotes[n].unmount_cmd = (char *)xnrealloc(
						remotes[n].unmount_cmd, rep_len + 1, sizeof(char));
					xstrsncpy(remotes[n].unmount_cmd, rep, rep_len + 1);
					free(rep);
					replaced = 1;
				}
			}

			if (!replaced) {
				remotes[n].unmount_cmd = (char *)xnrealloc(remotes[n].unmount_cmd,
					ret_len + 1, sizeof(char));
				xstrsncpy(remotes[n].unmount_cmd, ret, ret_len + 1);
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
	fclose(fp);

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

	size_t len = strlen(config_dir_gral) + 15;
	char *f = (char *)xnmalloc(len, sizeof(char));
	snprintf(f, len, "%s/prompts.clifm", config_dir_gral);

	if (stat(f, &a) != -1 && S_ISREG(a.st_mode))
		return f;

	if (!data_dir || !*data_dir)
		goto ERROR;

	char t[PATH_MAX];
	snprintf(t, sizeof(t), "%s/%s/prompts.clifm", data_dir, PROGRAM_NAME);
	if (stat(t, &a) == -1 || !S_ISREG(a.st_mode))
		goto ERROR;

	char *cmd[] = {"cp", "--", t, f, NULL};
	int ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	if (ret == EXIT_SUCCESS)
		return f;

ERROR:
	free(f);
	return (char *)NULL;
}

/* Load prompts from PROMPTS_FILE. */
int
load_prompts(void)
{
	free_prompts();
	free(prompts_file);
	prompts_file = set_prompts_file();
	if (!prompts_file || !*prompts_file)
		return EXIT_FAILURE;

	int fd;
	FILE *fp = open_fread(prompts_file, &fd);
	if (!fp) {
		xerror("'%s': %s\n", prompts_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t n = 0;
	prompts = (struct prompts_t *)xnmalloc(n + 1, sizeof(struct prompts_t));
	unset_prompt_values(n);

	size_t line_sz = 0;
	char *line = (char *)NULL;

	while (getline(&line, &line_sz, fp) > 0) {
		if (SKIP_LINE(*line))
			continue;

		if (*line == '[') {
			if (prompts[n].name)
				n++;
			prompts = (struct prompts_t *)xnrealloc(
				prompts, n + 2, sizeof(struct prompts_t));
			unset_prompt_values(n);

			char *name = strbtw(line, '[', ']');
			if (!name)
				continue;
			if (!*name) {
				free(name);
				name = (char *)NULL;
				continue;
			}
			size_t name_len = strlen(name);
			prompts[n].name = (char *)xnrealloc(prompts[n].name,
				name_len + 1, sizeof(char));
			xstrsncpy(prompts[n].name, name, name_len + 1);
			free(name);
			name = (char *)NULL;
		}

		if (!prompts[n].name)
			continue;

		char *ret = strchr(line, '=');
		if (!ret || !*(++ret))
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
			prompts[n].regular = (char *)xnrealloc(prompts[n].regular,
				ret_len + 1, sizeof(char));
			xstrsncpy(prompts[n].regular, ret, ret_len + 1);
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
			prompts[n].warning = (char *)xnrealloc(prompts[n].warning,
				ret_len + 1, sizeof(char));
			xstrsncpy(prompts[n].warning, ret, ret_len + 1);
		}
	}

	free(line);
	fclose(fp);

	if (prompts[n].name) {
		++n;
		prompts[n].name = (char *)NULL;
	}

	prompts_n = n;

	if (conf.encoded_prompt)
		expand_prompt_name(conf.encoded_prompt);

	return EXIT_SUCCESS;
}

void
unset_xargs(void)
{
	xargs.apparent_size = UNSET;
	xargs.auto_open = UNSET;
	xargs.autocd = UNSET;
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
	xargs.cwd_in_title = UNSET;
	xargs.desktop_notifications = UNSET;
	xargs.dirmap = UNSET;
	xargs.disk_usage = UNSET;
	xargs.disk_usage_analyzer = UNSET;
	xargs.eln_use_workspace_color = UNSET;
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
	xargs.fnftab = UNSET;
	xargs.smenutab = UNSET;
#endif /* !_NO_FZF */
	xargs.hidden = UNSET;
#ifndef _NO_HIGHLIGHT
	xargs.highlight = UNSET;
#endif /* !_NO_HIGHLIGHT */
	xargs.history = UNSET;
	xargs.horizontal_list = UNSET;
#ifndef _NO_ICONS
	xargs.icons = UNSET;
	xargs.icons_use_file_color = UNSET;
#endif /* !_NO_ICONS */
	xargs.int_vars = UNSET;
	xargs.light = UNSET;
	xargs.list_and_quit = UNSET;
	xargs.longview = UNSET;
	xargs.lscolors = UNSET;
	xargs.max_dirhist = UNSET;
	xargs.max_path = UNSET;
	xargs.mount_cmd = UNSET;

	xargs.no_bold = UNSET;

	xargs.no_dirjump = UNSET;
	xargs.noeln = UNSET;
	xargs.only_dirs = UNSET;
	xargs.open = UNSET;
	xargs.pager = UNSET;
	xargs.path = UNSET;
	xargs.printsel = UNSET;
	xargs.readonly = UNSET;
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

	xargs.stat = 0;

	xargs.stealth_mode = UNSET;

#ifndef _NO_SUGGESTIONS
	xargs.suggestions = UNSET;
#endif /* !_NO_SUGGESTIONS */

	xargs.tips = UNSET;
#ifndef _NO_TRASH
	xargs.trasrm = UNSET;
#endif /* !_NO_TRASH */
	xargs.trim_names = UNSET;
	xargs.virtual_dir_full_paths = UNSET;
	xargs.vt100 = UNSET;
	xargs.welcome_message = UNSET;
	xargs.warning_prompt = UNSET;
}

static int
check_nest_level(void)
{
	/* If running on a fully sanitized environment, no variable is imported
	 * at all, but CLIFMLVL is nevertheless consulted (by xsecure_env()) to
	 * know whether we are running a nested instance, in which case
	 * NESTING_LEVEL is set to 2. */
	if (xargs.secure_env_full == 1 && nesting_level == 2)
		return 2;

	char *level = getenv("CLIFMLVL");
	if (!level)
		goto FALLBACK;

	int a = atoi(level);
	if (a < 1 || a > MAX_SHELL_LEVEL)
		goto FALLBACK;

	return a + 1;

FALLBACK:
	return (getenv("CLIFM") ? 2 : 1);
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

	if ((nesting_level = check_nest_level()) > 1) {
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
		err(0, NOPRINT_PROMPT, "%s: setpgid: %s\n", PROGRAM_NAME, strerror(errno));
		exit(errno);
	} */

	/* Grab control of the terminal */
	tcsetpgrp(STDIN_FILENO, shell_pgid);

	/* Save default terminal attributes for shell */
	tcgetattr(STDIN_FILENO, &shell_tmodes);
}

/* Get current entries in the Selection Box, if any. */
int
get_sel_files(void)
{
	if (selfile_ok == 0 || config_ok == 0 || !sel_file)
		return EXIT_FAILURE;

	size_t selnbk = sel_n;
	/* First, clear the sel array, in case it was already used. */
	if (sel_n > 0) {
		int i = (int)sel_n;
		while (--i >= 0)
			free(sel_elements[i].name);
	}
	sel_n = 0;

	/* Open the tmp sel file and load its contents into the sel array. */
	int fd;
	FILE *fp = open_fread(sel_file, &fd);
	/*  sel_elements = xcalloc(1, sizeof(char *)); */
	if (!fp)
		return EXIT_FAILURE;

	struct stat a;
	/* Since this file contains only paths, PATH_MAX should be enough. */
	char line[PATH_MAX];
	while (fgets(line, (int)sizeof(line), fp) != NULL) {
		size_t len = strlen(line);
		if (len == 0) continue;

		if (line[len - 1] == '\n') {
			len--;
			line[len] = '\0';
		}

		/* Remove the ending slash: fstatat() won't take a symlink to dir as
		 * a symlink (but as a dir), if the file name ends with a slash. */
		if (len > 1 && line[len - 1] == '/') {
			len--;
			line[len] = '\0';
		}

		if (!*line || *line == '#' || len == 0)
			continue;

		if (fstatat(XAT_FDCWD, line, &a, AT_SYMLINK_NOFOLLOW) == -1)
			continue;

		sel_elements = (struct sel_t *)xnrealloc(sel_elements,
			sel_n + 2, sizeof(struct sel_t));
		sel_elements[sel_n].name = savestring(line, len);
		sel_elements[sel_n].size = (off_t)UNSET;
		/* Store device and inode number to identify later selected files
		 * and mark them in the files list. */
		sel_devino = (struct devino_t *)xnrealloc(sel_devino,
			sel_n + 1, sizeof(struct devino_t));
		sel_devino[sel_n].ino = a.st_ino;
		sel_devino[sel_n].dev = a.st_dev;
		sel_n++;
		sel_elements[sel_n].name = (char *)NULL;
		sel_elements[sel_n].size = (off_t)UNSET;
	}

	fclose(fp);
	/* If previous and current amount of sel files don't match (mostly
	 * because some selected files were removed), recreate the selections
	 * file to reflect current state. */
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
		cdpaths = (char **)xnrealloc(cdpaths, n + 2, sizeof(char *));

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

static void
get_paths_timestamps(const size_t n)
{
	if (n == 0)
		return;

	struct stat a;
	int i = (int)n;
	while (--i >= 0) {
		if (paths[i].path && stat(paths[i].path, &a) != -1)
			paths[i].mtime = a.st_mtime;
		else
			paths[i].mtime = 0;
	}
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
		ptr = savestring(_PATH_STDPATH, sizeof(_PATH_STDPATH) - 1);
#else
		size_t s = confstr(_CS_PATH, NULL, 0); /* Get value's size */
		char *p = (char *)xnmalloc(s, sizeof(char)); /* Allocate space */
		confstr(_CS_PATH, p, s);               /* Get value */
		ptr = p;
#endif /* !_PATH_STDPATH */
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
	paths = (struct paths_t *)xnmalloc(c + 1, sizeof(struct paths_t));

	/* Get each path in PATH */
	size_t n = 0;
	char *p = path_tmp, *q = p;
	while (1) {
		if (*q && *q != ':' && *(++q))
			continue;

		char d = *q;
		*q = '\0';
		if (*p && (q - p) > 0) {
			size_t len = (size_t)(q - p);
			if (len > 1 && p[len - 1] == '/') {
				p[len - 1] = '\0';
				len--;
			}

			paths[n].path = savestring(p, len);
			n++;
		}

		if (!d || n == c)
			break;

		p = ++q;
	}

	paths[n].path = (char *)NULL;
	free(path_tmp);

	get_paths_timestamps(n);

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
	snprintf(last_file, config_dir_len + 7, "%s/.last", config_dir);

	int fd;
	FILE *fp = open_fread(last_file, &fd);
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

		if (ws_n >= 0 && ws_n < MAX_WS && !workspaces[ws_n].path) {
			workspaces[ws_n].path = savestring(p + 2,
				strnlen(p + 2, sizeof(line) - 2));
		}
	}

	fclose(fp);
	free(last_file);
	return EXIT_SUCCESS;
}

/* Restore pinned dir from file */
int
load_pinned_dir(void)
{
	if (config_ok == 0 || !config_dir)
		return EXIT_FAILURE;

	char *pin_file = (char *)xnmalloc(config_dir_len + 6, sizeof(char));
	snprintf(pin_file, config_dir_len + 6, "%s/.pin", config_dir);

	int fd;
	FILE *fp = open_fread(pin_file, &fd);
	if (!fp) {
		free(pin_file);
		return EXIT_FAILURE;
	}

	char line[PATH_MAX] = "";
	if (fgets(line, (int)sizeof(line), fp) == NULL) {
		free(pin_file);
		fclose(fp);
		return EXIT_FAILURE;
	}

	if (!*line || !strchr(line, '/')) {
		free(pin_file);
		fclose(fp);
		return EXIT_FAILURE;
	}

	if (pinned_dir) {
		free(pinned_dir);
		pinned_dir = (char *)NULL;
	}

	pinned_dir = savestring(line, strlen(line));
	fclose(fp);
	free(pin_file);
	return EXIT_SUCCESS;
}

#if defined(__CYGWIN__)
static int
check_cmd_ext(const char *s)
{
	if (!s || !*s)
		return 1;

	switch (TOUPPER(*s)) {
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

/* Check whether the path NAME is a symbolic link to any other path specified
 * in PATH. Returns 1 if true or 0 otherwise.
 * Used to avoid scanning paths which are symlinks to another path, for example,
 * /bin and /sbin, which are usually symlinks to /usr/bin and /usr/sbin
 * respectively. */
static int
skip_this_path(char *name)
{
	if (!name || !*name)
		return 1;

	struct stat a;
	if (lstat(name, &a) == -1)
		return 1;

	if (!S_ISLNK(a.st_mode))
		return 0;

	char *rpath = realpath(name, NULL);
	if (!rpath)
		return 1;

	size_t i;
	for (i = 0; paths[i].path; i++) {
		if (strcmp(paths[i].path, rpath) == 0) {
			free(rpath);
			return 1;
		}
	}

	free(rpath);
	return 0;
}

/* Get the list of files in PATH, plus CliFM internal commands, aliases, and
 * action names, and store them into an array (bin_commands) to be read by
 * my_rl_completion(). */
void
get_path_programs(void)
{
	int i, l = 0, total_cmd = 0;
	int *cmd_n = (int *)0;
	struct dirent ***commands_bin = (struct dirent ***)NULL;

	if (conf.ext_cmd_ok == 1) {
		char tmp[PATH_MAX] = "";
		char *cwd = get_cwd(tmp, sizeof(tmp), 0);

		commands_bin = (struct dirent ***)xnmalloc(path_n,
			sizeof(struct dirent));
		cmd_n = (int *)xnmalloc(path_n, sizeof(int));

		i = (int)path_n;
		while (--i >= 0) {
			if (!paths[i].path || !*paths[i].path
			|| skip_this_path(paths[i].path) == 1
			|| xchdir(paths[i].path, NO_TITLE) == -1) {
				cmd_n[i] = 0;
				continue;
			}

			cmd_n[i] = scandir(paths[i].path, &commands_bin[i],
#if defined(__CYGWIN__)
					NULL, xalphasort);
#else
					conf.light_mode == 1 ? NULL : skip_nonexec, xalphasort);
#endif /* __CYGWIN__ */
			/* If paths[i] directory does not exist, scandir returns -1.
			 * Fedora, for example, adds HOME/bin and HOME/.local/bin to
			 * PATH disregarding if they exist or not. If paths[i] dir is
			 * empty do not use it either. */
			if (cmd_n[i] > 0)
				total_cmd += cmd_n[i];
		}

		xchdir(cwd, NO_TITLE);
	}

	/* Add internal commands */
	for (internal_cmds_n = 0; internal_cmds[internal_cmds_n].name;
		internal_cmds_n++);

	bin_commands = (char **)xnmalloc((size_t)total_cmd
		+ internal_cmds_n + aliases_n + actions_n + 2, sizeof(char *));

	i = (int)internal_cmds_n;
	while (--i >= 0) {
		bin_commands[l] = savestring(internal_cmds[i].name,
			internal_cmds[i].len);
		l++;
	}

	/* Now add aliases, if any */
	if (aliases_n > 0) {
		i = (int)aliases_n;
		while (--i >= 0) {
			bin_commands[l] = savestring(aliases[i].name,
				strlen(aliases[i].name));
			l++;
		}
	}

	/* And user defined actions too, if any */
	if (actions_n > 0) {
		i = (int)actions_n;
		while (--i >= 0) {
			bin_commands[l] = savestring(usr_actions[i].name,
				strlen(usr_actions[i].name));
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
	}

	free(commands_bin);
	free(cmd_n);
	path_progsn = (size_t)l;
	bin_commands[l] = (char *)NULL;
}

static void
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

static void
write_alias(const char *s, char *p)
{
	aliases = (struct alias_t *)xnrealloc(aliases, aliases_n + 2,
		sizeof(struct alias_t));
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

static int
alias_exists(const char *s)
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
	if (config_ok == 0 || !config_file)
		return;

	int fd;
	FILE *fp = open_fread(config_file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: alias: '%s': %s\n",
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
	fclose(fp);
}

static void
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
	xstrsncpy(old_pwd[dirhist_total_index], line, (size_t)len + 1);
	dirhist_total_index++;
}

int
load_dirhist(void)
{
	if (config_ok == 0 || !dirhist_file)
		return EXIT_FAILURE;

	truncate_file(dirhist_file, conf.max_dirhist, 1);

	int fd;
	FILE *fp = open_fread(dirhist_file, &fd);
	if (!fp)
		return EXIT_FAILURE;

	size_t dirs = 0;

	char tmp_line[PATH_MAX];
	while (fgets(tmp_line, (int)sizeof(tmp_line), fp))
		dirs++;

	if (dirs == 0) {
		fclose(fp);
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

	fclose(fp);
	old_pwd[dirhist_total_index] = (char *)NULL;
	free(line);
	dirhist_cur_index = dirhist_total_index - 1;
	return EXIT_SUCCESS;
}

static void
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
	if (config_ok == 0 || !config_file)
		return;

	int fd;
	FILE *fp = open_fread(config_file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: prompt: '%s': %s\n",
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
		prompt_cmds = (char **)xnrealloc(prompt_cmds,
		    prompt_cmds_n + 1, sizeof(char *));
		prompt_cmds[prompt_cmds_n] = savestring(
		    line + 10, (size_t)line_len - 10);
		prompt_cmds_n++;
	}

	free(line);
	fclose(fp);
}

/* Get the length of the current time format.
 * We need this to construct the time string in case of invalid timestamp (0),
 * and to calculate the space left to print file names in long view. */

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
	/* +1 = extra space to avoid hitting the screen right edge in long view. */
		prop_fields.len += (7 + 1);
		xstrsncpy(invalid_time_str, " -     ", sizeof(invalid_time_str));
		return;
	}

	/* Get length of the current time format. */
	struct tm tm;
	time_t t = time(NULL);
	char tim[MAX_TIME_STR];
	char *tfmt = conf.time_str ? conf.time_str : DEF_TIME_STYLE_OLDER;
	size_t l = localtime_r(&t, &tm) ? strftime(tim, sizeof(tim), tfmt, &tm) : 0;

	/* Construct the invalid time format string (used when we get an
	 * invalid file timestamp). */
	if (l > MAX_TIME_STR)
		l = MAX_TIME_STR;

	size_t i;
	*invalid_time_str = '-';
	for (i = 1; i < l; i++)
		invalid_time_str[i] = ' ';
	invalid_time_str[i] = '\0';

	/* Append the time string length to the properties total length, so that
	 * we can better calculate how much space left we have to print file names. */
	prop_fields.len += (int)(l + 1);
}
#pragma GCC diagnostic pop

static void
set_sudo_cmd(void)
{
	if (sudo_cmd)
		return;

	sudo_cmd = (xargs.secure_env != 1 && xargs.secure_env_full != 1
		&& xargs.secure_cmds != 1) ? getenv("CLIFM_SUDO_CMD") : (char *)NULL;

	if (!sudo_cmd || !*sudo_cmd) {
		sudo_cmd = DEF_SUDO_CMD;
		return;
	}

	char *sudo_path = get_cmd_path(sudo_cmd);
	if (sudo_path) {
		free(sudo_path);
		return;
	}

	err('w', PRINT_PROMPT, _("%s: %s: %s\nInvalid authentication program "
		"(falling back to '%s')\n"), PROGRAM_NAME, sudo_cmd,
		strerror(errno), DEF_SUDO_CMD);
	sudo_cmd = DEF_SUDO_CMD;
}

/* If some option was not set, set it to the default value. */
void
check_options(void)
{
	set_sudo_cmd();

	if (!conf.histignore_regex
	&& regcomp(&regex_hist, DEF_HISTIGNORE, REG_NOSUB | REG_EXTENDED) == 0) {
		conf.histignore_regex =
			savestring(DEF_HISTIGNORE, sizeof(DEF_HISTIGNORE) - 1);
	}

	if (conf.trim_names == UNSET) {
		if (xargs.trim_names == UNSET)
			conf.trim_names = DEF_TRIM_NAMES;
		else
			conf.trim_names = xargs.trim_names;
	}

	if (conf.columned == UNSET)
		conf.columned = DEF_COLUMNS;

	conf.max_name_len_bk = conf.max_name_len;
	if (conf.trim_names == 0)
		conf.max_name_len = UNSET;

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
		xstrsncpy(prop_fields_str, DEF_PROP_FIELDS, sizeof(prop_fields_str));
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

/*	if (!conf.wprompt_str) {
		if (conf.colorize == 1)
			conf.wprompt_str = savestring(DEF_WPROMPT_STR, sizeof(DEF_WPROMPT_STR) - 1);
		else
			conf.wprompt_str = savestring(DEF_WPROMPT_STR_NO_COLOR,
				sizeof(DEF_WPROMPT_STR_NO_COLOR) - 1);
	} */

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
			if (bin_flags & FZF_BIN_OK)
				fzftab = 1;
		} else {
			fzftab = xargs.fzftab;
		}

		if (xargs.fnftab == 1)
			tabmode = FNF_TAB;
		else if (xargs.fzftab == 1)
			tabmode = FZF_TAB;
		else if (xargs.smenutab == 1)
			tabmode = SMENU_TAB;
		else
			tabmode = STD_TAB;
	}

	if (!conf.fzftab_options) {
		if (conf.colorize == 1 || !getenv("FZF_DEFAULT_OPTS")) {
			if (conf.colorize == 1) {
				conf.fzftab_options =
					savestring(DEF_FZFTAB_OPTIONS, sizeof(DEF_FZFTAB_OPTIONS) - 1);
			} else {
				conf.fzftab_options =
					savestring(DEF_FZFTAB_OPTIONS_NO_COLOR,
					sizeof(DEF_FZFTAB_OPTIONS_NO_COLOR) - 1);
			}
/*			char *pp = conf.colorize == 1
				? DEF_FZFTAB_OPTIONS : DEF_FZFTAB_OPTIONS_NO_COLOR;
			conf.fzftab_options = savestring(pp, strlen(pp)); */
		} else {
			conf.fzftab_options = savestring("", 1);
		}
	}

	set_fzf_preview_border_type();

	smenutab_options_env = (xargs.secure_env_full != 1 && tabmode == SMENU_TAB)
		? getenv("CLIFM_SMENU_OPTIONS") : (char *)NULL;

	if (smenutab_options_env
	&& sanitize_cmd(smenutab_options_env, SNT_BLACKLIST) != 0) {
		err('w', PRINT_PROMPT, "%s: CLIFM_SMENU_OPTIONS contains unsafe "
			"characters (<>|;&$`). Falling back to default values.\n",
			PROGRAM_NAME);
		smenutab_options_env = (char *)NULL;
	}

#else
	tabmode = STD_TAB;
#endif /* _NO_FZF */

#ifndef _NO_LIRA
	if (xargs.stealth_mode == 1) {
		xargs.fzf_preview = conf.fzf_preview = 0;
	} else if (conf.fzf_preview == UNSET) {
		if (xargs.fzf_preview == UNSET)
			conf.fzf_preview = DEF_FZF_PREVIEW;
		else
			conf.fzf_preview = xargs.fzf_preview;
	}
#else
	if (conf.fzf_preview == UNSET)
		xargs.fzf_preview = conf.fzf_preview = 0;
#endif /* !_NO_LIRA */

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

	if (conf.log_msgs == UNSET)
		conf.log_msgs = DEF_LOG_MSGS;

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

	/* This block was needed when Shell was an option in the config file.
	 * Let's keep it in case we readd this option. For the time being, the
	 * shell used to run external commands is set via environment variables,
	 * either CLIFM_SHELL, or SHELL. */
/*	if (!user.shell) {
		struct user_t tmp_user = get_user_data();
		user.shell = tmp_user.shell;

		// We don't need these values of the user struct: free them
		free(tmp_user.name);
		free(tmp_user.home);
		free(tmp_user.groups);

		if (!user.shell)
			user.shell = savestring(FALLBACK_SHELL, sizeof(FALLBACK_SHELL) - 1);

		free(user.shell_basename);
		user.shell_basename = (char *)NULL;
		shell = get_sys_shell();
	} */

	if (!conf.term)
		conf.term = savestring(DEF_TERM_CMD, sizeof(DEF_TERM_CMD) - 1);

	if (conf.colorize == 0)
		expand_prompt_name(DEF_PROMPT_NO_COLOR_NAME);

	if (!conf.encoded_prompt || !*conf.encoded_prompt) {
		free(conf.encoded_prompt);
		if (conf.colorize == 1) {
			conf.encoded_prompt =
				savestring(DEFAULT_PROMPT, sizeof(DEFAULT_PROMPT) - 1);
		} else {
			conf.encoded_prompt =
				savestring(DEFAULT_PROMPT_NO_COLOR,
				sizeof(DEFAULT_PROMPT_NO_COLOR) - 1);
		}
	}

	if (!conf.wprompt_str) {
		if (conf.colorize == 1)
			conf.wprompt_str = savestring(DEF_WPROMPT_STR, sizeof(DEF_WPROMPT_STR) - 1);
		else
			conf.wprompt_str = savestring(DEF_WPROMPT_STR_NO_COLOR,
				sizeof(DEF_WPROMPT_STR_NO_COLOR) - 1);
	}

	if ((xargs.stealth_mode == 1 || home_ok == 0 ||
	config_ok == 0 || !config_file) && !*div_line)
		xstrsncpy(div_line, DEF_DIV_LINE, sizeof(div_line));

	if (xargs.stealth_mode == 1 && !conf.opener) {
		/* Since in stealth mode we have no access to the config file, we cannot
		 * use Lira, since it relays on a file. Set it thus to FALLBACK_OPENER,
		 * if not already set via command line */
		conf.opener = savestring(FALLBACK_OPENER, sizeof(FALLBACK_OPENER) - 1);
	}

#ifndef _NO_SUGGESTIONS
	if (term_caps.suggestions == 0)
		xargs.suggestions = conf.suggestions = 0;
#endif /* !_NO_SUGGESTIONS */
	if (term_caps.color == 0)
		xargs.colorize = conf.colorize = 0;
	if (term_caps.pager == 0)
		xargs.pager = conf.pager = 0;

	reset_opts();
}
