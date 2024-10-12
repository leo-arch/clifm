/* checks.c -- misc check functions */

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

#include <errno.h>
#include <string.h>
#include <unistd.h>

#if defined(MAC_OS_X_RENAMEAT_SYS_STDIO_H)
# include <sys/stdio.h> /* renameat(2) */
#endif /* MAC_OS_X_RENAMEAT_SYS_STDIO_H */

#if defined(HAVE_FILE_ATTRS)
# ifdef __TINYC__
#  undef SYNC_FILE_RANGE_WRITE_AND_WAIT /* Silence redefinition error */
# endif
#endif /* HAVE_FILE_ATTRS */

#include "aux.h"
#include "misc.h"
#include "sanitize.h" /* sanitize_cmd() */
#include "term_info.h"

#define TRUE_COLOR 16777216

/* Check whether parameter S is -f or --force.
 * Returns 1 if yes, and there is no "-f" or "--force" file in the current
 * dir, or 0 otherwise. */
int
is_force_param(const char *s)
{
	if (!s || !*s || *s != '-')
		return 0;

	struct stat a;
	if ((strcmp(s, "-f") == 0 && lstat("-f", &a) == -1)
	|| (strcmp(s, "--force") == 0 && lstat("--force", &a) == -1))
		return 1;

	return 0;
}

int
check_glob_char(const char *str, const int gflag)
{
	if (!str || !*str)
		return 0;
	return strpbrk(str, (gflag == GLOB_ONLY)
		? GLOB_CHARS : GLOB_REGEX_CHARS) ? 1 : 0;
}

int
is_file_in_cwd(char *name)
{
	if (!name || !*name || !workspaces[cur_ws].path)
		return 0;

	const char *s = strchr(name, '/');
	if (!s || !*(s + 1)) /* 'name' or 'name/' */
		return 1;

	char *rpath = normalize_path(name, strlen(name));
	if (!rpath)
		return 0;

	const char *cwd = workspaces[cur_ws].path;
	const size_t cwd_len = strlen(cwd);
	const size_t rpath_len = strlen(rpath);

	if (rpath_len < cwd_len
	|| strncmp(rpath, cwd, cwd_len) != 0
	|| (rpath_len > cwd_len && strchr(rpath + cwd_len + 1, '/'))) {
		free(rpath);
		return 0;
	}

	free(rpath);
	return 1;
}

int
is_url(const char *url)
{
	if ((*url == 'w' && url[1] == 'w' && url[2] == 'w' && url[3] == '.'
	&& url[4]) || strstr(url, "://") != NULL)
		return FUNC_SUCCESS;
	return FUNC_FAILURE;
}

/* See https://github.com/termstandard/colors#truecolor-detection */
static int
check_truecolor(void)
{
	char *c = getenv("COLORTERM");

	if (c && ((*c == 't' && strcmp(c + 1, "ruecolor") == 0)
	|| (*c == '2' && strcmp(c + 1, "4bit") == 0) ) )
		return 1;

	return 0;
}

static void
set_term_caps(const int i)
{
	const int true_color = check_truecolor();

	if (i == -1) { /* TERM not found in our terminfo database */
		term_caps.color = true_color == 1 ? TRUE_COLOR : 0;
		if (term_caps.color <= 8)
			memset(dim_c, '\0', sizeof(dim_c));
		term_caps.suggestions = 0;
		term_caps.pager = 0;
		term_caps.hide_cursor = 0;
		term_caps.home = 0;
		term_caps.clear = 0;
		term_caps.del_scrollback = 0;
		return;
	}

	term_caps.home = TERM_INFO[i].home;
	term_caps.hide_cursor = TERM_INFO[i].hide_cursor;
	term_caps.clear = TERM_INFO[i].ed;
	term_caps.del_scrollback = TERM_INFO[i].del_scrollback;
	term_caps.req_cur_pos = TERM_INFO[i].req_cur_pos;

	term_caps.color = true_color == 1 ? TRUE_COLOR
		: (TERM_INFO[i].color > 0 ? TERM_INFO[i].color : 0);
	if (term_caps.color <= 8)
		memset(dim_c, '\0', sizeof(dim_c));

	term_caps.suggestions = (TERM_INFO[i].cub == 1 && TERM_INFO[i].ed == 1
		&& TERM_INFO[i].el == 1) ? 1 : 0;

	term_caps.pager = (TERM_INFO[i].cub == 0 || TERM_INFO[i].el == 0) ? 0 : 1;
}

