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

/* Terminals known not to be able to handle escape sequences */
static const char *UNSUPPORTED_TERM[] = {"dumb", /*"cons25",*/ "emacs", NULL};

void
check_term(void)
{
	char *_term = getenv("TERM");
	if (!_term) {
		fprintf(stderr, _("%s: Error getting terminal type\n"), PROGRAM_NAME);
		exit(EXIT_FAILURE);
	}

	int i;
	for (i = 0; UNSUPPORTED_TERM[i]; i++) {
		if (*_term == *UNSUPPORTED_TERM[i]
		&& strcmp(_term, UNSUPPORTED_TERM[i]) == 0) {
			fprintf(stderr, _("%s: '%s': Unsupported terminal. This "
					"terminal cannot understand escape sequences\n"),
					PROGRAM_NAME, UNSUPPORTED_TERM[i]);
			exit(EXIT_FAILURE);
		}
	}

	return;
}

void
check_third_party_cmds(void)
{
	int udisks2ok = 0, udevilok = 0, fzfok = 0;
	int i = (int)path_progsn;
	while (--i >= 0) {
		if (*bin_commands[i] != 'u' && *bin_commands[i] != 'f')
			continue;

		if (strcmp(bin_commands[i], "fzf") == 0) {
			fzfok = 1;
		} else if (strcmp(bin_commands[i], "udisksctl") == 0) {
			udisks2ok = 1;
		} else {
			if (strcmp(bin_commands[i], "udevil") == 0)
				udevilok = 1;
		}

		if (udevilok && udisks2ok && fzfok)
			break;
	}

	if (fzftab && !fzfok) {
		_err('w', PRINT_PROMPT, _("%s: fzf not found. Falling back to "
			"standard TAB completion\n"), PROGRAM_NAME);
		fzftab = 0;
	}

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

	if ((file->st_mode & S_IFMT) == S_IFDIR) {
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

	if (!sudo) {
		fprintf(stderr, _("%s: %s: No such file or directory\n"),
				PROGRAM_NAME, p ? p : DEF_SUDO_CMD);
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
		fprintf(stderr, "'%s': %s\n", file, strerror(errno));
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

/* Returns 1 if CMD is found in CMDS_LIST and zero otherwise */
static int
found_cmd(char **cmds_list, int list_size, char *cmd)
{
	int found = 0;
	int i = list_size;

	while (--i >= 0) {
		if (*cmd == *cmds_list[i] && strcmp(cmd, cmds_list[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (found)
		return 1;
	return 0;
}

/* Check CMD against a list of internal commands */
int
is_internal_c(char *restrict cmd)
{
	int i;
	for (i = 0; internal_cmds[i]; i++);

	if (found_cmd(internal_cmds, i, cmd)) {
		return 1;
	} else {
		/* Check for the search and history functions as well */
		if ((*cmd == '/' && access(cmd, F_OK) != 0) || (*cmd == '!'
		&& (_ISDIGIT(cmd[1]) || (cmd[1] == '-' && _ISDIGIT(cmd[2]))
		|| cmd[1] == '!')))
			return 1;
	}

	return 0;
}

/* Check cmd against a list of internal commands. Used by parse_input_str()
 * to know if it should perform additional expansions, like glob, regex,
 * tilde, and so on. Only internal commands dealing with file names
 * should be checked here */
int
is_internal(char *restrict cmd)
{
	char *int_cmds[] = {
		"ac", "ad",
		"bb", "bleach",
		"bm", "bookmarks",
		"bl", "le",
		"br", "bulk",
		"c", "cp",
		"cd",
		"d", "dup",
		"exp", "export",
		"jc", "jp",
		"l", "le",
		"m", "mv",
		"mm", "mime",
		"n", "new",
		"o", "open",
		"paste",
		"p", "pr", "prop",
		"pin",
		"r",
		"s", "sel",
		"t", "tr", "trash",
		"te",
		"v", "vv",
		NULL};

	int i = (int)(sizeof(int_cmds) / sizeof(char *)) - 1;
	if (found_cmd(int_cmds, i, cmd))
		return 1;

	/* Check for the search function as well */
	else {
		if (*cmd == '/' && access(cmd, F_OK) != 0)
			return 1;
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
		if (*q == *bin_commands[i] && q[1] == bin_commands[i][1] && strcmp(q, bin_commands[i]) == 0) {
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

/* Returns the parsed aliased command in an array of strings, if
 * matching alias is found, or NULL if not. */
char **
check_for_alias(char **args)
{
	if (!aliases_n || !aliases || !args)
		return (char **)NULL;

	register int i = (int)aliases_n;

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
			fprintf(stderr, _("%s: Error parsing aliased command\n"), PROGRAM_NAME);
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
	if (!config_ok)
		return;

	/* Create the file, if it doesn't exist */
	FILE *fp = (FILE *)NULL;
	struct stat attr;

	int fd;
	if (stat(file, &attr) == -1) {
		fp = open_fstream_w(file, &fd);
		if (!fp) {
			_err(0, NOPRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			    file, strerror(errno));
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

	char *tmp = (char *)xnmalloc(strlen(config_dir) + 12, sizeof(char));
	sprintf(tmp, "%s/log.XXXXXX", config_dir); /* NOLINT */

	int fdd = mkstemp(tmp);
	if (fdd == -1) {
		fprintf(stderr, "log: %s: %s", tmp, strerror(errno));
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
