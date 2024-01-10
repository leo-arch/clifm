/* trash.c -- functions to manage the trash system */

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

#ifndef _NO_TRASH

#include "helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> /* access, unlinkat */
/* <time.h>, required by time(2) and localtime_r(2), is included by "aux.h" */

#include "aux.h"        /* gen_date_suffix, count_dir, open_fwrite, open_fread,
xatoi, url_encode, xnmalloc */
#include "checks.h"     /* is_file_in_cwd, is_number */
#include "colors.h"     /* colors_list */
#include "exec.h"       /* launch_execv */
#include "listing.h"    /* reload_dirlist */
#include "misc.h"       /* xerror, print_reload_msg */
#include "navigation.h" /* xchdir */
#include "properties.h" /* get_color_size */
#include "readline.h"   /* rl_no_hist, rl_get_y_or_n */
#include "sort.h"       /* skip_files, xalphasort, alphasort_insensitive */

static size_t
count_trashed_files(void)
{
	size_t n = 0;
	if (trash_ok == 1 && trash_files_dir != NULL) {
		filesn_t ret = count_dir(trash_files_dir, NO_CPOP);
		if (ret <= 2)
			n = 0;
		else
			n = (size_t)ret - 2;
	}

	return n;
}

/* Empty the trash can. */
static int
trash_clear(void)
{
	DIR *dir = opendir(trash_files_dir);
	if (!dir) {
		xerror("trash: '%s': %s\n", trash_files_dir, strerror(errno));
		return errno;
	}

	int exit_status = EXIT_SUCCESS;
	size_t n = 0;
	struct dirent *ent;

	while ((ent = readdir(dir))) {
		if (SELFORPARENT(ent->d_name))
			continue;

		int ret = EXIT_SUCCESS;

		size_t len = strlen(ent->d_name) + 11;
		char *info_file = xnmalloc(len, sizeof(char));
		snprintf(info_file, len, "%s.trashinfo", ent->d_name);

		len = strlen(trash_files_dir) + strlen(ent->d_name) + 2;
		char *file1 = xnmalloc(len, sizeof(char));
		snprintf(file1, len, "%s/%s", trash_files_dir, ent->d_name);

		len = strlen(trash_info_dir) + strlen(info_file) + 2;
		char *file2 = xnmalloc(len, sizeof(char));
		snprintf(file2, len, "%s/%s", trash_info_dir, info_file);

		if (unlinkat(XAT_FDCWD, file1, 0) == -1) {
			ret = errno;
			xerror("trash: '%s': %s\n", file1, strerror(errno));
		}
		if (unlinkat(XAT_FDCWD, file2, 0) == -1) {
			ret = errno;
			xerror("trash: '%s': %s\n", file2, strerror(errno));
		}

		free(file1);
		free(file2);
		free(info_file);

		if (ret != EXIT_SUCCESS) {
			xerror(_("trash: '%s': Error removing trashed file\n"), ent->d_name);
			exit_status = ret;
			/* If there is at least one error, return error */
		}

		n++;
	}

	closedir(dir);

	if (n == 0) {
		puts(_("trash: No trashed files"));
	} else if (exit_status == EXIT_SUCCESS) {
		if (conf.autols == 1)
			reload_dirlist();
		print_reload_msg(_("Trash can emptied [%zu file(s) deleted]\n"), n);
	}

	return exit_status;
}

static int
del_trash_file(const char *suffix)
{
	size_t len = strlen(trash_files_dir) + strlen(suffix) + 2;
	char *tfile = xnmalloc(len, sizeof(char));
	snprintf(tfile, len, "%s/%s", trash_files_dir, suffix);

	int ret = EXIT_SUCCESS;
	if (unlinkat(XAT_FDCWD, tfile, 0) == -1) {
		ret = errno;
		xerror(_("trash: '%s': %s\nTry removing the "
			"file manually\n"), tfile, strerror(errno));
	}

	free(tfile);
	return ret;
}

static int
gen_trashinfo_file(char *file, const char *suffix, const struct tm *tm)
{
	size_t len = strlen(trash_info_dir) + strlen(suffix) + 12;
	char *info_file = xnmalloc(len, sizeof(char));
	snprintf(info_file, len, "%s/%s.trashinfo", trash_info_dir, suffix);

	int fd = 0;
	FILE *fp = open_fwrite(info_file, &fd);
	if (!fp) {
		xerror("trash: '%s': %s\n", info_file, strerror(errno));
		free(info_file);
		return del_trash_file(suffix);
	}

	/* Encode path to URL format (RF 2396) */
	char *url_str = url_encode(file);
	if (!url_str) {
		xerror(_("trash: '%s': Error encoding path\n"), file);
		free(info_file);
		return EXIT_FAILURE;
	}

	fprintf(fp,
	    "[Trash Info]\nPath=%s\nDeletionDate=%d-%02d-%02dT%02d:%02d:%02d\n",
	    url_str, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec);

	free(url_str);
	free(info_file);
	fclose(fp);

	return EXIT_SUCCESS;
}

