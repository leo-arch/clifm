/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2016-2025, L. Abramovich <leo.clifm@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* archives.c -- archiving functions */

#ifndef _NO_ARCHIVING

#include "helpers.h"

#include <errno.h>
#include <string.h>
#include <readline/readline.h>

#if defined(_NO_MAGIC)
# if !defined(_BE_POSIX)
#  include <paths.h>
#  ifndef _PATH_DEVNULL
#   define _PATH_DEVNULL "/dev/null"
#  endif /* _PATH_DEVNULL */
# endif /* !_BE_POSIX */
#endif /* _NO_MAGIC */

#include "aux.h"
#include "checks.h"
#include "history.h"
#include "jump.h"
#include "listing.h"
#include "mime.h" /* xmagic() */
#include "misc.h"
#include "navigation.h"
#include "readline.h"
#include "spawn.h"

#define OP_ISO    1
#define OP_OTHERS 0

static char *
ask_user_for_path(void)
{
	const char *m = _("Extraction path ('q' to quit): ");

	const int poffset_bk = prompt_offset;
	prompt_offset = (int)strlen(m) + 1;
	rl_nohist = 1;
	alt_prompt = FILES_PROMPT;

	char *ext_path = secondary_prompt(m, NULL);

	alt_prompt = rl_nohist = 0;
	prompt_offset = poffset_bk;

	if (!ext_path)
		return (char *)NULL;

	char *p = (char *)NULL;
	if ((*ext_path == '"' || *ext_path == '\'')
	&& (p = remove_quotes(ext_path)))
		memmove(ext_path, p, strlen(p) + 1);

	char *unesc_path = unescape_str(ext_path, 0);
	if (unesc_path) {
		free(ext_path);
		ext_path = unesc_path;
	}

	return ext_path;
}

static char *
get_extraction_path(void)
{
	char *ext_path = ask_user_for_path();
	if (!ext_path || !*ext_path) {
		free(ext_path);
		return (char *)NULL;
	}

	if (TOUPPER(*ext_path) == 'Q' && !ext_path[1]) {
		free(ext_path);
		return (char *)NULL;
	}

	if (*ext_path == '~') {
		char *p = tilde_expand(ext_path);
		if (p) {
			free(ext_path);
			return p;
		}
	}

	if (*ext_path == '.' || strstr(ext_path, "../")) {
		char *p = xrealpath(ext_path, NULL);
		if (p) {
			free(ext_path);
			return p;
		}
	}

	return ext_path;
}

static char
get_operation(const int mode)
{
	char sel_op = 0;
	char *op = (char *)NULL;
	while (!op) {
		op = rl_no_hist(_("Operation: "), 0);
		if (!op)
			continue;
		if (!*op || op[1] != '\0') {
			free(op);
			op = (char *)NULL;
			continue;
		}

		switch (*op) {
		case 'e': /* fallthrough */
		case 'E': /* fallthrough */
		case 'l': /* fallthrough */
		case 'm': /* fallthrough */
		case 't': /* fallthrough */
		case 'r':
			if (mode == OP_ISO && *op == 'r') {
				free(op);
				op = (char *)NULL;
				break;
			}
			if (mode == OP_OTHERS && *op == 't') {
				free(op);
				op = (char *)NULL;
				break;
			}
			sel_op = *op;
			free(op);
			break;

		case 'q': /* fallthrough */
		case 'Q':
			free(op);
			return FUNC_SUCCESS;

		default:
			free(op);
			op = (char *)NULL;
			break;
		}

		if (sel_op)
			break;
	}

	return sel_op;
}

static int
extract_iso(char *file)
{
	int exit_status = FUNC_SUCCESS;

	/* 7z x -oDIR FILE (use FILE as DIR) */
	const size_t flen = strlen(file);
	char *o_option = xnmalloc(flen + 7, sizeof(char));
	snprintf(o_option, flen + 7, "-o%s.dir", file);

	/* Construct and execute cmd */
	char *cmd[] = {"7z", "x", o_option, file, NULL};
	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
		exit_status = FUNC_FAILURE;

	free(o_option);
	return exit_status;
}

