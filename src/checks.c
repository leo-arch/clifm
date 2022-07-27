/* checks.c -- misc check functions */

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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef __NetBSD__
# include <sys/param.h>
# if __NetBSD_Prereq__(9,99,63)
#  include <sys/acl.h>
#  define _ACL_OK
# endif /* __NetBSD_Prereq__ */
#elif !defined(__HAIKU__) && !defined(__OpenBSD__)
# include <sys/acl.h>
# define _ACL_OK
#endif /* __NetBSD__ */

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "aux.h"
#include "misc.h"
#include "term_info.h"

/* Terminals known not to be able to handle escape sequences */
static const char *UNSUPPORTED_TERM[] = {"dumb", /*"cons25",*/ "emacs", NULL};

int
check_glob_char(const char *str, const int gflag)
{
	if (!str || !*str)
		return 0;
	return strpbrk(str, (gflag == GLOB_ONLY) ? GLOB_CHARS : GLOB_REGEX_CHARS) ? 1 : 0;
}

int
is_file_in_cwd(char *name)
{
	if (!name || !*name || !workspaces[cur_ws].path)
		return 0;

	char *s = strchr(name, '/');
	if (!s || !*(s + 1)) /* 'name' or 'name/' */
		return 1;

	char rpath[PATH_MAX];
	*rpath = '\0';
	char *ret = realpath(name, rpath);
	if (!ret || !*rpath)
		return 0;

	char *cwd = workspaces[cur_ws].path;
	size_t cwd_len = strlen(cwd);
	size_t rpath_len = strlen(rpath);
	if (rpath_len < cwd_len)
		return 0;

	if (strncmp(rpath, cwd, cwd_len) != 0)
		return 0;

	if (strchr(rpath + cwd_len + 1, '/'))
		return 0;

	return 1;
}

int
is_url(char *url)
{
	if ((*url == 'w' && url[1] == 'w' && url[2] == 'w' && url[3] == '.'
	&& url[4]) || strstr(url, "://") != NULL)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

/* Check whether current terminal (_TERM) supports colors and requesting
 * cursor position (needed to print suggestions). If not, disable the
 * feature accordingly */
static void
check_term_support(const char *_term)
{
	size_t i, len = strlen(_term);
	/* Color and cursor position request support */
	int cs = 0, cprs = 0;

	for (i = 0; TERM_INFO[i].name; i++) {
		if (*_term != *TERM_INFO[i].name || len != TERM_INFO[i].len)
			continue;
		if (strcmp(_term, TERM_INFO[i].name) != 0)
			continue;
		if (TERM_INFO[i].req_curpos == 1)
			cprs = 1;
		if (TERM_INFO[i].color != -1)
			cs = 1;
		break;
	}

	xargs.colorize = (cs == 0) ? 0 : UNSET;
	if (getenv("CLIFM_FORCE_COLOR"))
		xargs.colorize = 1;
#ifndef _NO_SUGGESTIONS
	xargs.suggestions = (cprs == 0) ? 0 : UNSET;
#else
	UNUSED(cprs);
#endif /* _NO_SUGGESTIONS */
}

void
check_term(void)
{
	char *_term = getenv("TERM");
	if (!_term || !*_term) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: Error opening terminal: unknown\n"),
			PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	size_t i;
	for (i = 0; UNSUPPORTED_TERM[i]; i++) {
		if (*_term == *UNSUPPORTED_TERM[i]
		&& strcmp(_term, UNSUPPORTED_TERM[i]) == 0) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: %s: Unsupported terminal. This "
				"terminal cannot understand escape sequences\n"),
				PROGRAM_NAME, UNSUPPORTED_TERM[i]);
			exit(EXIT_FAILURE);
		}
	}

	check_term_support(_term);

	return;
}

