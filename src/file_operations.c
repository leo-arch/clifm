/* file_operations.c -- control multiple file operations */

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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#ifdef __TINYC__
# undef CHAR_MAX /* Silence redefinition error */
#endif /* __TINYC__ */

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "file_operations.h"
#include "history.h"
#include "init.h" /* get_sel_files() */
#include "listing.h"
#include "messages.h"
#include "mime.h"
#include "misc.h"
#include "navigation.h"
#include "readline.h"
#include "selection.h"
#include "spawn.h"

/* Struct to store information about files to be removed via the 'r' command. */
struct rm_info {
	char   *name;
	nlink_t links;
#if defined(__sun) || defined(__OpenBSD__) || defined(__DragonFly__) \
|| defined(__NetBSD__) || defined(__HAIKU__)
	int     pad0;
#endif /* __sun || __OpenBSD__ || __DragonFly__ || __NetBSD__ || __HAIKU__ */
	time_t  mtime;
	ino_t   ino;
	dev_t   dev;
	int     dir;
	int     exists;
#if defined(__OpenBSD__) || defined(__DragonFly__) || defined(__HAIKU__)
	int     pad1;
#endif /* __OpenBSD__ || __DragonFly__ || __HAIKU__ */
};

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
		const mode_t old_umask = umask(0);
		printf("%04o\n", old_umask);
		umask(old_umask);
		return FUNC_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(UMASK_USAGE);
		return FUNC_SUCCESS;
	}

	if (!IS_DIGIT(*arg) || !is_number(arg))
		goto ERROR;

	int new_umask = read_octal(arg);
	if (new_umask < 0 || new_umask > MAX_UMASK)
		goto ERROR;

	umask((mode_t)new_umask);
	printf(_("File-creation mask set to '%04o'\n"), new_umask);
	return FUNC_SUCCESS;

ERROR:
	xerror(_("umask: %s: Out of range (valid values are 000-777)\n"), arg);
	return FUNC_FAILURE;
}

#ifndef _NO_LIRA
static int
run_mime(char *file)
{
	if (!file || !*file)
		return FUNC_FAILURE;

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
 * on success or one on failure. */
int
open_file(char *file)
{
	if (!file || !*file)
		return FUNC_FAILURE;

	int ret = FUNC_SUCCESS;

	if (conf.opener) {
		if (*conf.opener == 'g' && strcmp(conf.opener, "gio") == 0) {
			char *cmd[] = {"gio", "open", file, NULL};
			ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
		} else {
			char *cmd[] = {conf.opener, file, NULL};
			ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
		}
	} else {
#ifndef _NO_LIRA
		ret = run_mime(file);
#else
		/* Fallback to (xdg-)open */
# if defined(__HAIKU__)
		char *cmd[] = {"open", file, NULL};
# elif defined(__APPLE__)
		char *cmd[] = {"/usr/bin/open", file, NULL};
# else
		char *cmd[] = {"xdg-open", file, NULL};
# endif /* __HAIKU__ */
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
#endif /* _NO_LIRA */
	}

	return ret;
}

int
xchmod(const char *file, const char *mode_str, const int flag)
{
	if (!file || !*file) {
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			_("xchmod: Empty buffer for file name\n"));
		return FUNC_FAILURE;
	}

	if (!mode_str || !*mode_str) {
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			_("xchmod: Empty buffer for mode\n"));
		return FUNC_FAILURE;
	}

	const int fd = open(file, O_RDONLY);
	if (fd == -1) {
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			"xchmod: '%s': %s\n", file, strerror(errno));
		return errno;
	}

	const mode_t mode = (mode_t)strtol(mode_str, 0, 8);
	if (fchmod(fd, mode) == -1) {
		close(fd);
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			"xchmod: '%s': %s\n", file, strerror(errno));
		return errno;
	}

	close(fd);
	return FUNC_SUCCESS;
}