static int
extract_iso_to_dir(char *file)
{
	/* 7z x -oDIR FILE (ask for DIR) */
	int exit_status = FUNC_SUCCESS;

	char *ext_path = get_extraction_path();
	if (!ext_path)
		return FUNC_FAILURE;

	const size_t len = strlen(ext_path);
	char *o_option = xnmalloc(len + 3, sizeof(char));
	snprintf(o_option, len + 3, "-o%s", ext_path);
	free(ext_path);

	/* Construct and execute cmd */
	char *cmd[] = {"7z", "x", o_option, file, NULL};
	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
		exit_status = FUNC_FAILURE;

	free(o_option);

	return exit_status;
}

static int
list_iso_contents(char *file)
{
	int exit_status = FUNC_SUCCESS;

	/* 7z l FILE */
	char *cmd[] = {"7z", "l", file, NULL};
	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
		exit_status = FUNC_FAILURE;

	return exit_status;
}

static int
test_iso(char *file)
{
	int exit_status = FUNC_SUCCESS;

	/* 7z t FILE */
	char *cmd[] = {"7z", "t", file, NULL};
	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
		exit_status = FUNC_FAILURE;

	return exit_status;
}

/* Create mountpoint for file. Returns the path to the mountpoint in case
 * of success, or NULL in case of error. */