static void
set_mount_cmd(const int udisks2ok, const int udevilok)
{
	if (xargs.mount_cmd == MNT_UDISKS2 && !udisks2ok && udevilok) {
		_err('w', PRINT_PROMPT, _("%s: udisks2 not found. Falling back to "
			"udevil\n"), PROGRAM_NAME);
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
/* fzf, fzy, and smenu are used as alternative TAB completion mechanisms
 * fzy and smenu fallback to default if not found
 * the default value is fzf, if found, or standard (readline) */
void
check_completion_mode(void)
{
	if (fzftab == 1) { // fzftab is zero only if running with --stdtab
		if (!(finder_flags & FZF_BIN_OK) && tabmode == FZF_TAB) {
			_err('w', PRINT_PROMPT, _("%s: fzf: Command not found. Falling back to "
				"standard TAB completion\n"), PROGRAM_NAME);
			tabmode = STD_TAB;
			fzftab = 0;
		}

		if (!(finder_flags & FZY_BIN_OK) && tabmode == FZY_TAB) {
			_err('w', PRINT_PROMPT, _("%s: fzy: Command not found. Falling back to the "
				"default value (fzf, if found, or standard)\n"), PROGRAM_NAME);
			tabmode = (finder_flags & FZF_BIN_OK) ? FZF_TAB : STD_TAB;
		} else if (!(finder_flags & SMENU_BIN_OK) && tabmode == SMENU_TAB) {
			_err('w', PRINT_PROMPT, _("%s: smenu: Command not found. Falling back to the "
				"default value (fzf, if found, or standard)\n"), PROGRAM_NAME);
			tabmode = (finder_flags & FZF_BIN_OK) ? FZF_TAB : STD_TAB;
		}

		if (tabmode == STD_TAB) {
			if (finder_flags & FZF_BIN_OK) /* We have the fzf binary, let's run in FZF mode */
				tabmode = FZF_TAB;
			else /* Either specified mode was not found or no mode was specified */
				fzftab = 0;
		}
	} else {
/*		_err('w', PRINT_PROMPT, _("%s: fzf: Command not found. Falling back to "
			"standard TAB completion.\nTo remove this warning set "
			"TabCompletionMode to the appropriate value in the configuration "
			"file (F10 or 'edit')\n"), PROGRAM_NAME); */
		tabmode = STD_TAB;
		fzftab = 0;
	}
}
#endif /* !_NO_FZF */

/* Let's check for third-party programs */
void
check_third_party_cmds(void)
{
	int udisks2ok = 0, udevilok = 0;
	int i = (int)path_progsn;

	while (--i >= 0) {
		if (*bin_commands[i] != 'u' && *bin_commands[i] != 'f' && *bin_commands[i] != 's')
			continue;

		if (*bin_commands[i] == 'f' && strcmp(bin_commands[i], "fzf") == 0) {
			finder_flags |= FZF_BIN_OK;
			if (fzftab == UNSET)
				fzftab = 1;
		}

		if (*bin_commands[i] == 'f' && strcmp(bin_commands[i], "fzy") == 0) {
			finder_flags |= FZY_BIN_OK;
			if (fzftab == UNSET)
				fzftab = 1;
		}

		if (*bin_commands[i] == 's' && strcmp(bin_commands[i], "smenu") == 0) {
			finder_flags |= SMENU_BIN_OK;
			if (fzftab == UNSET)
				fzftab = 1;
		}

		if (*bin_commands[i] == 'u' && strcmp(bin_commands[i], "udisksctl") == 0)
			udisks2ok = 1;
		if (*bin_commands[i] == 'u' && strcmp(bin_commands[i], "udevil") == 0)
			udevilok = 1;

		if (udevilok == 1 && udisks2ok == 1
		&& (finder_flags & (FZF_BIN_OK | FZY_BIN_OK | SMENU_BIN_OK)))
			break;
	}

	set_mount_cmd(udisks2ok, udevilok);
}

/* Return 1 if current user has access to FILE. Otherwise, return zero */
int
check_file_access(const struct stat *file)
{
	int f = 0; /* Hold file ownership flags */

	mode_t val = (file->st_mode & (mode_t)~S_IFMT);
	if (val & S_IRUSR) f |= R_USR;
	if (val & S_IXUSR) f |= X_USR;

	if (val & S_IRGRP) f |= R_GRP;
	if (val & S_IXGRP) f |= X_GRP;

	if (val & S_IROTH) f |= R_OTH;
	if (val & S_IXOTH) f |= X_OTH;

	if (S_ISDIR(file->st_mode)) {
		if ((f & R_USR) && (f & X_USR) && file->st_uid == user.uid)
			return 1;
		if ((f & R_GRP) && (f & X_GRP) && file->st_gid == user.gid)
			return 1;
		if ((f & R_OTH) && (f & X_OTH))
			return 1;
	} else {
		if ((f & R_USR) && file->st_uid == user.uid)
			return 1;
		if ((f & R_GRP) && file->st_gid == user.gid)
			return 1;
		if (f & R_OTH)
			return 1;
	}

	return 0;
}

char *
get_sudo_path(void)
{
	char *p = getenv("CLIFM_SUDO_CMD");
	char *sudo = get_cmd_path(p ? p : DEF_SUDO_CMD);
	int ret = errno;

	if (!sudo) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME,
			p ? p : DEF_SUDO_CMD, strerror(ENOENT));
		errno = ret;
		return (char *)NULL;
	}

	return sudo;
}