/* Check whether current terminal (ENV_TERM) supports colors and requesting
 * cursor position (needed to print suggestions). If not, disable the
 * feature accordingly. */
static void
check_term_support(const char *env_term)
{
	if (!env_term || !*env_term) {
		set_term_caps(-1);
		return;
	}

	size_t i;
	const size_t len = strlen(env_term);
	/* Color and cursor position request support. */
	int index = -1;

	if (*env_term == 'x' && strcmp(env_term, "xterm-kitty") == 0)
		flags |= KITTY_TERM;

	for (i = 0; TERM_INFO[i].name; i++) {
		if (*env_term != *TERM_INFO[i].name || len != TERM_INFO[i].len
		|| strcmp(env_term, TERM_INFO[i].name) != 0)
			continue;

		index = (int)i;
		break;
	}

	if (index == -1) {
		err('w', PRINT_PROMPT, _("%s: '%s': Terminal type not supported. "
			"Limited functionality is expected.\n"), PROGRAM_NAME, env_term);
	}

	set_term_caps(index);
}

/* Try to detect what kind of image capability the running terminal supports
 * (sixel, ueberzug, kitty protocol, and ascii).
 * Write the result into the CLIFM_IMG_SUPPORT environment variable.
 * This variable will be read by clifmimg to generate images using the
 * specified method. */
static void
check_img_support(void)
{
	if (xargs.stealth_mode == 1)
		/* If CLIFM_IMG_SUPPORT is unset, the clifmimg script falls back to
		 * the ascii method. */
		return;

	if (check_sixel_support() == 1) {
		setenv("CLIFM_IMG_SUPPORT", "sixel", 1);
	/* CLIFM_FIFO_UEBERZUG is set by the clifmrun script */
	} else if (getenv("CLIFM_FIFO_UEBERZUG")) {
		setenv("CLIFM_IMG_SUPPORT", "ueberzug", 1);
	/* CLIFM_KITTY_IMG is set by the clifmrun script */
	} else if (getenv("CLIFM_KITTY_IMG")) {
		setenv("CLIFM_IMG_SUPPORT", "kitty", 1);
	} else {
		setenv("CLIFM_IMG_SUPPORT", "ascii", 1);
	}
}

void
check_term(void)
{
	char *t = getenv("TERM");
	if (!t || !*t) {
		t = "xterm";
		err('w', PRINT_PROMPT, _("%s: The TERM environment variable is unset.\n"
			"Running in xterm compatibility mode\n"), PROGRAM_NAME);
	}

	check_term_support(t);
	check_img_support();
}

static void
set_mount_cmd(const int udisks2ok, const int udevilok)
{
	if (xargs.mount_cmd == MNT_UDISKS2 && !udisks2ok && udevilok) {
		err('w', PRINT_PROMPT, _("%s: udisks2: Command not found. Falling "
			"back to udevil\n"), PROGRAM_NAME);
		xargs.mount_cmd = MNT_UDEVIL;
		return;
	}

	if (xargs.mount_cmd != MNT_UDISKS2 && udevilok)
		xargs.mount_cmd = MNT_UDEVIL;
	else if (udisks2ok)
		xargs.mount_cmd = MNT_UDISKS2;
	else
		xargs.mount_cmd = UNSET;
}

#ifndef _NO_FZF
/* fzf, fnf, and smenu are used as alternative TAB completion mechanisms.
 * fnf and smenu fallback to default if not found.
 * The default value is fzf, if found, or standard (readline). */