static char *
create_mountpoint(char *file)
{
	char *mountpoint = (char *)NULL;
	char *p = strrchr(file, '/');
	char *tfile = (p && *(++p)) ? p : file;

	if (xargs.stealth_mode == 1) {
		const size_t len = strlen(tfile) + P_tmpdir_len + 15;
		mountpoint = xnmalloc(len, sizeof(char));
		snprintf(mountpoint, len, "%s/clifm-mounts/%s", P_tmpdir, tfile);
	} else {
		const size_t len = config_dir_len + strlen(tfile) + 9;
		mountpoint = xnmalloc(len, sizeof(char));
		snprintf(mountpoint, len, "%s/mounts/%s", config_dir, tfile);
	}

	char *dir_cmd[] = {"mkdir", "-pm700", mountpoint, NULL};
	if (launch_execv(dir_cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS) {
		free(mountpoint);
		mountpoint = (char *)NULL;
	}

	return mountpoint;
}

#if defined(__linux__)
static int
cd_to_mountpoint(char *file, char *mountpoint)
{
	if (xchdir(mountpoint, SET_TITLE) == -1) {
		xerror("archiver: '%s': %s\n", mountpoint, strerror(errno));
		return FUNC_FAILURE;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(mountpoint, strlen(mountpoint));
	add_to_jumpdb(workspaces[cur_ws].path);

	int exit_status = FUNC_SUCCESS;
	if (conf.autols == 1) {
		reload_dirlist();
		add_to_dirhist(workspaces[cur_ws].path);
	} else {
		printf("'%s': Successfully mounted on '%s'\n", file, mountpoint);
	}

	return exit_status;
}
#endif /* __linux__ */

static int
mount_iso(char *file)
{
#if !defined(__linux__)
	UNUSED(file);
	xerror("%s\n", _("mount: This feature is available for Linux only."));
	return FUNC_SUCCESS;
#else
	char *mountpoint = create_mountpoint(file);
	if (!mountpoint)
		return FUNC_FAILURE;

	/* Construct and execute the cmd */
	char *sudo = get_sudo_path();
	if (!sudo) {
		free(mountpoint);
		return FUNC_FAILURE;
	}

	char *cmd[] = {sudo, "mount", "-o", "loop", file, mountpoint, NULL};
	if (confirm_sudo_cmd(cmd) == 0) {
		free(mountpoint);
		free(sudo);
		return FUNC_SUCCESS;
	}

	if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS) {
		free(mountpoint);
		free(sudo);
		return FUNC_FAILURE;
	}
	free(sudo);

	int exit_status = cd_to_mountpoint(file, mountpoint);
	free(mountpoint);
	return exit_status;
#endif /* __linux__ */
}

/* Use 7z to
 * list (l)
 * extract (e)
 * extrat to dir (x -oDIR FILE)
 * test (t) */
static int
handle_iso(char *file)
{
	printf(_("%s[e]%sxtract %s[E]%sxtract-to-dir %s[l]%sist "
		 "%s[t]%sest %s[m]%sount %s[q]%suit\n"), BOLD, df_c, BOLD,
	    df_c, BOLD, df_c, BOLD, df_c, BOLD, df_c, BOLD, df_c);

	char sel_op = get_operation(OP_ISO);

	if (strchr(file, '\\')) {
		char *deq_file = unescape_str(file, 0);
		if (deq_file) {
			xstrsncpy(file, deq_file, strlen(deq_file) + 1);
			free(deq_file);
		}
	}

	switch (sel_op) {
	case 'e': return extract_iso(file);
	case 'E': return extract_iso_to_dir(file);
	case 'l': return list_iso_contents(file);
	case 'm': return mount_iso(file);
	case 't': return test_iso(file);
	default: return FUNC_SUCCESS;
	}
}

static int
create_iso_from_block_dev(char *in_file, char *out_file)
{
	size_t len = strlen(in_file) + 4;
	char *if_option = xnmalloc(len, sizeof(char));
	snprintf(if_option, len, "if=%s", in_file);

	len = strlen(out_file) + 4;
	char *of_option = xnmalloc(len, sizeof(char));
	snprintf(of_option, len, "of=%s", out_file);

	char *sudo = get_sudo_path();
	if (!sudo) {
		free(if_option);
		free(of_option);
		return FUNC_FAILURE;
	}

	int exit_status = FUNC_SUCCESS;
	char *cmd[] = {sudo, "dd", if_option, of_option, "bs=64k",
	    "conv=noerror,sync", "status=progress", NULL};

	if (confirm_sudo_cmd(cmd) == 1) {
		if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
			exit_status = FUNC_FAILURE;
	}

	free(sudo);
	free(if_option);
	free(of_option);

	return exit_status;
}

static int
create_iso(char *in_file, char *out_file)
{
	struct stat attr;
	if (lstat(in_file, &attr) == -1) {
		xerror("archiver: '%s': %s\n", in_file, strerror(errno));
		return FUNC_FAILURE;
	}

	/* If IN_FILE is a directory */
	if (S_ISDIR(attr.st_mode)) {
		char *cmd[] = {"mkisofs", "-R", "-o", out_file, in_file, NULL};
		if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
			return FUNC_FAILURE;
		return FUNC_SUCCESS;
	}

	/* If IN_FILE is a block device */
	if (S_ISBLK(attr.st_mode))
		return create_iso_from_block_dev(in_file, out_file);

	/* If any other file format */
	xerror(_("archiver: '%s': Invalid file format. File should be either "
		"a directory or a block device.\n"), in_file);
	return FUNC_FAILURE;
}

/* Check the MIME type of the file named FILE and look for "ISO 9660" in its
 * output. Returns zero if found, one if not, and -1 in case of error. */
static int
check_iso(char *file)
{
	if (!file || !*file) {
		xerror("%s\n", _("Error querying file type"));
		return (-1);
	}

	int is_iso = 0;

	char *t = xmagic(file, TEXT_DESC);
	if (!t) {
		xerror("%s\n", _("Error querying file type"));
		return (-1);
	}

	if (strstr(t, "ISO 9660"))
		is_iso = 1;

	free(t);
	return (is_iso == 1 ? FUNC_SUCCESS : FUNC_FAILURE);
}

static int
check_compressed(const char *line, const int test_iso)
{
	return (strstr(line, "archive") || strstr(line, "compressed")
	|| strncmp(line, "Debian binary package ", 22) == 0
	|| strncmp(line, "RPM ", 4) == 0
	|| (test_iso && strstr(line, "ISO 9660")));
}

/* Check the MIME type of the file named FILE and look for "archive" and
 * "compressed" strings. Returns zero if compressed, one if not, and -1
 * in case of error.
 * test_iso is used to determine if ISO files should be checked as
 * well: this is the case when called from open_function() or
 * mime_open(), since both need to check compressed and ISOs as
 * well (and there is no need to run two functions (is_compressed and
 * check_iso), when we can run just one). */
int
is_compressed(char *file, const int test_iso)
{
	if (!file || !*file) {
		xerror("%s\n", _("Error querying file type"));
		return (-1);
	}

	char *t = xmagic(file, TEXT_DESC);
	if (!t) {
		xerror("%s\n", _("Error querying file type"));
		return (-1);
	}

	const int compressed = check_compressed(t, test_iso);
	free(t);

	return (compressed == 1 ? FUNC_SUCCESS : FUNC_FAILURE);
}

static char *
add_default_extension(char *name)
{
	const size_t name_len = strlen(name);

	char *t = savestring(name, name_len);
	name = xnrealloc(name, name_len + 8, sizeof(char));
	snprintf(name, name_len + 8, "%s.tar.gz", t);
	free(t);

	return name;
}

/* Get archive name/type */
static char *
get_archive_filename(void)
{
	puts(_("Use extension to specify archive/compression type "
	       "(defaults to .tar.gz)\nExample: myarchive.xz"));
	char *name = (char *)NULL;
	while (!name) {
		flags |= NO_FIX_RL_POINT;
		name = rl_no_hist(_("Filename ('q' to quit): "), 0);
		flags &= ~NO_FIX_RL_POINT;

		if (!name || !*name) {
			free(name);
			name = (char *)NULL;
			continue;
		}

		if (*name == 'q' && name[1] == '\0') {
			free(name);
			return (char *)NULL;
		}

		char *dot = strrchr(name, '.');
		if (!dot) /* If no extension, add the default */
			return add_default_extension(name);

		if (dot == name) { /* Dot is first char */
			xerror("%s\n", _("Invalid filename"));
			free(name);
			name = (char *)NULL;
			continue;
		}
	}

	return name;
}

/* If MODE is 'c', compress IN_FILE producing a zstandard compressed
 * file named OUT_FILE. If MODE is 'd', extract, test or get
 * information about IN_FILE. OP is used only for the 'd' mode: it
 * tells if we have one or multiple file. Returns zero on success and
 * one on error. */
static int
zstandard(char *in_file, char *out_file, const char mode, const char op)
{
	int exit_status = FUNC_SUCCESS;
	char *deq_file = unescape_str(in_file, 0);
	if (!deq_file) {
		xerror(_("archiver: '%s': Error unescaping filename\n"), in_file);
		return FUNC_FAILURE;
	}

	if (mode == 'c') {
		if (out_file) {
			char *cmd[] = {"zstd", "-zo", out_file, deq_file, NULL};
			if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
				exit_status = FUNC_FAILURE;
		} else {
			char *cmd[] = {"zstd", "-z", deq_file, NULL};

			if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
				exit_status = FUNC_FAILURE;
		}

		free(deq_file);
		return exit_status;
	}

	/* mode == 'd' */

	/* op is non-zero when multiple files, including at least one
	 * zst file, are passed to the archiver function. */
	if (op != 0) {
		char option[3] = "";

		switch (op) {
		case 'e': xstrsncpy(option, "-d", sizeof(option)); break;
		case 't': xstrsncpy(option, "-t", sizeof(option)); break;
		case 'i': xstrsncpy(option, "-l", sizeof(option)); break;
		default: break;
		}

		char *cmd[] = {"zstd", option, deq_file, NULL};
		exit_status = launch_execv(cmd, FOREGROUND, E_NOFLAG);
		free(deq_file);

		if (exit_status != FUNC_SUCCESS)
			return FUNC_FAILURE;

		return FUNC_SUCCESS;
	}

	printf(_("%s[e]%sxtract %s[t]%sest %s[i]%snfo %s[q]%suit\n"),
	    BOLD, df_c, BOLD, df_c, BOLD, df_c, BOLD, df_c);

	char *operation = (char *)NULL;
	while (!operation) {
		operation = rl_no_hist(_("Operation: "), 0);
		if (!operation)
			continue;
		if (!*operation || operation[1] != '\0') {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch (*operation) {
		case 'e': {
			char *cmd[] = {"zstd", "-d", deq_file, NULL};
			if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
				exit_status = FUNC_FAILURE;
		} break;

		case 't': {
			char *cmd[] = {"zstd", "-t", deq_file, NULL};
			if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
				exit_status = FUNC_FAILURE;
		} break;

		case 'i': {
			char *cmd[] = {"zstd", "-l", deq_file, NULL};
			if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
				exit_status = FUNC_FAILURE;
		} break;

		case 'q':
			free(operation);
			free(deq_file);
			return FUNC_SUCCESS;

		default:
			free(operation);
			operation = (char *)NULL;
			break;
		}
	}

	free(operation);
	free(deq_file);
	return exit_status;
}

static int
compress_zstandard(char *name, char **args)
{
	if (!args[2]) /* Only one file */
		return zstandard(args[1], name, 'c', 0);

	int exit_status = FUNC_SUCCESS;

	/* Multiple files */
	printf(_("\n%sNOTE%s: Zstandard does not support compression of "
		 "multiple files into a single compressed file. Instead, files "
		 "will be compressed into multiple compressed files using their "
		 "original filenames.\n"), BOLD, df_c);

	size_t i;
	for (i = 1; args[i]; i++) {
		if (zstandard(args[i], NULL, 'c', 0) != FUNC_SUCCESS)
			exit_status = FUNC_FAILURE;
	}

	return exit_status;
}

static int
compress_others(char **args, char *name)
{
	size_t n = 0, i;
	for (i = 1; args[i]; i++);

	char *ext_ok = strrchr(name, '.');
	char **tcmd = xnmalloc(3 + i + 1, sizeof(char *));
	tcmd[0] = savestring("atool", 5);
	tcmd[1] = savestring("-a", 2);
	const size_t len = strlen(name) + (!ext_ok ? 7 : 0) + 1;
	tcmd[2] = xnmalloc(len, sizeof(char *));
	snprintf(tcmd[2], len, "%s%s", name, !ext_ok ? ".tar.gz" : "");
	n += 3;

	for (i = 1; args[i]; i++) {
		char *p = unescape_str(args[i], 0);
		if (!p)
			continue;
		tcmd[n] = savestring(p, strlen(p));
		free(p);
		n++;
	}
	tcmd[n] = (char *)NULL;

	const int ret = launch_execv(tcmd, FOREGROUND, E_NOFLAG);

	for (i = 0; tcmd[i]; i++)
		free(tcmd[i]);
	free(tcmd);

	return ret;
}

static int
compress_files(char **args)
{
	int exit_status = FUNC_SUCCESS;

	char *name = get_archive_filename();
	if (!name)
		return exit_status;

	char *ret = strrchr(name, '.');

	/* # ZSTANDARD # */
	if (ret && strcmp(ret, ".zst") == 0) {
		exit_status = compress_zstandard(name, args);
		free(name);
		return exit_status;
	}

	/* # ISO 9660 # */
	if (ret && strcmp(ret, ".iso") == 0) {
		exit_status = create_iso(args[1], name);
		free(name);
		return exit_status;
	}

	/* # OTHER FORMATS # */
	exit_status = compress_others(args, name);
	free(name);

	return exit_status;
}

/* Return one if at least one non-compressed file is found, zero otherwise */
static int
check_not_compressed(char **args)
{
	size_t i;
	for (i = 1; args[i]; i++) {
		char *deq = (char *)NULL;
		if (strchr(args[i], '\\')) {
			deq = unescape_str(args[i], 0);
			xstrsncpy(args[i], deq, strlen(deq) + 1);
			free(deq);
		}

		if (is_compressed(args[i], 1) != 0) {
			xerror(_("archiver: '%s': Not an archive/compressed file\n"), args[i]);
			return 1;
		}
	}

	return 0;
}

/* Check whether we have at least one Zstandard file */
static size_t
check_zstandard(char **args)
{
	size_t zst = 0, i;

	for (i = 1; args[i]; i++) {
		char *mime = xmagic(args[i], MIME_TYPE);
		if (!mime || !*mime)
			continue;
		if (*mime == 'a' && strcmp(mime, "application/zstd") == 0) {
			zst = 1;
			free(mime);
			break;
		}
		free(mime);
	}

	return zst;
}

static char
get_zstandard_operation(void)
{
	printf(_("%sNOTE%s: Using Zstandard\n"), BOLD, df_c);
	printf(_("%s[e]%sxtract %s[t]%sest %s[i]%snfo %s[q]%suit\n"),
	    BOLD, df_c, BOLD, df_c, BOLD, df_c, BOLD, df_c);

	char sel_op = 0;
	char *operation = (char *)NULL;

	while (!operation) {
		operation = rl_no_hist(_("Operation: "), 0);
		if (!operation)
			continue;
		if (!*operation || operation[1] != '\0') {
			free(operation);
			operation = (char *)NULL;
			continue;
		}

		switch (*operation) {
		case 'e': /* fallthrough */
		case 't': /* fallthrough */
		case 'i': sel_op = *operation; break;
		case 'q': break;

		default:
			free(operation);
			operation = (char *)NULL;
			continue;
		}
	}

	free(operation);
	return sel_op;
}

static int
decompress_zstandard(char **args)
{
	int exit_status = FUNC_SUCCESS;

	size_t files_num = 0, i;
	for (i = 1; args[i]; i++)
		files_num++;

	if (files_num == 1) {
		if (zstandard(args[1], NULL, 'd', 0) != FUNC_SUCCESS)
			exit_status = FUNC_FAILURE;
		return exit_status;
	}

	/* Multiple files */
	char sel_op = get_zstandard_operation();
	if (sel_op == 0) /* quit */
		return FUNC_SUCCESS;

	for (i = 1; args[i]; i++) {
		if (zstandard(args[i], NULL, 'd', sel_op) != FUNC_SUCCESS)
			exit_status = FUNC_FAILURE;
	}

	return exit_status;
}

static int
list_others(char **args)
{
	int exit_status = FUNC_SUCCESS;

	size_t i;
	for (i = 1; args[i]; i++) {
		printf(_("%s%sFile%s: %s\n"), (i > 1) ? "\n" : "", BOLD, df_c, args[i]);

		char *cmd[] = {"atool", "-l", args[i], NULL};
		if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
			exit_status = FUNC_FAILURE;
	}

	return exit_status;
}

static int
extract_to_dir_others(char **args)
{
	int exit_status = FUNC_SUCCESS;

	size_t i;
	for (i = 1; args[i]; i++) {
		/* Ask for extraction path */
		printf(_("%sFile%s: %s\n"), BOLD, df_c, args[i]);

		char *ext_path = get_extraction_path();
		if (!ext_path)
			break;

		/* Construct and execute cmd */
		char *cmd[] = {"atool", "-X", ext_path, args[i], NULL};
		if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
			exit_status = FUNC_FAILURE;

		free(ext_path);
	}

	return exit_status;
}

static int
extract_others(char **args)
{
	char **tcmd = (char **)NULL;
	size_t n = 0, i;

	for (i = 1; args[i]; i++);

	/* Construct the cmd */
	tcmd = xnmalloc(3 + i + 1, sizeof(char *));
	tcmd[0] = savestring("atool", 5);
	tcmd[1] = savestring("-x", 2);
	tcmd[2] = savestring("-e", 2);
	n += 3;
	for (i = 1; args[i]; i++) {
		tcmd[n] = savestring(args[i], strlen(args[i]));
		n++;
	}
	tcmd[n] = (char *)NULL;

	/* Launch it */
	int exit_status = FUNC_SUCCESS;
	if (launch_execv(tcmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
		exit_status = FUNC_FAILURE;

	for (i = 0; tcmd[i]; i++)
		free(tcmd[i]);
	free(tcmd);

	return exit_status;
}

static char *
get_repack_format(void)
{
	puts(_("Enter 'q' to quit"));

	char *format = (char *)NULL;
	while (!format) {
		format = rl_no_hist(_("New format (e.g.: .tar.xz): "), 0);
		if (!format)
			continue;

		/* Do not allow any of these characters (mitigate command injection) */
		const char invalid_c[] = " \t\n\"\\/'`@$><=;|&{([*?!%";
		if (!*format || (*format != '.' && *format != 'q')
		|| strpbrk(format, invalid_c)) {
			free(format);
			format = (char *)NULL;
			continue;
		}

		if (*format == 'q' && format[1] == '\0') {
			free(format);
			return (char *)NULL;
		}
	}

	return format;
}

static int
repack_others(char **args)
{
	/* Ask for new archive/compression format */
	char *format = get_repack_format();
	if (!format) /* quit */
		return FUNC_SUCCESS;

	/* Construct and execute the cmd */
	size_t n = 0, i;
	for (i = 1; args[i]; i++);

	char **tcmd = xnmalloc(4 + i + 1, sizeof(char *));
	tcmd[0] = savestring("arepack", 7);
	tcmd[1] = savestring("-F", 2);
	tcmd[2] = savestring(format, strlen(format));
	tcmd[3] = savestring("-e", 2);
	n += 4;
	for (i = 1; args[i]; i++) {
		tcmd[n] = savestring(args[i], strlen(args[i]));
		n++;
	}
	tcmd[n] = (char *)NULL;

	int exit_status = FUNC_SUCCESS;
	if (launch_execv(tcmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS)
		exit_status = FUNC_FAILURE;

	for (i = 0; tcmd[i]; i++)
		free(tcmd[i]);
	free(tcmd);
	free(format);

	return exit_status;
}

static int
list_mounted_files(char *mountpoint)
{
	if (xchdir(mountpoint, SET_TITLE) == -1) {
		xerror("archiver: '%s': %s\n", mountpoint, strerror(errno));
		return FUNC_FAILURE;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(mountpoint, strlen(mountpoint));
	add_to_jumpdb(workspaces[cur_ws].path);

	int exit_status = FUNC_SUCCESS;
	if (conf.autols == 1) {
		reload_dirlist();
		add_to_dirhist(workspaces[cur_ws].path);
	}

	return exit_status;
}

static int
mount_others(char **args)
{
	int exit_status = FUNC_SUCCESS;
	size_t files_num = 0, i;
	for (i = 1; args[i]; i++)
		files_num++;

	for (i = 1; args[i]; i++) {
		char *mountpoint = create_mountpoint(args[i]);
		if (!mountpoint)
			continue;

		/* Construct and execute cmd */
		char *cmd[] = {"archivemount", args[i], mountpoint, NULL};
		if (launch_execv(cmd, FOREGROUND, E_NOFLAG) != FUNC_SUCCESS) {
			free(mountpoint);
			continue;
		}

		/* List content of mountpoint if there is only one archive */
		if (files_num > 1) {
			printf(_("%s%s%s: Succesfully mounted on %s\n"),
				BOLD, args[i], df_c, mountpoint);
			free(mountpoint);
			continue;
		}

		if (list_mounted_files(mountpoint) == FUNC_FAILURE)
			exit_status = FUNC_FAILURE;
		free(mountpoint);
	}

	return exit_status;
}

static int
decompress_others(char **args)
{
	printf(_("%s[e]%sxtract %s[E]%sxtract-to-dir %s[l]%sist "
		 "%s[m]%sount %s[r]%sepack %s[q]%suit\n"), BOLD, df_c, BOLD,
	    df_c, BOLD, df_c, BOLD, df_c, BOLD, df_c, BOLD, df_c);

	char sel_op = get_operation(OP_OTHERS);

	switch (sel_op) {
	case 'e': return extract_others(args);
	case 'E': return extract_to_dir_others(args);
	case 'l': return list_others(args);
	case 'm': return mount_others(args);
	case 'r': return repack_others(args);
	default: break;
	}

	return FUNC_SUCCESS;
}

static int
decompress_files(char **args)
{
	if (check_not_compressed(args) == 1)
		return FUNC_FAILURE;

	/* # ISO 9660 # */
	const char *ret = strrchr(args[1], '.');
	if ((ret && strcmp(ret, ".iso") == 0) || check_iso(args[1]) == 0)
		return handle_iso(args[1]);

	/* Check if we have at least one Zstandard file */
	const size_t zst = check_zstandard(args);

	/* # ZSTANDARD # */
	if (zst == 1)
		return decompress_zstandard(args);

	/* # OTHER FORMATS # */
	return decompress_others(args);

}

/* Handle archives and/or compressed files (ARGS) according to MODE:
 * 'c' for archiving/compression, and 'd' for dearchiving/decompression
 * (including listing, extracting, repacking, and mounting). Returns
 * zero on success and one on error. Depends on 'zstd' for Zdtandard
 * files, 'atool' and 'archivemount' for the remaining types. */
int
archiver(char **args, const char mode)
{
	if (!args[1])
		return FUNC_FAILURE;

	if (mode == 'c')
		return compress_files(args);

	/* Decompression: mode == 'd' */
	return decompress_files(args);
}

#else
void *_skip_me_archiving;
#endif /* !_NO_ARCHIVING */

