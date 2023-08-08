/* file_operations.c -- control multiple file operations */

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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#ifdef __TINYC__
# undef CHAR_MAX /* Silence redefinition error */
#endif /* __TINYC__ */
#include <limits.h>
#include <fcntl.h>

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "exec.h"
#include "file_operations.h"
#include "history.h"
#include "listing.h"
#include "mime.h"
#include "misc.h"
#include "navigation.h"
#include "readline.h"
#include "selection.h"
#include "messages.h"

#define BULK_RENAME_TMP_FILE_HEADER "# CliFM - Rename files in bulk\n\
# Edit file names, save, and quit the editor (you will be\n\
# asked for confirmation)\n\
# Just quit the editor without any edit to cancel the operation\n\n"

#define BULK_RM_TMP_FILE_HEADER "# CliFM - Remove files in bulk\n\
# Remove the files you want to be deleted, save and exit\n\
# Just quit the editor without any edit to cancel the operation\n\n"

/* Macros and array used to print unsafe names description messages.
 * Used by validate_filename(). */
#define UNSAFE_DASH      0
#define UNSAFE_MIME      1
#define UNSAFE_ELN       2
#define UNSAFE_FASTBACK  3
#define UNSAFE_BTS_CONST 4
#define UNSAFE_SYS_KEY   5
#define UNSAFE_CONTROL   6
#define UNSAFE_META      7
#define UNSAFE_TOO_LONG  8
#ifdef _BE_POSIX
# define UNSAFE_NOT_PORTABLE 9
#endif

static char *const unsafe_name_msgs[] = {
	"Starts with a dash (-): command option flags collision",
	"Reserved (internal: MIME/file type expansion)",
	"Reserved (internal: ELN/range expansion)",
	"Reserved (internal: fastback expansion)",
	"Reserved (internal: bookmarks, tags, and selected files constructs)",
	"Reserved shell/system keyword",
	"Contains control characters",
	"Contains shell meta-characters",
	"Too long",
	"Contains characters not in the Portable Filename Character Set"
};

#ifdef _BE_POSIX
# define PORTABLE_CHARSET "abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_"

/* Return 0 if NAME is a portable file name. Otherwise, return 1. */
static int
is_portable_filename(const char *name, const size_t len)
{
	return len > strspn(name, PORTABLE_CHARSET);
}
#endif /* _BE_POSIX */

/* Print/set the file creation mode mask (umask). */
int
umask_function(char *arg)
{
	if (!arg) {
		mode_t old_umask = umask(0);
		printf("%04o\n", old_umask);
		umask(old_umask);
		return EXIT_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(UMASK_USAGE);
		return EXIT_SUCCESS;
	}

	if (!IS_DIGIT(*arg) || !is_number(arg))
		goto ERROR;

	int new_umask = read_octal(arg);
	if (new_umask < 0 || new_umask > 0777)
		goto ERROR;

	umask((mode_t)new_umask);
	printf(_("File-creation mask set to '%04o'\n"), new_umask);
	return EXIT_SUCCESS;

ERROR:
	xerror(_("umask: %s: Out of range (valid values are 000-777)\n"), arg);
	return EXIT_FAILURE;
}

static int
parse_bulk_remove_params(char *s1, char *s2, char **app, char **target)
{
	if (!s1 || !*s1) { /* No parameters */
		/* TARGET defaults to CWD and APP to default associated app */
		*target = workspaces[cur_ws].path;
		return EXIT_SUCCESS;
	}

	int stat_ret = 0;
	struct stat a;
	if ((stat_ret = stat(s1, &a)) == -1 || !S_ISDIR(a.st_mode)) {
		char *p = get_cmd_path(s1);
		if (!p) { /* S1 is neither a directory nor a valid application */
			int ec = stat_ret != -1 ? ENOTDIR : ENOENT;
			xerror("rr: %s: %s\n", s1, strerror(ec));
			return ec;
		}
		/* S1 is an application name. TARGET defaults to CWD */
		*target = workspaces[cur_ws].path;
		*app = s1;
		free(p);
		return EXIT_SUCCESS;
	}

	/* S1 is a valid directory */
	size_t tlen = strlen(s1);
	if (tlen > 2 && s1[tlen - 1] == '/')
		s1[tlen - 1] = '\0';
	*target = s1;

	if (!s2 || !*s2) /* No S2. APP defaults to default associated app */
		return EXIT_SUCCESS;

	char *p = get_cmd_path(s2);
	if (p) { /* S2 is a valid application name */
		*app = s2;
		free(p);
		return EXIT_SUCCESS;
	}
	/* S2 is not a valid application name */
	xerror("rr: %s: %s\n", s2, strerror(ENOENT));
	return ENOENT;
}