static char *
gen_dest_file(const char *file, const char *suffix, char **file_suffix)
{
	/* Create the trashed file name: orig_filename.suffix, where SUFFIX is
	 * the current date and time (plus an integer in case of dups). */
	char *filename = strrchr(file, '/');
	if (!filename || !*(++filename)) {
		xerror(_("trash: '%s': Error getting file name\n"), file);
		return (char *)NULL;
	}

	/* If the length of the trashed file name (orig_filename.suffix) is
	 * longer than NAME_MAX (255), trim the original filename, so that
	 * (original_filename_len + 1 (dot) + suffix_len) won't be longer
	 * than NAME_MAX. */
	size_t filename_len = strlen(filename);
	size_t suffix_len = strlen(suffix);
	int size = (int)(filename_len + suffix_len + 11) - NAME_MAX;
	/* len = filename.suffix.trashinfo */

	if (size > 0) {
		/* THIS IS NOT UNICODE AWARE */
		/* If SIZE is a positive value, that is, the trashed file name
		 * exceeds NAME_MAX by SIZE bytes, reduce the original file name
		 * SIZE bytes. Terminate the original file name (FILENAME) with
		 * a tilde (~), to let the user know it was trimmed. */
		filename[filename_len - (size_t)size - 1] = '~';
		filename[filename_len - (size_t)size] = '\0';
	}

	size_t slen = filename_len + suffix_len + 2 + 16;
	*file_suffix = xnmalloc(slen, sizeof(char));
	snprintf(*file_suffix, slen, "%s.%s", filename, suffix);

	/* NOTE: It is guaranteed (by check_trash_file()) that FILE does not
	 * end with a slash. */
	size_t dlen = strlen(trash_files_dir) + strlen(*file_suffix) + 2 + 16;
	char *dest = xnmalloc(dlen, sizeof(char));
	snprintf(dest, dlen, "%s/%s", trash_files_dir, *file_suffix);

	/* If destiny file exists (there's already a trashed file with this name),
	 * append an integer until it is made unique. */
	struct stat a;
	int inc = 1;
	while (lstat(dest, &a) == 0 && inc > 0) {
		snprintf(*file_suffix, slen, "%s.%s-%d", filename, suffix, inc);
		snprintf(dest, dlen, "%s/%s", trash_files_dir, *file_suffix);
		inc++;
	}

	return dest;
}

static int
trash_file(const char *suffix, const struct tm *tm, char *file)
{
	struct stat attr;
	if (lstat(file, &attr) == -1) {
		xerror("trash: '%s': %s\n", file, strerror(errno));
		return errno;
	}

	char *tmpfile = file;
	char full_path[PATH_MAX + 1];

	if (*file != '/') { /* If relative path, make it absolute. */
		if (!workspaces[cur_ws].path)
			return EXIT_FAILURE;

		if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1]) {
			/* We're in the root dir */
			snprintf(full_path, sizeof(full_path), "/%s", file);
		} else {
			snprintf(full_path, sizeof(full_path), "%s/%s",
				workspaces[cur_ws].path, file);
		}
		tmpfile = full_path;
	}

	char *file_suffix = (char *)NULL;
	char *dest = gen_dest_file(tmpfile, suffix, &file_suffix);
	if (!dest || !file_suffix) {
		free(dest);
		free(file_suffix);
		return EXIT_FAILURE;
	}

	/* Move the original file into the trash directory. */
	int mvcmd = 0;
	int ret = renameat(XAT_FDCWD, file, XAT_FDCWD, dest);
	if (ret != EXIT_SUCCESS && errno == EXDEV) {
		/* Destination file is on a different file system, which is why
		 * renameat(2) doesn't work: let's try with mv(1). */
		char *tmp_cmd[] = {"mv", "--", file, dest, NULL};
		ret = launch_execv(tmp_cmd, FOREGROUND, E_NOFLAG);
		mvcmd = 1;
	}

	free(dest);

	if (ret != EXIT_SUCCESS) {
		if (mvcmd == 0)
			xerror(_("trash: '%s': %s\n"), file, strerror(errno));
		free(file_suffix);
		return (mvcmd == 1 ? ret : errno);
	}

	ret = gen_trashinfo_file(tmpfile, file_suffix, tm);
	free(file_suffix);

	return ret;
}