/* Check a file's immutable bit. Returns 1 if true, zero if false, and
 * -1 in case of error */
int
check_immutable_bit(char *file)
{
#if !defined(FS_IOC_GETFLAGS) || !defined(FS_IMMUTABLE_FL)
	UNUSED(file);
	return 0;
#else
	int attr, fd, immut_flag = -1;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return -1;
	}

	ioctl(fd, FS_IOC_GETFLAGS, &attr);
	if (attr & FS_IMMUTABLE_FL)
		immut_flag = 1;
	else
		immut_flag = 0;

	close(fd);

	if (immut_flag)
		return 1;

	return 0;
#endif /* !defined(FS_IOC_GETFLAGS) || !defined(FS_IMMUTABLE_FL) */
}

/* Return 1 if FILE has some ACL property and zero if none
 * See: https://man7.org/tlpi/code/online/diff/acl/acl_view.c.html */
int
is_acl(char *file)
{
#ifndef _ACL_OK
	UNUSED(file);
	return 0;
#else
	if (!file || !*file)
		return 0;

	acl_t acl;
	acl = acl_get_file(file, ACL_TYPE_ACCESS);

	if (!acl)
		return 0;

	acl_entry_t entry;
	int entryid, num = 0;

	for (entryid = ACL_FIRST_ENTRY;; entryid = ACL_NEXT_ENTRY) {
		if (acl_get_entry(acl, entryid, &entry) != 1)
			break;
		num++;
	}

	acl_free(acl);

	/* If num > 3 we have something else besides owner, group, and others,
	 * that is, we have at least one ACL property */
	if (num > 3)
		return 1;

	return 0;
#endif /* _ACL_OK */
}

/* Check whether a given string contains only digits. Returns 1 if true
 * and 0 if false. It does not work with negative numbers */
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
 * for fused parameters in internal commands
 * Returns the index of the digit in STR or -1 if no digit is found */
static inline int
contains_digit(char *str)
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

/* Returns 1 if CMD is found in CMDS_LIST and zero otherwise */
static inline int
find_cmd(const struct cmdslist_t *cmds_list, const size_t list_size, char *cmd)
{
	int found = 0, i = (int)list_size;
	int c = -1, d = contains_digit(cmd);

	if (d != -1) {
		c = cmd[d];
		cmd[d] = '\0';
	}

	size_t cmd_len = strlen(cmd);
	while (--i >= 0) {
		if (*cmd == *cmds_list[i].name && cmd_len == cmds_list[i].len
		&& strcmp(cmd, cmds_list[i].name) == 0) {
			found = 1;
			break;
		}
	}

	if (d != -1)
		cmd[d] = (char)c;

	if (found)
		return 1;
	return 0;
}