static int
create_tmp_file(char **file, int *fd)
{
	size_t tmp_len = strlen(xargs.stealth_mode == 1 ? P_tmpdir : tmp_dir);
	size_t file_len = tmp_len + strlen(TMP_FILENAME) + 2;

	*file = (char *)xnmalloc(file_len, sizeof(char));
	snprintf(*file, file_len, "%s/%s", xargs.stealth_mode == 1
		? P_tmpdir : tmp_dir, TMP_FILENAME);

	errno = 0;
	*fd = mkstemp(*file);
	if (*fd == -1) {
		xerror("rr: mkstemp: %s: %s\n", *file, strerror(errno));
		free(*file);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static char
get_file_suffix(const mode_t type)
{
	switch (type) {
	case DT_DIR: return DIR_CHR;
	case DT_REG: return 0;
	case DT_LNK: return LINK_CHR;
	case DT_SOCK: return SOCK_CHR;
	case DT_FIFO: return FIFO_CHR;
#ifdef SOLARIS_DOORS
	case DT_DOOR: return DOOR_CHR;
#endif /* SOLARIS_DOORS */
	case DT_UNKNOWN: return UNKNOWN_CHR;
	default: return 0;
	}
}

static void
print_file(FILE *fp, const char *name, const mode_t type)
{
#ifndef _DIRENT_HAVE_D_TYPE
	UNUSED(type);
	char s = 0;
	struct stat a;
	if (lstat(name, &a) != -1)
		s = get_file_suffix(a.st_mode);
#else
	char s = get_file_suffix(type);
#endif /* !_DIRENT_HAVE_D_TYPE */

	if (s)
		fprintf(fp, "%s%c\n", name, s);
	else
		fprintf(fp, "%s\n", name);
}

static int
write_files_to_tmp(struct dirent ***a, filesn_t *n, const char *target,
	char *tmp_file)
{
	int fd = 0;
	FILE *fp = open_fwrite(tmp_file, &fd);
	if (!fp) {
		err('e', PRINT_PROMPT, "%s: rr: fopen: %s: %s\n", PROGRAM_NAME,
			tmp_file, strerror(errno));
		return errno;
	}

	fprintf(fp, "%s", _(BULK_RM_TMP_FILE_HEADER));

	if (target == workspaces[cur_ws].path) {
		filesn_t i;
		for (i = 0; i < files; i++)
			print_file(fp, file_info[i].name, file_info[i].type);
	} else {
		if (count_dir(target, CPOP) <= 2) {
			int tmp_err = EXIT_FAILURE;
			xerror(_("%s: %s: Directory empty\n"), PROGRAM_NAME, target);
			fclose(fp);
			return tmp_err;
		}

		*n = scandir(target, a, NULL, alphasort);
		if (*n == -1) {
			int tmp_err = errno;
			xerror("rr: %s: %s", target, strerror(errno));
			fclose(fp);
			return tmp_err;
		}

		filesn_t i;
		for (i = 0; i < *n; i++) {
			if (SELFORPARENT((*a)[i]->d_name))
				continue;
#ifndef _DIRENT_HAVE_D_TYPE
			struct stat attr;
			if (stat((*a)[i]->d_name, &attr) == -1)
				continue;
			print_file(fp, (*a)[i]->d_name, get_dt(attr.st_mode));
#else
			print_file(fp, (*a)[i]->d_name, (*a)[i]->d_type);
#endif /* !_DIRENT_HAVE_D_TYPE */
		}
	}

	fclose(fp);
	return EXIT_SUCCESS;
}

static int
open_tmp_file(struct dirent ***a, const filesn_t n, char *tmp_file, char *app)
{
	int exit_status = EXIT_SUCCESS;
	filesn_t i;

	if (!app || !*app) {
		open_in_foreground = 1;
		exit_status = open_file(tmp_file);
		open_in_foreground = 0;

		if (exit_status == EXIT_SUCCESS)
			return EXIT_SUCCESS;

		xerror(_("rr: %s: Cannot open file\n"), tmp_file);
		goto END;
	}

	char *cmd[] = {app, tmp_file, NULL};
	exit_status = launch_execv(cmd, FOREGROUND, E_NOFLAG);

	if (exit_status == EXIT_SUCCESS)
		return EXIT_SUCCESS;

END:
	for (i = 0; i < n && *a && (*a)[i]; i++)
		free((*a)[i]);
	free(*a);

	return exit_status;
}

static char **
get_files_from_tmp_file(const char *tmp_file, const char *target, const filesn_t n)
{
	size_t nfiles = (target == workspaces[cur_ws].path) ? (size_t)files : (size_t)n;
	char **tmp_files = (char **)xnmalloc(nfiles + 2, sizeof(char *));

	FILE *fp = fopen(tmp_file, "r");
	if (!fp)
		return (char **)NULL;

	size_t size = 0, i;
	char *line = (char *)NULL;
	ssize_t len = 0;

	i = 0;
	while ((len = getline(&line, &size, fp)) > 0) {
		if (*line == '#' || *line == '\n')
			continue;

		if (line[len - 1] == '\n') {
			line[len - 1] = '\0';
			len--;
		}

		if (len > 0 && (line[len - 1] == '/' || line[len - 1] == '@'
		|| line[len - 1] == '=' || line[len - 1] == '|'
		|| line[len - 1] == '?') ) {
			line[len - 1] = '\0';
			len--;
		}

		tmp_files[i] = savestring(line, (size_t)len);
		i++;
	}

	tmp_files[i] = (char *)NULL;

	free(line);
	fclose(fp);

	return tmp_files;
}

/* If FILE is not found in LIST, returns one; zero otherwise */
static int
remove_this_file(char *file, char **list)
{
	if (SELFORPARENT(file))
		return 0;

	size_t i;
	for (i = 0; list[i]; i++) {
		if (*file == *list[i] && strcmp(file, list[i]) == 0)
			return 0;
	}

	return 1;
}

static char **
get_remove_files(const char *target, char **tmp_files,
	struct dirent ***a, const filesn_t n)
{
	size_t i, j = 0;
	size_t l = (target == workspaces[cur_ws].path) ? (size_t)files : (size_t)n;
	char **rem_files = (char **)xnmalloc(l + 2, sizeof(char *));

	if (target == workspaces[cur_ws].path) {
		for (i = 0; i < (size_t)files; i++) {
			if (remove_this_file(file_info[i].name, tmp_files) == 1) {
				rem_files[j] = savestring(file_info[i].name,
					strlen(file_info[i].name));
				j++;
			}
		}
		rem_files[j] = (char *)NULL;
		return rem_files;
	}

	for (i = 0; i < (size_t)n; i++) {
		if (remove_this_file((*a)[i]->d_name, tmp_files) == 1) {
			char p[PATH_MAX];
			if (*target == '/') {
				snprintf(p, sizeof(p), "%s/%s", target, (*a)[i]->d_name);
			} else {
				snprintf(p, sizeof(p), "%s/%s/%s", workspaces[cur_ws].path,
					target, (*a)[i]->d_name);
			}
			rem_files[j] = savestring(p, strlen(p));
			j++;
		}
		free((*a)[i]);
	}

	free(*a);
	rem_files[j] = (char *)NULL;

	return rem_files;
}

static char *
get_rm_param(char ***rfiles, const int n)
{
	char *_param = (char *)NULL;
	struct stat a;
	int i = n;

	while (--i >= 0) {
		if (lstat((*rfiles)[i], &a) == -1)
			continue;

	/* We don't need interactivity here: the user already confirmed the
	 * operation before calling this function. */
		if (S_ISDIR(a.st_mode)) {
#if defined(_BE_POSIX)
			_param = savestring("-rf", 3);
#elif defined(__sun)
			if (bin_flags & BSD_HAVE_COREUTILS)
				_param = savestring("-drf", 4);
			else
				_param = savestring("-rf", 3);
#else
			_param = savestring("-drf", 4);
#endif /* _BE_POSIX */
			break;
		}
	}

	if (!_param) /* We have only regular files, no dir */
		_param = savestring("-f", 2);

	return _param;
}

static char **
construct_rm_cmd(char ***rfiles, char *_param, const size_t n)
{
	char **cmd = (char **)xnmalloc(n + 4, sizeof(char *));

#ifdef __sun
	if (bin_flags & BSD_HAVE_COREUTILS)
		cmd[0] = savestring("grm", 3);
	else
		cmd[0] = savestring("rm", 2);
#else
	cmd[0] = savestring("rm", 2);
#endif /* __sun */
	/* Using strnlen() here avoids a Redhat hardened compilation warning. */
	/* As returned by get_rm_param(), we know that _param is at most 5 bytes
	 * (including the terminating NUL byte). */
	cmd[1] = savestring(_param, strnlen(_param, 5));
	cmd[2] = savestring("--", 2);
	free(_param);

	int cmd_n = 3;
	size_t i;
	for (i = 0; i < n; i++) {
		cmd[cmd_n] = savestring((*rfiles)[i], strlen((*rfiles)[i]));
		cmd_n++;
	}
	cmd[cmd_n] = (char *)NULL;

	return cmd;
}

static int
bulk_remove_files(char ***rfiles)
{
	if (!*rfiles)
		return EXIT_FAILURE;

	puts(_("The following files will be removed:"));
	int n;
	for (n = 0; (*rfiles)[n]; n++)
		printf("%s->%s %s\n", mi_c, df_c, (*rfiles)[n]);

	if (n == 0)
		return EXIT_FAILURE;

	int i = n;
	if (rl_get_y_or_n(_("Continue? [y/n] ")) == 0) {
		while (--i >= 0)
			free((*rfiles)[i]);
		free(*rfiles);
		return 0;
	}

	char *_param = get_rm_param(rfiles, n);
	char **cmd = construct_rm_cmd(rfiles, _param, (size_t)n);

	int ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);

	i = n;
	while (--i >= 0)
		free((*rfiles)[i]);
	free(*rfiles);

	for (i = 0; cmd[i]; i++)
		free(cmd[i]);
	free(cmd);

	return ret;
}

static int
diff_files(char *tmp_file, const filesn_t n)
{
	FILE *fp = fopen(tmp_file, "r");
	if (!fp) {
		xerror("br: %s: %s\n", tmp_file, strerror(errno));
		return 0;
	}

	char line[PATH_MAX + 6];
	memset(line, '\0', sizeof(line));

	filesn_t c = 0;
	while (fgets(line, (int)sizeof(line), fp)) {
		if (*line != '#' && *line != '\n')
			c++;
	}

	fclose(fp);

	return (c < n ? 1 : 0);
}

static int
nothing_to_do(char **tmp_file, struct dirent ***a, const filesn_t n, const int fd)
{
	puts(_("rr: Nothing to do"));
	if (unlinkat(fd, *tmp_file, 0) == 1)
		xerror("rr: unlink: %s: %s\n", *tmp_file, strerror(errno));
	close(fd);
	free(*tmp_file);

	filesn_t i = n;
	while (--i >= 0)
		free((*a)[i]);
	free(*a);

	return EXIT_SUCCESS;
}

int
bulk_remove(char *s1, char *s2)
{
	if (s1 && IS_HELP(s1)) {
		puts(_(RR_USAGE));
		return EXIT_SUCCESS;
	}

	char *app = (char *)NULL, *target = (char *)NULL;
	int fd = 0, ret = 0, i = 0;
	filesn_t n = 0;

	if ((ret = parse_bulk_remove_params(s1, s2, &app, &target)) != EXIT_SUCCESS)
		return ret;

	char *tmp_file = (char *)NULL;
	if ((ret = create_tmp_file(&tmp_file, &fd)) != EXIT_SUCCESS)
		return ret;

	struct dirent **a = (struct dirent **)NULL;
	if ((ret = write_files_to_tmp(&a, &n, target, tmp_file)) != EXIT_SUCCESS)
		goto END;

	struct stat attr;
	stat(tmp_file, &attr);
	time_t old_t = attr.st_mtime;

	if ((ret = open_tmp_file(&a, n, tmp_file, app)) != EXIT_SUCCESS)
		goto END;

	stat(tmp_file, &attr);
	filesn_t num = (target == workspaces[cur_ws].path) ? files : n - 2;
	if (old_t == attr.st_mtime || diff_files(tmp_file, num) == 0)
		return nothing_to_do(&tmp_file, &a, n, fd);

	char **rfiles = get_files_from_tmp_file(tmp_file, target, n);
	if (!rfiles)
		goto END;

	char **rem_files = get_remove_files(target, rfiles, &a, n);

	ret = bulk_remove_files(&rem_files);

	for (i = 0; rfiles[i]; i++)
		free(rfiles[i]);
	free(rfiles);

END:
	if (unlinkat(fd, tmp_file, 0) == -1) {
		err('w', PRINT_PROMPT, "rr: unlink: %s: %s\n",
			tmp_file, strerror(errno));
	}
	close(fd);
	free(tmp_file);
	return ret;
}

#ifndef _NO_LIRA
static int
run_mime(char *file)
{
	if (!file || !*file)
		return EXIT_FAILURE;

	if (xargs.preview == 1 || xargs.open == 1)
		goto RUN;

	char *p = rl_line_buffer ? rl_line_buffer : (char *)NULL;

	/* Convert ELN into file name (rl_line_buffer) */
	if (p && *p >= '1' && *p <= '9') {
		const filesn_t a = xatof(p);
		if (a > 0 && a <= files && file_info[a - 1].name)
			p = file_info[a - 1].name;
	}

	if (p && ( (*p == 'i' && (strncmp(p, "import", 6) == 0
	|| strncmp(p, "info", 4) == 0))
	|| (*p == 'o' && (p[1] == ' ' || strncmp(p, "open", 4) == 0)) ) ) {
		char *cmd[] = {"mm", "open", file, NULL};
		return mime_open(cmd);
	}

RUN: {
	char *cmd[] = {"mm", file, NULL};
	return mime_open(cmd);
	}
}
#endif /* _NO_LIRA */

/* Open a file via OPENER, if set, or via LIRA. If not compiled with
 * Lira support, fallback to open (Haiku), or xdg-open. Returns zero
 * on success and one on failure */
int
open_file(char *file)
{
	if (!file || !*file)
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;

	if (conf.opener) {
		if (*conf.opener == 'g' && strcmp(conf.opener, "gio") == 0) {
			char *cmd[] = {"gio", "open", file, NULL};
			if (launch_execv(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} else {
			char *cmd[] = {conf.opener, file, NULL};
			if (launch_execv(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}
	} else {
#ifndef _NO_LIRA
		exit_status = run_mime(file);
#else
		/* Fallback to (xdg-)open */
# if defined(__HAIKU__)
		char *cmd[] = {"open", file, NULL};
# elif defined(__APPLE__)
		char *cmd[] = {"/usr/bin/open", file, NULL};
# else
		char *cmd[] = {"xdg-open", file, NULL};
# endif /* __HAIKU__ */
		if (launch_execv(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
#endif /* _NO_LIRA */
	}

	return exit_status;
}

int
xchmod(const char *file, const char *mode_str, const int flag)
{
	if (!file || !*file) {
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			"xchmod: Empty buffer for file name\n");
		return EXIT_FAILURE;
	}

	if (!mode_str || !*mode_str) {
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			"xchmod: Empty buffer for mode\n");
		return EXIT_FAILURE;
	}

	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			"xchmod: %s: %s\n", file, strerror(errno));
		return errno;
	}

	mode_t mode = (mode_t)strtol(mode_str, 0, 8);
	if (fchmod(fd, mode) == -1) {
		close(fd);
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			"xchmod: %s: %s\n", file, strerror(errno));
		return errno;
	}

	close(fd);
	return EXIT_SUCCESS;
}

/* Toggle executable bits on the file named FILE. */
int
toggle_exec(const char *file, mode_t mode)
{
	/* Set it only for owner, unset it for every one */
	(0100 & mode) ? (mode &= (mode_t)~0111) : (mode |= 0100);
	/* Set or unset S_IXUSR, S_IXGRP, and S_IXOTH */
//	(0100 & mode) ? (mode &= (mode_t)~0111) : (mode |= 0111);

	if (fchmodat(XAT_FDCWD, file, mode, 0) == -1) {
		xerror("te: Changing permissions of '%s': %s\n",
			file, strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static char *
get_dup_file_dest_dir(void)
{
	char *dir = (char *)NULL;

	puts(_("Enter destiny directory (Ctrl-d to quit)\n"
		"Tip: \".\" for current directory"));
	char _prompt[NAME_MAX];
	snprintf(_prompt, sizeof(_prompt), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	while (!dir) {
		int quoted = 0;
		dir = get_newname(_prompt, (char *)NULL, &quoted);
		UNUSED(quoted);
		if (!dir) /* The user pressed ctrl-d */
			return (char *)NULL;

		/* Expand ELN */
		if (IS_DIGIT(*dir) && is_number(dir)) {
			int n = atoi(dir);
			if (n > 0 && (filesn_t)n <= files) {
				free(dir);
				char *name = file_info[n - 1].name;
				dir = savestring(name, strlen(name));
			}
		} else if (*dir == '~') { // Expand tilde
			char *tmp = tilde_expand(dir);
			if (tmp) {
				free(dir);
				dir = tmp;
			}
		}

		/* Check if file exists, is a directory, and user has access */
		struct stat a;
		errno = 0;
		if (stat(dir, &a) == -1) {
			goto ERROR;
		} else if (!S_ISDIR(a.st_mode)) {
			errno = ENOTDIR;
			goto ERROR;
		} else if (check_file_access(a.st_mode, a.st_uid, a.st_gid) == 0) {
			errno = EACCES;
			goto ERROR;
		}

		break;

ERROR:
		xerror("dup: %s: %s\n", dir, strerror(errno));
		free(dir);
		dir = (char *)NULL;
	}

	return dir;
}

int
dup_file(char **cmd)
{
	if (!cmd[1] || IS_HELP(cmd[1])) {
		puts(_(DUP_USAGE));
		return EXIT_SUCCESS;
	}

	char *dest_dir = get_dup_file_dest_dir();
	if (!dest_dir)
		return EXIT_SUCCESS;

	size_t dlen = strlen(dest_dir);
	if (dlen > 1 && dest_dir[dlen - 1] == '/') {
		dest_dir[dlen - 1] = '\0';
		dlen--;
	}

	char *rsync_path = get_cmd_path("rsync");
	int exit_status =  EXIT_SUCCESS;

	size_t i;
	for (i = 1; cmd[i]; i++) {
		if (!cmd[i] || !*cmd[i])
			continue;

		/* 1. Construct file names. */
		char *source = cmd[i];
		if (strchr(source, '\\')) {
			char *deq_str = dequote_str(source, 0);
			if (!deq_str) {
				xerror("dup: %s: Error dequoting file name\n", source);
				continue;
			}

			xstrsncpy(source, deq_str, strlen(deq_str) + 1);
			free(deq_str);
		}

		/* Use source as destiny file name: source.copy, and, if already
		 * exists, source.copy-n, where N is an integer greater than zero */
		size_t source_len = strlen(source);
		int rem_slash = 0;
		if (strcmp(source, "/") != 0 && source_len > 0
		&& source[source_len - 1] == '/') {
			source[source_len - 1] = '\0';
			rem_slash = 1;
		}

		char *tmp = strrchr(source, '/');
		char *source_name;

		source_name = (tmp && *(tmp + 1)) ? tmp + 1 : source;

		char tmp_dest[PATH_MAX];
		if (*dest_dir == '/' && !dest_dir[1]) // root dir
			snprintf(tmp_dest, sizeof(tmp_dest), "/%s.copy", source_name);
		else
			snprintf(tmp_dest, sizeof(tmp_dest), "%s/%s.copy",
				dest_dir, source_name);

		char bk[PATH_MAX + 11];
		xstrsncpy(bk, tmp_dest, sizeof(bk));
		struct stat attr;
		size_t suffix = 1;
		while (stat(bk, &attr) == EXIT_SUCCESS) {
			snprintf(bk, sizeof(bk), "%s-%zu", tmp_dest, suffix);
			suffix++;
		}
		char *dest = savestring(bk, strlen(bk));

		if (rem_slash == 1)
			source[source_len - 1] = '/';

		/* 2. Run command. */
		if (rsync_path) {
			char *_cmd[] = {"rsync", "-aczvAXHS", "--progress", "--",
				source, dest, NULL};
			if (launch_execv(_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} else {
#ifdef _BE_POSIX
			char *_cmd[] = {"cp", "--", source, dest, NULL};
#elif defined(__sun)
			int g = (bin_flags & BSD_HAVE_COREUTILS);
			char *name = g ? "gcp" : "cp";
			char *opt = g ? "-a" : "--";
			char *_cmd[] = {name, opt, g ? "--" : source,
				g ? source : dest, g ? dest : NULL, NULL};
#else
			char *_cmd[] = {"cp", "-a", "--", source, dest, NULL};
#endif /* _BE_POSIX */
			if (launch_execv(_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		free(dest);
	}

	free(dest_dir);
	free(rsync_path);
	return exit_status;
}

/* Create the file named NAME, as a directory, if ending wit a slash, or as
 * a regular file otherwise.
 * Parent directories are created if they do not exist.
 * Returns EXIT_SUCCESS on success or EXIT_FAILURE on error. */
static int
create_file(char *name)
{
	struct stat a;
	char *ret = (char *)NULL;
	char *n = name;
	int status = EXIT_SUCCESS;

	/* Dir creation mode (777, or 700 if running in secure-mode). mkdir(3) will
	 * modify this according to the current umask value. */
	mode_t mode = (xargs.secure_env == 1 || xargs.secure_env_full == 1)
		? S_IRWXU : S_IRWXU | S_IRWXG | S_IRWXO;

	if (*n == '/') /* Skip root dir. */
		n++;

	/* Recursively create parent dirs (and dir itself if basename is a dir). */
	while ((ret = strchr(n, '/'))) {
		*ret = '\0';
		if (lstat(name, &a) != -1) /* dir exists */
			goto CONT;

		errno = 0;
		if (mkdirat(XAT_FDCWD, name, mode) == -1) {
			xerror("new: %s: %s\n", name, strerror(errno));
			status = EXIT_FAILURE;
			break;
		}

CONT:
		*ret = '/';
		n = ret + 1;
	}

	if (*n && status != EXIT_FAILURE) { /* Regular file */
		/* Regular file creation mode (666, or 600 in secure-mode). open(2)
		 * will modify this according to the current umask value. */
		if (xargs.secure_env == 1 || xargs.secure_env_full == 1)
			mode = S_IRUSR | S_IWUSR;
		else
			mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

		int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, mode);
		if (fd == -1) {
			xerror("new: %s: %s\n", name, strerror(errno));
			status = EXIT_FAILURE;
		} else {
			close(fd);
		}
	}

	return status;
}

static void
list_created_files(char **nfiles, const filesn_t nfiles_n)
{
	size_t i;
	int file_in_cwd = 0;
	filesn_t n = workspaces[cur_ws].path
		? count_dir(workspaces[cur_ws].path, NO_CPOP) - 2 : 0;

	if (n > 0 && n > files)
		file_in_cwd = 1;

	if (conf.autols == 1 && file_in_cwd == 1)
		reload_dirlist();

	for (i = 0; nfiles[i]; i++) {
		char *f = abbreviate_file_name(nfiles[i]);
		char *p = f ? f : nfiles[i];
		puts((*p == '.' && p[1] == '/' && p[2]) ? p + 2 : p);
		if (f && f != nfiles[i])
			free(f);
	}

	print_reload_msg("%zu file(s) created\n", (size_t)nfiles_n);
}

static int
is_range(const char *str)
{
	if (!str || !*str)
		return 0;

	char *p = strchr(str, '-');
	if (!p || !*(p + 1))
		return 0;

	*p = '\0';
	if (is_number(str) && is_number(p + 1)) {
		*p = '-';
		return 1;
	}

	*p = '-';
	return 0;
}

/* Return 1 if NAME is a safe filename, or 0 if not.
 * See https://dwheeler.com/essays/fixing-unix-linux-filenames.html */
static int
is_valid_filename(char *name)
{
	char *n = name;
	/* Trailing spaces were already removed (by get_newname()) */

	/* Starting with dash */
	if (*n == '-') {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_DASH]);
		return 0;
	}

	/* Reserved keyword (internal: MIME type and file type expansions) */
	if ((*n == '=' && n[1] >= 'a' && n[1] <= 'z' && !n[2]) || *n == '@') {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_MIME]);
		return 0;
	}

	/* Reserved keyword (internal: bookmarks, tags, and selected files
	 * constructs) */
	if (((*n == 'b' || *n == 's') && n[1] == ':')
	|| strcmp(n, "sel") == 0) {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_BTS_CONST]);
		return 0;
	}

	if (*n == 't' && n[1] == ':' && n[2]) {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_BTS_CONST]);
		return 0;
	}

	/* Reserved (internal: ELN/range expansion) */
	if (is_number(n) || is_range(n)) {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_ELN]);
		return 0;
	}

	/* "~" or ".": Reserved keyword */
	if ((*n == '~' || *n == '.') && !n[1]) {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_SYS_KEY]);
		return 0;
	}

	/* ".." or "./": Reserved keyword */
	if (*n == '.' && (n[1] == '.' || n[1] == '/') && !n[2]) {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_SYS_KEY]);
		return 0;
	}

	int only_dots = 1;
	char *s = name;
	while (*s) {
		/* Contains control characters (being not UTF-8 leading nor
		 * continuation bytes) */
		if (*s < ' ' && (*s & 0xC0) != 0xC0 && (*s & 0xC0) != 0x80) {
			printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_CONTROL]);
			return 0;
		}
		/* Contains shell meta-characters */
		if (strchr("*?:[]\"<>|(){}&'!\\;$", *s)) {
			printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_META]);
			return 0;
		}
		/* Only dots: Reserved keyword (internal: fastback expansion) */
		if (*s != '.')
			only_dots = 0;

		s++;
	}

	if (only_dots == 1) {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_FASTBACK]);
		return 0;
	}

	/* Name too long */
	if (s - name >= NAME_MAX) {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_TOO_LONG]);
		return 0;
	}