/* Remove NAME file and the corresponding .trashinfo file from the trash can */
static int
remove_file_from_trash(const char *name)
{
	char rm_file[PATH_MAX + 1];
	char rm_info[PATH_MAX + 1];
	snprintf(rm_file, sizeof(rm_file), "%s/%s", trash_files_dir, name);
	snprintf(rm_info, sizeof(rm_info), "%s/%s.trashinfo", trash_info_dir, name);

	int tmp_err = 0, err_file = 0, err_info = 0;
	struct stat a;
	if (stat(rm_file, &a) == -1) {
		xerror("trash: '%s': %s\n", rm_file, strerror(errno));
		err_file = tmp_err = errno;
	}
	if (stat(rm_info, &a) == -1) {
		if (err_file == EXIT_SUCCESS)
			xerror("trash: '%s': %s\n", rm_info, strerror(errno));
		err_info = tmp_err = errno;
	}

	if (err_file != EXIT_SUCCESS || err_info != EXIT_SUCCESS)
		return tmp_err;

	int ret = EXIT_SUCCESS;
	if (unlinkat(XAT_FDCWD, rm_file, 0) == -1) {
		ret = errno;
		xerror("trash: '%s': %s\n", rm_file, strerror(errno));
	}
	if (unlinkat(XAT_FDCWD, rm_info, 0) == -1) {
		ret = errno;
		xerror("trash: '%s': %s\n", rm_info, strerror(errno));
	}

	return ret;
}