void
check_completion_mode(void)
{
	if (fzftab == 1) { /* fzftab is zero only if running with --stdtab */
		if (!(bin_flags & FZF_BIN_OK) && tabmode == FZF_TAB) {
			err('w', PRINT_PROMPT, _("%s: fzf: Command not found. Falling "
				"back to standard TAB completion\n"), PROGRAM_NAME);
			tabmode = STD_TAB;
			fzftab = 0;
		}

		if (!(bin_flags & FNF_BIN_OK) && tabmode == FNF_TAB) {
			err('w', PRINT_PROMPT, _("%s: fnf: Command not found. Falling "
				"back to the default value (fzf, if found, or standard)\n"),
				PROGRAM_NAME);
			tabmode = (bin_flags & FZF_BIN_OK) ? FZF_TAB : STD_TAB;
		} else if (!(bin_flags & SMENU_BIN_OK) && tabmode == SMENU_TAB) {
			err('w', PRINT_PROMPT, _("%s: smenu: Command not found. Falling "
				"back to the default value (fzf, if found, or standard)\n"),
				PROGRAM_NAME);
			tabmode = (bin_flags & FZF_BIN_OK) ? FZF_TAB : STD_TAB;
		}

		if (tabmode == STD_TAB) {
			if (bin_flags & FZF_BIN_OK)
				/* We have the fzf binary, let's run in FZF mode */
				tabmode = FZF_TAB;
			else
				/* Either specified mode was not found or no mode was
				 * specified */
				fzftab = 0;
		}
	} else {
		tabmode = STD_TAB;
		fzftab = 0;
	}
}
#endif /* !_NO_FZF */

/* Same thing as check_third_party_cmds(), but slower, since we need to run
 * malloc(3) and access(3) for each checked command. */
static void
check_third_party_cmds_alt(void)
{
	int udisks2ok = 0, udevilok = 0;
	char *p = (char *)NULL;

	if ( (p = get_cmd_path("fzf")) ) {
		free(p);
		bin_flags |= FZF_BIN_OK;
		if (fzftab == UNSET) fzftab = 1;
	}

	if ( (p = get_cmd_path("fnf")) ) {
		free(p);
		bin_flags |= FNF_BIN_OK;
		if (fzftab == UNSET) fzftab = 1;
	}

	if ( (p = get_cmd_path("smenu")) ) {
		free(p);
		bin_flags |= SMENU_BIN_OK;
		if (fzftab == UNSET) fzftab = 1;
	}

	if ( (p = get_cmd_path("udisksctl")) ) {
		free(p); udisks2ok = 1;
	}

	if ( (p = get_cmd_path("udevil")) ) {
		free(p); udevilok = 1;
	}

#if defined(USE_DU1) && !defined(HAVE_GNU_DU) && !defined(_BE_POSIX)
	if ( (p = get_cmd_path("gdu")) ) {
		free(p); bin_flags |= GNU_DU_BIN_GDU;
	}
#endif /* USE_DU1 && !HAVE_GNU_DU && !_BE_POSIX */

#if defined(CHECK_COREUTILS)
	if ( (p = get_cmd_path("grm")) ) {
		free(p); bin_flags |= BSD_HAVE_COREUTILS;
	}
#endif /* CHECK_COREUTILS */

	set_mount_cmd(udisks2ok, udevilok);
}

/* Let's check for third-party programs */
void
check_third_party_cmds(void)
{
#if defined(USE_DU1) && defined(HAVE_GNU_DU)
	bin_flags |= GNU_DU_BIN_DU;
#endif /* USE_DU1 && HAVE_GNU_DU */

	if (conf.ext_cmd_ok == 0) {
		/* We haven't loaded system binaries. Let's run an alternative,
		 * though slower, check. */
		check_third_party_cmds_alt();
		return;
	}


	int udisks2ok = 0, udevilok = 0, check_coreutils = 0;
#if defined(CHECK_COREUTILS)
	check_coreutils = 1;
#endif /* CHECK_COREUTILS */

	int i = (int)path_progsn;

	while (--i >= 0) {
		if (*bin_commands[i] != 'u' && *bin_commands[i] != 'f'
		&& *bin_commands[i] != 's'
#if defined(USE_DU1) && !defined(HAVE_GNU_DU) && !defined(_BE_POSIX)
		&& *bin_commands[i] != 'g')
#else
		)
#endif /* USE_DU1 && !HAVE_GNU_DU && !_BE_POSIX */
			continue;

		if (*bin_commands[i] == 'f' && strcmp(bin_commands[i], "fzf") == 0) {
			bin_flags |= FZF_BIN_OK;
			if (fzftab == UNSET)
				fzftab = 1;
		}

		if (*bin_commands[i] == 'f' && strcmp(bin_commands[i], "fnf") == 0) {
			bin_flags |= FNF_BIN_OK;
			if (fzftab == UNSET)
				fzftab = 1;
		}

		if (*bin_commands[i] == 's' && strcmp(bin_commands[i], "smenu") == 0) {
			bin_flags |= SMENU_BIN_OK;
			if (fzftab == UNSET)
				fzftab = 1;
		}

		if (*bin_commands[i] == 'u'
		&& strcmp(bin_commands[i], "udisksctl") == 0)
			udisks2ok = 1;
		if (*bin_commands[i] == 'u' && strcmp(bin_commands[i], "udevil") == 0)
			udevilok = 1;

#if defined(USE_DU1) && !defined(HAVE_GNU_DU) && !defined(_BE_POSIX)
		if (*bin_commands[i] == 'g' && strcmp(bin_commands[i] + 1, "du") == 0)
			bin_flags |= GNU_DU_BIN_GDU;
#endif /* USE_DU1 && !HAVE_GNU_DU && !_BE_POSIX */

#if defined(CHECK_COREUTILS)
		if (*bin_commands[i] == 'g' && strcmp(bin_commands[i] + 1, "rm") == 0) {
			bin_flags |= BSD_HAVE_COREUTILS;
			check_coreutils = 0;
		}
#endif /* CHECK_COREUTILS */

		if (udevilok == 1 && udisks2ok == 1 && check_coreutils == 0
		&& (bin_flags & (FZF_BIN_OK & FNF_BIN_OK & SMENU_BIN_OK
#if defined(USE_DU1) && !defined(HAVE_GNU_DU) && !defined(_BE_POSIX)
		& GNU_DU_BIN_GDU)))
#else
		)))
