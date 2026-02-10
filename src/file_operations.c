/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* file_operations.c -- control multiple file operations */

/* path_common_prefix(), append_to_buf(), and relpath() were taken from
 * GNU coreutils (2024), licensed GPL3+, and modified to fit our needs.
 * Modified code is licensed GPL2+. */

#include "helpers.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#ifdef __TINYC__
# undef CHAR_MAX /* Silence redefinition error */
#endif /* __TINYC__ */

#include "aux.h" /* gen_default_answer, make_filename_unique */
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
#include "safe_names.h" /* validate_filename */
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

/* Print/set the file creation mode mask (umask). */
int
umask_function(const char *arg)
{
	if (!arg) {
		const mode_t old_umask = umask(0); /* flawfinder: ignore */
		/* umask is set to zero, and then changed back, only to get the
		 * current value. */
		printf("%04o\n", old_umask);
		umask(old_umask); /* flawfinder: ignore */
		return FUNC_SUCCESS;
	}

	if (IS_HELP(arg)) {
		puts(UMASK_USAGE);
		return FUNC_SUCCESS;
	}

	if (!IS_DIGIT(*arg) || !is_number(arg))
		goto ERROR;

	int new_umask = octal2int(arg);
	if (new_umask < 0 || new_umask > MAX_UMASK)
		goto ERROR;

	umask((mode_t)new_umask); /* flawfinder: ignore */
	printf(_("File-creation mask set to '%04o'\n"), new_umask);
	return FUNC_SUCCESS;

ERROR:
	xerror(_("umask: %s: Out of range (valid values are 000-777)\n"), arg);
	return FUNC_FAILURE;
}

/* Open a file via OPENER, if set, or via LIRA. If not compiled with
 * Lira support, fallback to open (Haiku), or xdg-open. Returns zero
 * on success or one on failure. */
int
open_file(char *file)
{
	if (!file || !*file)
		return FUNC_FAILURE;

	int ret = FUNC_SUCCESS;

	if (conf.opener && *conf.opener) {
		if (*conf.opener == 'g' && strcmp(conf.opener, "gio") == 0) {
			char *cmd[] = {"gio", "open", file, NULL};
			ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
		} else {
			char *cmd[] = {conf.opener, file, NULL};
			ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
		}
	} else {
#ifndef _NO_LIRA
		char *cmd[] = {"mime", "open", file, NULL};
		ret = mime_open(cmd);
#else
		/* Fallback to OS-specific openers */
# if defined(__HAIKU__)
		char *cmd[] = {"open", file, NULL};
# elif defined(__APPLE__)
		char *cmd[] = {"/usr/bin/open", file, NULL};
# elif defined(__CYGWIN__)
		char *cmd[] = {"cygstart", file, NULL};
# else
		char *cmd[] = {"xdg-open", file, NULL};
# endif /* __HAIKU__ */
		ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
#endif /* !_NO_LIRA */
	}

	return ret;
}