#ifdef _BE_POSIX
	if (is_portable_filename(name, (size_t)(s - name)) != EXIT_SUCCESS) {
		printf("%s: %s\n", name, unsafe_name_msgs[UNSAFE_NOT_PORTABLE]);
		return 0;
	}
#endif /* _BE_POSIX */

	return 1;
}

/* Returns 0 if the file path NAME (or any component in it) does not exist and
 * is an invalid name. Otherwise, it returns 1.
 * If NAME is escaped, it is replaced by the unescaped name. */
static int
validate_filename(char **name)
{
	if (!*name || !*(*name))
		return 0;

	char *deq = dequote_str(*name, 0);
	if (!deq) {
		xerror(_("new: %s: Error dequoting file name\n"), *name);
		return 0;
	}

	free(*name);
	*name = deq;

	char *tmp = *deq != '/' ? deq : deq + 1;
	char *p = tmp, *q = tmp;
	struct stat a;
	int ret = 1;

	while (1) {
		if (!*q) { /* Basename */
			if (lstat(deq, &a) == -1)
				ret = is_valid_filename(p);
			break;
		}

		if (*q != '/')
			goto NEXT;

		/* Path component */
		*q = '\0';

		if (lstat(deq, &a) == -1 && (ret = is_valid_filename(p)) == 0) {
			*q = '/';
			break;
		}

		*q = '/';

		if (!*(q + 1))
			break;

		p = q + 1;

NEXT:
		++q;
	}

	return ret;
}