#endif /* USE_DU1 && !HAVE_GNU_DU && !_BE_POSIX */
			break;
	}

	set_mount_cmd(udisks2ok, udevilok);
}

/* Return 1 if at least one of the user's groups match the file gid GID.
 * Otherwise, return 0. */
static int
check_user_groups(const gid_t gid)
{
	int i;
	for (i = 0; i < user.ngroups; i++) {
		if (user.groups[i] == gid)
			return 1;
	}

	return 0;
}

/* Return 1 if current user has access (read for files and read/exec for dirs)
 * to the file with mode MODE, uid UID, and gid GID. Otherwise, 0 is
 * returned. */
int
check_file_access(const mode_t mode, const uid_t uid, const gid_t gid)
{
	if (user.uid == 0) /* We are root */
		return 1;

	int f = 0; /* Hold file ownership flags */

	const mode_t val = (mode & (mode_t)~S_IFMT);
	if (val & S_IRUSR) f |= R_USR;
	if (val & S_IXUSR) f |= X_USR;

	if (val & S_IRGRP) f |= R_GRP;
	if (val & S_IXGRP) f |= X_GRP;

	if (val & S_IROTH) f |= R_OTH;
	if (val & S_IXOTH) f |= X_OTH;

	if (S_ISDIR(mode)) {
		if ((f & R_USR) && (f & X_USR) && uid == user.uid)
			return 1;
		if ((f & R_GRP) && (f & X_GRP) && (gid == user.gid
		|| check_user_groups(gid) == 1))
			return 1;
		if ((f & R_OTH) && (f & X_OTH))
			return 1;
	} else {
		if ((f & R_USR) && uid == user.uid)
			return 1;
		if ((f & R_GRP) && (gid == user.gid
		|| check_user_groups(gid) == 1))
			return 1;
		if (f & R_OTH)
			return 1;
	}

	return 0;
}

char *
get_sudo_path(void)
{
	if (!sudo_cmd) {
		errno = ENOENT;
		return (char *)NULL;
	}

	char *sudo_path = get_cmd_path(sudo_cmd);
	int ret = errno;

	if (!sudo_path) {
		xerror("%s: '%s': %s\n", PROGRAM_NAME, sudo_cmd, strerror(ENOENT));
		errno = ret;
		return (char *)NULL;
	}

	return sudo_path;
}

/* Check whether a given string contains only digits. Returns 1 if true
 * and 0 if false. It does not work with negative numbers. */
int
is_number(const char *restrict str)
{
	for (; *str; str++)
		if (*str > '9' || *str < '0')
			return 0;

	return 1;
}

/* Check if command STR contains a digit and this digit is not the first
 * char of STR. Used by find_cmd() (called by is_internal_c()) to check
 * for fused parameters in internal commands.
 * Returns the index of the digit in STR or -1 if no digit is found. */