static int
remove_from_trash_params(char **args)
{
	size_t rem_files = 0;
	size_t i;
	int exit_status = EXIT_SUCCESS;

	for (i = 0; args[i]; i++) {
		if (*args[i] == '*' && !args[i][1])
			return trash_clear();

		char *d = (char *)NULL;
		if (strchr(args[i], '\\'))
			d = unescape_str(args[i], 0);

		if (remove_file_from_trash(d ? d : args[i]) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
		else
			rem_files++;

		free(d);
	}

	if (conf.autols == 1 && exit_status == EXIT_SUCCESS)
		reload_dirlist();

	print_reload_msg(_("%zu file(s) removed from the trash can\n"), rem_files);
	print_reload_msg(_("%zu total trashed file(s)\n"), trash_n - rem_files);

	return exit_status;
}

static int
print_trashfiles(struct dirent ***ent, const int files_n)
{
	/* Let's change to the trash dir to get the correct file colors */
	if (xchdir(trash_files_dir, NO_TITLE) == -1) {
		xerror("trash: '%s': %s\n", trash_files_dir, strerror(errno));
		return EXIT_FAILURE;
	}

	printf(_("%s%sTrashed files%s\n\n"), df_c, BOLD, df_c);

	/* Enable trash suffix removal in colors_list() to get correct file
	 * color by extension. */
	flags |= STATE_COMPLETING;
	cur_comp_type = TCMP_UNTRASH;

	uint8_t tpad = DIGINUM(files_n);
	size_t i;
	for (i = 0; i < (size_t)files_n; i++) {
		printf("%s%*zu%s ", el_c, tpad, i + 1, df_c);
		colors_list((*ent)[i]->d_name, NO_ELN, NO_PAD, PRINT_NEWLINE);
	}

	flags &= ~STATE_COMPLETING;
	cur_comp_type = TCMP_NONE;

	if (xchdir(workspaces[cur_ws].path, NO_TITLE) == -1) {
		xerror("trash: '%s': %s\n", workspaces[cur_ws].path, strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static char **
list_and_get_input(struct dirent ***trash_files, const int files_n,
	const int is_undel)
{
	if (conf.clear_screen == 1)
		CLEAR;

	int ret = print_trashfiles(trash_files, files_n);
	if (ret != EXIT_SUCCESS)
		return (char **)NULL;

	/* Get input */
	printf(_("\n%sEnter 'q' to quit\n"
		"File(s) to be %s (ex: 1 2-6, or *):\n"), df_c,
		is_undel == 1 ? _("restored") : _("removed"));

	char *line = (char *)NULL;
	char tprompt[(MAX_COLOR * 2) + 7];
	snprintf(tprompt, sizeof(tprompt), "\001%s\002>\001%s\002 ", mi_c, tx_c);

	while (!line)
		line = rl_no_hist(tprompt);

	char **input = get_substr(line, ' ');
	free(line);

	return input;
}

static void
free_files_and_input(char ***input, struct dirent ***tfiles, const int tfiles_n)
{
	size_t i;
	for (i = 0; (*input)[i]; i++)
		free((*input)[i]);
	free(*input);

	for (i = 0; i < (size_t)tfiles_n; i++)
		free((*tfiles)[i]);
	free(*tfiles);
}

static size_t
remove_from_trash_all(struct dirent ***tfiles, const int tfiles_n,
	int *status)
{
	size_t n = 0;
	size_t i;

	for (i = 0; i < (size_t)tfiles_n; i++) {
		int ret = remove_file_from_trash((*tfiles)[i]->d_name);
		if (ret != EXIT_SUCCESS) {
			xerror(_("trash: '%s': Cannot remove file from the "
				"trash can\n"), (*tfiles)[i]->d_name);
			*status = EXIT_FAILURE;
		} else {
			n++;
		}
	}

	return n;
}

static struct dirent **
load_trashed_files(int *n, int *status)
{
	*status = EXIT_SUCCESS;

	struct dirent **tfiles = (struct dirent **)NULL;
	*n = scandir(trash_files_dir, &tfiles,
		skip_files, conf.unicode == 1 ? alphasort
		: (conf.case_sens_list == 1 ? xalphasort : alphasort_insensitive));

	if (*n <= -1) {
		*status = errno;
		xerror("trash: '%s': %s\n", trash_files_dir, strerror(errno));
	} else if (*n == 0) {
		puts(_("trash: No trashed files"));
	}

	return tfiles;
}

/* 'trash del' screen. */
static int
remove_from_trash(char **args)
{
	/* Remove from trash files passed as parameters */
	if (args[2])
		return remove_from_trash_params(args + 2);

	/* No parameters: list, take input, and remove */

	int files_n = 0;
	int exit_status = 0;
	struct dirent **trash_files = load_trashed_files(&files_n, &exit_status);

	if (files_n <= 0)
		return exit_status;

	exit_status = EXIT_SUCCESS;
	size_t i;

	char **input = list_and_get_input(&trash_files, files_n, 0);
	if (!input) {
		for (i = 0; i < (size_t)files_n; i++)
			free(trash_files[i]);
		free(trash_files);
		return EXIT_FAILURE;
	}

	/* Remove files */

	/* First check for exit, wildcard, and non-number args */
	for (i = 0; input[i]; i++) {
		if (strcmp(input[i], "q") == 0) { /* Quit */
			free_files_and_input(&input, &trash_files, files_n);
			if (conf.autols == 1) reload_dirlist();
			return exit_status;
		}

		if (strcmp(input[i], "*") == 0) { /* Asterisk */
			size_t n = remove_from_trash_all(&trash_files, files_n, &exit_status);
			free_files_and_input(&input, &trash_files, files_n);
			if (conf.autols == 1) reload_dirlist();
			print_reload_msg(_("%zu file(s) removed from the trash can\n"), n);
			print_reload_msg(_("%zu trashed file(s)\n"), count_trashed_files());
			return exit_status;
		}

		/* Non-number or invalid ELN */
		int num = atoi(input[i]);
		if (!is_number(input[i]) || num <= 0 || num > files_n) {
			xerror(_("trash: %s: Invalid ELN\n"), input[i]);
			free_files_and_input(&input, &trash_files, files_n);
			return EXIT_FAILURE;
		}
	}

	/* At this point we now all input fields are valid ELNs */
	/* If all args are numbers, and neither 'q' nor wildcard */
	for (i = 0; input[i]; i++) {
		int num = atoi(input[i]);

		int ret = remove_file_from_trash(trash_files[num - 1]->d_name);
		if (ret != EXIT_SUCCESS) {
			xerror(_("trash: '%s': Cannot remove file from the trash can\n"),
				trash_files[num - 1]->d_name);
			exit_status = EXIT_FAILURE;
		}
	}

	free_files_and_input(&input, &trash_files, files_n);

	size_t n = count_trashed_files();
	if (n > 0) {
		remove_from_trash(args);
	} else {
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(_("0 trashed file(s)\n"));
	}

	return exit_status;
}

/* Read original path from the trashinfo file FILE for the trashed file SRC.
 * STATUS is updated to reflect success (0) or error. */
static char *
read_original_path(const char *file, const char *src, int *status)
{
	*status = EXIT_SUCCESS;

	int fd = 0;
	FILE *fp = open_fread(file, &fd);
	if (!fp) {
		xerror(_("undel: Info file for '%s' not found. Try restoring "
			"the file manually.\n"), src);
		*status = errno;
		return (char *)NULL;
	}

	char *orig_path = (char *)NULL;
	/* The max length for line is: Path=(5) + PATH_MAX + \n(1) */
	char line[PATH_MAX + 6]; *line = '\0';

	while (fgets(line, (int)sizeof(line), fp)) {
		if (*line == 'P' && strncmp(line, "Path=", 5) == 0) {
			char *p = strchr(line, '=');
			if (!p || !*(++p))
				break;
			orig_path = savestring(p, strnlen(p, sizeof(line) - 1));
		}
	}

	fclose(fp);

	/* If original path is NULL or empty, return error */
	if (!orig_path || !*orig_path) {
		free(orig_path);
		*status = EXIT_FAILURE;
		return (char *)NULL;
	}

	/* Remove new line char from original path, if any */
	size_t orig_path_len = strlen(orig_path);
	if (orig_path_len > 0 && orig_path[orig_path_len - 1] == '\n')
		orig_path[orig_path_len - 1] = '\0';

	/* Decode original path's URL format */
	char *url_decoded = url_decode(orig_path);

	if (!url_decoded) {
		xerror(_("undel: '%s': Error decoding original path\n"), orig_path);
		free(orig_path);
		*status = EXIT_FAILURE;
		return (char *)NULL;
	}

	free(orig_path);
	return url_decoded;
}

static int
create_untrash_parent(char *dir)
{
	/* NOTE: We should be using our own create_dirs() here, but it fails! */
	char *cmd[] = {"mkdir", "-p", "--", dir, NULL};
	return launch_execv(cmd, FOREGROUND, E_NOFLAG);
}

static int
check_untrash_dest(char *file)
{
	if (!file || !*file) {
		xerror(_("undel: File name is NULL or empty\n"));
		return EXIT_FAILURE;
	}

	char *p = strrchr(file, '/');
	if (!p) {
		xerror(_("undel: '%s': No directory specified\n"), file);
		return EXIT_FAILURE;
	}

	char c = *(p + 1);
	*(p + 1) = '\0';
	char *parent_dir = file;

	int ret = access(parent_dir, F_OK | X_OK | W_OK);
	if (ret != 0) {
		if (errno == ENOENT) {
			if (create_untrash_parent(parent_dir) != EXIT_SUCCESS) {
				*(p + 1) = c;
				return EXIT_FAILURE;
			}
		} else {
			xerror("undel: '%s': %s\n", parent_dir, strerror(errno));
			*(p + 1) = c;
			return errno;
		}
	}

	*(p + 1) = c;

	struct stat a;
	if (stat(file, &a) != -1) {
		xerror(_("undel: '%s': Destination file exists\n"), file);
		return EEXIST;
	}

	return EXIT_SUCCESS;
}

static int
untrash_file(char *file)
{
	if (!file)
		return EXIT_FAILURE;

	char undel_file[PATH_MAX + 1];
	char undel_info[PATH_MAX + 1];
	snprintf(undel_file, sizeof(undel_file), "%s/%s", trash_files_dir, file);
	snprintf(undel_info, sizeof(undel_info), "%s/%s.trashinfo",
		trash_info_dir, file);

	int ret = EXIT_SUCCESS;
	char *orig_path = read_original_path(undel_info, file, &ret);
	if (!orig_path)
		return ret;

	ret = check_untrash_dest(orig_path);
	if (ret != EXIT_SUCCESS) {
		if (conf.autols == 1)
			press_any_key_to_continue(0);
		free(orig_path);
		return ret;
	}

	ret = renameat(XAT_FDCWD, undel_file, XAT_FDCWD, orig_path);
	if (ret == -1) {
		if (errno == EXDEV) {
			/* Destination file is on a different file system, which is why
			 * rename(3) doesn't work: let's try with mv(1). */
			char *cmd[] = {"mv", "--", undel_file, orig_path, NULL};
			ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
			if (ret != EXIT_SUCCESS) {
				if (conf.autols == 1)
					press_any_key_to_continue(0);
				free(orig_path);
				return ret;
			}
		} else {
			xerror("undel: '%s': %s\n", undel_file, strerror(errno));
			if (conf.autols == 1)
				press_any_key_to_continue(0);
			free(orig_path);
			return errno;
		}
	}

	free(orig_path);

	if (unlinkat(XAT_FDCWD, undel_info, 0) == -1) {
		xerror(_("undel: '%s': %s\n"), undel_info, strerror(errno));
		return errno;
	}

	return EXIT_SUCCESS;
}

/* Untrash/restore all trashed files. */
static int
untrash_all(struct dirent ***tfiles, const int tfiles_n, const int free_files)
{
	size_t i;
	size_t untrashed = 0;
	int status = EXIT_SUCCESS;

	for (i = 0; i < (size_t)tfiles_n; i++) {
		if (untrash_file((*tfiles)[i]->d_name) != 0)
			status = EXIT_FAILURE;
		else
			untrashed++;
		if (free_files == 1)
			free((*tfiles)[i]);
	}

	if (free_files == 1)
		free(*tfiles);

	if (status == EXIT_SUCCESS) {
		if (conf.autols == 1) reload_dirlist();

		size_t n = count_trashed_files();
		print_reload_msg(_("%zu file(s) restored\n"), untrashed);
		print_reload_msg(_("%zu total trashed file(s)\n"), n);
	}

	return status;
}

/* Untrash files passed as parameters (ARGS). */
static int
untrash_files(char **args)
{
	int status = EXIT_SUCCESS;
	size_t i;
	size_t untrashed = 0;

	for (i = 0; args[i]; i++) {
		char *d = (char *)NULL;
		if (strchr(args[i], '\\'))
			d = unescape_str(args[i], 0);

		if (untrash_file(d ? d : args[i]) != EXIT_SUCCESS)
			status = EXIT_FAILURE;
		else
			untrashed++;

		free(d);
	}

	if (status == EXIT_SUCCESS) {
		if (conf.autols == 1) reload_dirlist();

		size_t n = count_trashed_files();
		print_reload_msg(_("%zu file(s) restored\n"), untrashed);
		print_reload_msg(_("%zu total trashed file(s)\n"), n);
	}

	return status;
}

int
untrash_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (trash_ok == 0 || !trash_dir || !trash_files_dir || !trash_info_dir) {
		xerror(_("%s: Trash function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	int exit_status = EXIT_SUCCESS;

	if (args[1] && *args[1] != '*' && strcmp(args[1], "a") != 0
	&& strcmp(args[1], "all") != 0)
		return untrash_files(args + 1);

	/* Get trashed files */
	int files_n = 0;
	struct dirent **trash_files = load_trashed_files(&files_n, &exit_status);

	if (files_n <= 0)
		return exit_status;

	/* if "undel all" (or "u a" or "u *") */
	if (args[1] && (strcmp(args[1], "*") == 0 || strcmp(args[1], "a") == 0
	|| strcmp(args[1], "all") == 0))
		return untrash_all(&trash_files, files_n, 1);

	/* List files and get input */
	char **input = list_and_get_input(&trash_files, files_n, 1);
	if (!input) {
		for (size_t i = 0; i < (size_t)files_n; i++)
			free(trash_files[i]);
		free(trash_files);
		return EXIT_FAILURE;
	}

	/* First check for quit, *, and non-number args */
	int free_and_return = 0;
	int reload_files = 0;

	size_t i;
	for (i = 0; input[i]; i++) {
		if (strcmp(input[i], "q") == 0) {
			free_and_return = reload_files = 1;
			continue;
		}

		if (strcmp(input[i], "*") == 0) {
			int ret = untrash_all(&trash_files, files_n, 0);
			free_files_and_input(&input, &trash_files, files_n);
			return ret;
		}

		int num = atoi(input[i]);
		if (!is_number(input[i]) || num <= 0 || num > files_n) {
			xerror(_("undel: %s: Invalid ELN\n"), input[i]);
			exit_status = EXIT_FAILURE;
			free_and_return = 1;
		}
	}

	/* Free and return if any of the above conditions is true. */
	if (free_and_return == 1) {
		free_files_and_input(&input, &trash_files, files_n);
		if (conf.autols == 1 && reload_files == 1) reload_dirlist();
		return exit_status;
	}

	/* Undelete trashed files */
	for (i = 0; input[i]; i++) {
		int num = atoi(input[i]);
		if (untrash_file(trash_files[num - 1]->d_name) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	free_files_and_input(&input, &trash_files, files_n);

	/* If some trashed file still remains, reload the undel screen */
	size_t n = count_trashed_files();

	if (n > 0) {
		if (conf.clear_screen == 1) CLEAR;
		untrash_function(args);
	} else {
		if (conf.autols == 1) reload_dirlist();
		print_reload_msg(_("%zu trashed file(s)\n"), count_trashed_files());
	}

	return exit_status;
}

static void
print_trashdir_size(void)
{
	int base = xargs.si == 1 ? 1000 : 1024;
	int status = 0;

	printf(_("\n%sTotal size: "), df_c);
	if (term_caps.suggestions == 1)
		{fputs("Calculating...", stdout); fflush(stdout);}

	const off_t full_size = dir_size(trash_files_dir, 0, &status) * base;
	char *human_size = construct_human_size(full_size);

	char err[sizeof(xf_c) + 6]; *err = '\0';
	if (status != 0)
		snprintf(err, sizeof(err), "%s%c%s", xf_c, DU_ERR_CHAR, NC);

	char s[MAX_SHADE_LEN]; *s = '\0';
	if (conf.colorize == 1)
		get_color_size(full_size, s, sizeof(s));

	if (term_caps.suggestions == 1)
		{MOVE_CURSOR_LEFT(14); ERASE_TO_RIGHT; fflush(stdout);}

	printf(_("%s%s%s%s\n"), err, s, human_size, df_c);
}

/* List files currently in the trash can */
static int
list_trashed_files(void)
{
	if (!trash_files_dir || !*trash_files_dir) {
		xerror("%s\n", _("trash: The trash directory is undefined\n"));
		return EXIT_FAILURE;
	}

	struct dirent **trash_files = (struct dirent **)NULL;
	int files_n = scandir(trash_files_dir, &trash_files,
			skip_files, conf.unicode ? alphasort : (conf.case_sens_list
			? xalphasort : alphasort_insensitive));

	if (files_n == -1) {
		xerror("trash: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	if (files_n <= 0) {
		puts(_("trash: No trashed files"));
		return EXIT_SUCCESS;
	}

	if (conf.clear_screen == 1)
		CLEAR;

	HIDE_CURSOR;

	int ret = print_trashfiles(&trash_files, files_n);

	for (size_t i = 0; i < (size_t)files_n; i++)
		free(trash_files[i]);
	free(trash_files);

	UNHIDE_CURSOR;

	if (ret != EXIT_SUCCESS)
		return ret;

	print_trashdir_size();

	return EXIT_SUCCESS;
}

/* Make sure we are trashing a valid (trashable) file */
static int
check_trash_file(char *file)
{
	char tmp_file[PATH_MAX + 1];
	if (*file == '/') /* If absolute path */
		xstrsncpy(tmp_file, file, sizeof(tmp_file));
	else { /* If relative path, add path to check against TRASH_DIR */
		if (*workspaces[cur_ws].path == '/' && !workspaces[cur_ws].path[1])
			snprintf(tmp_file, sizeof(tmp_file), "/%s", file);
		else
			snprintf(tmp_file, sizeof(tmp_file), "%s/%s",
				workspaces[cur_ws].path, file);
	}

	/* Do not trash any of the parent directories of TRASH_DIR */
	if (strncmp(tmp_file, trash_dir, strnlen(tmp_file, sizeof(tmp_file))) == 0) {
		xerror(_("trash: Cannot trash '%s'\n"), tmp_file);
		return EXIT_FAILURE;
	}

	/* Do no trash TRASH_DIR itself nor anything inside it (trashed files) */
	if (strncmp(tmp_file, trash_dir, strlen(trash_dir)) == 0) {
		fputs(_("trash: Use 'trash del' to remove trashed files\n"), stderr);
		return EXIT_FAILURE;
	}

	size_t l = strlen(file);
	if (l > 1 && file[l - 1] == '/')
		/* Do not trash (move) symlinks ending with a slash. According to 'info mv':
		 * "_Warning_: Avoid specifying a source name with a trailing slash, when
		 * it might be a symlink to a directory. Otherwise, 'mv' may do something
		 * very surprising, since its behavior depends on the underlying rename
		 * system call. On a system with a modern Linux-based kernel, it fails
		 * with 'errno=ENOTDIR'.  However, on other systems (at least FreeBSD 6.1
		 * and Solaris 10) it silently renames not the symlink but rather the
		 * directory referenced by the symlink." */
		file[l - 1] = '\0';

	struct stat a;
	if (lstat(file, &a) == -1) {
		xerror(_("trash: '%s': %s\n"), file, strerror(errno));
		return errno;
	}

	return EXIT_SUCCESS;
}

/* List successfully trashed files. */
static void
list_ok_trashed_files(char **args, const int *trashed, const size_t trashed_n)
{
	if (print_removed_files == 0)
		return;

	size_t i;
	for (i = 0; i < trashed_n; i++) {
		if (!args[trashed[i]] || !*args[trashed[i]])
			continue;

		char *p = args[trashed[i]];
		if (strchr(args[trashed[i]], '\\')
		&& !(p = unescape_str(args[trashed[i]], 0)) ) {
			xerror(_("trash: '%s': Error unescaping file name\n"),
				args[trashed[i]]);
			continue;
		}

		char *tmp = abbreviate_file_name(p);
		if (!tmp) {
			xerror(_("trash: '%s': Error abbreviating file name\n"), p);
			if (p && p != args[trashed[i]])
				free(p);
			continue;
		}

		char *name = (*tmp == '.' && tmp[1] == '/') ? tmp + 2 : tmp;

		puts(name);

		if (tmp && tmp != p)
			free(tmp);
		if (p && p != args[trashed[i]])
			free(p);
	}
}

/* Trash files passed as arguments to the trash command */
static int
trash_files_args(char **args)
{
	time_t rawtime = time(NULL);
	struct tm t;
	char *suffix = localtime_r(&rawtime, &t) ? gen_date_suffix(t) : (char *)NULL;
	if (!suffix)
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS, cwd = 0;
	size_t i, trashed_files = 0, n = 0;
	for (i = 1; args[i]; i++);
	int *successfully_trashed = xnmalloc(i + 1, sizeof(int));

	for (i = 1; args[i]; i++) {
		if (trash_n + trashed_files >= MAX_TRASH) {
			xerror("%s\n", _("trash: Cannot trash any more files"));
			exit_status = EXIT_FAILURE;
			break;
		}

		char *deq_file = unescape_str(args[i], 0);
		if (!deq_file) {
			xerror(_("trash: '%s': Error unescaping file name\n"), args[i]);
			continue;
		}

		/* Make sure we are trashing a valid file */
		if (check_trash_file(deq_file) != EXIT_SUCCESS) {
			exit_status = EXIT_FAILURE;
			free(deq_file);
			continue;
		}

		if (cwd == 0)
			cwd = is_file_in_cwd(deq_file);

		/* Once here, everything is fine: trash the file */
		if (trash_file(suffix, &t, deq_file) == EXIT_SUCCESS) {
			trashed_files++;
			if (print_removed_files == 1) {
				/* Store indices of successfully trashed files */
				successfully_trashed[n] = (int)i;
				n++;
			}
		} else {
			cwd = 0;
			exit_status = EXIT_FAILURE;
		}

		free(deq_file);
	}

	free(suffix);

	if (exit_status == EXIT_SUCCESS) {
		if (conf.autols == 1 && cwd == 1)
			reload_dirlist();

		goto PRINT_TRASHED;

	} else if (trashed_files > 0) {
		/* An error occured, but at least one file was trashed as well.
		 * If this file was in the current dir, the screen will be refreshed
		 * after this function (by inotify/kqueue), hidding the error message.
		 * So let's pause here to prevent the error from being hidden, and
		 * then refresh the list of files ourselves. */
		if (conf.autols == 1) {
			press_any_key_to_continue(0);
			reload_dirlist();
		}
	}

PRINT_TRASHED:
	list_ok_trashed_files(args, successfully_trashed, n);
	print_reload_msg(_("%zu file(s) trashed\n"), trashed_files);
	print_reload_msg(_("%zu total trashed file(s)\n"),
		trash_n + trashed_files);

	free(successfully_trashed);
	return exit_status;
}

int
trash_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (trash_ok == 0 || !trash_dir || !trash_info_dir || !trash_files_dir) {
		xerror(_("%s: Trash function disabled\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* List trashed files ('tr' or 'tr ls') */
	if (!args[1] || (*args[1] == 'l'
	&& (strcmp(args[1], "ls") == 0 || strcmp(args[1], "list") == 0)))
		return list_trashed_files();

	trash_n = count_trashed_files();

	if (*args[1] == 'd' && strcmp(args[1], "del") == 0)
		return remove_from_trash(args);

	if ((*args[1] == 'c' && strcmp(args[1], "clear") == 0)
	|| (*args[1] == 'e' && strcmp(args[1], "empty") == 0)) {
		struct stat a;
		if (trash_n > 0 && lstat(*args[1] == 'c' ? "clear" : "empty", &a) == 0
		&& rl_get_y_or_n(_("Empty the trash can? [y/n] ")) == 0)
			return EXIT_SUCCESS;
		return trash_clear();
	}

	else
		return trash_files_args(args);
}
#else
void *_skip_me_trash;
#endif /* !_NO_TRASH */
