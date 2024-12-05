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

	if (is_cmd_in_path("fzf") == 1) {
		bin_flags |= FZF_BIN_OK;
		if (fzftab == UNSET) fzftab = 1;
	}

	if (is_cmd_in_path("fnf") == 1) {
		bin_flags |= FNF_BIN_OK;
		if (fzftab == UNSET) fzftab = 1;
	}

	if (is_cmd_in_path("smenu") == 1) {
		bin_flags |= SMENU_BIN_OK;
		if (fzftab == UNSET) fzftab = 1;
	}

	if (is_cmd_in_path("udiskctl") == 1)
		udisks2ok = 1;

	if (is_cmd_in_path("udevil") == 1)
		udevilok = 1;

#if defined(USE_DU1) && !defined(HAVE_GNU_DU) && !defined(_BE_POSIX)
	if (is_cmd_in_path("gdu") == 1)
		bin_flags |= GNU_DU_BIN_GDU;
#endif /* USE_DU1 && !HAVE_GNU_DU && !_BE_POSIX */

#if defined(CHECK_COREUTILS)
	if (is_cmd_in_path("grm") == 1)
		bin_flags |= BSD_HAVE_COREUTILS;
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

/* Return 1 if the command CMD exists (is found in PATH) and is executable
 * (and readable) by the current user. Otherwise, return 0.
 * This code is based on the file_status() function used by which(1). */
int
is_exec_cmd(const char *cmd)
{
	if (!cmd || !*cmd)
		return 0;

	struct stat a;
	if (stat(cmd, &a) == -1 || !S_ISREG(a.st_mode))
		return 0;

	/* The file is read/exec by others (world). This is the most common case
	 * for files in PATH. */
	if ((a.st_mode & S_IXOTH) && (a.st_mode & S_IROTH))
		return 1;

	/* We're root: we only need the exec bit to be set for any of user,
	 * group, or others. */
	if (user.uid == (uid_t)0 && (a.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		return 1;

	/* We're the owner of the file. */
	if (user.uid == a.st_uid && (a.st_mode & S_IXUSR) && (a.st_mode & S_IRUSR))
		return 1;

	/* We're are in the file's group. */
	if (check_user_groups(a.st_gid) == 1 && (a.st_mode & S_IXGRP)
	&& (a.st_mode & S_IRGRP))
		return 1;

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
 * char of STR. Used by is_internal_cmd() to check for fused parameters in
 * internal commands.
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

/* Return 1 if CMD is an internal command matching the flags in FLAG.
 * Otherwise, return 0. */
int
is_internal_cmd(char *cmd, const int flag, const int check_hist,
	const int check_search)
{
	if (!cmd || !*cmd)
		return 0;

	/* Old is_internal_f() function */
	if ((flags & STATE_COMPLETING) && (flag & PARAM_FNAME_NUM)
	&& (*cmd == 'w' || (*cmd == 'm' && *(cmd + 1) == 'f')
	|| (*cmd == 's' && (*(cmd + 1) == 't' || *(cmd + 1) == 'o')) ) )
		return 0;

	char c = 0;
	const int d = contains_digit(cmd);

	if (d != -1) {
		c = cmd[d];
		cmd[d] = '\0';
	}

	const size_t clen = strlen(cmd);
	ssize_t i = (ssize_t)internal_cmds_n;
	int found = 0;

	while (--i >= 0) {
		if (((flag & ALL_CMDS) || (internal_cmds[i].flag & flag))
		&& clen == internal_cmds[i].len && *cmd == *internal_cmds[i].name
		&& strcmp(cmd + 1, internal_cmds[i].name + 1) == 0) {
			found = 1;
			break;
		}
	}

	if (d != -1)
		cmd[d] = c;

	if (found == 1)
		return 1;

	if ((check_search == 1 && (*cmd == '/' && access(cmd, F_OK) != 0))
	|| (check_hist == 1 && (*cmd == '!' && (IS_DIGIT(cmd[1])
	|| (cmd[1] == '-' && IS_DIGIT(cmd[2])) || cmd[1] == '!'))))
		return 1;

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