static int
contains_digit(const char *str)
{
	if (!str || !*str || !*(++str))
		return (-1);

	int i = 1;

	while (*str) {
		if (*str >= '1' && *str <= '9')
			return i;
		str++;
		i++;
	}

	return (-1);
}

/* Return 1 if CMD is found in CMDS_LIST or zero otherwise. */
static int
find_cmd(const struct cmdslist_t *cmds_list, const size_t list_size, char *cmd)
{
	int found = 0;
	int i = (int)list_size;
	char c = 0;
	const int d = contains_digit(cmd);

	if (d != -1) {
		c = cmd[d];
		cmd[d] = '\0';
	}

	const size_t cmd_len = strlen(cmd);
	while (--i >= 0) {
		if (*cmd == *cmds_list[i].name && cmd_len == cmds_list[i].len
		&& strcmp(cmd, cmds_list[i].name) == 0) {
			found = 1;
			break;
		}
	}

	if (d != -1)
		cmd[d] = c;

	return found;
}

/* Check whether S is an action name.
 * Returns 1 if true or 0 otherwise. */
int
is_action_name(const char *s)
{
	if (actions_n == 0)
		return 0;

	int n = (int)actions_n;
	while (--n >= 0) {
		if (*s == *usr_actions[n].name && strcmp(s, usr_actions[n].name) == 0)
			return 1;
	}

	return 0;
}

/* Return 1 if CMD is an internal command, or zero otherwise. */
int
is_internal_c(char *restrict cmd)
{
	if (find_cmd(internal_cmds, internal_cmds_n, cmd))
		return 1;

	/* Check for the search and history functions as well */
	if ((*cmd == '/' && access(cmd, F_OK) != 0) || (*cmd == '!'
	&& (IS_DIGIT(cmd[1]) || (cmd[1] == '-' && IS_DIGIT(cmd[2]))
	|| cmd[1] == '!')))
		return 1;

	return 0;
}

/* Check cmd against a list of internal commands. Used by parse_input_str()
 * to know whether it should perform additional expansions, like glob, regex,
 * tilde, and so on. Only internal commands dealing with ELN/filenames
 * should be checked here. */
int
is_internal(char *restrict cmd)
{
	static struct cmdslist_t const int_cmds[] = {
		{"ac", 2},
		{"ad", 2},
		{"bb", 2},
		{"bleach", 6},
		{"bm", 2},
		{"bookmarks", 9},
		{"bl", 2},
		{"br", 2},
		{"bulk", 4},
		{"c", 1},
		{"cd", 2},
		{"d", 1},
		{"dup", 3},
		{"exp", 3},
		{"export", 6},
		{"jc", 2},
		{"jp", 2},
		{"l", 1},
		{"le", 2},
		{"m", 1},
		{"mm", 2},
		{"mime", 4},
		{"n", 1},
		{"new", 3},
		{"o", 1},
		{"oc", 2},
		{"open", 4},
		{"paste", 5},
		{"p", 1},
		{"pc", 2},
		{"pp", 2},
		{"pr", 2},
		{"prop", 4},
		{"pin", 3},
		{"r", 1},
		{"s", 1},
		{"sel", 3},
		{"t", 1},
		{"tr", 2},
		{"trash", 5},
		{"tag", 3},
		{"ta", 2},
		{"te", 2},
		{"v", 1},
		{"vv", 2},
		{NULL, 0}
	};

	static size_t i = 0;
	if (i == 0)
		i = (sizeof(int_cmds) / sizeof(struct cmdslist_t)) - 1;

	if (find_cmd(int_cmds, i, cmd))
		return 1;

	/* Check for the search function as well */
	if (*cmd == '/' && access(cmd, F_OK) != 0)
		return 1;

	return 0;
}

/* Check CMD against a list of internal commands taking ELN's or numbers
 * as parameters. Used by split_fusedcmd(). */