static int
format_new_filename(char **name)
{
	char *p = *name;
	if (*(*name) == '\'' || *(*name) == '"')
		p = remove_quotes(*name);

	if (!p || !*p)
		return EXIT_FAILURE;

	size_t flen = strlen(p);
	int is_dir = (flen > 1 && p[flen - 1] == '/') ? 1 : 0;
	if (is_dir == 1)
		p[flen - 1 ] = '\0'; /* Remove ending slash */

	char *npath = (char *)NULL;
	if (p == *name) {
		char *tilde = (char *)NULL;
		if (*p == '~')
			tilde = tilde_expand(p);

		size_t len = tilde ? strlen(tilde) : flen;
		npath = normalize_path(tilde ? tilde : p, len);
		free(tilde);

	} else { /* Quoted string. Copy it verbatim. */
		npath = savestring(p, flen);
	}

	if (!npath)
		return EXIT_FAILURE;

	size_t name_len = strlen(npath) + 2;
	*name = (char *)xrealloc(*name, name_len * sizeof(char));
	snprintf(*name, name_len, "%s%c", npath, is_dir == 1 ? '/' : 0);
	free(npath);

	return EXIT_SUCCESS;
}

static void
press_key_to_continue(void)
{
	fputs(_("Press any key to continue ..."), stdout);
	xgetchar();
	putchar('\n');
}

static int
err_file_exists(char *name, const int multi)
{
	char *n = abbreviate_file_name(name);
	char *p = n ? n : name;

	xerror("new: %s: %s\n", (*p == '.' && p[1] == '/' && p[2])
		? p + 2 : p, strerror(EEXIST));

	if (n && n != name)
		free(n);

	if (multi == 1)
		press_key_to_continue();

	return EXIT_FAILURE;
}