/* Toggle executable bits on the file named FILE. */
int
toggle_exec(const char *file, mode_t mode)
{
	/* Set it only for owner, unset it for every one else. */
	(0100 & mode) ? (mode &= (mode_t)~0111) : (mode |= 0100);

	if (fchmodat(XAT_FDCWD, file, mode, 0) == -1) {
		xerror(_("te: Changing permissions of '%s': %s\n"),
			file, strerror(errno));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
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
			const int n = atoi(dir);
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
		xerror("dup: '%s': %s\n", dir, strerror(errno));
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
		return FUNC_SUCCESS;
	}

	char *dest_dir = get_dup_file_dest_dir();
	if (!dest_dir)
		return FUNC_SUCCESS;

	size_t dlen = strlen(dest_dir);
	if (dlen > 1 && dest_dir[dlen - 1] == '/') {
		dest_dir[dlen - 1] = '\0';
		dlen--;
	}

	char *rsync_path = get_cmd_path("rsync");
	int exit_status = FUNC_SUCCESS;

	size_t i;
	for (i = 1; cmd[i]; i++) {
		if (!cmd[i] || !*cmd[i])
			continue;

		/* 1. Construct file names. */
		char *source = cmd[i];
		if (strchr(source, '\\')) {
			char *deq_str = unescape_str(source, 0);
			if (!deq_str) {
				xerror(_("dup: '%s': Error unescaping file name\n"), source);
				continue;
			}

			xstrsncpy(source, deq_str, strlen(deq_str) + 1);
			free(deq_str);
		}

		/* Use source as destiny file name: source.copy, and, if already
		 * exists, source.copy-n, where N is an integer greater than zero. */
		const size_t source_len = strlen(source);
		int rem_slash = 0;
		if (strcmp(source, "/") != 0 && source_len > 0
		&& source[source_len - 1] == '/') {
			source[source_len - 1] = '\0';
			rem_slash = 1;
		}

		char *tmp = strrchr(source, '/');
		char *source_name;

		source_name = (tmp && *(tmp + 1)) ? tmp + 1 : source;

		char tmp_dest[PATH_MAX + 1];
		if (*dest_dir == '/' && !dest_dir[1]) /* Root dir */
			snprintf(tmp_dest, sizeof(tmp_dest), "/%s.copy", source_name);
		else
			snprintf(tmp_dest, sizeof(tmp_dest), "%s/%s.copy",
				dest_dir, source_name);

		char bk[PATH_MAX + 11];
		xstrsncpy(bk, tmp_dest, sizeof(bk));
		struct stat attr;
		size_t suffix = 1;
		while (stat(bk, &attr) == FUNC_SUCCESS) {
			snprintf(bk, sizeof(bk), "%s-%zu", tmp_dest, suffix);
			suffix++;
		}
		char *dest = savestring(bk, strnlen(bk, sizeof(bk)));

		if (rem_slash == 1)
			source[source_len - 1] = '/';

		/* 2. Run command. */
		if (rsync_path) {
			char *dup_cmd[] = {"rsync", "-aczvAXHS", "--progress", "--",
				source, dest, NULL};
			if (launch_execv(dup_cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
				exit_status = FUNC_FAILURE;
		} else {
#ifdef _BE_POSIX
			char *dup_cmd[] = {"cp", "--", source, dest, NULL};
#elif defined(__sun)
			int g = (bin_flags & BSD_HAVE_COREUTILS);
			char *name = g ? "gcp" : "cp";
			char *opt = g ? "-a" : "--";
			char *dup_cmd[] = {name, opt, g ? "--" : source,
				g ? source : dest, g ? dest : NULL, NULL};
#else
			char *dup_cmd[] = {"cp", "-a", "--", source, dest, NULL};
#endif /* _BE_POSIX */
			if (launch_execv(dup_cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
				exit_status = FUNC_FAILURE;
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
 * Returns FUNC_SUCCESS on success or FUNC_FAILURE on error. */
static int
create_file(char *name, const int is_md)
{
	struct stat a;
	char *ret = (char *)NULL;
	char *n = name;
	char *errname = is_md == 1 ? "md" : "new";
	int status = FUNC_SUCCESS;

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
			xerror("%s: '%s': %s\n", errname, name, strerror(errno));
			status = FUNC_FAILURE;
			break;
		}

CONT:
		*ret = '/';
		n = ret + 1;
	}

	if (*n && status != FUNC_FAILURE) { /* Regular file */
		/* Regular file creation mode (666, or 600 in secure-mode). open(2)
		 * will modify this according to the current umask value. */
		if (xargs.secure_env == 1 || xargs.secure_env_full == 1)
			mode = S_IRUSR | S_IWUSR;
		else
			mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

		const int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, mode);
		if (fd == -1) {
			xerror("%s: '%s': %s\n", errname, name, strerror(errno));
			status = FUNC_FAILURE;
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

	print_reload_msg(_("%zu file(s) created\n"), (size_t)nfiles_n);
}

static int
is_range(const char *str)
{
	if (!str || !*str)
		return 0;

	char *p = strchr(str, '-');
	if (!p)
		return 0;

	*p = '\0';
	if (is_number(str) && (!p[1] || is_number(p + 1))) {
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
	if ((*n > '0' && is_number(n)) || is_range(n)) {
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
	if (is_portable_filename(name, (size_t)(s - name)) != FUNC_SUCCESS) {
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
validate_filename(char **name, const int is_md)
{
	if (!*name || !*(*name))
		return 0;

	char *deq = unescape_str(*name, 0);
	if (!deq) {
		xerror(_("%s: '%s': Error unescaping file name\n"),
			is_md ? "md" : "new", *name);
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
		return FUNC_FAILURE;

	const size_t flen = strlen(p);
	const int is_dir = (flen > 1 && p[flen - 1] == '/') ? 1 : 0;
	if (is_dir == 1)
		p[flen - 1 ] = '\0'; /* Remove ending slash */

	char *npath = (char *)NULL;
	if (p == *name) {
		char *tilde = (char *)NULL;
		if (*p == '~')
			tilde = tilde_expand(p);

		const size_t len = tilde ? strlen(tilde) : flen;
		npath = normalize_path(tilde ? tilde : p, len);
		free(tilde);

	} else { /* Quoted string. Copy it verbatim. */
		npath = savestring(p, flen);
	}

	if (!npath)
		return FUNC_FAILURE;

	const size_t name_len = strlen(npath) + 2;
	*name = xnrealloc(*name, name_len, sizeof(char));
	snprintf(*name, name_len, "%s%c", npath, is_dir == 1 ? '/' : 0);
	free(npath);

	return FUNC_SUCCESS;
}

static int
err_file_exists(char *name, const int multi, const int is_md)
{
	char *n = abbreviate_file_name(name);
	char *p = n ? n : name;

	xerror("%s: '%s': %s\n", is_md ? "md" : "new",
		(*p == '.' && p[1] == '/' && p[2]) ? p + 2 : p, strerror(EEXIST));

	if (n && n != name)
		free(n);

	if (multi == 1)
		press_any_key_to_continue(0);

	return FUNC_FAILURE;
}

/* Ask the user for a new file name and create the file. */
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
		return FUNC_SUCCESS;

	if (validate_filename(&filename, 0) == 0) {
		xerror(_("new: '%s': Unsafe file name\n"), filename);
		if (rl_get_y_or_n(_("Continue? [y/n] ")) == 0) {
			free(filename);
			return FUNC_SUCCESS;
		}
	}

	int exit_status = quoted == 0 ? format_new_filename(&filename)
		: FUNC_SUCCESS;
	if (exit_status != FUNC_SUCCESS)
		goto ERROR;

	struct stat a;
	if (lstat(filename, &a) == 0) {
		exit_status = err_file_exists(filename, 0, 0);
		goto ERROR;
	}

	exit_status = create_file(filename, 0);
	if (exit_status == FUNC_SUCCESS) {
		char *f[] = { filename, (char *)NULL };
		list_created_files(f, 1);
	}

ERROR:
	free(filename);
	return exit_status;
}

/* (l)stat(2), just as access(2), sees "file" and "file/" as different
 * file names. So, let's check the existence of the file FILE ignoring the
 * trailing slash, if any. */
static int
check_file_existence(char *file)
{
	int s = 0;
	const size_t l = strlen(file);
	if (l > 0 && file[l - 1] == '/') {
		file[l - 1] = '\0';
		s = 1;
	}

	struct stat a;
	const int ret = lstat(file, &a);

	if (s == 1)
		file[l - 1] = '/';

	return ret;
}

/* Create files as specified in ARGS: as directories (if ending with slash)
 * or as regular files otherwise. If coming from 'md' command, IS_MD is set to
 * 1 (so that we can fail as such). */
int
create_files(char **args, const int is_md)
{
	if (args[0] && IS_HELP(args[0])) {
		puts(_(NEW_USAGE));
		return FUNC_SUCCESS;
	}

	int exit_status = FUNC_SUCCESS;
	size_t i;

	/* If no argument provided, ask the user for a filename, create it and exit. */
	if (!args[0])
		/* This function is never executed by 'md', but always by 'n'. */
		return ask_and_create_file();

	size_t argsn;
	for (argsn = 0; args[argsn]; argsn++);

	/* Store pointers to actually created files into a pointers array.
	 * We'll use this later to print the names of actually created files. */
	char **new_files = xnmalloc(argsn + 1, sizeof(char *));
	filesn_t new_files_n = 0;

	for (i = 0; args[i]; i++) {
		if (validate_filename(&args[i], is_md) == 0) {
			xerror(_("%s: '%s': Unsafe file name\n"),
				is_md ? "md" : "new", args[i]);
			if (rl_get_y_or_n(_("Continue? [y/n] ")) == 0)
				continue;
		}

		/* Properly format filename. */
		if (format_new_filename(&args[i]) == FUNC_FAILURE) {
			exit_status = FUNC_FAILURE;
			continue;
		}

		/* Skip existent files. */
		if (check_file_existence(args[i]) == 0) {
			exit_status = err_file_exists(args[i], 0, is_md);
			continue;
		}

		const int ret = create_file(args[i], is_md);
		if (ret == FUNC_SUCCESS) {
			new_files[new_files_n] = args[i];
			new_files_n++;
		} else {
			exit_status = ret;
		}
	}

	new_files[new_files_n] = (char *)NULL;

	if (new_files_n > 0) {
		if (exit_status != FUNC_SUCCESS && conf.autols == 1)
			press_any_key_to_continue(0);
		list_created_files(new_files, new_files_n);
	}

	free(new_files);
	return exit_status;
}

/* Create one directory for each name specified in ARGS. Parent dirs are
 * created if required. */
int
create_dirs(char **args)
{
	if (!args[0] || IS_HELP(args[0])) {
		puts(_(MD_USAGE));
		return FUNC_SUCCESS;
	}

	/* Append an ending slash to all names, so that create_files() will create
	 * them all as directories. */
	size_t i;
	for (i = 0; args[i]; i++) {
		const size_t len = strlen(args[i]);
		if (len > 0 && args[i][len - 1] == '/')
			continue;

		char *tmp = savestring(args[i], len);
		args[i] = xnrealloc(args[i], len + 2, sizeof(char));
		snprintf(args[i], len + 2, "%s/", tmp);
		free(tmp);
	}

	return create_files(args, 1);
}

/* FILE is a broken symbolic link (stat(2) failed). Err appropriately. */
static int
err_no_link(const char *file)
{
	const int saved_errno = errno;

	char target[PATH_MAX + 1]; *target = '\0';
	const ssize_t tlen =
		readlinkat(XAT_FDCWD, file, target, sizeof(target) - 1);
	if (tlen != -1)
		target[tlen] = '\0';

	xerror(_("open: '%s': Broken symbolic link to '%s'\n"), file,
		*target ? target : "???");

	return saved_errno;
}

int
open_function(char **cmd)
{
	if (!cmd)
		return FUNC_FAILURE;

	if (!cmd[1] || IS_HELP(cmd[1])) {
		puts(_(OPEN_USAGE));
		return FUNC_SUCCESS;
	}

	const char *const errname = "open";

	if (*cmd[0] == 'o' && (!cmd[0][1] || strcmp(cmd[0], "open") == 0)) {
		if (strchr(cmd[1], '\\')) {
			char *deq_path = unescape_str(cmd[1], 0);
			if (!deq_path) {
				xerror(_("%s: '%s': Error unescaping file name\n"),
					errname, cmd[1]);
				return FUNC_FAILURE;
			}

			xstrsncpy(cmd[1], deq_path, strlen(deq_path) + 1);
			free(deq_path);
		}
	}

	char *file = cmd[1];

	/* Check file existence. */
	struct stat attr;
	errno = 0;
	if (lstat(file, &attr) == -1) {
		xerror("%s: '%s': %s\n", errname, cmd[1], strerror(errno));
		return errno;
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
		const int ret = get_link_ref(file);
		if (ret == -1) {
			return err_no_link(file);
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
		xerror(_("%s: '%s' (%s): Cannot open file\nTry "
			"'APP FILE' or 'open FILE APP'\n"), errname, cmd[1], file_type);
		return FUNC_FAILURE;
	}

	/* At this point we know that the file to be openend is either a regular
	 * file or a symlink to a regular file. So, just open the file. */

	if (!cmd[2] || (*cmd[2] == '&' && !cmd[2][1]))
		return open_file(file);

	/* Some application was specified to open the file. Use it. */
	char *tmp_cmd[] = {cmd[2], file, NULL};
	const int ret =
		launch_execv(tmp_cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);

	if (ret == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	if (ret == E_NOEXEC) /* EACCESS && ENOEXEC */
		xerror("%s: %s: %s\n", errname, cmd[2], NOEXEC_MSG);
	else if (ret == E_NOTFOUND) /* ENOENT */
		xerror("%s: %s: %s\n", errname, cmd[2], NOTFOUND_MSG);
	else
		xerror("%s: %s: %s\n", errname, cmd[2], strerror(errno));

	return ret;
}

static char *
get_new_link_target(char *cur_target)
{
	puts(_("Edit target (Ctrl-d to quit)"));
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

	return new_target;
}

static void
print_current_target(char *target)
{
	fputs(_("Current target -> "), stdout);

	struct stat a;
	if (lstat(target, &a) != -1) {
		colors_list(target, NO_ELN, NO_PAD, PRINT_NEWLINE);
	} else if (*target) {
		printf(_("%s%s%s (broken link)\n"), uf_c, target, df_c);
	} else {
		puts(_("??? (broken link)"));
	}

	return;
}

/* Relink the symbolic link LINK to a new target. */
int
edit_link(char *link)
{
	if (!link || !*link || IS_HELP(link)) {
		puts(LE_USAGE);
		return FUNC_SUCCESS;
	}

	/* Dequote the file name, if necessary. */
	if (strchr(link, '\\')) {
		char *tmp = unescape_str(link, 0);
		if (!tmp) {
			xerror(_("le: '%s': Error unescaping file name\n"), link);
			return FUNC_FAILURE;
		}

		xstrsncpy(link, tmp, strlen(tmp) + 1);
		free(tmp);
	}

	const size_t len = strlen(link);
	if (len > 0 && link[len - 1] == '/')
		link[len - 1] = '\0';

	/* Check if we have a valid symbolic link */
	struct stat attr;
	if (lstat(link, &attr) == -1) {
		xerror("le: '%s': %s\n", link, strerror(errno));
		return FUNC_FAILURE;
	}

	if (!S_ISLNK(attr.st_mode)) {
		xerror(_("le: '%s': Not a symbolic link\n"), link);
		return FUNC_FAILURE;
	}

	/* Get file pointed to by symlink and report to the user */
	char target[PATH_MAX + 1]; *target = '\0';
	const ssize_t tlen = readlinkat(XAT_FDCWD, link, target, sizeof(target) - 1);
	if (tlen != -1)
		target[tlen] = '\0';

	print_current_target(target);

	char *new_path = get_new_link_target(target);
	if (new_path && strcmp(new_path, target) == 0) {
		free(new_path);
		puts(_("le: Nothing to do"));
		return (FUNC_SUCCESS);
	}

	if (!new_path) /* The user pressed C-d */
		return FUNC_SUCCESS;

	/* Check new_path existence and warn the user if it does not exist. */
	if (lstat(new_path, &attr) == -1) {
		xerror("'%s': %s\n", new_path, strerror(errno));
		if (rl_get_y_or_n(_("Relink as broken symbolic link? [y/n] ")) == 0) {
			free(new_path);
			return FUNC_SUCCESS;
		}
	}

	/* Finally, remove the link and recreate it as link to new_path. */
	if (unlinkat(XAT_FDCWD, link, 0) == -1
	|| symlinkat(new_path, XAT_FDCWD, link) == -1) {
		free(new_path);
		xerror(_("le: Cannot relink symbolic link '%s': %s\n"),
			link, strerror(errno));
		return FUNC_FAILURE;
	}

	printf(_("'%s' relinked to "), link);
	fflush(stdout);
	colors_list(new_path, NO_ELN, NO_PAD, PRINT_NEWLINE);
	free(new_path);

	return FUNC_SUCCESS;
}

/* Return the length of the longest common prefix of canonical PATH1
 * and PATH2, ensuring only full path components are matched.
 * Return 0 on no match. */
static int
path_common_prefix(char const *path1, char const *path2)
{
	int i = 0;
	int ret = 0;

	/* We already know path1[0] and path2[0] are '/'. Special case '//',
	 * which is only present in a canonical name on platforms where it
	 * is distinct. */
	if ((path1[1] == '/') != (path2[1] == '/'))
		return 0;

	while (*path1 && *path2) {
		if (*path1 != *path2)
			break;
		if (*path1 == '/')
			ret = i + 1;
		path1++;
		path2++;
		i++;
	}

	if ((!*path1 && !*path2) || (!*path1 && *path2 == '/')
	|| (!*path2 && *path1 == '/'))
		ret = i;

	return ret;
}

/* If *PBUF is not NULL then append STR to *PBUF and update *PBUF to point}
 * to the end of the buffer and adjust *PLEN to reflect the remaining space.
 * Return 0 on success and 1 on failure (no more buffer space). */
static int
append_to_buf(char const *str, char **pbuf, size_t *plen)
{
	if (*pbuf) {
		const size_t slen = strlen(str);
		if (slen >= *plen)
			return 1;

		memcpy(*pbuf, str, slen + 1);
		*pbuf += slen;
		*plen -= slen;
	}

	return 0;
}

/* Generate a link target for TARGET relative to LINK_NAME and copy it into
 * BUF, whose size is LEN.
 * Rerturn 0 on success and 1 on failure. */
static int
relpath(const char *target, const char *link_name, char *buf, size_t len)
{
	int buf_err = 0;

	/* Skip the prefix common to LINK_NAME and TARGET.  */
	int common_index = path_common_prefix(link_name, target);
	if (common_index == 0)
		return 0;

	const char *link_suffix = link_name + common_index;
	const char *target_suffix = target + common_index;

	/* Skip over extraneous '/'. */
	if (*link_suffix == '/')
		link_suffix++;
	if (*target_suffix == '/')
		target_suffix++;

	/* Replace remaining components of LINK_NAME with '..', to get
     to a common directory. Then append the remainder of TARGET.  */
	if (*link_suffix) {
		buf_err |= append_to_buf("..", &buf, &len);
		for (; *link_suffix; ++link_suffix) {
			if (*link_suffix == '/')
				buf_err |= append_to_buf("/..", &buf, &len);
		}

		if (*target_suffix) {
			buf_err |= append_to_buf("/", &buf, &len);
			buf_err |= append_to_buf(target_suffix, &buf, &len);
		}
	} else {
		buf_err |= append_to_buf(*target_suffix ? target_suffix : ".",
			&buf, &len);
	}

	if (buf_err == 1) {
		xerror(_("link: Error generating relative path: %s\n"),
			strerror(ENAMETOOLONG));
	}

	return buf_err;
}

static char *
gen_relative_target(char *link_name, char *target)
{
	char *norm_link = normalize_path(link_name, strlen(link_name));
	if (!norm_link) {
		xerror(_("link: '%s': Error normalizing path\n"), link_name);
		return (char *)NULL;
	}

	char *p = strrchr(norm_link, '/');
	if (p)
		*p = '\0';

	char *norm_target = normalize_path(target, strlen(target));
	if (!norm_target) {
		free(norm_link);
		xerror(_("link: '%s': Error normalizing path\n"), target);
		return (char *)NULL;
	}

	char *resolved_target = xnmalloc(PATH_MAX + 1, sizeof(char));
	const int ret = relpath(norm_target, norm_link, resolved_target,
		PATH_MAX + 1);

	free(norm_link);
	free(norm_target);

	if (ret != 0) {
		free(resolved_target);
		return (char *)NULL;
	}

	return resolved_target;
}

/* Create a symbolic link to ARGS[0] named ARGS[1]. If args[1] is not specified,
 * the link is created as target_basename.link.
 * Returns 0 in case of success, or 1 otherwise. */
int
symlink_file(char **args)
{
	if (!args[0] || !*args[0] || IS_HELP(args[0])) {
		puts(LINK_USAGE);
		return FUNC_SUCCESS;
	}

	size_t len = strlen(args[0]);
	if (len > 1 && args[0][len - 1] == '/')
		args[0][len - 1] = '\0';

	if (strchr(args[0], '\\')) {
		char *deq = unescape_str(args[0], 0);
		if (deq) {
			free(args[0]);
			args[0] = deq;
		}
	}

	if (args[1] && strchr(args[1], '\\')) {
		char *deq = unescape_str(args[1], 0);
		if (deq) {
			free(args[1]);
			args[1] = deq;
		}
	}

	char *target = args[0];
	char *link_name = args[1];
	char tmp[PATH_MAX + 1];
	struct stat a;

	if (!link_name || !*link_name) {
		char *p = strrchr(target, '/');
		snprintf(tmp, sizeof(tmp), "%s.link", (p && p[1]) ? p + 1 : target);

		int suffix = 1;
		while (lstat(tmp, &a) == 0 && suffix < INT_MAX) {
			snprintf(tmp, sizeof(tmp), "%s.link-%d",
				(p && p[1]) ? p + 1 : target, suffix);
			suffix++;
		}

		link_name = tmp;
	}

	len = strlen(link_name);
	if (len > 1 && link_name[len - 1] == '/')
		link_name[len - 1] = '\0';

	if (lstat(target, &a) == -1) {
		printf("link: '%s': %s\n", target, strerror(errno));
		if (rl_get_y_or_n(_("Create broken symbolic link? [y/n] ")) == 0)
			return FUNC_SUCCESS;
	}

	if (lstat(link_name, &a) != -1 && S_ISLNK(a.st_mode)) {
		printf("link: '%s': %s\n", link_name, strerror(EEXIST));
		if (rl_get_y_or_n(_("Overwrite this file? [y/n] ")) == 0)
			return FUNC_SUCCESS;

		if (unlinkat(XAT_FDCWD, link_name, 0) == -1) {
			xerror(_("link: Cannot unlink '%s': %s\n"),
				link_name, strerror(errno));
			return FUNC_FAILURE;
		}
	}

	char *resolved_target = target;

	switch (conf.link_creat_mode) {
	case LNK_CREAT_ABS:
		resolved_target = normalize_path(target, strlen(target)); break;
	case LNK_CREAT_REL:
		resolved_target = gen_relative_target(link_name, target); break;
	default: break;
	}

	if (!resolved_target)
		return FUNC_FAILURE;

	if (symlinkat(resolved_target, XAT_FDCWD, link_name) == -1) {
		xerror(_("link: Cannot create symbolic link '%s': %s\n"),
			link_name, strerror(errno));
		if (resolved_target != target)
			free(resolved_target);
		return FUNC_FAILURE;
	}

	if (resolved_target != target)
		free(resolved_target);

	return FUNC_SUCCESS;
}

static int
vv_rename_files(char **args)
{
	char **tmp = xnmalloc(args_n + 2, sizeof(char *));
	tmp[0] = savestring("br", 2);

	size_t i, l = strlen(args[args_n]), c = 1;

	if (l > 0 && args[args_n][l - 1] == '/')
		args[args_n][l - 1] = '\0';

	char *dest = args[args_n];

	for (i = 1; i < args_n && args[i]; i++) {
		l = strlen(args[i]);
		if (l > 0 && args[i][l - 1] == '/')
			args[i][l - 1] = '\0';

		char p[PATH_MAX + 1];
		char *s = strrchr(args[i], '/');
		snprintf(p, sizeof(p), "%s/%s", dest, (s && *(++s)) ? s : args[i]);

		tmp[c] = savestring(p, strnlen(p, sizeof(p)));
		c++;
	}

	tmp[c] = (char *)NULL;
	const int ret = bulk_rename(tmp);

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
		return FUNC_FAILURE;
	}

	struct stat a;
	if (stat(file, &a) == -1) {
		xerror("vv: '%s': %s\n", file, strerror(errno));
		return FUNC_FAILURE;
	}

	if (!S_ISDIR(a.st_mode)) {
		xerror("vv: '%s': %s\n", file, strerror(ENOTDIR));
		return FUNC_FAILURE;
	}

	if (strcmp(workspaces[cur_ws].path, file) == 0) {
		xerror("%s\n", _("vv: Destiny directory is the current directory"));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
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

/* Return 1 if at least one file is selected in the current directory.
 * Otherwise, 0 is returned. */
int
cwd_has_sel_files(void)
{
	filesn_t i = files;
	while (--i >= 0) {
		if (file_info[i].sel == 1)
			return 1;
	}

	return 0;
}

/* Run CMD (either cp(1) or mv(1)) via execv().
 * skip_force is true (1) when the -f,--force parameter has been provided to
 * either 'c' or 'm' commands: it intructs cp/mv to run non-interactivelly
 * (no -i). */
static int
run_cp_mv_cmd(char **cmd, const int skip_force)
{
	if (!cmd)
		return FUNC_FAILURE;

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
			char *p = unescape_str(cmd[1], 0);
			struct stat a;
			const int ret = lstat(p ? p : cmd[1], &a);
			free(p);
			if (ret == -1) {
				xrename = 0;
				xerror("m: %s: %s\n", cmd[1], strerror(errno));
				return errno;
			}
		}

		new_name = get_new_filename(cmd[1]);
		if (!new_name)
			return FUNC_SUCCESS;
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

	/* wcp(1) does not support end of options (--). */
	if (strcmp(cmd[0], "wcp") != 0) {
		tcmd[n] = savestring("--", 2);
		n++;
	}

	size_t i;
	for (i = 1; cmd[i]; i++) {
		/* The -f,--force parameter is internal. Skip it.
		 * It instructs cp/mv to run non-interactively (no -i param). */
		if (skip_force == 1 && i == 1 && is_force_param(cmd[i]) == 1)
			continue;
		p = unescape_str(cmd[i], 0);
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
	const int ret = launch_execv(tcmd, FOREGROUND, E_NOFLAG);

	for (i = 0; i < n; i++)
		free(tcmd[i]);
	free(tcmd);

	if (ret != FUNC_SUCCESS)
		return ret;

	/* Error messages are printed by launch_execv() itself. */

	/* If 'rm sel' and command is successful, deselect everything. */
	if (is_sel && *cmd[0] == 'r' && cmd[0][1] == 'm' && (!cmd[0][2]
	|| cmd[0][2] == ' ')) {
		int j = (int)sel_n;
		while (--j >= 0)
			free(sel_elements[j].name);
		sel_n = 0;
		save_sel();
	}

	if (sel_n > 0 && *cmd[0] == 'm' && cwd_has_sel_files())
		/* Just in case a selected file in the current dir was renamed. */
		get_sel_files();

#ifdef GENERIC_FS_MONITOR
	if (*cmd[0] == 'm')
		reload_dirlist();
#endif /* GENERIC_FS_MONITOR */

	return FUNC_SUCCESS;
}

/* Launch the command associated to 'c' (also 'v' and 'vv') or 'm'
 * internal commands. */
int
cp_mv_file(char **args, const int copy_and_rename, const int force)
{
	/* vv command */
	if (copy_and_rename == 1
	&& validate_vv_dest_dir(args[args_n]) == FUNC_FAILURE)
		return FUNC_FAILURE;

	if (*args[0] == 'm' && args[1]) {
		const size_t len = strlen(args[1]);
		if (len > 0 && args[1][len - 1] == '/')
			args[1][len - 1] = '\0';
	}

	if (is_sel == 0 && copy_and_rename == 0)
		return run_cp_mv_cmd(args, force);

	size_t n = 0;
	char **tcmd = xnmalloc(3 + args_n + 2, sizeof(char *));
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
		p = unescape_str(args[i], 0);
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

	const int ret = launch_execv(tcmd, FOREGROUND, E_NOFLAG);

	for (i = 0; tcmd[i]; i++)
		free(tcmd[i]);
	free(tcmd);

	if (ret != FUNC_SUCCESS)
		return ret;

	if (copy_and_rename == 1) /* vv command */
		return vv_rename_files(args);

	/* If 'mv sel' and command is successful deselect everything,
	 * since sel files are not there anymore. */
	if (*args[0] == 'm' && args[0][1] == 'v'
	&& (!args[0][2] || args[0][2] == ' '))
		deselect_all();

	return FUNC_SUCCESS;
}

/* Print the file name FNAME, quoted if it contains an space.
 * A slash is appended if FNAME is a directory (ISDIR >= 1). */
static void
print_file_name(char *fname, const int isdir)
{
	if (strchr(fname, ' ')) {
		if (strchr(fname, '\''))
			printf("\"%s%s\"\n", fname, isdir >= 1 ? "/" : "");
		else
			printf("'%s%s'\n", fname, isdir >= 1 ? "/" : "");
	} else {
		fputs(fname, stdout);
		puts(isdir >= 1 ? "/" : "");
	}
}

static void
print_removed_file_info(const struct rm_info info)
{
	char *p = abbreviate_file_name(info.name);

	print_file_name(p ? p : info.name, info.dir);

	/* Name removed, but file is still linked to another name (hardlink) */
	if (info.dir == 0 && info.links > 1) {
		const nlink_t l = info.links - 1;
		xerror(_("r: '%s': File may still exist (%jd more "
			"%s linked to this file before this operation)\n"), info.name,
			(intmax_t)l, l > 1 ? _("names were") : _("name was"));
	}

	if (p && p != info.name)
		free(p);
}

/* Print the list of files removed via the most recent call to the 'r' command */
static void
list_removed_files(struct rm_info *info, const size_t start,
	const int cwd)
{
	size_t i, c = 0;

	struct stat a;
	for (i = start; info[i].name; i++) {
		if (lstat(info[i].name, &a) == -1 && errno == ENOENT) {
			info[i].exists = 0;
			c++;
		}
	}

	if (c == 0) /* No file was removed */
		return;

	if (conf.autols == 1 && cwd == 1)
		reload_dirlist();

	for (i = start; info[i].name; i++) {
		if (!info[i].name || !*info[i].name || info[i].exists == 1)
			continue;

		print_removed_file_info(info[i]);
	}

	print_reload_msg(_("%zu file(s) removed\n"), c);
}

/* Print files to be removed and ask the user for confirmation.
 * Returns 0 if no or 1 if yes. */
static int
rm_confirm(struct rm_info *info, const size_t start, const int have_dirs)
{
	printf(_("File(s) to be removed%s:\n"),
		have_dirs == 1 ? _(" (recursively)") : "");

	size_t i;
	for (i = start; info[i].name; i++)
		print_file_name(info[i].name, info[i].dir);

	return rl_get_y_or_n(_("Continue? [y/n] "));
}

static int
check_rm_files(struct rm_info *info, const size_t start, const char *errname)
{
	struct stat a;
	size_t i;
	int ret = FUNC_SUCCESS;

	for (i = start; info[i].name; i++) {
		if (lstat(info[i].name, &a) == -1)
			continue;

		if (info[i].mtime != a.st_mtime || info[i].dev != a.st_dev
		|| info[i].ino != a.st_ino) {
			xerror(_("%s: '%s': File changed on disk!\n"),
				errname, info[i].name);
			ret = FUNC_FAILURE;
		}
	}

	if (ret == FUNC_FAILURE) {
		return (rl_get_y_or_n(_("Remove files anyway? [y/n] ")) == 0
			? FUNC_FAILURE : FUNC_SUCCESS);
	}

	return ret;
}

static struct rm_info
fill_rm_info_struct(char **filename, struct stat *a)
{
	struct rm_info info;

	info.name = *filename;
	info.dir = (S_ISDIR(a->st_mode));
	info.links = a->st_nlink;
	info.mtime = a->st_mtime;
	info.dev = a->st_dev;
	info.ino = a->st_ino;
	info.exists = 1;

	return info;
}

int
remove_files(char **args)
{
	int cwd = 0, exit_status = FUNC_SUCCESS, errs = 0;
	char *err_name = (args[0] && *args[0] == 'r' && args[0][1] == 'r')
		? "rr" : "r";

	int i;
	for (i = 0; args[i]; i++);
	const size_t num = i > 0 ? (size_t)i - 1 : (size_t)i;

	struct stat a;
	char **rm_cmd = xnmalloc(num + 4, sizeof(char *));
	/* Let's keep information about files to be removed. */
	struct rm_info *info = xnmalloc(num + 4, sizeof(struct rm_info));

	int j, have_dirs = 0;
	int rm_force = conf.rm_force == 1 ? 1 : 0;

	i = (is_force_param(args[1]) == 1) ? 2 : 1;
	if (i == 2)
		rm_force = 1;

	for (j = 3; args[i]; i++) {
		/* Let's start storing file names in 3: 0 is for 'rm', and 1
		 * and 2 for parameters, including end of parameters (--). */

		/* If we have a symlink to dir ending with a slash, stat(2) takes it
		 * as a directory, and then rm(1) complains that cannot remove it,
		 * because "Is a directory". So, let's remove the ending slash:
		 * stat(2) will take it as the symlink it is and rm(1) will remove
		 * the symlink (not the target) without complains. */
		const size_t len = strlen(args[i]);
		if (len > 0 && args[i][len - 1] == '/')
			args[i][len - 1] = '\0';

		/* Check if at least one file is in the current directory. If not,
		 * there is no need to refresh the screen. */
		if (cwd == 0)
			cwd = is_file_in_cwd(args[i]);

		char *tmp = unescape_str(args[i], 0);
		if (!tmp) {
			xerror(_("%s: '%s': Error unescaping file name\n"), err_name, args[i]);
			continue;
		}

		if (lstat(tmp, &a) != -1) {
			rm_cmd[j] = savestring(tmp, strlen(tmp));
			info[j] = fill_rm_info_struct(&rm_cmd[j], &a);
			if (info[j].dir == 1)
				have_dirs++;
			j++;
		} else {
			xerror("%s: '%s': %s\n", err_name, tmp, strerror(errno));
			errs++;
		}

		free(tmp);
	}

	rm_cmd[j] = info[j].name = (char *)NULL;

	if (j == 3) { /* No file to be deleted */
		free(rm_cmd);
		free(info);
		return FUNC_FAILURE;
	}

	if (rm_force == 1 && errs > 0 && j > 3 && conf.autols == 1)
		press_any_key_to_continue(0);

	if (rm_force == 0 && rm_confirm(info, 3, have_dirs) == 0)
		goto END;

	/* Make sure that files to be removed have not changed between the
	 * beginning of the operation and the user confirmation. */
	if (check_rm_files(info, 3, err_name) == FUNC_FAILURE)
		goto END;

	rm_cmd[0] = "rm";
	rm_cmd[1] = have_dirs >= 1 ? "-rf" : "-f";
	rm_cmd[2] = "--";

	exit_status = launch_execv(rm_cmd, FOREGROUND, E_NOFLAG);
	if (exit_status != FUNC_SUCCESS) {
#ifndef BSD_KQUEUE
		if (num > 1 && conf.autols == 1) /* Only if we have multiple files */
#else
		/* Kqueue refreshes the screen even if there was only one file
		 * to be modified and it failed. */
		if (conf.autols == 1)
#endif /* !BSD_KQUEUE */
			press_any_key_to_continue(0);
	}

	if (is_sel && exit_status == FUNC_SUCCESS)
		deselect_all();

	if (print_removed_files == 1)
		list_removed_files(info, 3, cwd);

END:
	for (i = 3; rm_cmd[i]; i++)
		free(rm_cmd[i]);
	free(rm_cmd);

	free(info);
	return exit_status;
}

/* Export files in CWD (if FILENAMES is NULL), or files in FILENAMES,
 * into a temporary file. Return the address of this empty file if
 * success (it must be freed) or NULL in case of error. */
char *
export_files(char **filenames, const int open)
{
	const size_t len = strlen(tmp_dir) + (sizeof(TMP_FILENAME) - 1) + 2;
	char *tmp_file = xnmalloc(len, sizeof(char));
	snprintf(tmp_file, len, "%s/%s", tmp_dir, TMP_FILENAME);

	const int fd = mkstemp(tmp_file);
	if (fd == -1) {
		xerror("exp: '%s': %s\n", tmp_file, strerror(errno));
		free(tmp_file);
		return (char *)NULL;
	}

	size_t i;
	FILE *fp = fdopen(fd, "w");
	if (!fp) {
		xerror("exp: '%s': %s\n", tmp_file, strerror(errno));
		if (unlinkat(fd, tmp_file, 0) == -1)
			xerror("exp: unlink: '%s': %s\n", tmp_file, strerror(errno));
		close(fd);
		free(tmp_file);
		return (char *)NULL;
	}

	/* If no argument, export files in CWD. */
	if (!filenames[1]) {
		char buf[PATH_MAX + 1];
		for (i = 0; file_info[i].name; i++) {
			char *name = file_info[i].name;
			if (virtual_dir == 1) {
				*buf = '\0';
				if (xreadlink(XAT_FDCWD, name, buf,	sizeof(buf)) == -1
				|| !*buf)
					continue;
				name = buf;
			}

			if (!name || !*name)
				continue;

			fprintf(fp, "%s\n", name);
		}
	} else {
		for (i = 1; filenames[i]; i++) {
			if (SELFORPARENT(filenames[i]))
				continue;

			fprintf(fp, "%s\n", filenames[i]);
		}
	}

	fclose(fp);

	if (open == 0)
		return tmp_file;

	const int ret = open_file(tmp_file);
	if (ret == FUNC_SUCCESS)
		return tmp_file;

	if (unlink(tmp_file) == -1)
		xerror("exp: unlink: '%s': %s\n", tmp_file, strerror(errno));
	free(tmp_file);
	return (char *)NULL;
}

/* Create a symlink in CWD for each file name in ARGS.
 * If the destiny file exists, a positive integer suffix is appended to
 * make the file name unique. */
int
batch_link(char **args)
{
	if (!args || !args[0] || IS_HELP(args[0])) {
		puts(_(BL_USAGE));
		return FUNC_SUCCESS;
	}

	size_t i;
	size_t symlinked = 0;
	int exit_status = FUNC_SUCCESS;

	for (i = 0; args[i]; i++) {
		char *filename = unescape_str(args[i], 0);
		if (!filename) {
			exit_status = FUNC_FAILURE;
			xerror(_("bl: '%s': Error unescaping name\n"), args[i]);
			continue;
		}

		/* Remove ending slash */
		const size_t l = strlen(filename);
		if (l > 1 && filename[l - 1] == '/')
			filename[l - 1] = '\0';

		struct stat a;
		if (lstat(filename, &a) == -1) {
			exit_status = errno;
			xerror("bl: '%s': %s\n", filename, strerror(errno));
			free(filename);
			continue;
		}

		char *s = strrchr(filename, '/');
		char *basename = s && *(++s) ? s : filename;

		/* + 64 = Make some room for suffix */
		const size_t tmp_len = strlen(basename) + 64;
		char *tmp = xnmalloc(tmp_len, sizeof(char));
		xstrsncpy(tmp, basename, tmp_len);

		size_t suffix = 1;
		while (lstat(tmp, &a) != -1) {
			snprintf(tmp, tmp_len, "%s-%zu", basename, suffix);
			suffix++;
		}

		if (symlinkat(filename, XAT_FDCWD, tmp) == -1) {
			exit_status = errno;
			xerror(_("bl: Cannot create symbolic link '%s': %s\n"),
				tmp, strerror(errno));
		} else {
			symlinked++;
		}

		free(filename);
		free(tmp);
	}

	if (conf.autols == 1 && symlinked > 0) {
		if (exit_status != FUNC_SUCCESS)
			press_any_key_to_continue(0);
		reload_dirlist();
	}
	print_reload_msg(_("%zu symbolic link(s) created\n"), symlinked);

	return exit_status;
}
