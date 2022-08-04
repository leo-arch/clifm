/* file_operations.c -- control multiple file operations */

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
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <readline/readline.h>
#include <limits.h>
#include <fcntl.h>

#include <dirent.h>

#ifndef _NO_ARCHIVING
# include "archives.h"
#endif
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

static char
ask_user_y_or_n(const char *msg, char default_answer)
{
	char *answer = (char *)NULL;
	while (!answer) {
		answer = rl_no_hist(msg);
		if (answer && *answer && *(answer + 1)) {
			free(answer);
			answer = (char *)NULL;
			continue;
		}

		if (!answer) /* Defaults to DEFAULT_ANSWER*/
			return default_answer;

		switch (*answer) {
		case 'y': /* fallthrough */
		case 'Y': free(answer); return 'y';

		case 'n': /* fallthrough */
		case 'N': free(answer); return 'n';
		case '\0': free(answer); return default_answer;

		default:
			free(answer);
			answer = (char *)NULL;
			break;
		}
	}

	free(answer);
	return default_answer;
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
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "rr: %s: %s\n", s1, strerror(ec));
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
	_err(ERR_NO_STORE, NOPRINT_PROMPT, "rr: %s: %s\n", s2, strerror(ENOENT));
	return ENOENT;
}

static int
create_tmp_file(char **file, int *fd)
{
	size_t tmp_len = strlen(xargs.stealth_mode == 1 ? P_tmpdir : tmp_dir);
	*file = (char *)xnmalloc(tmp_len + strlen(TMP_FILENAME) + 2, sizeof(char));
	sprintf(*file, "%s/%s", xargs.stealth_mode == 1 ? P_tmpdir : tmp_dir, TMP_FILENAME);

	errno = 0;
	*fd = mkstemp(*file);
	if (*fd == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "rr: mkstemp: %s: %s\n", *file, strerror(errno));
		free(*file);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static char
get_file_suffix(mode_t type)
{
	switch(type) {
	case DT_DIR: return '/';
	case DT_REG: return 0;
	case DT_LNK: return '@';
	case DT_SOCK: return '=';
	case DT_FIFO: return '|';
	case DT_UNKNOWN: return '?';
	default: return 0;
	}
}

static void
print_file(FILE *fp, char *name, mode_t type)
{
#ifndef _DIRENT_HAVE_D_TYPE
	UNUSED(type);
	char s = 0;
	struct stat a;
	if (lstat(name, &a) != -1)
		s = get_file_suffix(a.st_mode);
#else
	char s = get_file_suffix(type);
#endif
	if (s)
		fprintf(fp, "%s%c\n", name, s);
	else
		fprintf(fp, "%s\n", name);
}

static int
write_files_to_tmp(struct dirent ***a, int *n, const char *target, const char *tmp_file)
{
	FILE *fp = fopen(tmp_file, "w");
	if (!fp) {
		_err('e', PRINT_PROMPT, "%s: rr: fopen: %s: %s\n", PROGRAM_NAME, tmp_file, strerror(errno));
		return errno;
	}

	fprintf(fp, _(BULK_RM_TMP_FILE_HEADER));

	size_t i;
	if (target == workspaces[cur_ws].path) {
		for (i = 0; i < files; i++)
			print_file(fp, file_info[i].name, file_info[i].type);
	} else {
		if (count_dir(target, CPOP) <= 2) {
			fprintf(stderr, _("%s: %s: Directory empty\n"), PROGRAM_NAME, target);
			fclose(fp);
			unlink(tmp_file);
			return EXIT_FAILURE;
		}
		*n = scandir(target, a, NULL, alphasort);
		if (*n == -1) {
			fclose(fp);
			unlink(tmp_file);
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "rr: %s: %s", target, strerror(errno));
			return errno;
		}
		for (i = 0; i < (size_t)*n; i++) {
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
open_tmp_file(struct dirent ***a, int n, char *tmp_file, char *app)
{
	if (!app || !*app) {
		open_in_foreground = 1;
		int exit_status = open_file(tmp_file);
		open_in_foreground = 0;

		if (exit_status == EXIT_SUCCESS)
			return EXIT_SUCCESS;

		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("rr: %s: Cannot open file\n"), tmp_file);
		if (unlink(tmp_file) == -1) {
			_err('e', PRINT_PROMPT, "rr: unlink: %s: %s\n",	tmp_file, strerror(errno));
		}

		size_t i;
		for (i = 0; i < (size_t)n && *a && (*a)[i]; i++)
			free((*a)[i]);
		free(*a);
		return exit_status;
	}

	char *cmd[] = {app, tmp_file, NULL};
	int exit_status = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	if (exit_status == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	if (unlink(tmp_file) == -1) {
		_err('e', PRINT_PROMPT, "%s: rr: unlink: %s: %s\n", PROGRAM_NAME,
			tmp_file, strerror(errno));
	}

	size_t i;
	for (i = 0; i < (size_t)n && *a && (*a)[i]; i++)
		free((*a)[i]);
	free(*a);
	return exit_status;
}

static char **
get_files_from_tmp_file(const char *tmp_file, const char *target, const int n)
{
	size_t nfiles = (target == workspaces[cur_ws].path) ? files : (size_t)n;
	char **tmp_files = (char **)xnmalloc(nfiles + 2, sizeof(char *));

	FILE *fp = fopen(tmp_file, "r");

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
		if (line[len - 1] == '/' || line[len - 1] == '@' || line[len - 1] == '='
		|| line[len - 1] == '|' || line[len - 1] == '?') {
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
get_remove_files(const char *target, char **tmp_files, struct dirent ***a, const int n)
{
	size_t i, j = 0, l = (target == workspaces[cur_ws].path) ? files : (size_t)n;
	char **rem_files = (char **)xnmalloc(l + 2, sizeof(char *));

	if (target == workspaces[cur_ws].path) {
		for (i = 0; i < files; i++) {
			if (remove_this_file(file_info[i].name, tmp_files) == 1) {
				rem_files[j] = savestring(file_info[i].name, strlen(file_info[i].name));
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
				snprintf(p, PATH_MAX, "%s/%s", target, (*a)[i]->d_name);
			} else {
				snprintf(p, PATH_MAX, "%s/%s/%s", workspaces[cur_ws].path,
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
get_rm_param(char ***rfiles, int n)
{
	char *_param = (char *)NULL;
	struct stat attr;
	int i = n;

	while (--i >= 0) {
		if (lstat((*rfiles)[i], &attr) == -1)
			continue;

		if (S_ISDIR(attr.st_mode)) {
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__) || defined(_BE_POSIX)
			_param = savestring("-r", 2);
#else /* Linux and FreeBSD only */
			_param = savestring("-dIr", 4);
#endif /* __NetBSD__ || __OpenBSD__ || __APPLE__ || _BE_POSIX */
			break;
		}
	}

	if (!_param) {
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__) || defined(_BE_POSIX)
		_param = savestring("-f", 2);
#else /* Linux and FreeBSD only */
		_param = savestring("-I", 2);
#endif /* __NetBSD__ || __OpenBSD__ || __APPLE__ || _BE_POSIX */
	}

	return _param;
}

static char **
construct_rm_cmd(char ***rfiles, char *_param, size_t n)
{
	char **cmd = (char **)xnmalloc(n + 4, sizeof(char *));

	cmd[0] = savestring("rm", 2);
	cmd[1] = savestring(_param, strlen(_param));
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
	int n;
	for (n = 0; (*rfiles)[n]; n++)
		printf("%s\n", (*rfiles)[n]);

	if (n == 0)
		return EXIT_FAILURE;

	char a = ask_user_y_or_n("Continue? [y/N] ", 'n');

	int i = n;
	if (a == 'n') {
		while (--i >= 0)
			free((*rfiles)[i]);
		free(*rfiles);
		return 0;
	}

	char *_param = get_rm_param(rfiles, n);
	char **cmd = construct_rm_cmd(rfiles, _param, (size_t)n);

	int ret = launch_execve(cmd, FOREGROUND, E_NOFLAG);

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
diff_files(char *tmp_file, int n)
{
	FILE *fp = fopen(tmp_file, "r");
	char line[PATH_MAX + 6];
	memset(line, '\0', sizeof(line));

	int c = 0;
	while (fgets(line, (int)sizeof(line), fp)) {
		if (*line != '#' && *line != '\n')
			c++;
	}

	fclose(fp);
	if (c == n)
		return 0;

	return 1;
}

static int
nothing_to_do(char **tmp_file, struct dirent ***a, int n)
{
	printf(_("rr: Nothing to do\n"));
	unlink(*tmp_file);
	free(*tmp_file);

	int i = n;
	while (--i >= 0)
		free((*a)[i]);
	free(*a);

	return EXIT_SUCCESS;
}

int
bulk_remove(char *s1, char *s2)
{
	if (s1 && IS_HELP(s1))
		{ puts(_(RR_USAGE)); return EXIT_SUCCESS; }

	char *app = (char *)NULL, *target = (char *)NULL;
	int fd = 0, n = 0, ret = 0, i = 0;

	if ((ret = parse_bulk_remove_params(s1, s2, &app, &target)) != EXIT_SUCCESS)
		return ret;

	char *tmp_file = (char *)NULL;
	if ((ret = create_tmp_file(&tmp_file, &fd)) != EXIT_SUCCESS)
		return ret;

	struct dirent **a = (struct dirent **)NULL;
	if ((ret = write_files_to_tmp(&a, &n, target, tmp_file)) != EXIT_SUCCESS)
		{ free(tmp_file); return ret; }

	struct stat attr;
	stat(tmp_file, &attr);
	time_t old_t = attr.st_mtime;

	if ((ret = open_tmp_file(&a, n, tmp_file, app)) != EXIT_SUCCESS)
		{ free(tmp_file); return ret; }

	stat(tmp_file, &attr);
	int num = (target == workspaces[cur_ws].path) ? (int)files : n - 2;
	if (old_t == attr.st_mtime || diff_files(tmp_file, num) == 0)
		return nothing_to_do(&tmp_file, &a, n);

	char **__files = get_files_from_tmp_file(tmp_file, target, n);
	char **rem_files = get_remove_files(target, __files, &a, n);

	ret = bulk_remove_files(&rem_files);

	for (i = 0; __files[i]; i++) free(__files[i]);
	free(__files);

	unlinkat(fd, tmp_file, 0);
	free(tmp_file);
	return ret;
}

#ifndef _NO_LIRA
static int
run_mime(char *file)
{
	if (!file || !*file)
		return EXIT_FAILURE;

	char *p = rl_line_buffer ? rl_line_buffer : (char *)NULL;

	/* Convert ELN into file name (rl_line_buffer) */
	if (p && *p >= '1' && *p <= '9') {
		int a = atoi(p);
		if (a > 0 && (size_t)a <= files && file_info[a - 1].name)
			p = file_info[a - 1].name;
	}

	if (p && ( (*p == 'i' && (strncmp(p, "import", 6) == 0
	|| strncmp(p, "info", 4) == 0))
	|| (*p == 'o' && (p[1] == ' ' || strncmp(p, "open", 4) == 0)) ) ) {
		char *cmd[] = {"mm", "open", file, NULL};
		return mime_open(cmd);
	}

	char *cmd[] = {"mm", file, NULL};
	return mime_open(cmd);
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

	if (opener) {
		if (*opener == 'g' && strcmp(opener, "gio") == 0) {
			char *cmd[] = {"gio", "open", file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} else {
			char *cmd[] = {opener, file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
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
		if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
#endif /* _NO_LIRA */
	}

	return exit_status;
}

int
xchmod(const char *file, const char *mode_str)
{
	if (!file || !*file) {
		_err('e', PRINT_PROMPT, "xchmod: Empty buffer for file name\n");
		return EXIT_FAILURE;
	}

	if (!mode_str || !*mode_str) {
		_err('e', PRINT_PROMPT, "xchmod: Empty buffer for mode\n");
		return EXIT_FAILURE;
	}

	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		_err('e', PRINT_PROMPT, "xchmod: %s: %s\n", file, strerror(errno));
		return errno;
	}

	mode_t mode = (mode_t)strtol(mode_str, 0, 8);
	if (fchmod(fd, mode) == -1) {
		close(fd);
		_err('e', PRINT_PROMPT, "xchmod: %s: %s\n", file, strerror(errno));
		return errno;
	}

	close(fd);
	return EXIT_SUCCESS;
}

/* Toggle executable bit on file */
int
toggle_exec(const char *file, mode_t mode)
{
	/* Set or unset S_IXUSR, S_IXGRP, and S_IXOTH */
	(0100 & mode) ? (mode &= (mode_t)~0111) : (mode |= 0111);

	log_function(NULL);

	int fd = open(file, O_WRONLY);
	if (fd == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "xchmod: %s: %s\n", file, strerror(errno));
		return EXIT_FAILURE;
	}

	if (fchmod(fd, mode) == -1) {
		close(fd);
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "xchmod: %s: %s\n", file, strerror(errno));
		return EXIT_FAILURE;
	}

	close(fd);

	return EXIT_SUCCESS;
}

static char *
get_dup_file_dest_dir(void)
{
	char *dir = (char *)NULL;

	puts("Enter '.' for current directory ('q' to quit)");
	while (!dir) {
		dir = rl_no_hist("Destiny directory: ");
		if (!dir)
			continue;
		if (!*dir) {
			free(dir);
			dir = (char *)NULL;
			continue;
		}
		if (*dir == 'q' && !*(dir + 1)) {
			free(dir);
			return (char *)NULL;
		}
		if (access(dir , R_OK | W_OK | X_OK) == -1) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, "dup: %s: %s\n",	dir, strerror(errno));
			free(dir);
			dir = (char *)NULL;
			continue;
		}
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

	log_function(NULL);

	char *rsync_path = get_cmd_path("rsync");
	int exit_status =  EXIT_SUCCESS;

	size_t i;
	for (i = 1; cmd[i]; i++) {
		if (!cmd[i] || !*cmd[i])
			continue;
		char *source = cmd[i];
		if (strchr(source, '\\')) {
			char *deq_str = dequote_str(source, 0);
			if (!deq_str) {
				_err(ERR_NO_STORE, NOPRINT_PROMPT, "dup: %s: Error dequoting file name\n",
					source);
				continue;
			}
			strcpy(source, deq_str);
			free(deq_str);
		}

		/* Use source as destiny file name: source.copy, and, if already
		 * exists, source.copy-n, where N is an integer greater than zero */
		size_t source_len = strlen(source);
		int rem_slash = 0;
		if (strcmp(source, "/") != 0 && source_len > 0 && source[source_len - 1] == '/') {
			source[source_len - 1] = '\0';
			rem_slash = 1;
		}

		char *tmp = strrchr(source, '/');
		char *source_name;

		if (tmp && *(tmp + 1))
			source_name = tmp + 1;
		else
			source_name = source;

		char tmp_dest[PATH_MAX];
		if (strcmp(dest_dir, "/") != 0)
			snprintf(tmp_dest, sizeof(tmp_dest), "%s/%s.copy", dest_dir, source_name);
		else
			snprintf(tmp_dest, sizeof(tmp_dest), "%s%s.copy", dest_dir, source_name);

		char bk[PATH_MAX + 11];
		xstrsncpy(bk, tmp_dest, PATH_MAX);
		struct stat attr;
		size_t suffix = 1;
		while (stat(bk, &attr) == EXIT_SUCCESS) {
			snprintf(bk, sizeof(bk), "%s-%zu", tmp_dest, suffix);
			suffix++;
		}
		char *dest = savestring(bk, strlen(bk));

		if (rem_slash == 1)
			source[source_len - 1] = '/';

		if (rsync_path) {
			char *_cmd[] = {"rsync", "-aczvAXHS", "--progress", source, dest, NULL};
			if (launch_execve(_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} else {
#if !defined(_BE_POSIX)
			char *_cmd[] = {"cp", "-a", source, dest, NULL};
#else
			char *_cmd[] = {"cp", source, dest, NULL};
#endif
			if (launch_execve(_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		free(dest);
	}

	free(dest_dir);
	free(rsync_path);
	return exit_status;
} 

int
create_file(char **cmd)
{
	if (cmd[1] && IS_HELP(cmd[1])) {
		puts(_(NEW_USAGE));
		return EXIT_SUCCESS;
	}

	log_function(NULL);

	int exit_status = EXIT_SUCCESS;
	int free_cmd = 0;

	/* If no argument provided, ask the user for a filename */
	if (!cmd[1]) {
		char *filename = (char *)NULL;
		while (!filename) {
			puts(_("End filename with a slash to create a directory"));
			filename = rl_no_hist(_("Filename ('q' to quit): "));

			if (!filename)
				continue;

			if (!*filename) {
				free(filename);
				filename = (char *)NULL;
				continue;
			}
		}

		if (*filename == 'q' && !filename[1]) {
			free(filename);
			return EXIT_SUCCESS;
		}

		/* Once we have the filename, reconstruct the cmd array */
		char **tmp_cmd = (char **)xnmalloc(args_n + 3, sizeof(char *));
		tmp_cmd[0] = (char *)xnmalloc(2, sizeof(char));
		*tmp_cmd[0] = 'n';
		tmp_cmd[0][1] = '\0';
		tmp_cmd[1] = (char *)xnmalloc(strlen(filename) + 1, sizeof(char));
		strcpy(tmp_cmd[1], filename);
		tmp_cmd[2] = (char *)NULL;
		cmd = tmp_cmd;
		free_cmd = 1;
		free(filename);
	}

	/* Properly format filenames */
	size_t i, hlen = workspaces[cur_ws].path ? strlen(workspaces[cur_ws].path) : 0;
	for (i = 1; cmd[i]; i++) {
		char *npath = normalize_path(cmd[i], strlen(cmd[i]));
		if (!npath) {
			*cmd[i] = '\0'; /* Invalidate this entry */
			continue;
		}

		int is_dir = 0;
		size_t flen = strlen(cmd[i]);
		if (flen > 1 && cmd[i][flen - 1] == '/')
			is_dir = 1;

		cmd[i] = (char *)xrealloc(cmd[i], (strlen(npath) + 2) * sizeof(char));
		sprintf(cmd[i], "%s%c", npath, is_dir == 1 ? '/' : 0);
		free(npath);

		/* If the file already exists, skip it */
		struct stat a;
		if (lstat(cmd[i], &a) == 0) {
			int ret = strncmp(cmd[i], workspaces[cur_ws].path, hlen);
			char *name = (ret == 0 && *(cmd[i] + hlen) && *(cmd[i] + hlen + 1))
				? cmd[i] + hlen + 1 : cmd[i];
			fprintf(stderr, "%s: File exists\n", name);
			if (cmd[i + 1]) {
				printf("Press any key to continue ...");
				xgetchar();
				putchar('\n');
			}
			exit_status = EXIT_FAILURE;
			*cmd[i] = '\0'; /* Invalidate entry */

			continue;
		}

		/* If we have DIR/FILE and DIR doesn't exit, create DIR */
		char *ls = strrchr(cmd[i], '/');
		if (!ls || !*(ls + 1) || ls == cmd[i])
			continue; /* Last slash is either the first or the last char */

		*ls = '\0';
		if (stat(cmd[i], &a) != -1) { /* Parent dir exists */
			*ls = '/';
			continue;
		}

		char *md_cmd[] = {"mkdir", "-p", cmd[i], NULL};
		if (launch_execve(md_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			*cmd[i] = '\0'; /* Invalidate this entry */
			if (cmd[i + 1]) {
				printf("Press any key to continue ...");
				xgetchar();
				putchar('\n');
			}
			exit_status = EXIT_FAILURE;
		}
		*ls = '/';
	}

	/* Construct commands */
	size_t files_num = i - 1;

	char **nfiles = (char **)xnmalloc(files_num + 2, sizeof(char *));
	char **ndirs = (char **)xnmalloc(files_num + 3, sizeof(char *));

	/* Let's use 'touch' for files and 'mkdir -p' for dirs */
	nfiles[0] = (char *)xnmalloc(6, sizeof(char));
	strcpy(nfiles[0], "touch");

	ndirs[0] = (char *)xnmalloc(6, sizeof(char));
	strcpy(ndirs[0], "mkdir");

	ndirs[1] = (char *)xnmalloc(3, sizeof(char));
	ndirs[1][0] = '-';
	ndirs[1][1] = 'p';
	ndirs[1][2] = '\0';

	size_t cnfiles = 1, cndirs = 2;

	for (i = 1; cmd[i]; i++) {
		if (!*cmd[i])
			continue;
		size_t cmd_len = strlen(cmd[i]);
		/* File names ending with a slash are taken as directory names */
		if (cmd[i][cmd_len - 1] == '/') {
			ndirs[cndirs] = cmd[i];
			cndirs++;
		} else {
			nfiles[cnfiles] = cmd[i];
			cnfiles++;
		}
	}

	ndirs[cndirs] = (char *)NULL;
	nfiles[cnfiles] = (char *)NULL;

	size_t total = (cndirs - 2) + (cnfiles - 1);
	int ret = 0;
	/* Execute commands */
	if (cnfiles > 1) {
		if ((ret = launch_execve(nfiles, FOREGROUND, E_NOFLAG)) != EXIT_SUCCESS) {
			if (total > 1) {
				printf("Press any key to continue ...");
				xgetchar();
				putchar('\n');
			}
			exit_status = ret;
		}
	}

	if (cndirs > 2) {
		if ((ret = launch_execve(ndirs, FOREGROUND, E_NOFLAG)) != EXIT_SUCCESS) {
			if (total > 1) {
				printf("Press any key to continue ...");
				xgetchar();
				putchar('\n');
			}
			exit_status = ret;
		}
	}

	free(nfiles[0]);
	free(ndirs[0]);
	free(ndirs[1]);
	free(nfiles);
	free(ndirs);

	int file_in_cwd = 0;
	int n = workspaces[cur_ws].path ? count_dir(workspaces[cur_ws].path, NO_CPOP) - 2 : 0;
	if (n > 0 && (size_t)n > files)
		file_in_cwd = 1;

	if (total > 0) {
		if (autols == 1 && file_in_cwd == 1)
			reload_dirlist();

		for (i = 1; cmd[i]; i++) {
			if (!*cmd[i])
				continue;

			int dup = 0;
			for (size_t j = i + 1; cmd[j]; j++) {
				if (*cmd[i] == *cmd[j] && strcmp(cmd[i], cmd[j]) == 0) {
					dup = 1;
					break;
				}
			}
			if (dup == 1) {
				total--;
				continue;
			}

			struct stat a;
			if (stat(cmd[i], &a) != -1) {
				ret = workspaces[cur_ws].path
					? strncmp(cmd[i], workspaces[cur_ws].path, hlen) : -1;
				char *name = (ret == 0 && *(cmd[i] + hlen) && *(cmd[i] + hlen + 1))
					? cmd[i] + hlen + 1 : cmd[i];
				printf("%s\n", name);
			} else {
				total--;
			}
		}
		if (total > 0)
			print_reload_msg("%zu file(s) created\n", total);
	}

	if (free_cmd == 1) {
		for (i = 0; cmd[i]; i++)
			free(cmd[i]);
		free(cmd);
	}

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
				_err(ERR_NO_STORE, NOPRINT_PROMPT, _("open: %s: Error dequoting filename\n"),
					cmd[1]);
				return EXIT_FAILURE;
			}

			strcpy(cmd[1], deq_path);
			free(deq_path);
		}
	}

	char *file = cmd[1];

	/* Check file existence */
	struct stat attr;
	if (lstat(file, &attr) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "open: %s: %s\n", cmd[1], strerror(errno));
		return EXIT_FAILURE;
	}

	/* Check file type: only directories, symlinks, and regular files
	 * will be opened */
	char no_open_file = 1;
	const char *file_type = (char *)NULL;
	const char *types[] = {
		"block device",
		"character device",
		"socket",
		"FIFO/pipe",
		"unknown file type",
		NULL};

	switch ((attr.st_mode & S_IFMT)) {
	/* Store file type to compose and print the error message, if necessary */
	case S_IFBLK: file_type = types[OPEN_BLK]; break;
	case S_IFCHR: file_type = types[OPEN_CHR]; break;
	case S_IFSOCK: file_type = types[OPEN_SOCK]; break;
	case S_IFIFO: file_type = types[OPEN_FIFO]; break;
	case S_IFDIR: return cd_function(file, CD_PRINT_ERROR);
	case S_IFLNK: {
		int ret = get_link_ref(file);
		if (ret == -1) {
			fprintf(stderr, _("%s: open: %s: Broken symbolic link\n"), PROGRAM_NAME, file);
			return EXIT_FAILURE;
		} else if (ret == S_IFDIR) {
			return cd_function(file, CD_PRINT_ERROR);
		} else if (ret != S_IFREG) {
			switch (ret) {
			case S_IFBLK: file_type = types[OPEN_BLK]; break;
			case S_IFCHR: file_type = types[OPEN_CHR]; break;
			case S_IFSOCK: file_type = types[OPEN_SOCK]; break;
			case S_IFIFO: file_type = types[OPEN_FIFO]; break;
			default: file_type = types[OPEN_UNK]; break;
			}
		}
		}
		break;
	case S_IFREG: no_open_file = 0;	break;
	default: file_type = types[OPEN_UNK]; break;
	}

	/* If neither directory nor regular file nor symlink (to directory
	 * or regular file), print the corresponding error message and
	 * exit */
	if (no_open_file) {
		fprintf(stderr, _("%s: open: %s (%s): Cannot open file\nTry "
			"'APPLICATION FILENAME'\n"), PROGRAM_NAME, cmd[1], file_type);
		return EXIT_FAILURE;
	}

	/* At this point we know that the file to be openend is either a regular
	 * file or a symlink to a regular file. So, just open the file */
	if (!cmd[2] || (*cmd[2] == '&' && !cmd[2][1])) {
		int ret = open_file(file);
		if (!opener && ret == EXIT_FAILURE) {
			fputs("Add a new entry to the mimelist file ('mime "
				"edit' or F6) or run 'open FILE APPLICATION'\n", stderr);
			return EXIT_FAILURE;
		}
		return ret;
	}

	/* If some application was specified to open the file */
	char *tmp_cmd[] = {cmd[2], file, NULL};
	int ret = launch_execve(tmp_cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);
	if (ret == EXIT_SUCCESS)
		return EXIT_SUCCESS;

	if (ret == ENOENT) { /* 2: No such file or directory */
		fprintf(stderr, "%s: open: %s: Command not found\n", PROGRAM_NAME, cmd[2]);
		return 127;
	}

	fprintf(stderr, "%s: open: %s: %s\n", PROGRAM_NAME, cmd[2], strerror(ret));
	return ret;
}

/* Relink symlink to new path */
int
edit_link(char *link)
{
	if (!link || !*link)
		return EXIT_FAILURE;

	log_function(NULL);

	/* Dequote the file name, if necessary */
	if (strchr(link, '\\')) {
		char *tmp = dequote_str(link, 0);
		if (!tmp) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("le: %s: Error dequoting file\n"), link);
			return EXIT_FAILURE;
		}

		strcpy(link, tmp);
		free(tmp);
	}

	size_t len = strlen(link);
	if (link[len - 1] == '/')
		link[len - 1] = '\0';

	/* Check we have a valid symbolic link */
	struct stat attr;
	if (lstat(link, &attr) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "le: %s: %s\n", link, strerror(errno));
		return EXIT_FAILURE;
	}

	if (!S_ISLNK(attr.st_mode)) {
		fprintf(stderr, _("le: %s: Not a symbolic link\n"), link);
		return EXIT_FAILURE;
	}

	/* Get file pointed to by symlink and report to the user */
	char *real_path = realpath(link, NULL);
	if (!real_path) {
		printf(_("%s%s%s currently pointing to nowhere (broken link)\n"), or_c, link, df_c);
	} else {
		printf(_("%s%s%s currently pointing to "), ln_c, link, df_c);
		colors_list(real_path, NO_ELN, NO_PAD, PRINT_NEWLINE);
		free(real_path);
		real_path = (char *)NULL;
	}

	char *new_path = (char *)NULL;
	/* Enable autocd and auto-open (in case they are not already
	 * enabled) to allow TAB completion for ELN's */
	int autocd_status = autocd, auto_open_status = auto_open;
	autocd = auto_open = 1;

	while (!new_path) {
		new_path = rl_no_hist(_("New path ('q' to quit): "));
		if (!new_path)
			continue;
		if (!*new_path) {
			free(new_path);
			new_path = (char *)NULL;
			continue;
		}

		if (*new_path == 'q' && !new_path[1]) {
			free(new_path);
			return EXIT_SUCCESS;
		}
	}

	/* Set autocd and auto-open to their original values */
	autocd = autocd_status;
	auto_open = auto_open_status;

	/* If an ELN, replace by the corresponding file name */
	if (is_number(new_path)) {
		int a = atoi(new_path);
		if (a > 0 && a <= (int)files) {
			--a;
			if (file_info[a].name)
				new_path = savestring(file_info[a].name, strlen(file_info[a].name));
		} else {
			fprintf(stderr, _("le: %s: Invalid ELN\n"), new_path);
			free(new_path);
			return EXIT_FAILURE;
		}
	}

	/* Remove terminating space. TAB completion puts a final space
	 * after file names */
	size_t path_len = strlen(new_path);
	if (new_path[path_len - 1] == ' ')
		new_path[path_len - 1] = '\0';

	/* Dequote new path, if needed */
	if (strchr(new_path, '\\')) {
		char *tmp = dequote_str(new_path, 0);
		if (!tmp) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("le: %s: Error dequoting file\n"), new_path);
			free(new_path);
			return EXIT_FAILURE;
		}

		strcpy(new_path, tmp);
		free(tmp);
	}

	/* Check new_path existence and warn the user if it does not
	 * exist */
	if (lstat(new_path, &attr) == -1) {
		printf("'%s': %s\n", new_path, strerror(errno));
		char *answer = (char *)NULL;
		while (!answer) {
			answer = rl_no_hist(_("Relink as a broken symbolic link? [y/n] "));
			if (!answer)
				continue;
			if (!*answer) {
				free(answer);
				answer = (char *)NULL;
				continue;
			}

			if (*answer != 'y' && *answer != 'n') {
				free(answer);
				answer = (char *)NULL;
				continue;
			}

			if (answer[1]) {
				free(answer);
				answer = (char *)NULL;
				continue;
			}

			if (*answer == 'y') {
				free(answer);
				break;
			} else {
				free(answer);
				free(new_path);
				return EXIT_SUCCESS;
			}
		}
	}

	/* Finally, relink the symlink to new_path */
#if !defined(_BE_POSIX)
	char *cmd[] = {"ln", "-sfn", new_path, link, NULL};
#else
	char *cmd[] = {"ln", "-sf", new_path, link, NULL};
#endif /* _BE_POSIX */
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
		free(new_path);
		return EXIT_FAILURE;
	}

	real_path = realpath(link, NULL);
	printf(_("%s%s%s successfully relinked to "), real_path ? ln_c
			: or_c, link, df_c);
	colors_list(new_path, NO_ELN, NO_PAD, PRINT_NEWLINE);
	free(new_path);
	if (real_path)
		free(real_path);

	return EXIT_SUCCESS;
}

/* Launch the command associated to 'c' (also 'v' and 'vv') or 'm'
 * internal commands */
int
copy_function(char **args, int copy_and_rename)
{
	log_function(NULL);

	if (*args[0] == 'm' && args[1]) {
		size_t len = strlen(args[1]);
		if (args[1][len - 1] == '/')
			args[1][len - 1] = '\0';
	}

	if (!is_sel)
		return run_and_refresh(args);

	size_t n = 0;
	char **tcmd = (char **)xnmalloc(2 + args_n + 2, sizeof(char *));
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

	size_t i;
	for (i = 1; args[i]; i++) {
		p = dequote_str(args[i], 0);
		if (!p)
			continue;
		tcmd[n] = savestring(p, strlen(p));
		free(p);
		n++;
	}

	if (sel_is_last) {
		tcmd[n] = savestring(".", 1);
		n++;
	}

	tcmd[n] = (char *)NULL;

	int ret = launch_execve(tcmd, FOREGROUND, E_NOFLAG);

	for (i = 0; tcmd[i]; i++)
		free(tcmd[i]);
	free(tcmd);

	if (ret != EXIT_SUCCESS)
		return ret;

	if (copy_and_rename) { /* vv */
		char **tmp = (char **)xnmalloc(sel_n + 3, sizeof(char *));
		tmp[0] = savestring("br", 2);

		size_t j;
		for (j = 0; j < sel_n; j++) {
			size_t arg_len = strlen(sel_elements[j].name);

			if (sel_elements[j].name[arg_len - 1] == '/')
				sel_elements[j].name[arg_len - 1] = '\0';

			if (*args[args_n] == '~') {
				char *exp_dest = tilde_expand(args[args_n]);
				args[args_n] = xrealloc(args[args_n],
				    (strlen(exp_dest) + 1) * sizeof(char));
				strcpy(args[args_n], exp_dest);
				free(exp_dest);
			}

			size_t dest_len = strlen(args[args_n]);
			if (args[args_n][dest_len - 1] == '/')
				args[args_n][dest_len - 1] = '\0';

			char dest[PATH_MAX];
			strcpy(dest, (sel_is_last || strcmp(args[args_n], ".") == 0)
					 ? workspaces[cur_ws].path : args[args_n]);

			char *ret_val = strrchr(sel_elements[j].name, '/');
			char *tmp_str = (char *)xnmalloc(strlen(dest)
					+ strlen(ret_val + 1) + 2, sizeof(char));

			sprintf(tmp_str, "%s/%s", dest, ret_val + 1);

			tmp[j + 1] = savestring(tmp_str, strlen(tmp_str));
			free(tmp_str);
		}

		tmp[j + 1] = (char *)NULL;
		bulk_rename(tmp);

		for (i = 0; tmp[i]; i++)
			free(tmp[i]);
		free(tmp);
		return EXIT_SUCCESS;
	}

	/* If 'mv sel' and command is successful deselect everything,
	 * since sel files are note there anymore */
	if (*args[0] == 'm' && args[0][1] == 'v'
	&& (!args[0][2] || args[0][2] == ' '))
		deselect_all();

#if defined(__HAIKU__)// || defined(__APPLE__)
	if (autols == 1)
		reload_dirlist();
#endif

	return EXIT_SUCCESS;
}

/* Print the list of files removed via the most recent call to the 'r' command */
static void
list_removed_files(char **cmd, const size_t start, const int cwd)
{
	size_t i, c = 0;
	for (i = start; cmd[i]; i++);
	char **removed_files = (char **)xnmalloc(i + 1, sizeof(char *));

	struct stat a;
	for (i = start; cmd[i]; i++) {
		if (lstat(cmd[i], &a) == -1 && errno == ENOENT) {
			removed_files[c] = cmd[i];
			c++;
		}
	}
	removed_files[c] = (char *)NULL;

	if (c == 0) { /* No file was removed */
		free(removed_files);
		return;
	}

	if (autols == 1 && cwd == 1)
		reload_dirlist();
	for (i = 0; i < c; i++) {
		if (!removed_files[i] || !*removed_files[i])
			continue;
		char *p = abbreviate_file_name(removed_files[i]);
		printf("%s\n", p ? p : removed_files[i]);
		if (p && p != removed_files[i])
			free(p);
	}
	print_reload_msg(_("%zu file(s) removed\n"), c);

	free(removed_files);
}

int
remove_file(char **args)
{
	int cwd = 0, exit_status = EXIT_SUCCESS, errs = 0;

	log_function(NULL);

	struct stat a;
	char **rm_cmd = (char **)xnmalloc(args_n + 4, sizeof(char *));
	int i, j = 3, dirs = 0;

	for (i = 1; args[i]; i++) {
		/* Check if at least one file is in the current directory. If not,
		 * there is no need to refresh the screen */
		if (cwd == 0)
			cwd = is_file_in_cwd(args[i]);

		char *tmp = (char *)NULL;
		if (strchr(args[i], '\\')) {
			tmp = dequote_str(args[i], 0);
			if (tmp) {
				/* Start storing file names in 3: 0 is for 'rm', and 1
				 * and 2 for parameters, including end of parameters (--) */
				if (lstat(tmp, &a) != -1) {
					rm_cmd[j] = savestring(tmp, strlen(tmp));
					j++;
					if (S_ISDIR(a.st_mode))
						dirs = 1;
				} else {
					_err(ERR_NO_STORE, NOPRINT_PROMPT, "r: %s: %s\n", tmp, strerror(errno));
					errs++;
				}
				free(tmp);
			} else {
				_err(ERR_NO_STORE, NOPRINT_PROMPT, "r: %s: Error dequoting file name\n", args[i]);
				continue;
			}
		} else {
			if (lstat(args[i], &a) != -1) {
				rm_cmd[j] = savestring(args[i], strlen(args[i]));
				j++;
				if (S_ISDIR(a.st_mode))
					dirs = 1;
			} else {
				_err(ERR_NO_STORE, NOPRINT_PROMPT, "r: %s: %s\n", args[i], strerror(errno));
				errs++;
			}
		}
	}

	rm_cmd[j] = (char *)NULL;

	if (errs > 0 && j > 3) { /* If errors but at least one file was deleted */
		fputs("Press any key to continue... ", stdout);
		xgetchar();
	}

	if (j == 3) { /* No file to be deleted */
		free(rm_cmd);
		return EXIT_FAILURE;
	}

	rm_cmd[0] = savestring("rm", 2);
	if (dirs == 1)
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__) || defined(_BE_POSIX)
		rm_cmd[1] = savestring("-r", 2);
#else /* Linux and FreeBSD only */
		rm_cmd[1] = savestring("-dIr", 4);
#endif /* __NetBSD__ || __OpenBSD__ || __APPLE__ || _BE_POSIX */
	else
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__) || defined(_BE_POSIX)
		rm_cmd[1] = savestring("-f", 2);
#else /* Linux and FreeBSD only */
		rm_cmd[1] = savestring("-I", 2);
#endif /* __NetBSD__ || __OpenBSD__ || __APPLE__ || _BE_POSIX */
	rm_cmd[2] = savestring("--", 2);

	if (launch_execve(rm_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;
#if defined(__HAIKU__)
	else {
		if (cwd == 1 && autols == 1 && strcmp(args[1], "--help") != 0
		&& strcmp(args[1], "--version") != 0)
			reload_dirlist();
	}
#endif /* __HAIKU__ */

	if (is_sel && exit_status == EXIT_SUCCESS)
		deselect_all();

	if (print_removed_files == 1)
		list_removed_files(rm_cmd, 3, cwd);

	for (i = 0; rm_cmd[i]; i++)
		free(rm_cmd[i]);
	free(rm_cmd);

	return exit_status;
}

/* Rename a bulk of files (ARGS) at once. Takes files to be renamed
 * as arguments, and returns zero on success and one on error. The
 * procedude is quite simple: file names to be renamed are copied into
 * a temporary file, which is opened via the mime function and shown
 * to the user to modify it. Once the file names have been modified and
 * saved, modifications are printed on the screen and the user is
 * asked whether to perform the actual bulk renaming or not.
 * This bulk rename method, just because it is quite simple and
 * KISS, was taken from the fff filemanager. So, thanks fff! BTW, this method
 * is also implemented by ranger and nnn */
int
bulk_rename(char **args)
{
	if (!args || !args[1] || IS_HELP(args[1])) {
		puts(_(BULK_USAGE));
		return EXIT_SUCCESS;
	}

	log_function(NULL);

	int exit_status = EXIT_SUCCESS;

	char bulk_file[PATH_MAX];
	if (xargs.stealth_mode == 1)
		snprintf(bulk_file, PATH_MAX - 1, "%s/%s", P_tmpdir, TMP_FILENAME);
	else
		snprintf(bulk_file, PATH_MAX - 1, "%s/%s", tmp_dir, TMP_FILENAME);

	int fd = mkstemp(bulk_file);
	if (fd == -1) {
		_err('e', PRINT_PROMPT, "br: mkstemp: %s: %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i, arg_total = 0;
	FILE *fp = (FILE *)NULL;

#ifdef __HAIKU__
	fp = fopen(bulk_file, "w");
	if (!fp) {
		_err('e', PRINT_PROMPT, "br: fopen: %s: %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}
#endif

#ifndef __HAIKU__
	dprintf(fd, BULK_RENAME_TMP_FILE_HEADER);
#else
	fprintf(fp, BULK_RENAME_TMP_FILE_HEADER);
#endif

	struct stat attr;
	size_t counter = 0;
	/* Copy all files to be renamed to the bulk file */
	for (i = 1; args[i]; i++) {
		/* Dequote file name, if necessary */
		if (strchr(args[i], '\\')) {
			char *deq_file = dequote_str(args[i], 0);
			if (!deq_file) {
				_err(ERR_NO_STORE, NOPRINT_PROMPT, _("br: %s: Error "
					"dequoting file name\n"), args[i]);
				continue;
			}
			strcpy(args[i], deq_file);
			free(deq_file);
		}

		/* Resolve "./" and "../" */
		if (*args[i] == '.' && (args[i][1] == '/' || (args[i][1] == '.'
		&& args[i][2] == '/') ) ) {
			char *p = realpath(args[i], NULL);
			if (!p) {
				_err(ERR_NO_STORE, NOPRINT_PROMPT, "br: %s: %s\n", args[i],
					strerror(errno));
				continue;
			}
			free(args[i]);
			args[i] = p;
		}

		if (lstat(args[i], &attr) == -1) {
			fprintf(stderr, "br: %s: %s\n", args[i], strerror(errno));
			continue;
		} else {
			counter++;
		}

#ifndef __HAIKU__
		dprintf(fd, "%s\n", args[i]);
#else
		fprintf(fp, "%s\n", args[i]);
#endif
	}
#ifdef __HAIKU__
	fclose(fp);
#endif
	arg_total = i;
	close(fd);

	if (counter == 0) { /* No valid file name */
		if (unlinkat(fd, bulk_file, 0) == -1)
			_err('e', PRINT_PROMPT, "br: unlinkat: %s: %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}

	fp = open_fstream_r(bulk_file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "br: %s: %s\n", bulk_file, strerror(errno));
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
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "br: %s\n",
			errno != 0 ? strerror(errno) : "Error opening temporary file");
		if (unlinkat(fd, bulk_file, 0) == -1)
			_err('e', PRINT_PROMPT, "br: unlinkat: %s: %s\n", bulk_file, strerror(errno));
		close_fstream(fp, fd);
		return EXIT_FAILURE;
	}

	close_fstream(fp, fd);
	fp = open_fstream_r(bulk_file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "br: %s: %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Compare the new modification time to the stored one: if they
	 * match, nothing was modified */
	fstat(fd, &attr);
	if (mtime_bfr == (time_t)attr.st_mtime) {
		puts(_("br: Nothing to do"));
		if (unlinkat(fd, bulk_file, 0) == -1) {
			_err('e', PRINT_PROMPT, "br: unlinkat: %s: %s\n", bulk_file, strerror(errno));
			exit_status = EXIT_FAILURE;
		}
		close_fstream(fp, fd);
		return exit_status;
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
		fputs(_("br: Line mismatch in renaming file\n"), stderr);
		if (unlinkat(fd, bulk_file, 0) == -1)
			_err('e', PRINT_PROMPT, "br: unlinkat: %s: %s\n", bulk_file, strerror(errno));
		close_fstream(fp, fd);
		return EXIT_FAILURE;
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
	if (!modified) {
		puts(_("br: Nothing to do"));
		if (unlinkat(fd, bulk_file, 0) == -1) {
			_err('e', PRINT_PROMPT, "br: unlinkat: %s: %s\n", bulk_file, strerror(errno));
			exit_status = EXIT_FAILURE;
		}
		free(line);
		close_fstream(fp, fd);
		return exit_status;
	}

	/* Ask the user for confirmation */
	char *answer = (char *)NULL;
	while (!answer) {
		answer = rl_no_hist(_("Continue? [y/N] "));
		if (answer && *answer && strlen(answer) > 1) {
			free(answer);
			answer = (char *)NULL;
			continue;
		}

		if (!answer) {
			free(line);
			close_fstream(fp, fd);
			return EXIT_SUCCESS;
		}

		switch (*answer) {
		case 'y': /* fallthrough */
		case 'Y': break;

		case 'n': /* fallthrough */
		case 'N': /* fallthrough */
		case '\0':
			free(answer);
			free(line);
			close_fstream(fp, fd);
			return EXIT_SUCCESS;

		default:
			free(answer);
			answer = (char *)NULL;
			break;
		}
	}

	free(answer);

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
			if (renameat(AT_FDCWD, args[i], AT_FDCWD, line) == -1)
				exit_status = EXIT_FAILURE;
		}

		i++;
	}

	free(line);

	if (unlinkat(fd, bulk_file, 0) == -1) {
		_err('e', PRINT_PROMPT, "br: unlinkat: %s: %s\n", bulk_file, strerror(errno));
		exit_status = EXIT_FAILURE;
	}
	close_fstream(fp, fd);

#if defined(__HAIKU__)// || defined(__APPLE__)
	if (autols == 1)
		reload_dirlist();
#endif

	return exit_status;
}

/* Export files in CWD (if FILENAMES is NULL), or files in FILENAMES,
 * into a temporary file. Return the address of this empt file if
 * success (it must be freed) or NULL in case of error */
char *export(char **filenames, int open)
{
	char *tmp_file = (char *)xnmalloc(strlen(tmp_dir) + 14, sizeof(char));
	sprintf(tmp_file, "%s/%s", tmp_dir, TMP_FILENAME);

	int fd = mkstemp(tmp_file);
	if (fd == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "exp: %s: %s\n", tmp_file, strerror(errno));
		free(tmp_file);
		return (char *)NULL;
	}
	
	size_t i;
#ifdef __HAIKU__
	FILE *fp = fopen(tmp_file, "w");
	if (!fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "exp: %s: %s\n", tmp_file, strerror(errno));
		free(tmp_file);
		return (char *)NULL;
	}
#endif

	/* If no argument, export files in CWD */
	if (!filenames[1]) {
		for (i = 0; file_info[i].name; i++)
#ifndef __HAIKU__
			dprintf(fd, "%s\n", file_info[i].name);
#else
			fprintf(fp, "%s\n", file_info[i].name);
#endif
	} else {
		for (i = 1; filenames[i]; i++) {
			if (*filenames[i] == '.' && (!filenames[i][1] || (filenames[i][1] == '.' && !filenames[i][2])))
				continue;
#ifndef __HAIKU__
			dprintf(fd, "%s\n", filenames[i]);
#else
			fprintf(fp, "%s\n", filenames[i]);
#endif
		}
	}
#ifdef __HAIKU__
	fclose(fp);
#endif
	close(fd);

	if (!open)
		return tmp_file;

	int ret = open_file(tmp_file);
	if (ret == EXIT_SUCCESS) {
		return tmp_file;
	} else {
		free(tmp_file);
		return (char *)NULL;
	}
}

int
batch_link(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1] || IS_HELP(args[1])) {
		puts(_(BL_USAGE));
		return EXIT_SUCCESS;
	}

	log_function(NULL);

	puts("Suffix defaults to '.link'");
	char *suffix = rl_no_hist(_("Enter links suffix ('q' to quit): "));

	if (suffix && *suffix == 'q' && !*(suffix + 1)) {
		free(suffix);
		return EXIT_SUCCESS;
	}

	size_t i;
	int exit_status = EXIT_SUCCESS;
	char tmp[NAME_MAX];

	for (i = 1; args[i]; i++) {
		char *linkname = (char *)NULL;

		if (!suffix || !*suffix) {
			snprintf(tmp, NAME_MAX, "%s.link", args[i]);
			linkname = tmp;
		} else {
			snprintf(tmp, NAME_MAX, "%s%s", args[i], suffix);
			linkname = tmp;
		}

		char *ptr = strrchr(linkname, '/');
		if (symlinkat(args[i], AT_FDCWD, ptr ? ++ptr : linkname) == -1) {
			exit_status = EXIT_FAILURE;
			_err(ERR_NO_STORE, NOPRINT_PROMPT, _("bl: symlinkat: %s: Cannot create "
				"symlink: %s\n"), ptr ? ptr : linkname, strerror(errno));
		}
	}

#if defined(__HAIKU__)// || defined(__APPLE__)
	if (exit_status == EXIT_SUCCESS && autols) {
		free_dirlist();

		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}
#endif

	free(suffix);
	return exit_status;
}