int
is_internal_f(const char *restrict cmd)
{
	/* If we are completing/suggesting, do not take 'ws', 'mf', and 'st/sort'
	 * commands into account: they do not take ELN/filenames as parameters,
	 * but just numbers, in which case no ELN-filename completion should
	 * be made. */
	if (flags & STATE_COMPLETING
	&& (*cmd == 'w' || (*cmd == 'm' && *(cmd + 1) == 'f')
	|| (*cmd == 's' && (*(cmd + 1) == 't' || *(cmd + 1) == 'o')) ) )
		return 0;

	static struct cmdslist_t const int_cmds[] = {
		{"ac", 2},
		{"ad", 2},
		{"alias", 5}, /* 'alias import' takes file names */
		{"bb", 2},
		{"bh", 2}, // REMOVE AS SOON AS REPLACED BY DH
		{"fh", 2}, // REMOVE AS SOON AS REPLACED BY DH
		{"dh", 2},
		{"bl", 2},
		{"bleach", 6},
		{"bm", 2},
		{"bookmarks", 9},
		{"br", 2},
		{"bulk", 4},
		{"c", 1},
		{"cd", 2},
		{"d", 1},
		{"dup", 3},
		{"ds", 2},
		{"desel", 5},
		{"exp", 3},
		{"l", 1},
		{"ln", 2},
		{"le", 2},
		{"m", 1},
		{"mime", 4},
		{"mm", 2},
		{"md", 2},
		{"mf", 2},
		{"n", 1},
		{"new", 3},
		{"o", 1},
		{"oc", 2},
		{"open", 4},
		{"ow", 2},
		{"p", 1},
		{"pc", 2},
		{"pp", 2},
		{"pr", 2},
		{"prop", 4},
		{"paste", 5},
		{"pin", 3},
		{"r", 1},
		{"rr", 2},
		{"s", 1},
		{"sel", 3},
		{"st", 2},
		{"sort", 4},
		{"t", 1},
		{"tr", 2},
		{"trash", 5},
		{"tag", 3},
		{"ta", 2},
		{"te", 2},
		{"tl", 2},
		{"v", 1},
		{"vv", 2},
		{"ws", 2},
		{"x", 1},
		{"X", 1},
		{NULL, 0}
	};

	static int n = 0;
	if (n == 0)
		n = (int)(sizeof(int_cmds) / sizeof(struct cmdslist_t)) - 1;
	const size_t cmd_len = strlen(cmd);

	int i = n;
	while (--i >= 0) {
		if (*cmd == *int_cmds[i].name && cmd_len == int_cmds[i].len
		&& strcmp(cmd, int_cmds[i].name) == 0) {
			return 1;
		}
	}

	return 0;
}

/* Return one if STR is a command in PATH or zero if not. */
int
is_bin_cmd(char *str)
{
	char *p = str, *q = str;
	int index = 0, space_index = -1;

	while (*p) {
		if (*p == ' ') {
			*p = '\0';
			space_index = index;
			break;
		}
		p++;
		index++;
	}

	size_t i;
	for (i = 0; bin_commands[i]; i++) {
		if (*q == *bin_commands[i] && q[1] == bin_commands[i][1]
		&& strcmp(q, bin_commands[i]) == 0) {
			if (space_index != -1)
				q[space_index] = ' ';
			return 1;
		}
	}

	if (space_index != -1)
		q[space_index] = ' ';

	return 0;
}

int
check_regex(char *str)
{
	if (!str || !*str)
		return FUNC_FAILURE;

	int char_found = 0;
	char *p = str;

	while (*p) {
		/* If STR contains at least one of the following chars */
		if (*p == '*' || *p == '?' || *p == '[' || *p == '{' || *p == '^'
		|| *p == '.' || *p == '|' || *p == '+' || *p == '$') {
			char_found = 1;
			break;
		}

		p++;
	}

	/* And if STR is not a file name, take it as a possible regex */
	if (char_found == 1 && access(str, F_OK) == -1)
		return FUNC_SUCCESS;

	return FUNC_FAILURE;
}

/* Returns the parsed aliased command in an array of strings if
 * matching alias is found, or NULL if not. */