int
xchmod(const char *file, const char *mode_str, const int flag)
{
	if (!file || !*file) {
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			_("xchmod: Empty buffer for filename\n"));
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
		const int saved_errno = errno;
		close(fd);
		err(flag == 1 ? 'e' : 0, flag == 1 ? PRINT_PROMPT : NOPRINT_PROMPT,
			"xchmod: '%s': %s\n", file, strerror(saved_errno));
		return saved_errno;
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
	char *dir = NULL;

	puts(_("Enter destination directory (Ctrl+d to quit)\n"
		"Tip: \".\" for the current directory"));
	char n_prompt[NAME_MAX];
	snprintf(n_prompt, sizeof(n_prompt), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	while (!dir) {
		int quoted = 0;
		dir = get_newname(n_prompt, NULL, &quoted);
		UNUSED(quoted);
		if (!dir) /* The user pressed ctrl+d */
			return NULL;

		/* Expand ELN */
		if (IS_DIGIT(*dir) && is_number(dir)) {
			const int n = xatoi(dir);
			if (n > 0 && (filesn_t)n <= g_files_num) {
				free(dir);
				const char *name = file_info[n - 1].name;
				dir = savestring(name, strlen(name));
			}
		} else {
			if (*dir == '~') { /* Expand tilde */
				char *tmp = tilde_expand(dir);
				if (tmp) {
					free(dir);
					dir = tmp;
				}
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
		} else {
			if (check_file_access(a.st_mode, a.st_uid, a.st_gid) == 0) {
				errno = EACCES;
				goto ERROR;
			}
		}

		break;

ERROR:
		xerror("dup: '%s': %s\n", dir, strerror(errno));
		free(dir);
		dir = NULL;
	}

	return dir;
}

static char *
construct_dup_destination(char *source, const char *dest_dir)
{
	if (strchr(source, '\\')) {
		char *deq_str = unescape_str(source);
		if (!deq_str) {
			xerror(_("dup: '%s': Error unescaping filename\n"), source);
			return NULL;
		}

		/* An unescaped filename is always <= than the original filename,
		 * so that there's no need to reallocate the buffer. */
		xstrsncpy(source, deq_str, strlen(deq_str) + 1);
		free(deq_str);
	}

	/* Use source as destination filename: source.copy, and, if already
	 * exists, source.copy-n, where N is an integer greater than zero. */
	const size_t source_len = strlen(source);
	int rem_slash = 0;
	if (source_len > 1 && source[source_len - 1] == '/') {
		source[source_len - 1] = '\0';
		rem_slash = 1;
	}

	char *tmp = strrchr(source, '/');
	const char *source_name = (tmp && tmp[1]) ? tmp + 1 : source;

	char tmp_dest[PATH_MAX + 1];
	if (*dest_dir == '/' && !dest_dir[1]) /* Root dir */
		snprintf(tmp_dest, sizeof(tmp_dest), "/%s.copy", source_name);
	else
		snprintf(tmp_dest, sizeof(tmp_dest), "%s/%s.copy",
			dest_dir, source_name);

	char *dest = make_filename_unique(tmp_dest);

	if (rem_slash == 1)
		source[source_len - 1] = '/';

	return dest;
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

	const int rsync_ok = is_cmd_in_path("rsync");
	int exit_status = FUNC_SUCCESS;

	for (size_t i = 1; cmd[i]; i++) {
		if (!cmd[i] || !*cmd[i])
			continue;

		/* Construct destination filename. */
		char *source = cmd[i];
		char *dest = construct_dup_destination(source, dest_dir);
		if (!dest)
			continue;

		/* Run command. */
		if (rsync_ok == 1) {
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
	return exit_status;
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

static char *
extract_template_name_from_filename(const char *basename, int *t_auto)
{
	/* Explicit template name: file@template */
	char *tname = strrchr(basename, '@');
	if (tname && tname != basename && tname[1]) {
		*t_auto = 0;
		return tname + 1;
	}

	/* Automatic template (taken from file extension). */
	*t_auto = 1;
	tname = strrchr(basename, '.');
	if (tname && tname != basename && tname[1])
		return tname + 1;

	return NULL;
}

/* Return 1 if the template NAME is found in the templates list,
 * or 0 otherwise. */
static int
find_template(const char *name)
{
	if (!file_templates)
		return 0;

	for (size_t i = 0; file_templates[i]; i++) {
		if (*name == *file_templates[i]
		&& strcmp(name, file_templates[i]) == 0)
			return 1;
	}

	return 0;
}

/* Create the file show absolute path is ABS_PATH, and whose basename is
 * BASENAME, from the corresponing template.
 * Returns 1 in case of success, 0 in there's no template for this file
 * (or cp(1) fails), or -1 in case of error. */
static int
create_from_template(char *abs_path, const char *basename)
{
	if (!file_templates || !templates_dir || !*templates_dir
	|| !abs_path || !*abs_path || !basename || !*basename)
		return 0;

	int t_auto = 1;
	const char *t_name =
		extract_template_name_from_filename(basename, &t_auto);
	if (!t_name)
		return 0;

	if (find_template(t_name) == 0) {
		if (t_auto == 0) {
			xerror(_("new: '%s': No such template\n"), t_name);
			return (-1);
		}
		return 0;
	}

	if (t_auto == 0) {
		/* src_file@template: Remove template name from source filename. */
		char *p = strrchr(abs_path, '@');
		if (p)
			*p = '\0';
	}

	char template_file[PATH_MAX + 1];
	snprintf(template_file, sizeof(template_file),
		"%s/%s", templates_dir, t_name);

	struct stat a;
	if (lstat(template_file, &a) == -1 || !S_ISREG(a.st_mode))
		return 0;

	if (lstat(abs_path, &a) != -1) {
		err_file_exists(abs_path, 0, 0);
		return (-1);
	}

	char *cmd[] = {"cp", "--", template_file, abs_path, NULL};
	/* Let's copy the template file. STDERR and STDOUT are silenced: in case
	 * of error, we'll try to create a plain empty regular file via open(2)
	 * and print the error message in case of failure. */
	const int ret = launch_execv(cmd, FOREGROUND, E_MUTE);

	return (ret == 0);
}

/* Create the file named NAME, as a directory, if ending wit a slash, or as
 * a regular file otherwise.
 * Parent directories are created if they do not exist.
 * Returns FUNC_SUCCESS on success or FUNC_FAILURE on error. */
static int
create_file(char *name, const int is_md)
{
	struct stat a;
	char *ret = NULL;
	const char *n = name;
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
		const int retval = create_from_template(name, n);
		if (retval != 0) {
			if (retval == -1)
				// file@template: No such template, or destination file exists
				status = FUNC_FAILURE;
			goto END;
		}

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

END:
	return status;
}

static void
list_created_files(char **nfiles, const filesn_t nfiles_n)
{
	int file_in_cwd = 0;
	const filesn_t n = workspaces[cur_ws].path
		? count_dir(workspaces[cur_ws].path, NO_CPOP) - 2 : 0;

	if (n > 0 && n > g_files_num)
		file_in_cwd = 1;

	if (conf.autols == 1 && file_in_cwd == 1)
		reload_dirlist();

	for (size_t i = 0; nfiles[i]; i++) {
		char *f = abbreviate_file_name(nfiles[i]);
		const char *p = f ? f : nfiles[i];
		puts((*p == '.' && p[1] == '/' && p[2]) ? p + 2 : p);
		if (f && f != nfiles[i])
			free(f);
	}

	print_reload_msg(SET_SUCCESS_PTR, xs_cb, _("%zu file(s) created\n"),
		(size_t)nfiles_n);
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
	const int is_dir = (flen > 1 && p[flen - 1] == '/');
	if (is_dir == 1)
		p[flen - 1 ] = '\0'; /* Remove ending slash */

	char *npath = NULL;
	if (p == *name) {
		char *tilde = *p == '~' ? tilde_expand(p) : NULL;
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

/* Ask the user for a new filename and create the file. */
static int
ask_and_create_file(void)
{
	puts(_("Enter new filename (Ctrl+d to quit)\n"
		"Tip: End name with a slash to create a directory"));
	char n_prompt[NAME_MAX];
	snprintf(n_prompt, sizeof(n_prompt), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	int quoted = 0;
	char *filename = get_newname(n_prompt, NULL, &quoted);
	if (!filename) /* The user pressed Ctrl+d */
		return FUNC_SUCCESS;

	if (validate_filename(&filename, 0) == 0) {
		xerror(_("new: '%s': Unsafe filename\n"), filename);
		if (rl_get_y_or_n(_("Continue?"), 0) == 0) {
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
		char *args[] = { filename, NULL };
		list_created_files(args, 1);
	}

ERROR:
	free(filename);
	return exit_status;
}

/* (l)stat(2), just as access(2), sees "file" and "file/" as different
 * filenames. So, let's check the existence of the file FILE ignoring the
 * trailing slash, if any. */
static int
check_file_existence(char *file)
{
	int s = 0;
	const size_t l = strlen(file);
	if (l > 1 && file[l - 1] == '/') {
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

	/* If no argument provided, ask the user for a filename, create it and exit. */
	if (!args[0])
		/* This function is never executed by 'md', but always by 'n'. */
		return ask_and_create_file();

	size_t argsn;
	for (argsn = 0; args[argsn]; argsn++);

	/* Store pointers to actually created files in a pointers array.
	 * We'll use this later to print the names of actually created files. */
	char **new_files = xnmalloc(argsn + 1, sizeof(char *));
	filesn_t new_files_n = 0;

	for (size_t i = 0; args[i]; i++) {
		if (validate_filename(&args[i], is_md) == 0) {
			xerror(_("%s: '%s': Unsafe filename\n"),
				is_md ? "md" : "new", args[i]);
			if (rl_get_y_or_n(_("Continue?"), 0) == 0)
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
			new_files[new_files_n++] = args[i];
		} else {
			exit_status = ret;
		}
	}

	new_files[new_files_n] = NULL;

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
	for (size_t i = 0; args[i]; i++) {
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

static const char *
get_file_type_str(const int file_type)
{
	switch (file_type) {
	case OPEN_BLK:  return "block device";
	case OPEN_CHR:  return "character device";
#ifdef __sun
	case OPEN_DOOR: return "door";
#endif /* __sun */
	case OPEN_FIFO: return "FIFO/pipe";
	case OPEN_SOCK: return "socket";
	default:        return "unknown file type";
	};
}

static int
get_file_type(const mode_t mode, const char *filename)
{
	switch ((mode & S_IFMT)) {
	case S_IFBLK:  return OPEN_BLK;
	case S_IFCHR:  return OPEN_CHR;
	case S_IFSOCK: return OPEN_SOCK;
	case S_IFIFO:  return OPEN_FIFO;
#ifdef __sun
	case S_IFDOOR: return OPEN_DOOR;
#endif /* __sun */
	case S_IFDIR:  return OPEN_DIR;
	case S_IFREG:  return OPEN_REG;

	case S_IFLNK:
		switch (get_link_ref(filename)) {
		case -1:       return OPEN_LINK_ERR;
		case S_IFDIR:  return OPEN_DIR;
		case S_IFREG:  return OPEN_REG;
		case S_IFBLK:  return OPEN_BLK;
		case S_IFCHR:  return OPEN_CHR;
		case S_IFSOCK: return OPEN_SOCK;
		case S_IFIFO:  return OPEN_FIFO;
#ifdef __sun
		case S_IFDOOR: return OPEN_DOOR;
#endif /* __sun */
		default:       return OPEN_UNKNOWN;
		}
		break;

	default: return OPEN_UNKNOWN;
	}
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

	if ((*cmd[0] == 'o' && (!cmd[0][1] || strcmp(cmd[0], "open") == 0))
	&& strchr(cmd[1], '\\')) {
		char *deq_path = unescape_str(cmd[1]);
		if (!deq_path) {
			xerror(_("%s: '%s': Error unescaping filename\n"), errname, cmd[1]);
			return FUNC_FAILURE;
		}

		xstrsncpy(cmd[1], deq_path, strlen(deq_path) + 1);
		free(deq_path);
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
	const int file_type = get_file_type(attr.st_mode, file);

	switch (file_type) {
	case OPEN_DIR: return cd_function(file, CD_PRINT_ERROR);
	case OPEN_LINK_ERR: return err_no_link(file);
	case OPEN_REG: break;
	default:
		xerror(_("%s: '%s' (%s): Cannot open file\nTry "
			"'APP FILE' or 'open FILE APP'\n"), errname, file,
			get_file_type_str(file_type));
		return FUNC_FAILURE;
	}

	/* At this point we know that the file to be openend is either a regular
	 * file or a symlink to a regular file. So, just open the file. */

	/* A single file with no opening application */
	if (!cmd[2] || (*cmd[2] == '&' && !cmd[2][1]))
		return open_file(file);

	/* Multiple files */
	if (!is_cmd_in_path(cmd[2])) {
		return mime_open_multiple_files(cmd + 1);
	}

	/* A single file plus an opening application. */
	char *tmp_cmd[] = {cmd[2], file, NULL};
	const int ret =
		launch_execv(tmp_cmd, bg_proc ? BACKGROUND : FOREGROUND, E_NOSTDERR);

	if (ret == FUNC_SUCCESS)
		return FUNC_SUCCESS;

	/* Since STDERR is silenced, we print the error message here. */
	if (ret == E_NOEXEC) /* EACCESS && ENOEXEC */
		xerror("%s: %s: %s\n", errname, cmd[2], NOEXEC_MSG);
	else if (ret == E_NOTFOUND) /* ENOENT */
		xerror("%s: %s: %s\n", errname, cmd[2], NOTFOUND_MSG);
	else
		xerror(_("%s: '%s' failed with error code %d\n"), errname, cmd[2], ret);

	return ret;
}

static char *
get_new_link_target(char *cur_target)
{
	puts(_("Edit target (Ctrl+d to quit)"));
	char n_prompt[NAME_MAX];
	snprintf(n_prompt, sizeof(n_prompt), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	char *new_target = NULL;
	while (!new_target) {
		int quoted = 0;
		new_target = get_newname(n_prompt, cur_target, &quoted);
		UNUSED(quoted);

		if (!new_target) /* The user pressed Ctrl+d */
			return NULL;
	}

	char *tmp = NULL;
	if (*new_target == '~' && (tmp = tilde_expand(new_target))) {
		free(new_target);
		new_target = tmp;
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
	printf(_("Current target %s%s%s "), dn_c, SET_MSG_PTR, df_c);

	struct stat a;
	if (lstat(target, &a) != -1) {
		colors_list(target, NO_ELN, NO_PAD, PRINT_NEWLINE);
	} else if (*target) {
		printf(_("%s%s%s (broken link)\n"), uf_c, target, df_c);
	} else {
		puts(_("??? (broken link)"));
	}
}

/* Relink the symbolic link LINK to a new target. */
int
edit_link(char *link)
{
	if (!link || !*link || IS_HELP(link)) {
		puts(LE_USAGE);
		return FUNC_SUCCESS;
	}

	/* Dequote the filename, if necessary. */
	if (strchr(link, '\\')) {
		char *tmp = unescape_str(link);
		if (!tmp) {
			xerror(_("le: '%s': Error unescaping filename\n"), link);
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
		if (rl_get_y_or_n(_("Relink as broken symbolic link?"), 0) == 0) {
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

/* If PBUF is not NULL, append STR to PBUF, update PBUF to point to the
 * end of the buffer, and adjust PLEN to reflect the remaining space.
 * Return 0 on success and 1 on failure (no remaining space). */
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
		return NULL;
	}

	char *p = strrchr(norm_link, '/');
	if (p)
		*p = '\0';

	char *norm_target = normalize_path(target, strlen(target));
	if (!norm_target) {
		free(norm_link);
		xerror(_("link: '%s': Error normalizing path\n"), target);
		return NULL;
	}

	char *resolved_target = xnmalloc(PATH_MAX + 1, sizeof(char));
	const int ret = relpath(norm_target, norm_link, resolved_target,
		PATH_MAX + 1);

	free(norm_link);
	free(norm_target);

	if (ret != 0) {
		free(resolved_target);
		return NULL;
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
		char *deq = unescape_str(args[0]);
		if (deq) {
			free(args[0]);
			args[0] = deq;
		}
	}

	if (args[1] && strchr(args[1], '\\')) {
		char *deq = unescape_str(args[1]);
		if (deq) {
			free(args[1]);
			args[1] = deq;
		}
	}

	char *target = args[0];
	char *link_name = args[1];
	char buf[PATH_MAX + 1];
	struct stat a;

	if (!link_name || !*link_name) {
		const char *p = strrchr(target, '/');
		snprintf(buf, sizeof(buf), "%s-link", (p && p[1]) ? p + 1 : target);

		char *unique_name = make_filename_unique(buf);
		if (unique_name) {
			xstrsncpy(buf, unique_name, sizeof(buf));
			free(unique_name);
			link_name = buf;
		} else {
			xerror(_("link: Cannot create symbolic link to '%s'\n"), target);
			return FUNC_FAILURE;
		}
	}

	len = strlen(link_name);
	if (len > 1 && link_name[len - 1] == '/')
		link_name[len - 1] = '\0';

	if (lstat(target, &a) == -1) {
		printf("link: '%s': %s\n", target, strerror(errno));
		if (rl_get_y_or_n(_("Create broken symbolic link?"), 0) == 0)
			return FUNC_SUCCESS;
	}

	if (lstat(link_name, &a) != -1 && S_ISLNK(a.st_mode)) {
		printf("link: '%s': %s\n", link_name, strerror(EEXIST));
		if (rl_get_y_or_n(_("Overwrite this file?"),
		conf.default_answer.overwrite) == 0) {
			return FUNC_SUCCESS;
		}

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

	const int ret = symlinkat(resolved_target, XAT_FDCWD, link_name);
	if (resolved_target != target)
		free(resolved_target);

	if (ret == -1) {
		xerror(_("link: Cannot create symbolic link '%s': %s\n"),
			link_name, strerror(errno));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static int
vv_rename_files(char **args, const size_t copied)
{
	char **tmp = xnmalloc(args_n + 2, sizeof(char *));
	tmp[0] = savestring("br", 2);

	size_t i, l = strlen(args[args_n]), c = 1;

	if (l > 0 && args[args_n][l - 1] == '/')
		args[args_n][l - 1] = '\0';

	const char *dest = sel_is_last == 1 ? "." : args[args_n];
	const size_t n = args_n + (sel_is_last == 1);

	for (i = 1; i < n && args[i]; i++) {
		if (!*args[i])
			continue;

		l = strlen(args[i]);
		if (l > 0 && args[i][l - 1] == '/')
			args[i][l - 1] = '\0';

		char p[PATH_MAX + 1];
		const char *s = strrchr(args[i], '/');
		snprintf(p, sizeof(p), "%s/%s", dest, (s && *(++s)) ? s : args[i]);

		tmp[c++] = savestring(p, strnlen(p, sizeof(p)));
	}

	tmp[c] = NULL;

	size_t renamed = 0;
	const int ret = bulk_rename(tmp, &renamed, 0);

	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(SET_SUCCESS_PTR, xs_cb, _("%zu file(s) copied\n"), copied);
	if (renamed > 0) {
		print_reload_msg(SET_SUCCESS_PTR, xs_cb,
			_("%zu file(s) renamed\n"), renamed);
	} else {
		print_reload_msg(NULL, NULL, _("%zu file(s) renamed\n"), renamed);
	}

	for (i = 0; tmp[i]; i++)
		free(tmp[i]);
	free(tmp);

	return ret;
}

static int
vv_create_new_dir(const char *dir)
{
	fprintf(stderr, _("vv: '%s': directory does not exist.\n"), dir);
	if (rl_get_y_or_n(_("Create it?"), 0) == 0)
		return (-1);

	char tmp[PATH_MAX + 1];
	snprintf(tmp, sizeof(tmp), "%s/", dir);

	return create_file(tmp, 1);
}

static int
validate_vv_dest_dir(const char *file)
{
	if (args_n == 0) {
		fprintf(stderr, "%s\n", VV_USAGE);
		return (-1);
	}

	struct stat a;
	if (stat(file, &a) == -1) {
		if (errno == ENOENT) {
			return vv_create_new_dir(file);
		} else {
			xerror("vv: '%s': %s\n", file, strerror(errno));
			return FUNC_FAILURE;
		}
	}

	if (!S_ISDIR(a.st_mode) && sel_is_last == 0) {
		xerror("vv: '%s': %s\n", file, strerror(ENOTDIR));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static char *
get_new_filename(char *cur_name)
{
	char n_prompt[NAME_MAX];
	snprintf(n_prompt, sizeof(n_prompt), _("Enter new name (Ctrl+d to quit)\n"
		"\001%s\002>\001%s\002 "), mi_c, tx_c);

	char *new_name = NULL;
	while (!new_name) {
		int quoted = 0;
		new_name = get_newname(n_prompt, cur_name, &quoted);
		UNUSED(quoted);

		if (!new_name) /* The user pressed Ctrl+d */
			return NULL;

		if (is_blank_name(new_name) == 1) {
			free(new_name);
			new_name = NULL;
		}
	}

	size_t len = strlen(new_name);
	if (len > 0 && new_name[len - 1] == ' ') {
		len--;
		new_name[len] = '\0';
	}

	char *n = normalize_path(new_name, len);
	free(new_name);

	return n;
}

/* Return 1 if at least one file is selected in the current directory.
 * Otherwise, 0 is returned. */
int
cwd_has_sel_files(void)
{
	filesn_t i = g_files_num;
	while (--i >= 0) {
		if (file_info[i].sel == 1)
			return 1;
	}

	return 0;
}

#define IS_MVCMD(s) (*(s) == 'm' || (*(s) == 'a' \
	&& strncmp((s), "advmv", 5) == 0))

static int
print_cp_mv_summary_msg(const char *c, const size_t n, const int cwd)
{
	if (conf.autols == 1 && cwd == 1)
		reload_dirlist();

	if (IS_MVCMD(c))
		print_reload_msg(SET_SUCCESS_PTR, xs_cb, _("%zu file(s) moved\n"), n);
	else
		print_reload_msg(SET_SUCCESS_PTR, xs_cb, _("%zu file(s) copied\n"), n);

	return FUNC_SUCCESS;
}

static char *
get_rename_dest_filename(char *name, int *status)
{
	if (!name || !*name) {
		*status = EINVAL;
		return NULL;
	}

	/* Check source file existence. */
	struct stat a;
	char *p = unescape_str(name);
	const int ret = lstat(p ? p : name, &a);
	free(p);

	if (ret == -1) {
		*status = errno;
		alt_prompt = 0;
		xerror("m: '%s': %s\n", name, strerror(errno));
		return NULL;
	}

	/* Get destination filename. */
	char *new_name = get_new_filename(name);
	if (!new_name) { /* The user pressed Ctrl+d */
		*status = FUNC_SUCCESS;
		return NULL;
	}

	if (validate_filename(&new_name, 0) == 0) {
		xerror(_("m: '%s': Unsafe filename\n"), new_name);
		if (rl_get_y_or_n(_("Continue?"), 0) == 0) {
			*status = FUNC_SUCCESS;
			free(new_name);
			return NULL;
		}
	}

	return new_name;
}

static char **
construct_cp_mv_cmd(char **cmd, char *new_name, int *cwd, const size_t force)
{
	size_t n = 0;
	char **tcmd = xnmalloc(3 + args_n + 2, sizeof(char *));

	char *p = strchr(cmd[0], ' ');
	if (p && p[1]) {
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
	if (*tcmd[0] != 'w' || strcmp(tcmd[0], "wcp") != 0)
		tcmd[n++] = savestring("--", 2);

	/* The -f,--force parameter is internal. Skip it.
	 * It instructs cp/mv to skip confirmation prompts. */
	size_t i = force == 1 ? 2 : 1;

	for (; cmd[i]; i++) {
		if (!*cmd[i] /* File skipped by the user in the confirmation prompt. */
		|| !(p = unescape_str(cmd[i])))
			continue;
		tcmd[n] = savestring(p, strlen(p));
		free(p);
		if (*cwd == 0)
			*cwd = is_file_in_cwd(tcmd[n]);
		n++;
	}

	/* Append extra parameters as required. */
	if (is_sel > 0 && sel_is_last == 1) { /* E.g., "m sel" */
		tcmd[n] = savestring(".", 1);
		*cwd = 1;
		n++;
	} else {
		if (new_name) { /* Interactive rename: "m FILE" */
			tcmd[n] = new_name;
			if (*cwd == 0)
				*cwd = is_file_in_cwd(tcmd[n]);
			n++;
		}
	}

	tcmd[n] = NULL;
	return tcmd;
}

static int
handle_nodir_overwrite(char *arg, const char *cmd_name)
{
	char *file = unescape_str(arg);
	if (!file)
		return 0;

	struct stat a;
	if (lstat(file, &a) != -1) {
		char msg[PATH_MAX + 28];
		snprintf(msg, sizeof(msg), _("%s: '%s': Overwrite this file?"),
			cmd_name, file);
		if (rl_get_y_or_n(msg, conf.default_answer.overwrite) == 0) {
			free(file);
			return 0;
		}
	}

	free(file);
	return 1;
}

static int
check_overwrite(char **args, const int force, size_t *skipped)
{
	const int append_curdir = (sel_is_last == 1 && sel_n > 0);
	const size_t files_num = args_n + (append_curdir == 1);

	const char *cmd_name = IS_MVCMD(args[0]) ? "m" : "c";
	if (append_curdir == 0 && validate_filename(&args[args_n], 0) == 0) {
		xerror(_("%s: '%s': Unsafe filename\n"), cmd_name, args[args_n]);
		if (rl_get_y_or_n(_("Continue?"), 0) == 0)
			return 0;
	}

	if (!args || files_num <= 1 || force == 1)
		return 1;

	const char *dest = append_curdir == 1 ? "." : args[args_n];

	struct stat a;
	if (!dest || stat(dest, &a) == -1)
		return 1;

	if (!S_ISDIR(a.st_mode))
		return handle_nodir_overwrite(args[2], cmd_name);

	char msg[PATH_MAX + 28];
	char buf[PATH_MAX + 1];
	const size_t dest_len = strlen(dest);
	const int ends_with_slash =
		(dest_len > 1 && dest[dest_len - 1] == '/');

	for (size_t i = 1; i < files_num; i++) {
		char *p = unescape_str(args[i]);
		if (!p)
			continue;

		const char *s = strrchr(p, '/');
		const char *basename = (s && s[1]) ? s + 1 : p;

		if (ends_with_slash == 0)
			snprintf(buf, sizeof(buf), "%s/%s", dest, basename);
		else
			snprintf(buf, sizeof(buf), "%s%s", dest, basename);

		free(p);

		if (lstat(buf, &a) == -1)
			continue;

		snprintf(msg, sizeof(msg), _("%s: '%s': Overwrite this file?"),
			cmd_name, buf);
		if (rl_get_y_or_n(msg, conf.default_answer.overwrite) == 0) {
			/* Nullify this entry. It will be skipped later. */
			*args[i] = '\0';
			(*skipped)++;
		}
	}

	/* If skipped == files_num - 1, there are no source files left, only
	 * the destination file. There's nothing to do. */
	return (*skipped < files_num - 1);
}

/* Remove trainling slashes from source files in ARGS. */
static void
remove_dirslash_from_source(char **args)
{
	if (args_n <= 1)
		return;

	for (size_t i = 1; i < args_n; i++) {
		if (!args[i])
			break;
		if (!*args[i])
			continue;

		const size_t len = strlen(args[i]);
		if (len > 1 && args[i][len - 1] == '/')
			args[i][len - 1] = '\0';
	}
}

/* Launch the command associated to 'c' (also 'v' and 'vv') or 'm'
 * internal commands. */
int
cp_mv_file(char **args, const int copy_and_rename, const int force)
{
	int ret = 0;
	size_t skipped = 0;

	if (!args || !args[0])
		return FUNC_FAILURE;

	if (check_overwrite(args, force, &skipped) == 0)
		return FUNC_SUCCESS;

	/* vv command */
	if (copy_and_rename == 1
	&& (ret = validate_vv_dest_dir(args[args_n])) != FUNC_SUCCESS)
		return ret == -1 ? FUNC_SUCCESS : FUNC_FAILURE;

	/* m command */
	char *new_name = NULL;
	if (IS_MVCMD(args[0]) && args[1]) {
		const size_t len = strlen(args[1]);
		if (len > 0 && args[1][len - 1] == '/')
			args[1][len - 1] = '\0';

		if (alt_prompt == FILES_PROMPT) { /* Interactive rename. */
			int status = 0;
			if (!(new_name = get_rename_dest_filename(args[1], &status)))
				return status;
		}
	}

	/* rsync(1) won't copy directories with a trailing slash. Remove it. */
	if (*args[0] == 'r' && args[1])
		remove_dirslash_from_source(args);

	/* Number of files to operate on. */
	/* In case of 'm FILE' (interactive rename), args_n is 1, and 1 is
	 * the number we want. */
	const size_t force_param = (args[1] && is_force_param(args[1]));
	const size_t files_num =
		args_n - (args_n > 1 && sel_is_last == 0) - skipped - force_param;

	int cwd = 0;
	char **tcmd = construct_cp_mv_cmd(args, new_name, &cwd, force_param);
	if (!tcmd)
		return FUNC_FAILURE;

	ret = launch_execv(tcmd, FOREGROUND, E_NOFLAG);

	for (size_t i = 0; tcmd[i]; i++)
		free(tcmd[i]);
	free(tcmd);

	if (ret != FUNC_SUCCESS)
		return ret;

	if (copy_and_rename == 1) /* vv command */
		return vv_rename_files(args, files_num);

	if (sel_n > 0 && IS_MVCMD(args[0])) {
		if (is_sel > 0) {
			/* If 'mv sel' and command is successful deselect everything:
			 * selected files are not there anymore. */
			deselect_all();
		} else {
			if (cwd_has_sel_files())
			/* Just in case a selected file in the current dir was renamed. */
				get_sel_files();
		}
	}

	return print_cp_mv_summary_msg(args[0], files_num, cwd);
}
#undef IS_MVCMD

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
list_removed_files(struct rm_info *info, const size_t start, const int cwd)
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

	if (print_removed_files == 1) {
		for (i = start; info[i].name; i++) {
			if (!info[i].name || !*info[i].name || info[i].exists == 1)
				continue;

			print_removed_file_info(info[i]);
		}
	}

	print_reload_msg(SET_SUCCESS_PTR, xs_cb, _("%zu file(s) removed\n"), c);
}

/* Print files to be removed and ask the user for confirmation.
 * Returns 0 if no or 1 if yes. */
static int
rm_confirm(const struct rm_info *info, const size_t start, const int have_dirs)
{
	printf(_("File(s) to be removed%s:\n"),
		have_dirs > 0 ? _(" (recursively)") : "");

	for (size_t i = start; info[i].name; i++)
		print_file_name(info[i].name, info[i].dir);

	return rl_get_y_or_n(_("Continue?"), conf.default_answer.remove);
}

static int
check_rm_files(const struct rm_info *info, const size_t start,
	const char *errname)
{
	struct stat a;
	int ret = FUNC_SUCCESS;

	for (size_t i = start; info[i].name; i++) {
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
		return (rl_get_y_or_n(_("Remove files anyway?"),
			conf.default_answer.remove) == 0
				? FUNC_FAILURE : FUNC_SUCCESS);
	}

	return ret;
}

static struct rm_info
fill_rm_info_struct(char **filename, struct stat *a)
{
	struct rm_info info = {0};

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
		/* Let's start storing filenames in 3: 0 is for 'rm', and 1
		 * and 2 for parameters, including end of parameters (--). */

		/* If we have a symlink to dir ending with a slash, stat(2) takes it
		 * as a directory, and then rm(1) complains that cannot remove it,
		 * because "Is a directory". So, let's remove the ending slash:
		 * stat(2) will take it as the symlink it is and rm(1) will remove
		 * the symlink (not the target) without complains. */
		const size_t len = strlen(args[i]);
		if (len > 1 && args[i][len - 1] == '/')
			args[i][len - 1] = '\0';

		/* Check if at least one file is in the current directory. If not,
		 * there is no need to refresh the screen. */
		if (cwd == 0)
			cwd = is_file_in_cwd(args[i]);

		char *tmp = unescape_str(args[i]);
		if (!tmp) {
			xerror(_("%s: '%s': Error unescaping filename\n"), err_name, args[i]);
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

	rm_cmd[j] = info[j].name = NULL;

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

	if (is_sel > 0 && exit_status == FUNC_SUCCESS)
		deselect_all();

	list_removed_files(info, 3, cwd);

END:
	for (i = 3; rm_cmd[i]; i++)
		free(rm_cmd[i]);
	free(rm_cmd);

	free(info);
	return exit_status;
}

/* Export files in CWD (if FILENAMES is NULL), or files in FILENAMES,
 * to a temporary file. Return the address of this empty file if
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
		return NULL;
	}

	FILE *fp = fdopen(fd, "w");
	if (!fp) {
		xerror("exp: '%s': %s\n", tmp_file, strerror(errno));
		if (unlinkat(XAT_FDCWD, tmp_file, 0) == -1)
			xerror("exp: unlink: '%s': %s\n", tmp_file, strerror(errno));
		close(fd);
		free(tmp_file);
		return NULL;
	}

	/* If no argument, export files in CWD. */
	if (!filenames[1]) {
		char buf[PATH_MAX + 1];
		for (size_t i = 0; file_info[i].name; i++) {
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
		for (size_t i = 1; filenames[i]; i++) {
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

	if (unlinkat(XAT_FDCWD, tmp_file, 0) == -1)
		xerror("exp: unlink: '%s': %s\n", tmp_file, strerror(errno));
	free(tmp_file);
	return NULL;
}

/* Create a symlink in CWD for each filename in ARGS.
 * If the destination file exists, a positive integer suffix is appended to
 * make the filename unique. */
int
batch_link(char **args)
{
	if (!args || !args[0] || IS_HELP(args[0])) {
		puts(_(BL_USAGE));
		return FUNC_SUCCESS;
	}

	size_t symlinked = 0;
	int exit_status = FUNC_SUCCESS;

	for (size_t i = 0; args[i]; i++) {
		char *filename = unescape_str(args[i]);
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
		const char *basename = s && *(++s) ? s : filename;

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
	print_reload_msg(SET_SUCCESS_PTR, xs_cb,
		_("%zu symbolic link(s) created\n"), symlinked);

	return exit_status;
}
