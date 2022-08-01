/* archives.c -- archiving functions */

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
#ifndef _NO_ARCHIVING

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <readline/readline.h>

#include "aux.h"
#include "exec.h"
#include "history.h"
#include "jump.h"
#include "listing.h"
#include "misc.h"
#include "navigation.h"
#include "readline.h"
#include "checks.h"

#ifndef _NO_MAGIC
#include "mime.h"
#endif

#define OP_ISO    1
#define OP_OTHERS 0

#define QUERY_ARCHIVE 1
#define QUERY_ISO     0

static char *
ask_user_for_path(void)
{
	char *ext_path = (char *)NULL;

	while (!ext_path) {
		int bk = warning_prompt;
		warning_prompt = 0;
		ext_path = readline(_("Extraction path ('q' to quit): "));
		warning_prompt = bk;
		if (!ext_path)
			continue;
		if (!*ext_path) {
			free(ext_path);
			ext_path = (char *)NULL;
			continue;
		}
	}

	return ext_path;
}

static char *
get_extraction_path(void)
{
	char *ext_path = ask_user_for_path();
	if (!ext_path)
		return (char *)NULL;

	if (!*ext_path) {
		free(ext_path);
		return (char *)NULL;
	}

	if (TOUPPER(*ext_path) == 'Q' && !*(ext_path + 1)) {
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

	if (*ext_path == '.') {
		char *p = realpath(ext_path, NULL);
		if (p) {
			free(ext_path);
			return p;
		}
	}

	return ext_path;
}

static char get_operation(const int mode)
{
	char sel_op = 0;
	char *op = (char *)NULL;
	while (!op) {
		op = rl_no_hist(_("Operation: "));
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
			return EXIT_SUCCESS;

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
	int exit_status = EXIT_SUCCESS;

	/* 7z x -oDIR FILE (use FILE as DIR) */
	char *o_option = (char *)xnmalloc(strlen(file) + 7, sizeof(char));
	sprintf(o_option, "-o%s.dir", file); /* NOLINT */

	/* Construct and execute cmd */
	char *cmd[] = {"7z", "x", o_option, file, NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

	free(o_option);
	return exit_status;
}

static int
extract_iso_to_dir(char *file)
{
	/* 7z x -oDIR FILE (ask for DIR) */
	int exit_status = EXIT_SUCCESS;

	char *ext_path = get_extraction_path();
	if (!ext_path)
		return EXIT_FAILURE;

	char *o_option = (char *)xnmalloc(strlen(ext_path) + 3, sizeof(char));
	sprintf(o_option, "-o%s", ext_path); /* NOLINT */
	free(ext_path);

	/* Construct and execute cmd */
	char *cmd[] = {"7z", "x", o_option, file, NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

	free(o_option);

	return exit_status;
}

static int
list_iso_contents(char *file)
{
	int exit_status = EXIT_SUCCESS;

	/* 7z l FILE */
	char *cmd[] = {"7z", "l", file, NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

	return exit_status;
}

static int
test_iso(char *file)
{
	int exit_status = EXIT_SUCCESS;

	/* 7z t FILE */
	char *cmd[] = {"7z", "t", file, NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

	return exit_status;
}

/* Create mountpoint for file. Returns the path to the mountpoint in case
 * of success, or NULL in case of error */
static char *
create_mountpoint(char *file)
{
	char *mountpoint = (char *)NULL;
	char *p = strrchr(file, '/');
	char *tfile = (p && *(++p)) ? p : file;

	if (xargs.stealth_mode == 1) {
		mountpoint = (char *)xnmalloc(strlen(tfile) + P_tmpdir_len + 15,
					sizeof(char));
		sprintf(mountpoint, "%s/clifm-mounts/%s", P_tmpdir, tfile); /* NOLINT */
	} else {
		mountpoint = (char *)xnmalloc(config_dir_len + strlen(tfile) + 9,
					sizeof(char));
		sprintf(mountpoint, "%s/mounts/%s", config_dir, tfile); /* NOLINT */
	}

	char *dir_cmd[] = {"mkdir", "-pm700", mountpoint, NULL};
	if (launch_execve(dir_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
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
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: %s: %s\n", mountpoint, strerror(errno));
		return EXIT_FAILURE;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(mountpoint, strlen(mountpoint));
	add_to_jumpdb(workspaces[cur_ws].path);

	int exit_status = EXIT_SUCCESS;
	if (autols == 1) {
		reload_dirlist();
		add_to_dirhist(workspaces[cur_ws].path);
	} else {
		printf("%s: Successfully mounted on %s\n", file, mountpoint);
	}

	return exit_status;
}
#endif /* __linux__ */

static int
mount_iso(char *file)
{
#if !defined(__linux__)
	UNUSED(file);
	fprintf(stderr, "mount: This feature is for Linux only\n");
	return EXIT_SUCCESS;
#else
	char *mountpoint = create_mountpoint(file);
	if (!mountpoint)
		return EXIT_FAILURE;

	/* Construct and execute the cmd */
	char *sudo = get_sudo_path();
	if (!sudo) {
		free(mountpoint);
		return EXIT_FAILURE;
	}

	char *cmd[] = {sudo, "mount", "-o", "loop", file, mountpoint, NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
		free(mountpoint);
		free(sudo);
		return EXIT_FAILURE;
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

	char *ret = strchr(file, '\\');
	if (ret) {
		char *deq_file = dequote_str(file, 0);
		if (deq_file) {
			strcpy(file, deq_file); /* NOLINT */
			free(deq_file);
		}
		ret = (char *)NULL;
	}

	switch (sel_op) {
	case 'e': return extract_iso(file);
	case 'E': return extract_iso_to_dir(file);
	case 'l': return list_iso_contents(file);
	case 'm': return mount_iso(file);
	case 't': return test_iso(file);
	default: return EXIT_SUCCESS;
	}
}

static int
create_iso_from_block_dev(char *in_file, char *out_file)
{
	char *if_option = (char *)xnmalloc(strlen(in_file) + 4, sizeof(char));
	sprintf(if_option, "if=%s", in_file); /* NOLINT */

	char *of_option = (char *)xnmalloc(strlen(out_file) + 4, sizeof(char));
	sprintf(of_option, "of=%s", out_file); /* NOLINT */

	char *sudo = get_sudo_path();
	if (!sudo) {
		free(if_option);
		free(of_option);
		return EXIT_FAILURE;
	}

	int exit_status = EXIT_SUCCESS;
	char *cmd[] = {sudo, "dd", if_option, of_option, "bs=64k",
	    "conv=noerror,sync", "status=progress", NULL};
	if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

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
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: %s: %s\n", in_file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* If IN_FILE is a directory */
	if (S_ISDIR(attr.st_mode)) {
		char *cmd[] = {"mkisofs", "-R", "-o", out_file, in_file, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	/* If IN_FILE is a block device */
	if (S_ISBLK(attr.st_mode))
		return create_iso_from_block_dev(in_file, out_file);

	/* If any other file format */
	_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: %s: Invalid file format. File should "
			"be either a directory or a block device\n", in_file);
	return EXIT_FAILURE;
}

/* Run the 'file' command on FILE and look for "ISO 9660" and
 * string in its output. Returns zero if found, one if not, and -1
 * in case of error */
static int
check_iso(char *file)
{
	if (!file || !*file) {
		fputs(_("Error querying file type\n"), stderr);
		return (-1);
	}

	int is_iso = 0;

#ifndef _NO_MAGIC
	char *t = xmagic(file, TEXT_DESC);
	if (!t) {
		fputs(_("Error querying file type\n"), stderr);
		return (-1);
	}

	char *ret = strstr(t, "ISO 9660");
	if (ret)
		is_iso = 1;

	free(t);
#else
	char *rand_ext = gen_rand_str(6);
	if (!rand_ext)
		return (-1);

	char iso_tmp_file[PATH_MAX];
	if (xargs.stealth_mode == 1)
		snprintf(iso_tmp_file, PATH_MAX - 1, "%s/.temp%s", P_tmpdir, rand_ext);
	else
		snprintf(iso_tmp_file, PATH_MAX - 1, "%s/.temp%s", tmp_dir, rand_ext);
	free(rand_ext);

	int fd;
	FILE *fp = open_fstream_w(iso_tmp_file, &fd);
	if (!fp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: %s: %s\n", iso_tmp_file, strerror(errno));
		return (-1);
	}

	FILE *fpp = fopen("/dev/null", "w");
	if (!fpp) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: /dev/null: %s\n", strerror(errno));
		close_fstream(fp, fd);
		return (-1);
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(fp), STDOUT_FILENO) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: %s\n", strerror(errno));
		close_fstream(fp, fd);
		fclose(fpp);
		return (-1);
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(fpp), STDERR_FILENO) == -1) {
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: %s\n", strerror(errno));
		close_fstream(fp, fd);
		fclose(fpp);
		return (-1);
	}

	close_fstream(fp, fd);
	fclose(fpp);

	char *cmd[] = {"file", "-b", file, NULL};
	int retval = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (retval != EXIT_SUCCESS)
		return (-1);

	if (access(iso_tmp_file, F_OK) == 0) {
		fp = open_fstream_r(iso_tmp_file, &fd);
		if (fp) {
			char line[255] = "";
			if (fgets(line, (int)sizeof(line), fp) == NULL) {
				close_fstream(fp, fd);
				unlink(iso_tmp_file);
				return EXIT_FAILURE;
			}
			char *ret = strstr(line, "ISO 9660");
			if (ret)
				is_iso = 1;
			close_fstream(fp, fd);
		}
		unlink(iso_tmp_file);
	}
#endif /* !_NO_MAGIC */

	if (is_iso)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

/* Run the 'file' command on FILE and look for "archive" and
 * "compressed" strings in its output. Returns zero if compressed,
 * one if not, and -1 in case of error.
 * test_iso is used to determine if ISO files should be checked as
 * well: this is the case when called from open_function() or
 * mime_open(), since both need to check compressed and ISOs as
 * well (and there is no need to run two functions (is_compressed and
 * check_iso), when we can run just one) */
int
is_compressed(char *file, int test_iso)
{
	if (!file || !*file) {
		fputs(_("Error querying file type\n"), stderr);
		return (-1);
	}

	int compressed = 0;

#ifndef _NO_MAGIC
	char *t = xmagic(file, TEXT_DESC);
	if (!t) {
		fputs(_("Error querying file type\n"), stderr);
		return (-1);
	}

	char *ret = strstr(t, "archive");
	if (ret) {
		compressed = 1;
	} else {
		ret = strstr(t, "compressed");
		if (ret) {
			compressed = 1;
		} else {
			if (test_iso) {
				ret = strstr(t, "ISO 9660");
				if (ret)
					compressed = 1;
			}
		}
	}

	free(t);
#else
	char *rand_ext = gen_rand_str(6);
	if (!rand_ext)
		return (-1);

	char archiver_tmp_file[PATH_MAX];
	if (xargs.stealth_mode == 1)
		snprintf(archiver_tmp_file, PATH_MAX - 1, "%s/.clifm%s", P_tmpdir, rand_ext);
	else
		snprintf(archiver_tmp_file, PATH_MAX - 1, "%s/.clifm%s", tmp_dir, rand_ext);
	free(rand_ext);

	int fd;
	FILE *fp = open_fstream_w(archiver_tmp_file, &fd);
	if (!fp) {
//		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, archiver_tmp_file, strerror(errno));
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: %s: %s\n", archiver_tmp_file, strerror(errno));
		return (-1);
	}

	FILE *fpp = fopen("/dev/null", "w");
	if (!fpp) {
//		fprintf(stderr, "%s: /dev/null: %s\n", PROGRAM_NAME, strerror(errno));
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: /dev/null: %s\n", strerror(errno));
		close_fstream(fp, fd);
		return (-1);
	}

	int stdout_bk = dup(STDOUT_FILENO); /* Store original stdout */
	int stderr_bk = dup(STDERR_FILENO); /* Store original stderr */

	/* Redirect stdout to the desired file */
	if (dup2(fileno(fp), STDOUT_FILENO) == -1) {
//		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: %s\n", strerror(errno));
		close_fstream(fp, fd);
		fclose(fpp);
		return (-1);
	}

	/* Redirect stderr to /dev/null */
	if (dup2(fileno(fpp), STDERR_FILENO) == -1) {
//		fprintf(stderr, "%s: %s\n", PROGRAM_NAME, strerror(errno));
		_err(ERR_NO_STORE, NOPRINT_PROMPT, "archiver: %s\n", strerror(errno));
		close_fstream(fp, fd);
		fclose(fpp);
		return -1;
	}

	close_fstream(fp, fd);
	fclose(fpp);

	char *cmd[] = {"file", "-b", file, NULL};
	int retval = launch_execve(cmd, FOREGROUND, E_NOFLAG);

	dup2(stdout_bk, STDOUT_FILENO); /* Restore original stdout */
	dup2(stderr_bk, STDERR_FILENO); /* Restore original stderr */
	close(stdout_bk);
	close(stderr_bk);

	if (retval != EXIT_SUCCESS)
		return (-1);

	if (access(archiver_tmp_file, F_OK) == 0) {
		fp = open_fstream_r(archiver_tmp_file, &fd);
		if (fp) {
			char line[255];
			if (fgets(line, (int)sizeof(line), fp) == NULL) {
				close_fstream(fp, fd);
				unlink(archiver_tmp_file);
				return EXIT_FAILURE;
			}
			char *ret = strstr(line, "archive");

			if (ret) {
				compressed = 1;
			} else {
				ret = strstr(line, "compressed");
				if (ret) {
					compressed = 1;
				} else {
					if (test_iso) {
						ret = strstr(line, "ISO 9660");
						if (ret)
							compressed = 1;
					}
				}
			}

			close_fstream(fp, fd);
		}

		unlink(archiver_tmp_file);
	}
#endif /* !_NO_MAGIC */

	if (compressed)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

static char *
add_default_extension(char *name)
{
	size_t name_len = strlen(name);

	char *t = savestring(name, name_len);
	name = (char *)xrealloc(name, (name_len + 8) * sizeof(char));
	sprintf(name, "%s.tar.gz", t); /* NOLINT */
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
		name = rl_no_hist(_("File name ('q' to quit): "));
		if (!name)
			continue;
		if (!*name) {
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
			fprintf(stderr, _("Invalid file name\n"));
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
 * one on error */
static int
zstandard(char *in_file, char *out_file, char mode, char op)
{
	int exit_status = EXIT_SUCCESS;
	char *deq_file = dequote_str(in_file, 0);
	if (!deq_file) {
//		fprintf(stderr, _("archiver: %s: Error dequoting file name\n"), in_file);
		_err(ERR_NO_STORE, NOPRINT_PROMPT, _("archiver: %s: Error dequoting file name\n"),
			in_file);
		return EXIT_FAILURE;
	}

	if (mode == 'c') {
		if (out_file) {
			char *cmd[] = {"zstd", "-zo", out_file, deq_file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} else {
			char *cmd[] = {"zstd", "-z", deq_file, NULL};

			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		free(deq_file);
		return exit_status;
	}

	/* mode == 'd' */

	/* op is non-zero when multiple files, including at least one
	 * zst file, are passed to the archiver function */
	if (op != 0) {
		char option[3] = "";

		switch (op) {
		case 'e': strcpy(option, "-d"); break;
		case 't': strcpy(option, "-t"); break;
		case 'i': strcpy(option, "-l"); break;
		default: break;
		}

		char *cmd[] = {"zstd", option, deq_file, NULL};
		exit_status = launch_execve(cmd, FOREGROUND, E_NOFLAG);
		free(deq_file);

		if (exit_status != EXIT_SUCCESS)
			return EXIT_FAILURE;

		return EXIT_SUCCESS;
	}

	printf(_("%s[e]%sxtract %s[t]%sest %s[i]%snfo %s[q]%suit\n"),
	    BOLD, df_c, BOLD, df_c, BOLD, df_c, BOLD, df_c);

	char *operation = (char *)NULL;
	while (!operation) {
		operation = rl_no_hist(_("Operation: "));
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
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} break;

		case 't': {
			char *cmd[] = {"zstd", "-t", deq_file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} break;

		case 'i': {
			char *cmd[] = {"zstd", "-l", deq_file, NULL};
			if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		} break;

		case 'q':
			free(operation);
			free(deq_file);
			return EXIT_SUCCESS;

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

	int exit_status = EXIT_SUCCESS;

	/* Multiple files */
	printf(_("\n%sNOTE%s: Zstandard does not support compression of "
		 "multiple files into one single compressed file. Files will "
		 "be compressed rather into multiple compressed files using the "
		 "original file names\n"), BOLD, df_c);

	size_t i;
	for (i = 1; args[i]; i++) {
		if (zstandard(args[i], NULL, 'c', 0) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
compress_others(char **args, char *name)
{
	size_t n = 0, i;
	for (i = 1; args[i]; i++);

	char *ext_ok = strrchr(name, '.');
	char **tcmd = (char **)xnmalloc(3 + i + 1, sizeof(char *));
	tcmd[0] = savestring("atool", 5);
	tcmd[1] = savestring("-a", 2);
	tcmd[2] = (char *)xnmalloc(strlen(name) + (!ext_ok ? 7 : 0) + 1, sizeof(char *));
	sprintf(tcmd[2], "%s%s", name, !ext_ok ? ".tar.gz" : ""); /* NOLINT */
	n += 3;

	for (i = 1; args[i]; i++) {
		char *p = dequote_str(args[i], 0);
		if (!p)
			continue;
		tcmd[n] = savestring(p, strlen(p));
		free(p);
		n++;
	}
	tcmd[n] = (char *)NULL;

	int ret = launch_execve(tcmd, FOREGROUND, E_NOFLAG);

	for (i = 0; tcmd[i]; i++)
		free(tcmd[i]);
	free(tcmd);

	return ret;
}

static int
compress_files(char **args)
{
	int exit_status = EXIT_SUCCESS;

	char *name = get_archive_filename();
	if (!name)
		return exit_status;

	/* # ZSTANDARD # */
	char *ret = strrchr(name, '.');
	if (strcmp(ret, ".zst") == 0) {
		exit_status = compress_zstandard(name, args);
		free(name);
		return exit_status;
	}

	/* # ISO 9660 # */
	if (strcmp(ret, ".iso") == 0) {
		exit_status = create_iso(args[1], name);
		free(name);
		return exit_status;
	}

	/* # OTHER FORMATS # */
	/* Construct the command */
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
			deq = dequote_str(args[i], 0);
			strcpy(args[i], deq); /* NOLINT */
			free(deq);
		}

		if (is_compressed(args[i], 1) != 0) {
			_err(ERR_NO_STORE, NOPRINT_PROMPT,
				_("archiver: %s: Not an archive/compressed file\n"), args[i]);
			return 1;
		}
	}

	return 0;
}

/* Check if we have at least one Zstandard file */
static size_t
check_zstandard(char **args)
{
	size_t zst = 0, i;

	for (i = 1; args[i]; i++) {
		if (args[i][strlen(args[i]) - 1] != 't')
			continue;
		char *ret = strrchr(args[i], '.');
		if (ret && strcmp(ret, ".zst") == 0) {
			zst = 1;
			break;
		}
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
		operation = rl_no_hist(_("Operation: "));
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
	int exit_status = EXIT_SUCCESS;

	size_t files_num = 0, i;
	for (i = 1; args[i]; i++)
		files_num++;

	if (files_num == 1) {
		if (zstandard(args[1], NULL, 'd', 0) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
		return exit_status;
	}

	/* Multiple files */
	char sel_op = get_zstandard_operation();
	if (sel_op == 0) /* quit */
		return EXIT_SUCCESS;

	for (i = 1; args[i]; i++) {
		if (zstandard(args[i], NULL, 'd', sel_op) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
list_others(char **args)
{
	int exit_status = EXIT_SUCCESS;

	size_t i;
	for (i = 1; args[i]; i++) {
		printf(_("%s%sFile%s: %s\n"), (i > 1) ? "\n" : "", BOLD, df_c, args[i]);

		char *cmd[] = {"atool", "-l", args[i], NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

static int
extract_to_dir_others(char **args)
{
	int exit_status = EXIT_SUCCESS;

	size_t i;
	for (i = 1; args[i]; i++) {
		/* Ask for extraction path */
		printf(_("%sFile%s: %s\n"), BOLD, df_c, args[i]);

		char *ext_path = get_extraction_path();
		if (!ext_path)
			break;

		/* Construct and execute cmd */
		char *cmd[] = {"atool", "-X", ext_path, args[i], NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;

		free(ext_path);
		ext_path = (char *)NULL;
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
	tcmd = (char **)xnmalloc(3 + i + 1, sizeof(char *));
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
	int exit_status = EXIT_SUCCESS;
	if (launch_execve(tcmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

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
		format = rl_no_hist(_("New format (ex: .tar.xz): "));
		if (!format)
			continue;

		/* Do not allow any of these characters (mitigate command injection) */
		char invalid_c[] = " \t\n\"\\/'`@$><=;|&{([*?!%";
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
		return EXIT_SUCCESS;

	/* Construct and execute the cmd */
	size_t n = 0, i;
	for (i = 1; args[i]; i++);

	char **tcmd = (char **)xnmalloc(4 + i + 1, sizeof(char *));
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

	int exit_status = EXIT_SUCCESS;
	if (launch_execve(tcmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;

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
		fprintf(stderr, "archiver: %s: %s\n", mountpoint, strerror(errno));
		return EXIT_FAILURE;
	}

	free(workspaces[cur_ws].path);
	workspaces[cur_ws].path = savestring(mountpoint, strlen(mountpoint)); /* NOLINT */
	add_to_jumpdb(workspaces[cur_ws].path);

	int exit_status = EXIT_SUCCESS;
	if (autols) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
		add_to_dirhist(workspaces[cur_ws].path);
	}

	return exit_status;
}

static int
mount_others(char **args)
{
	int exit_status = EXIT_SUCCESS;
	size_t files_num = 0, i;
	for (i = 1; args[i]; i++)
		files_num++;

	for (i = 1; args[i]; i++) {
		char *mountpoint = create_mountpoint(args[i]);
		if (!mountpoint)
			continue;

		/* Construct and execute cmd */
		char *cmd[] = {"archivemount", args[i], mountpoint, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS) {
			free(mountpoint);
			continue;
		}

		/* List content of mountpoint if there is only one archive */
		if (files_num > 1) {
			printf(_("%s%s%s: Succesfully mounted on %s\n"), BOLD, args[i], df_c, mountpoint);
			free(mountpoint);
			continue;
		}

		if (list_mounted_files(mountpoint) == EXIT_FAILURE)
			exit_status = EXIT_FAILURE;
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

	return EXIT_SUCCESS;
}

static int
decompress_files(char **args)
{
	if (check_not_compressed(args) == 1)
		return EXIT_FAILURE;

	/* # ISO 9660 # */
	char *ret = strrchr(args[1], '.');
	if ((ret && strcmp(ret, ".iso") == 0) || check_iso(args[1]) == 0)
		return handle_iso(args[1]);

	/* Check if we have at least one Zstandard file */
	size_t zst = check_zstandard(args);

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
archiver(char **args, char mode)
{
	if (!args[1])
		return EXIT_FAILURE;

	if (mode == 'c')
		return compress_files(args);

	/* Decompression: mode == 'd' */
	return decompress_files(args);
}

#else
void *__skip_me_archiving;
#endif /* !_NO_ARCHIVING */