char **
check_for_alias(char **args)
{
	/* Do not expand alias if first word is an ELN or the alias name
	 * starts with a backslash. */
	if (aliases_n == 0 || !aliases || !args || flags & FIRST_WORD_IS_ELN
	|| *args[0] == '\\')
		return (char **)NULL;

	int i = (int)aliases_n;

	while (--i >= 0) {
		if (!aliases[i].name || !aliases[i].cmd || !*aliases[i].name
		|| !*aliases[i].cmd)
			continue;

		if (*aliases[i].name != *args[0]
		|| strcmp(args[0], aliases[i].name) != 0)
			continue;

		if (xargs.secure_cmds == 1
		&& sanitize_cmd(aliases[i].cmd, SNT_GRAL) == FUNC_FAILURE)
			continue;

		args_n = 0; /* Reset args_n to be used by parse_input_str() */

		char **alias_comm = parse_input_str(aliases[i].cmd);
		if (!alias_comm) {
			flags |= FAILED_ALIAS; /* Prevent exec_cmd() from being executed */
			return (char **)NULL;
		}

		size_t j;
		/* Add input parameters, if any, to alias_comm */
		if (args[1]) {
			for (j = 1; args[j]; j++) {
				alias_comm = xnrealloc(alias_comm,
				    ++args_n + 2, sizeof(char *));
				alias_comm[args_n] = savestring(args[j], strlen(args[j]));
			}
		}

		alias_comm[args_n + 1] = (char *)NULL;

		/* Free original command */
		for (j = 0; args[j]; j++)
			free(args[j]);
		free(args);
		return alias_comm;
	}

	return (char **)NULL;
}

/* Keep only the last MAX records in FILE.
 * If CHECK_DUPS is 1, skip consecutive equal entries. */
void
truncate_file(char *file, const int max, const int check_dups)
{
	if (config_ok == 0 || !file || !*file)
		return;

	char *tmp_name = (char *)NULL;
	FILE *orig_fp = (FILE *)NULL;
	struct stat attr;

	int orig_fd = 0;
	if (stat(file, &attr) == -1) {
		/* File doesn't exist: create it and exit. */
		orig_fp = open_fwrite(file, &orig_fd);
		if (!orig_fp) {
			err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME, file,
				strerror(errno));
		} else {
			fclose(orig_fp);
		}

		return;
	}

	/* Open the original file for read. */
	if (!(orig_fp = open_fread(file, &orig_fd))) {
		err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME, file,
			strerror(errno));
		return;
	}

	int n = 0, c;

	/* Count newline chars to get the amount of lines in the file. */
	while ((c = fgetc(orig_fp)) != EOF && n < INT_MAX) { /* Flawfinder: ignore */
		if (c == '\n')
			n++;
	}

	if (c == EOF && ferror(orig_fp)) {
		err('w', PRINT_PROMPT, _("%s: '%s': Error reading file\n"),
			PROGRAM_NAME, file);
		goto EXIT;
	}

	if (n <= max)
		goto EXIT;

	/* Set the file pointer to the beginning of the original file. */
	fseek(orig_fp, 0, SEEK_SET);

	const size_t len = config_dir_len + (sizeof(TMP_FILENAME) - 1) + 2;
	tmp_name = xnmalloc(len, sizeof(char));
	snprintf(tmp_name, len, "%s/%s", config_dir, TMP_FILENAME);

	int tmp_fd = mkstemp(tmp_name);
	if (tmp_fd == -1) {
		err('w', PRINT_PROMPT, "%s: '%s': %s", PROGRAM_NAME, tmp_name,
			strerror(errno));
		goto EXIT;
	}

	FILE *tmp_fp = fdopen(tmp_fd, "w");
	if (!tmp_fp) {
		unlinkat(tmp_fd, tmp_name, 0);
		err('w', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME, tmp_name,
			strerror(errno));
		close(tmp_fd);
		goto EXIT;
	}

	int i = 1;
	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;
	char *prev_line = (char *)NULL;
	ssize_t prev_line_len = 0;

	while ((line_len = getline(&line, &line_size, orig_fp)) > 0) {
		/* Skip consecutive equal entries. */
		if (check_dups == 1 && prev_line && line_len == prev_line_len
		&& strcmp(line, prev_line) == 0)
			continue;

		/* Delete old entries, i.e., copy only new ones. */
		if (i >= n - (max - 1))
			fputs(line, tmp_fp);
		i++;

		if (check_dups == 1) {
			free(prev_line);
			prev_line = savestring(line, (size_t)line_len);
			prev_line_len = line_len;
		}
	}

	free(prev_line);
	free(line);

	renameat(tmp_fd, tmp_name, orig_fd, file);
	fclose(tmp_fp);

EXIT:
	free(tmp_name);
	fclose(orig_fp);
}