static int
ask_and_create_file(void)
{
	puts(_("Enter new file name (Ctrl-d to quit)\n"
		"Tip: End name with a slash to create a directory"));
	char _prompt[NAME_MAX];
	snprintf(_prompt, sizeof(_prompt), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	int quoted = 0;
	char *filename = get_newname(_prompt, (char *)NULL, &quoted);
	if (!filename) /* The user pressed Ctrl-d */
		return EXIT_SUCCESS;

	if (validate_filename(&filename) == 0) {
		xerror(_("new: %s: Unsafe file name\n"), filename);
		if (rl_get_y_or_n(_("Continue? [y/n] ")) == 0) {
			free(filename);
			return EXIT_SUCCESS;
		}
	}

	int exit_status = quoted == 0 ? format_new_filename(&filename)
		: EXIT_SUCCESS;
	if (exit_status == EXIT_FAILURE)
		goto ERROR;

	struct stat a;
	if (lstat(filename, &a) == 0) {
		exit_status = err_file_exists(filename, 0);
		goto ERROR;
	}

	exit_status = create_file(filename);
	if (exit_status == EXIT_SUCCESS) {
		char *f[] = { filename, (char *)NULL };
		list_created_files(f, 1);
	}

ERROR:
	free(filename);
	return exit_status;
}

int
create_files(char **args)
{
	if (args[0] && IS_HELP(args[0])) {
		puts(_(NEW_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;
	size_t i;

	/* If no argument provided, ask the user for a filename, create it and exit. */
	if (!args[0])
		return ask_and_create_file();

	/* Store pointers to actually created files into a pointers array.
	 * We'll use this later to print the names of actually created files. */
	char **new_files = (char **)xnmalloc(args_n + 1, sizeof(char *));
	filesn_t new_files_n = 0;

	for (i = 0; args[i]; i++) {
		if (validate_filename(&args[i]) == 0) {
			xerror(_("new: %s: Unsafe file name\n"), args[i]);
			if (rl_get_y_or_n(_("Continue? [y/n] ")) == 0)
				continue;
		}

		/* Properly format filename. */
		if (format_new_filename(&args[i]) == EXIT_FAILURE) {
			exit_status = EXIT_FAILURE;
			continue;
		}

		/* Skip existent files. */
		struct stat a;
		if (lstat(args[i], &a) == 0) {
			exit_status = err_file_exists(args[i], args_n > 1);
			continue;
		}

		int ret = create_file(args[i]);
		if (ret == EXIT_SUCCESS) {
			new_files[new_files_n] = args[i];
			new_files_n++;
		} else if (args_n > 1) {
			exit_status = EXIT_FAILURE;
			press_key_to_continue();
		}
	}

	new_files[new_files_n] = (char *)NULL;

	if (new_files_n > 0)
		list_created_files(new_files, new_files_n);

	free(new_files);
	return exit_status;
}

int
open_function(char **cmd)
{
	if (!cmd)
		return EXIT_FAILURE;

	if (!cmd[1] || IS_HELP(cmd[1])) {
		puts(_(OPEN_USAGE));
		return EXIT_SUCCESS;
	}

	if (*cmd[0] == 'o' && (!cmd[0][1] || strcmp(cmd[0], "open") == 0)) {
		if (strchr(cmd[1], '\\')) {
			char *deq_path = dequote_str(cmd[1], 0);
			if (!deq_path) {
				xerror(_("open: %s: Error dequoting filename\n"), cmd[1]);
				return EXIT_FAILURE;
			}

			xstrsncpy(cmd[1], deq_path, strlen(deq_path) + 1);
			free(deq_path);
		}
	}

	char *file = cmd[1];

	/* Check file existence. */
	struct stat attr;
	if (lstat(file, &attr) == -1) {
		xerror("open: %s: %s\n", cmd[1], strerror(errno));
		return EXIT_FAILURE;
	}

	/* Check file type: only directories, symlinks, and regular files
	 * will be opened. */
	char no_open_file = 1;
	const char *file_type = (char *)NULL;
	const char *const types[] = {
		"block device",
		"character device",
		"socket",
		"FIFO/pipe",
		"unknown file type",
#ifdef __sun
		"door",
#endif /* __sun */
		NULL};

	switch ((attr.st_mode & S_IFMT)) {
	/* Store file type to compose and print the error message, if necessary. */
	case S_IFBLK: file_type = types[OPEN_BLK]; break;
	case S_IFCHR: file_type = types[OPEN_CHR]; break;
	case S_IFSOCK: file_type = types[OPEN_SOCK]; break;
	case S_IFIFO: file_type = types[OPEN_FIFO]; break;
#ifdef __sun
	case S_IFDOOR: file_type = types[OPEN_DOOR]; break;
#endif /* __sun */
	case S_IFDIR: return cd_function(file, CD_PRINT_ERROR);

	case S_IFLNK: {
		int ret = get_link_ref(file);
		if (ret == -1) {
			xerror(_("open: %s: Broken symbolic link\n"), file);
			return EXIT_FAILURE;
		} else if (ret == S_IFDIR) {
			return cd_function(file, CD_PRINT_ERROR);
		} else {
			switch (ret) {
			case S_IFREG: no_open_file = 0; break;
			case S_IFBLK: file_type = types[OPEN_BLK]; break;
			case S_IFCHR: file_type = types[OPEN_CHR]; break;
			case S_IFSOCK: file_type = types[OPEN_SOCK]; break;
			case S_IFIFO: file_type = types[OPEN_FIFO]; break;
#ifdef __sun
			case S_IFDOOR: file_type = types[OPEN_DOOR]; break;
#endif /* __sun */
			default: file_type = types[OPEN_UNKNOWN]; break;
			}
		}
		}
		break;

	case S_IFREG: no_open_file = 0;	break;
	default: file_type = types[OPEN_UNKNOWN]; break;
	}

	/* If neither directory nor regular file nor symlink (to directory
	 * or regular file), print the corresponding error message and exit. */
	if (no_open_file == 1) {
		xerror(_("open: %s (%s): Cannot open file\nTry "
			"'APP FILE' or 'open FILE APP'\n"), cmd[1], file_type);
		return EXIT_FAILURE;
	}

	int ret = EXIT_SUCCESS;

	/* At this point we know that the file to be openend is either a regular
	 * file or a symlink to a regular file. So, just open the file. */

	if (!cmd[2] || (*cmd[2] == '&' && !cmd[2][1])) {
		ret = open_file(file);
		if (!conf.opener && ret == EXIT_FAILURE) {
			xerror(_("%s: Add a new entry to the mimelist file "
				"('mime edit' or F6) or run 'APP FILE' or 'open FILE APP'\n"),
				PROGRAM_NAME);
			return EXIT_FAILURE;
		}
		return ret;
	}

	/* Some application was specified to open the file. Use it. */
	char *tmp_cmd[] = {cmd[2], file, NULL};
	ret = launch_execv(tmp_cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);
	if (ret == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	if (ret == EXEC_NOTFOUND || ret == EACCES) {
		xerror("open: %s: %s\nTry 'open --help' for more "
			"information\n", cmd[2], NOTFOUND_MSG);
		return EXEC_NOTFOUND;
	}

	xerror("open: %s: %s\n", cmd[2], strerror(ret));
	return ret;
}

static char *
get_new_link_target(char *cur_target)
{
	puts("Edit target (Ctrl-d to quit)");
	char _prompt[NAME_MAX];
	snprintf(_prompt, sizeof(_prompt), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	char *new_target = (char *)NULL;
	while (!new_target) {
		int quoted = 0;
		new_target = get_newname(_prompt, cur_target, &quoted);
		UNUSED(quoted);

		if (!new_target) /* The user pressed Ctrl-d */
			return (char *)NULL;
	}

	size_t l = strlen(new_target);
	if (l > 0 && new_target[l - 1] == ' ') {
		l--;
		new_target[l] = '\0';
	}

	char *n = normalize_path(new_target, l);
	free(new_target);

	return n;
}

static void
print_current_target(const char *link, char **target)
{
	fputs(_("Current target -> "), stdout);

	if (*target) {
		colors_list(*target, NO_ELN, NO_PAD, PRINT_NEWLINE);
		return;
	}

	char tmp[PATH_MAX + 1];
	ssize_t len = readlinkat(XAT_FDCWD, link, tmp, sizeof(tmp) - 1);

	if (len != -1) {
		tmp[len] = '\0';
		printf(_("%s%s%s (broken link)\n"), uf_c, tmp, df_c);
		free(*target);
		*target = savestring(tmp, strlen(tmp));
		return;
	}

	puts(_("??? (broken link)"));
	return;
}

/* Relink the symbolic link LINK to a new target. */
int
edit_link(char *link)
{
	if (!link || !*link || IS_HELP(link)) {
		puts(LE_USAGE);
		return EXIT_SUCCESS;
	}

	/* Dequote the file name, if necessary. */
	if (strchr(link, '\\')) {
		char *tmp = dequote_str(link, 0);
		if (!tmp) {
			xerror(_("le: %s: Error dequoting file\n"), link);
			return EXIT_FAILURE;
		}

		xstrsncpy(link, tmp, strlen(tmp) + 1);
		free(tmp);
	}

	size_t len = strlen(link);
	if (len > 0 && link[len - 1] == '/')
		link[len - 1] = '\0';

	/* Check if we have a valid symbolic link */
	struct stat attr;
	if (lstat(link, &attr) == -1) {
		xerror("le: %s: %s\n", link, strerror(errno));
		return EXIT_FAILURE;
	}

	if (!S_ISLNK(attr.st_mode)) {
		xerror(_("le: %s: Not a symbolic link\n"), link);
		return EXIT_FAILURE;
	}

	/* Get file pointed to by symlink and report to the user */
	char *real_path = realpath(link, NULL);
	print_current_target(link, &real_path);

	char *new_path = get_new_link_target(real_path);
	if (new_path && strcmp(new_path, real_path) == 0) {
		free(real_path);
		free(new_path);
		puts(_("le: Nothing to do"));
		return (EXIT_SUCCESS);
	}

	free(real_path);
	if (!new_path) /* The user pressed C-d */
		return EXIT_SUCCESS;

	/* Check new_path existence and warn the user if it does not exist. */
	if (lstat(new_path, &attr) == -1) {
		xerror("%s: %s\n", new_path, strerror(errno));
		if (rl_get_y_or_n(_("Relink as broken symbolic link? [y/n] ")) == 0) {
			free(new_path);
			return EXIT_SUCCESS;
		}
	}

	/* Finally, remove the link and recreate it as link to new_path. */
	if (unlinkat(XAT_FDCWD, link, 0) == -1
	|| symlinkat(new_path, XAT_FDCWD, link) == -1) {
		free(new_path);
		xerror(_("le: Cannot relink symbolic link '%s': %s\n"),
			link, strerror(errno));
		return EXIT_FAILURE;
	}

	printf(_("'%s' relinked to "), link);
	fflush(stdout);
	colors_list(new_path, NO_ELN, NO_PAD, PRINT_NEWLINE);
	free(new_path);

	return EXIT_SUCCESS;
}

/* Create a symbolic link to ARGS[0] named ARGS[1]. If args[1] is not specified,
 * the link is created as target_basename.link.
 * Returns 0 in case of success, or 1 otherwise. */
int
symlink_file(char **args)
{
	if (!args[0] || !*args[0] || IS_HELP(args[0])) {
		puts(LINK_USAGE);
		return EXIT_SUCCESS;
	}

	size_t len = strlen(args[0]);
	if (len > 1 && args[0][len - 1] == '/')
		args[0][len - 1] = '\0';

	if (strchr(args[0], '\\')) {
		char *deq = dequote_str(args[0], 0);
		if (deq) {
			free(args[0]);
			args[0] = deq;
		}
	}

	char *target = args[0];
	char *link_name = args[1];
	char tmp[PATH_MAX];

	if (!link_name || !*link_name) {
		char *p = strrchr(target, '/');
		snprintf(tmp, sizeof(tmp), "%s.link", (p && p[1]) ? p + 1 : target);
		link_name = tmp;
	}

	len = strlen(link_name);
	if (len > 1 && link_name[len - 1] == '/')
		link_name[len - 1] = '\0';

	struct stat a;
	if (lstat(target, &a) == -1) {
		printf("link: %s: %s\n", target, strerror(errno));
		if (rl_get_y_or_n(_("Create broken symbolic link? [y/n] ")) == 0)
			return EXIT_SUCCESS;
	}

	if (lstat(link_name, &a) != -1 && S_ISLNK(a.st_mode)) {
		printf("link: %s: %s\n", link_name, strerror(EEXIST));
		if (rl_get_y_or_n(_("Overwrite this file? [y/n] ")) == 0)
			return EXIT_SUCCESS;

		if (unlinkat(XAT_FDCWD, link_name, 0) == -1) {
			xerror("link: %s: %s\n", link_name, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	char *abs_path = normalize_path(target, strlen(target));
	if (!abs_path) {
		xerror(_("link: %s: Error getting absolute path\n"), target);
		return EXIT_FAILURE;
	}

	int ret = symlinkat(abs_path, XAT_FDCWD, link_name);
	free(abs_path);

	if (ret == -1) {
		xerror(_("link: Cannot create symbolic link '%s': %s\n"),
			link_name, strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int
vv_rename_files(char **args)
{
	char **tmp = (char **)xnmalloc(args_n + 2, sizeof(char *));
	tmp[0] = savestring("br", 2);

	size_t i, l = strlen(args[args_n]), c = 1;
	if (l > 0 && args[args_n][l - 1] == '/')
		args[args_n][l - 1] = '\0';

	char *dest = args[args_n];

	for (i = 1; i < args_n && args[i]; i++) {
		l = strlen(args[i]);
		if (l > 0 && args[i][l - 1] == '/')
			args[i][l - 1] = '\0';

		char p[PATH_MAX];
		char *s = strrchr(args[i], '/');
		snprintf(p, sizeof(p), "%s/%s", dest, (s && *(++s)) ? s : args[i]);

		tmp[c] = savestring(p, strlen(p));
		c++;
	}

	tmp[c] = (char *)NULL;
	int ret = bulk_rename(tmp);

	for (i = 0; tmp[i]; i++)
		free(tmp[i]);
	free(tmp);

	return ret;
}

static int
validate_vv_dest_dir(const char *file)
{
	if (args_n == 0) {
		fprintf(stderr, "%s\n", VV_USAGE);
		return EXIT_FAILURE;
	}

	struct stat a;
	if (stat(file, &a) == -1) {
		xerror("vv: %s: %s\n", file, strerror(errno));
		return EXIT_FAILURE;
	}

	if (!S_ISDIR(a.st_mode)) {
		xerror(_("vv: %s: Not a directory\n"), file);
		return EXIT_FAILURE;
	}

	if (strcmp(workspaces[cur_ws].path, file) == 0) {
		xerror("%s\n", _("vv: Destiny directory is the current directory"));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static char *
get_new_filename(char *cur_name)
{
	char _prompt[NAME_MAX];
	snprintf(_prompt, sizeof(_prompt), _("Enter new name (Ctrl-d to quit)\n"
		"\001%s\002>\001%s\002 "), mi_c, tx_c);

	char *new_name = (char *)NULL;
	while (!new_name) {
		int quoted = 0;
		new_name = get_newname(_prompt, cur_name, &quoted);
		UNUSED(quoted);

		if (!new_name) /* The user pressed Ctrl-d */
			return (char *)NULL;

		if (is_blank_name(new_name) == 1) {
			free(new_name);
			new_name = (char *)NULL;
		}
	}

	size_t l = strlen(new_name);
	if (l > 0 && new_name[l - 1] == ' ') {
		l--;
		new_name[l] = '\0';
	}

	char *n = normalize_path(new_name, l);
	free(new_name);

	return n;
}

/* Run CMD (either cp(1) or mv(1)) via execv().
 * skip_force is true (1) when the -f,--force parameter has been provided to
 * either 'c' or 'm' commands: it intructs cp/mv to run non-interactivelly
 * (no -i). */
static int
run_cp_mv_cmd(char **cmd, const int skip_force)
{
	if (!cmd)
		return EXIT_FAILURE;

	char *new_name = (char *)NULL;
	if (xrename == 1) {
		if (!cmd[1])
			return EINVAL;

		/* If we have a number, either it was not expanded by parse_input_str(),
		 * in which case it is an invalid ELN, or it was expanded to a file
		 * named as a number. Let's check if we have such file name in the
		 * files list. */
		if (is_number(cmd[1])) {
			filesn_t i = files;
			while (--i >= 0) {
				if (*cmd[1] != *file_info[i].name)
					continue;
				if (strcmp(cmd[1], file_info[i].name) == 0)
					break;
			}
			if (i == -1) {
				xerror(_("%s: %s: No such ELN\n"), PROGRAM_NAME, cmd[1]);
				xrename = 0;
				return ENOENT;
			}
		} else {
			char *p = dequote_str(cmd[1], 0);
			struct stat a;
			int ret = lstat(p ? p : cmd[1], &a);
			free(p);
			if (ret == -1) {
				xrename = 0;
				xerror("m: %s: %s\n", cmd[1], strerror(errno));
				return errno;
			}
		}

		new_name = get_new_filename(cmd[1]);
		if (!new_name)
			return EXIT_SUCCESS;
	}

	char **tcmd = xnmalloc(3 + args_n + 2, sizeof(char *));
	size_t n = 0;
	char *p = strchr(cmd[0], ' ');
	if (p && *(p + 1)) {
		*p = '\0';
		p++;
		tcmd[0] = savestring(cmd[0], strlen(cmd[0]));
		tcmd[1] = savestring(p, strlen(p));
		n += 2;
	} else {
		tcmd[0] = savestring(cmd[0], strlen(cmd[0]));
		n++;
	}

	/* wcp(1) does not support end of options (--) */
	if (strcmp(cmd[0], "wcp") != 0) {
		tcmd[n] = savestring("--", 2);
		n++;
	}

	size_t i;
	for (i = 1; cmd[i]; i++) {
		/* The -f,--force parameter is internal. Skip it.
		 * It instructs cp/mv to run non-interactively (no -i param) */
		if (skip_force == 1 && i == 1 && is_force_param(cmd[i]) == 1)
			continue;
		p = dequote_str(cmd[i], 0);
		if (!p)
			continue;
		tcmd[n] = savestring(p, strlen(p));
		free(p);
		n++;
	}

	if (cmd[1] && !cmd[2] && *cmd[0] == 'c' && *(cmd[0] + 1) == 'p'
	&& *(cmd[0] + 2) == ' ') {
		tcmd[n][0] = '.';
		tcmd[n][1] = '\0';
		n++;
	} else {
		if (new_name) {
			p = (char *)NULL;
			if (*new_name == '~')
				p = tilde_expand(new_name);
			char *q = p ? p : new_name;
			tcmd[n] = savestring(q, strlen(q));
			free(new_name);
			free(p);
			n++;
		}
	}

	tcmd[n] = (char *)NULL;
	int ret = launch_execv(tcmd, FOREGROUND, E_NOFLAG);

	for (i = 0; i < n; i++)
		free(tcmd[i]);
	free(tcmd);

	if (ret != EXIT_SUCCESS)
		return ret;

	/* Error messages are printed by launch_execv() itself */

	/* If 'rm sel' and command is successful, deselect everything */
	if (is_sel && *cmd[0] == 'r' && cmd[0][1] == 'm' && (!cmd[0][2]
	|| cmd[0][2] == ' ')) {
		int j = (int)sel_n;
		while (--j >= 0)
			free(sel_elements[j].name);
		sel_n = 0;
		save_sel();
	}

#ifdef GENERIC_FS_MONITOR
	if (*cmd[0] == 'm')
		reload_dirlist();
#endif /* GENERIC_FS_MONITOR */

	return EXIT_SUCCESS;
}

/* Launch the command associated to 'c' (also 'v' and 'vv') or 'm'
 * internal commands. */
int
cp_mv_file(char **args, const int copy_and_rename, const int force)
{
	/* vv command */
	if (copy_and_rename == 1
	&& validate_vv_dest_dir(args[args_n]) == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (*args[0] == 'm' && args[1]) {
		size_t len = strlen(args[1]);
		if (len > 0 && args[1][len - 1] == '/')
			args[1][len - 1] = '\0';
	}

	if (is_sel == 0 && copy_and_rename == 0)
		return run_cp_mv_cmd(args, force);

	size_t n = 0;
	char **tcmd = (char **)xnmalloc(3 + args_n + 2, sizeof(char *));
	char *p = strchr(args[0], ' ');
	if (p && *(p + 1)) {
		*p = '\0';
		p++;
		tcmd[0] = savestring(args[0], strlen(args[0]));
		tcmd[1] = savestring(p, strlen(p));
		n += 2;
	} else {
		tcmd[0] = savestring(args[0], strlen(args[0]));
		n++;
	}

	/* wcp(1) does not support end of options (--) */
	if (strcmp(tcmd[0], "wcp") != 0) {
		tcmd[n] = savestring("--" , 2);
		n++;
	}

	size_t i = force == 1 ? 2 : 1;
	for (; args[i]; i++) {
		p = dequote_str(args[i], 0);
		if (!p)
			continue;
		tcmd[n] = savestring(p, strlen(p));
		free(p);
		n++;
	}

	if (sel_is_last == 1) {
		tcmd[n] = savestring(".", 1);
		n++;
	}

	tcmd[n] = (char *)NULL;

	int ret = launch_execv(tcmd, FOREGROUND, E_NOFLAG);

	for (i = 0; tcmd[i]; i++)
		free(tcmd[i]);
	free(tcmd);

	if (ret != EXIT_SUCCESS)
		return ret;

	if (copy_and_rename == 1) /* vv command */
		return vv_rename_files(args);

	/* If 'mv sel' and command is successful deselect everything,
	 * since sel files are not there anymore. */
	if (*args[0] == 'm' && args[0][1] == 'v'
	&& (!args[0][2] || args[0][2] == ' '))
		deselect_all();

	return EXIT_SUCCESS;
}

/* Print the list of files removed via the most recent call to the 'r' command */
static void
list_removed_files(char **cmd, const size_t *dirs, const size_t start,
	const int cwd)
{
	size_t i, c = 0;
	for (i = start; cmd[i]; i++);
	char **removed_files = (char **)xnmalloc(i + 1, sizeof(char *));
	size_t *_dirs = (size_t *)xnmalloc(i + 1, sizeof(size_t));

	struct stat a;
	for (i = start; cmd[i]; i++) {
		_dirs[c] = 0;
		if (lstat(cmd[i], &a) == -1 && errno == ENOENT) {
			removed_files[c] = cmd[i];
			_dirs[c] = dirs[i];
			c++;
		}
	}
	removed_files[c] = (char *)NULL;

	if (c == 0) { /* No file was removed */
		free(removed_files);
		free(_dirs);
		return;
	}

	if (conf.autols == 1 && cwd == 1)
		reload_dirlist();

	for (i = 0; i < c; i++) {
		if (!removed_files[i] || !*removed_files[i])
			continue;

		char *p = abbreviate_file_name(removed_files[i]);
		fputs(p ? p : removed_files[i], stdout);
		puts(_dirs[i] == 1 ? "/" : "");

		if (p && p != removed_files[i])
			free(p);
	}

	print_reload_msg(_("%zu file(s) removed\n"), c);

	free(_dirs);
	free(removed_files);
}

/* Return the appropriate parameters for (g)rm(1), depending on:
 * 1. The installed version of rm
 * 2. The list of files to be removed contains at least 1 dir (DIRS)
 * 3. We should run interactively (RM_FORCE) */
/*
static char *
set_rm_params(const int dirs, const int rm_force)
{
	if (dirs == 1) {
#if defined(_BE_POSIX)
		return (rm_force == 1 ? "-rf" : "-r");
#elif defined(CHECK_COREUTILS)
		if (bin_flags & BSD_HAVE_COREUTILS)
			return (rm_force == 1 ? "-drf" : "-dIr");
		else
# if defined(__sun)
			return (rm_force == 1 ? "-rf" : "-r");
# else
			return (rm_force == 1 ? "-drf" : "-dr");
# endif // __sun
#else
		return (rm_force == 1 ? "-drf" : "-dIr");
#endif // _BE_POSIX
	}

// No directories
#if defined(_BE_POSIX)
	return "-f";
#elif defined(CHECK_COREUTILS)
	if (bin_flags & BSD_HAVE_COREUTILS)
		return (rm_force == 1 ? "-f" : "-I");
	else
		return "-f";
#else
	return (rm_force == 1 ? "-f" : "-I");
#endif // _BE_POSIX
} */

/* Print files to be removed and ask the user for confirmation.
 * Returns 0 if no or 1 if yes. */
static int
rm_confirm(char **args, const size_t *dirs, const size_t start,
	const int have_dirs)
{
	printf(_("r: File(s) to be removed%s:\n"),
		have_dirs == 1 ? _(" (recursively)") : "");

	size_t i;
	for (i = start; args[i]; i++)
		printf("%s%c\n", args[i], dirs[i] == 1 ? '/' : 0);

	return rl_get_y_or_n(_("Continue? [y/n] "));
}

int
remove_file(char **args)
{
	int cwd = 0, exit_status = EXIT_SUCCESS, errs = 0;

	struct stat a;
	char **rm_cmd = (char **)xnmalloc(args_n + 4, sizeof(char *));
	/* Let's remember which removed files were directories. DIRS wil be later
	 * passed to list_removed_files() to append a slash to directories when
	 * reporting removed files. A bit convoluted, but it works. */
	size_t *dirs = (size_t *)xnmalloc(args_n + 4, sizeof(size_t));

	int i, j, have_dirs = 0;
	int rm_force = conf.rm_force == 1 ? 1 : 0;

	i = (is_force_param(args[1]) == 1) ? 2 : 1;
	if (i == 2)
		rm_force = 1;

	for (j = 3; args[i]; i++) {
		/* Let's start storing file names in 3: 0 is for 'rm', and 1
		 * and 2 for parameters, including end of parameters (--). */

		/* If we have a symlink to dir ending with a slash, stat(3) takes it
		 * as a directory, and then rm(1) complains that cannot remove it,
		 * because "Is a directory". So, let's remove the ending slash:
		 * stat(3) will take it as the symlink it is and rm(1) will remove
		 * the symlink (not the target), without complains. */
		size_t len = strlen(args[i]);
		if (len > 0 && args[i][len - 1] == '/')
			args[i][len - 1] = '\0';

		/* Check if at least one file is in the current directory. If not,
		 * there is no need to refresh the screen. */
		if (cwd == 0)
			cwd = is_file_in_cwd(args[i]);

		char *tmp = dequote_str(args[i], 0);
		if (!tmp) {
			xerror(_("r: %s: Error dequoting file name\n"), args[i]);
			continue;
		}

		if (lstat(tmp, &a) != -1) {
			rm_cmd[j] = savestring(tmp, strlen(tmp));
			if (S_ISDIR(a.st_mode)) {
				dirs[j] = 1;
				have_dirs = 1;
			} else {
				dirs[j] = 0;
			}
			j++;
		} else {
			xerror("r: %s: %s\n", tmp, strerror(errno));
			errs++;
		}

		free(tmp);
	}

	rm_cmd[j] = (char *)NULL;

	if (errs > 0 && j > 3) { /* If at least one error, fail anyway. */
		fputs(_("r: No files were removed\n"), stderr);
		goto END;
//		fputs(_("Press any key to continue... "), stdout);
//		xgetchar();
	}

	if (j == 3) { /* No file to be deleted */
		free(rm_cmd);
		free(dirs);
		return EXIT_FAILURE;
	}

	if (rm_force == 0 && ((flags & REMOVE_ELN) || have_dirs == 1 || j > 5)
	&& rm_confirm(rm_cmd, dirs, 3, have_dirs) == 0)
		goto END;

//#if defined(CHECK_COREUTILS)
//	rm_cmd[0] = (bin_flags & BSD_HAVE_COREUTILS) ? "grm" : "rm";
//#else
//	rm_cmd[0] = "rm";
//#endif /* CHECK_COREUTILS */
//	rm_cmd[1] = set_rm_params(have_dirs, rm_force);
	rm_cmd[0] = "rm";
	rm_cmd[1] = have_dirs == 1 ? "-rf" : "-f";
	rm_cmd[2] = "--";

	if (launch_execv(rm_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

	if (is_sel && exit_status == EXIT_SUCCESS)
		deselect_all();

	if (print_removed_files == 1)
		list_removed_files(rm_cmd, dirs, 3, cwd);

END:
	for (i = 3; rm_cmd[i]; i++)
		free(rm_cmd[i]);
	free(rm_cmd);

	free(dirs);

	return exit_status;
}

/* Rename a bulk of files (ARGS) at once. Takes files to be renamed
 * as arguments, and returns zero on success and one on error. The
 * procedude is quite simple: file names to be renamed are copied into
 * a temporary file, which is opened via the mime function and shown
 * to the user to modify it. Once the file names have been modified and
 * saved, modifications are printed on the screen and the user is
 * asked whether to perform the actual bulk renaming or not.
 *
 * This bulk rename method is the same used by the fff filemanager,
 * ranger, and nnn */
int
bulk_rename(char **args)
{
	if (!args || !args[1] || IS_HELP(args[1])) {
		puts(_(BULK_USAGE));
		return EXIT_SUCCESS;
	}

	int exit_status = EXIT_SUCCESS;

	char bulk_file[PATH_MAX];
	snprintf(bulk_file, sizeof(bulk_file), "%s/%s",
		xargs.stealth_mode == 1 ? P_tmpdir : tmp_dir, TMP_FILENAME);

	int fd = mkstemp(bulk_file);
	if (fd == -1) {
		xerror("br: mkstemp: %s: %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i, arg_total = 0;
	FILE *fp = (FILE *)NULL;

#if defined(__HAIKU__) || defined(__sun)
	int tmp_fd = 0;
	fp = open_fwrite(bulk_file, &tmp_fd);
	if (!fp) {
		if (unlink(bulk_file) == -1)
			xerror("br: unlink: %s: %s\n", bulk_file, strerror(errno));
		xerror("br: fopen: %s: %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}
#endif /* __HAIKU__ || __sun */

#if !defined(__HAIKU__) && !defined(__sun)
	dprintf(fd, BULK_RENAME_TMP_FILE_HEADER);
#else
	fprintf(fp, BULK_RENAME_TMP_FILE_HEADER);
#endif /* !__HAIKU__ && !__sun */

	struct stat attr;
	size_t counter = 0;
	/* Copy all files to be renamed to the bulk file */
	for (i = 1; args[i]; i++) {
		/* Dequote file name, if necessary */
		if (strchr(args[i], '\\')) {
			char *deq_file = dequote_str(args[i], 0);
			if (!deq_file) {
				xerror(_("br: %s: Error dequoting file name\n"), args[i]);
				continue;
			}

			xstrsncpy(args[i], deq_file, strlen(deq_file) + 1);
			free(deq_file);
		}

		/* Resolve "./" and "../" */
		if (*args[i] == '.' && (args[i][1] == '/' || (args[i][1] == '.'
		&& args[i][2] == '/') ) ) {
			char *p = realpath(args[i], NULL);
			if (!p) {
				xerror("br: %s: %s\n", args[i], strerror(errno));
				continue;
			}
			free(args[i]);
			args[i] = p;
		}

		if (lstat(args[i], &attr) == -1) {
			xerror("br: %s: %s\n", args[i], strerror(errno));
			continue;
		}

		counter++;

#if !defined(__HAIKU__) && !defined(__sun)
		dprintf(fd, "%s\n", args[i]);
#else
		fprintf(fp, "%s\n", args[i]);
#endif /* !__HAIKU__ && !__sun */
	}
#if defined(__HAIKU__) || defined(__sun)
	fclose(fp);
#endif /* __HAIKU__ || __sun */
	arg_total = i;
	close(fd);

	if (counter == 0) { /* No valid file name */
		if (unlink(bulk_file) == -1)
			xerror("br: unlink: %s: %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}

	fp = open_fread(bulk_file, &fd);
	if (!fp) {
		if (unlink(bulk_file) == -1)
			xerror("br: unlink: %s: %s\n", bulk_file, strerror(errno));
		xerror("br: %s: %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Store the last modification time of the bulk file. This time
	 * will be later compared to the modification time of the same
	 * file after shown to the user */
	fstat(fd, &attr);
	time_t mtime_bfr = (time_t)attr.st_mtime;

	/* Open the bulk file */
	open_in_foreground = 1;
	exit_status = open_file(bulk_file);
	open_in_foreground = 0;

	if (exit_status != EXIT_SUCCESS) {
		xerror("br: %s\n", errno != 0
			? strerror(errno) : _("Error opening temporary file"));
		goto ERROR;
	}

	fclose(fp);
	fp = open_fread(bulk_file, &fd);
	if (!fp) {
		if (unlink(bulk_file) == -1)
			xerror("br: unlink: %s: %s\n", bulk_file, strerror(errno));
		xerror("br: %s: %s\n", bulk_file, strerror(errno));
		return errno;
	}

	/* Compare the new modification time to the stored one: if they
	 * match, nothing was modified */
	fstat(fd, &attr);
	if (mtime_bfr == (time_t)attr.st_mtime) {
		puts(_("br: Nothing to do"));
		goto ERROR;
	}

	/* Make sure there are as many lines in the bulk file as files
	 * to be renamed */
	size_t file_total = 1;
	char tmp_line[256];
	while (fgets(tmp_line, (int)sizeof(tmp_line), fp)) {
		if (!*tmp_line || *tmp_line == '\n' || *tmp_line == '#')
			continue;
		file_total++;
	}

	if (arg_total != file_total) {
		xerror("%s\n", _("br: Line mismatch in renaming file"));
		goto ERROR;
	}

	/* Go back to the beginning of the bulk file, again */
	fseek(fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;
	int modified = 0;

	i = 1;
	/* Print what would be done */
	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!*line || *line == '\n' || *line == '#')
			continue;
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		if (args[i] && strcmp(args[i], line) != 0) {
			printf("%s %s->%s %s\n", args[i], mi_c, df_c, line);
			modified++;
		}

		i++;
	}

	/* If no file name was modified */
	if (modified == 0) {
		free(line);
		puts(_("br: Nothing to do"));
		goto ERROR;
	}

	/* Ask the user for confirmation */
	if (rl_get_y_or_n("Continue? [y/n] ") == 0) {
		free(line);
		goto ERROR;
	}

	/* Once again */
	fseek(fp, 0L, SEEK_SET);

	i = 1;

	/* Rename each file */
	while ((line_len = getline(&line, &line_size, fp)) > 0) {
		if (!*line || *line == '\n' || *line == '#')
			continue;

		if (!args[i]) {
			i++;
			continue;
		}

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';
		if (args[i] && strcmp(args[i], line) != 0) {
			if (renameat(XAT_FDCWD, args[i], XAT_FDCWD, line) == -1)
				exit_status = errno;
		}

		i++;
	}

	free(line);

	if (unlinkat(fd, bulk_file, 0) == -1) {
		xerror("br: unlinkat: %s: %s\n", bulk_file, strerror(errno));
		exit_status = errno;
	}
	fclose(fp);

#ifdef GENERIC_FS_MONITOR
	if (exit_status == EXIT_SUCCESS)
		reload_dirlist();
#endif /* GENERIC_FS_MONITOR */

	return exit_status;

ERROR:
	if (unlinkat(fd, bulk_file, 0) == -1) {
		xerror("br: unlinkat: %s: %s\n", bulk_file, strerror(errno));
		exit_status = errno;
	}
	fclose(fp);
	return exit_status;
}

/* Export files in CWD (if FILENAMES is NULL), or files in FILENAMES,
 * into a temporary file. Return the address of this empt file if
 * success (it must be freed) or NULL in case of error */
char *
export_files(char **filenames, const int open)
{
	size_t len = strlen(tmp_dir) + 14;
	char *tmp_file = (char *)xnmalloc(len, sizeof(char));
	snprintf(tmp_file, len, "%s/%s", tmp_dir, TMP_FILENAME);

	int fd = mkstemp(tmp_file);
	if (fd == -1) {
		xerror("exp: %s: %s\n", tmp_file, strerror(errno));
		free(tmp_file);
		return (char *)NULL;
	}
	
	size_t i;
#if defined(__HAIKU__) || defined(__sun)
	int tmp_fd = 0;
	FILE *fp = open_fwrite(tmp_file, &tmp_fd);
	if (!fp) {
		if (unlink(tmp_file) == -1)
			xerror("exp: unlink: %s: %s\n", tmp_file, strerror(errno));
		xerror("exp: %s: %s\n", tmp_file, strerror(errno));
		free(tmp_file);
		return (char *)NULL;
	}
#endif /* __HAIKU__ || __sun */

	/* If no argument, export files in CWD. */
	if (!filenames[1]) {
		for (i = 0; file_info[i].name; i++)
#if !defined(__HAIKU__) && !defined(__sun)
			dprintf(fd, "%s\n", file_info[i].name);
#else
			fprintf(fp, "%s\n", file_info[i].name);
#endif /* !__HAIKU__ && !__sun */
	} else {
		for (i = 1; filenames[i]; i++) {
			if (*filenames[i] == '.' && (!filenames[i][1]
			|| (filenames[i][1] == '.' && !filenames[i][2])))
				continue;
#if !defined(__HAIKU__) && !defined(__sun)
			dprintf(fd, "%s\n", filenames[i]);
#else
			fprintf(fp, "%s\n", filenames[i]);
#endif /* !__HAIKU__ && !__sun */
		}
	}
#if defined(__HAIKU__) || defined(__sun)
	fclose(fp);
#endif /* __HAIKU__ || __sun */
	close(fd);

	if (!open)
		return tmp_file;

	int ret = open_file(tmp_file);
	if (ret == EXIT_SUCCESS)
		return tmp_file;

	if (unlink(tmp_file) == -1)
		xerror("exp: unlink: %s: %s\n", tmp_file, strerror(errno));
	free(tmp_file);
	return (char *)NULL;
}

/* Create a symlink for each file in ARGS + 1
 * Ask the user for a custom suffix for new symlinks (defaults to .link)
 * If the destiny file exists, append a positive integer suffix to make
 * it unique */
int
batch_link(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1] || IS_HELP(args[1])) {
		puts(_(BL_USAGE));
		return EXIT_SUCCESS;
	}

	puts("Suffix defaults to '.link'");
	flags |= NO_FIX_RL_POINT;
	char *suffix = rl_no_hist(_("Enter links suffix ('q' to quit): "));
	flags &= ~NO_FIX_RL_POINT;

	if (suffix && *suffix == 'q' && !*(suffix + 1)) {
		free(suffix);
		return EXIT_SUCCESS;
	}

	size_t i;
	int exit_status = EXIT_SUCCESS;
	char tmp[NAME_MAX];

	for (i = 1; args[i]; i++) {
		if (!suffix || !*suffix) {
			snprintf(tmp, sizeof(tmp), "%s.link", args[i]);
		} else {
			if (*suffix == '.')
				snprintf(tmp, sizeof(tmp), "%s%s", args[i], suffix);
			else
				snprintf(tmp, sizeof(tmp), "%s.%s", args[i], suffix);
		}

		struct stat a;
		size_t added_suffix = 1;
		char cur_suffix[24];
		while (stat(tmp, &a) == EXIT_SUCCESS) {
			char *d = strrchr(tmp, '-');
			if (d && *(d + 1) && is_number(d + 1))
				*d = '\0';
			snprintf(cur_suffix, sizeof(cur_suffix), "-%zu", added_suffix);
			/* Using strnlen() here avoids a Redhat hardened compilation warning. */
			xstrncat(tmp, strnlen(tmp, sizeof(tmp)), cur_suffix, sizeof(tmp));
			added_suffix++;
		}

		char *ptr = strrchr(tmp, '/');
		if (symlinkat(args[i], XAT_FDCWD, (ptr && ++ptr) ? ptr : tmp) == -1) {
			exit_status = errno;
			xerror(_("bl: symlinkat: %s: Cannot create symlink: %s\n"),
				ptr ? ptr : tmp, strerror(errno));
		}
	}

	free(suffix);
	return exit_status;
}