int
is_internal_c(char *restrict cmd)
{
	if (find_cmd(internal_cmds, internal_cmds_n, cmd))
		return 1;

	/* Check for the search and history functions as well */
	if ((*cmd == '/' && access(cmd, F_OK) != 0) || (*cmd == '!'
	&& (_ISDIGIT(cmd[1]) || (cmd[1] == '-' && _ISDIGIT(cmd[2]))
	|| cmd[1] == '!')))
		return 1;

	return 0;
}

/* Check cmd against a list of internal commands. Used by parse_input_str()
 * to know if it should perform additional expansions, like glob, regex,
 * tilde, and so on. Only internal commands dealing with ELN/filenames
 * should be checked here */
int
is_internal(char *restrict cmd)
{
	const struct cmdslist_t int_cmds[] = {
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
		{"cp", 2},
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
		{"mv", 2},
		{"mm", 2},
		{"mime", 4},
		{"n", 1},
		{"new", 3},
		{"o", 1},
		{"open", 4},
		{"paste", 5},
		{"p", 1},
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
 * as parameters. Used by split_fusedcmd() */
int
is_internal_f(const char *restrict cmd)
{
	/* If we are completing/suggesting, do not take 'ws', 'mf', and 'st' commands
	 * into account: they do not take ELN/filenames as parameters, but just
	 * numbers, in which case no ELN-filename completion should be made */
	if (flags & STATE_COMPLETING
	&& (*cmd == 'w' || (*cmd == 'm' && *(cmd + 1) == 'f')
	|| (*cmd == 's' && (*(cmd + 1) == 't' || *(cmd + 1) == 'o')) ) )
		return 0;

	const struct cmdslist_t int_cmds[] = {
		{"ac", 2},
		{"ad", 2},
		{"bb", 2},
		{"bl", 2},
		{"bleach", 6},
		{"bm", 2},
		{"bookmarks", 9},
		{"br", 2},
		{"bulk", 4},
		{"c", 1},
		{"cp", 2},
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
// TESTING MIME!
		{"mm", 2},
// TESTING MIME!
		{"mv", 2},
		{"md", 2},
		{"mkdir", 5},
		{"mf", 2},
		{"n", 1},
		{"new", 3},
		{"o", 1},
		{"open", 4},
		{"ow", 2},
		{"p", 1},
		{"pp", 2},
		{"pr", 2},
		{"prop", 4},
		{"paste", 5},
		{"pin", 3},
		{"r", 1},
		{"rm", 2},
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
		{"unlink", 6},
		{"ws", 2},
		{NULL, 0}
	};

	static int n = 0;
	if (n == 0)
		n = (int)(sizeof(int_cmds) / sizeof(struct cmdslist_t)) - 1;
	size_t cmd_len = strlen(cmd);

	int i = n;
	while (--i >= 0) {
		if (*cmd == *int_cmds[i].name && cmd_len == int_cmds[i].len
		&& strcmp(cmd, int_cmds[i].name) == 0) {
			return 1;
		}
	}

	return 0;
}

/* Return one if STR is a command in PATH or zero if not */
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
		return EXIT_FAILURE;

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
	if (char_found) {
		if (access(str, F_OK) == -1)
			return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

/* Returns the parsed aliased command in an array of strings if
 * matching alias is found, or NULL if not */
char **
check_for_alias(char **args)
{
	/* Do not expand alias is first word is an ELN */
	if (!aliases_n || !aliases || !args || flags & FIRST_WORD_IS_ELN)
		return (char **)NULL;

	register int i;
	if (autocd == 1 || auto_open == 1) {
		/* Do not expand alias is first word is a file name in CWD */
		struct stat a;
		if (stat(args[0], &a) == 0 && ((S_ISDIR(a.st_mode) && autocd == 1)
		|| (!S_ISDIR(a.st_mode) && auto_open == 1)))
			return (char **)NULL;
	}

	i = (int)aliases_n;

	while (--i >= 0) {
		if (!aliases[i].name || !aliases[i].cmd || !*aliases[i].name
		|| !*aliases[i].cmd)
			continue;

		if (*aliases[i].name != *args[0] || strcmp(args[0], aliases[i].name) != 0)
			continue;

		args_n = 0; /* Reset args_n to be used by parse_input_str() */

		/* Parse the aliased cmd */
		char **alias_comm = parse_input_str(aliases[i].cmd);
		if (!alias_comm) {
			args_n = 0;
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("%s: Error parsing aliased command\n"),
				PROGRAM_NAME);
			return (char **)NULL;
		}

		size_t j;
		/* Add input parameters, if any, to alias_comm */
		if (args[1]) {
			for (j = 1; args[j]; j++) {
				alias_comm = (char **)xrealloc(alias_comm,
				    (++args_n + 2) * sizeof(char *));
				alias_comm[args_n] = savestring(args[j], strlen(args[j]));
			}
		}

		/* Add a terminating NULL string */
		alias_comm[args_n + 1] = (char *)NULL;

		/* Free original command */
		for (j = 0; args[j]; j++)
			free(args[j]);
		free(args);
		return alias_comm;
	}

	return (char **)NULL;
}

/* Keep only the last MAX records in FILE */
void
check_file_size(char *file, int max)
{
	if (!config_ok || !file)
		return;

	/* Create the file, if it doesn't exist */
	FILE *fp = (FILE *)NULL;
	struct stat attr;

	int fd;
	if (stat(file, &attr) == -1) {
		fp = open_fstream_w(file, &fd);
		if (!fp) {
			_err(0, NOPRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		} else {
			close_fstream(fp, fd);
		}

		return; /* Return anyway, for, being a new empty file, there's
		no need to truncate it */
	}

	/* Once we know the files exists, keep only MAX entries */
	fp = open_fstream_r(file, &fd);
	if (!fp) {
		_err(0, NOPRINT_PROMPT, "%s: log: %s: %s\n", PROGRAM_NAME,
		    file, strerror(errno));
		return;
	}

	int n = 0, c;

	/* Count newline chars to get amount of lines in the file */
	while ((c = fgetc(fp)) != EOF) {
		if (c == '\n')
			n++;
	}

	if (n <= max) {
		close_fstream(fp, fd);
		return;
	}

	/* Set the file pointer to the beginning of the log file */
	fseek(fp, 0, SEEK_SET);

	char *tmp = (char *)xnmalloc(config_dir_len + 12, sizeof(char));
	sprintf(tmp, "%s/log.XXXXXX", config_dir); /* NOLINT */

	int fdd = mkstemp(tmp);
	if (fdd == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "log: %s: %s", tmp, strerror(errno));
		close_fstream(fp, fd);
		free(tmp);
		return;
	}

#ifdef __HAIKU__
	FILE *fpp = fopen(tmp, "w");
	if (!fpp) {
		_err('e', PRINT_PROMPT, "%s: %s: %s\n", PROGRAM_NAME, tmp, strerror(errno));
		close_fstream(fp, fd);
		free(tmp);
		return;
	}
#endif

	int i = 1;
	size_t line_size = 0;
	char *line = (char *)NULL;

	while (getline(&line, &line_size, fp) > 0) {
		/* Delete old entries = copy only new ones */
		if (i++ >= n - (max - 1))
#ifndef __HAIKU__
			dprintf(fdd, "%s", line);
#else
			fprintf(fpp, "%s", line);
#endif
	}

#ifdef __HAIKU__
	fclose(fpp);
#endif

	free(line);
	unlinkat(fd, file, 0);
	renameat(fdd, tmp, fd, file);
	close(fdd);
	close_fstream(fp, fd);
	free(tmp);

	return;
}
