/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* bulk_rename.c -- bulk rename files */

/* The run_action function is taken from NNN's run_selected_plugin function
 * (https://github.com/jarun/nnn/blob/master/src/nnn.c), licensed under BSD-2-Clause.
 * All changes are licensed under GPL-2.0-or-later. */

#include "helpers.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#if defined(MAC_OS_X_RENAMEAT_SYS_STDIO_H)
# include <sys/stdio.h> /* renameat(2) */
#endif /* MAC_OS_X_RENAMEAT_SYS_STDIO_H */

#include "aux.h" /* press_any_key_to_continue(), abbreviate_file_name(), open_fread() */
#include "checks.h" /* is_file_in_cwd() */
#include "file_operations.h" /* open_file() */
#include "init.h" /* get_sel_files() */
#include "listing.h" /* reload_dirlist() */
#include "messages.h" /* BULK_RENAME_USAGE */
#include "misc.h" /* xerror(), print_reload_msg() */
#include "readline.h" /* rl_get_y_or_n() */
#include "spawn.h" /* launch_execv() */

#define BULK_RENAME_TMP_FILE_HEADER "# Clifm - Rename files in bulk\n\
# Edit filenames, save, and quit the editor (you will be\n\
# prompted to confirm).\n\
# Quit the editor without saving to cancel the operation.\n\n"

#define IS_BR_COMMENT(l) (*(l) == '#' && (l)[1] == ' ')

/* Error opening tmp file FILE. Err accordingly. */
static int
err_open_tmp_file(const char *file, const int fd)
{
	xerror("br: open: '%s': %s\n", file, strerror(errno));
	if (unlinkat(XAT_FDCWD, file, 0) == -1)
		xerror("br: unlink: '%s': %s\n", file, strerror(errno));
	close(fd);

	return FUNC_FAILURE;
}

/* Rename OLDPATH as NEWPATH.
 * NEWPATH is checked for existence before renaming, in which case the user
 * is asked for confirmation. */
static int
rename_file(char *oldpath, char *newpath)
{
	/* Some renameat(2) implementations (DragonFly) do not like NEWPATH to
	 * end with a slash (in case of renaming directories). */
	size_t nlen = strlen(newpath);
	if (nlen > 1 && newpath[nlen - 1] == '/') {
		nlen--;
		newpath[nlen] = '\0';
	}

	char *npath = normalize_path(newpath, nlen);
	if (!npath || !*npath) {
		free(npath);
		xerror(_("br: '%s': Error normalizing path\n"), newpath);
		return FUNC_FAILURE;
	}

	struct stat a;
	if (lstat(npath, &a) == 0) {
		xerror("br: '%s': %s\n", newpath, strerror(EEXIST));
		if (rl_get_y_or_n(_("Overwrite this file?"),
		conf.default_answer.overwrite) == 0) {
			free(npath);
			return EEXIST;
		}
	}

	if (renameat(XAT_FDCWD, oldpath, XAT_FDCWD, npath) == 0) {
		free(npath);
		return 0;
	}

	if (errno != EXDEV) {
		xerror(_("br: Cannot rename '%s' to '%s': %s\n"), oldpath,
			newpath, strerror(errno));
		free(npath);
		return errno;
	}

	char *cmd[] = {"mv", "--", oldpath, npath, NULL};
	const int ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);
	free(npath);
	return ret;
}

/* Write filenames in ARGS into the temporary file TMPFILE, whose file
 * descriptor is FD. Update WRITTEN to the number of actually written
 * filenames, and make ATTR hold the stat attributes of the temporary
 * file after writing into it all filenames. */
static int
write_files_to_tmp(char ***args, const char *tmpfile, const int fd,
	struct stat *attr, size_t *written)
{
	FILE *fp = fdopen(fd, "w");
	if (!fp)
		return err_open_tmp_file(tmpfile, fd);

	fprintf(fp, BULK_RENAME_TMP_FILE_HEADER);

	/* Copy all files to be renamed into the bulk file */
	for (size_t i = 1; (*args)[i]; i++) {
		/* Dequote filename, if necessary */
		if (strchr((*args)[i], '\\')) {
			char *deq_file = unescape_str((*args)[i], 0);
			if (!deq_file) {
				xerror(_("br: '%s': Error unescaping filename\n"), (*args)[i]);
				press_any_key_to_continue(0);
				continue;
			}

			xstrsncpy((*args)[i], deq_file, strlen(deq_file) + 1);
			free(deq_file);
		}

		/* Resolve "./" and "../" */
		if (*(*args)[i] == '.' && ((*args)[i][1] == '/' || ((*args)[i][1] == '.'
		&& (*args)[i][2] == '/') ) ) {
			char *p = normalize_path((*args)[i], strlen((*args)[i]));
			if (!p) {
				xerror(_("br: '%s': Error normalizing path\n"), (*args)[i]);
				press_any_key_to_continue(0);
				continue;
			}
			free((*args)[i]);
			(*args)[i] = p;
		}

		struct stat a;
		if (lstat((*args)[i], &a) == -1) {
			/* Last parameter may be opening app (:APP) */
			if (i != args_n || *(*args)[i] != ':') {
				xerror("br: '%s': %s\n", (*args)[i], strerror(errno));
				press_any_key_to_continue(0);
			}
			continue;
		}

		fprintf(fp, "%s\n", (*args)[i]);
		(*written)++;
	}

	if (*written == 0 || fstat(fd, attr) == -1) {
		if (unlinkat(XAT_FDCWD, tmpfile, 0) == -1)
			xerror("br: unlink: '%s': %s\n", tmpfile, strerror(errno));
		fclose(fp);
		return FUNC_FAILURE;
	}

	fclose(fp);
	return FUNC_SUCCESS;
}

static size_t
print_and_count_modified_names(char **args, char **new_names)
{
	size_t modified = 0;

	/* Print what would be done */
	for (size_t i = 0; new_names[i]; i++) {
		if (args[i] && strcmp(args[i], new_names[i]) != 0) {
			char *a = abbreviate_file_name(args[i]);
			char *b = abbreviate_file_name(new_names[i]);

			printf("%s %s%s%s %s\n", a ? a : args[i], mi_c, SET_MSG_PTR,
				df_c, b ? b : new_names[i]);

			if (a && a != args[i])
				free(a);
			if (b && b != new_names[i])
				free(b);

			modified++;
		}
	}

	if (modified == 0)
		puts(_("br: Nothing to do"));

	return modified;
}

/* Open FILE via APP (default associated application for text files if
 * omitted). */
static int
open_tmpfile(char *app, char *file)
{
	char *application = (char *)NULL;
	struct stat a;

	if (app && *app == ':' && app[1] && lstat(app, &a) == -1)
		application = app + 1;

	if (application) {
		char *cmd[] = {application, file, NULL};
		const int ret = launch_execv(cmd, FOREGROUND, E_NOFLAG);

		if (ret != FUNC_SUCCESS)
			unlinkat(XAT_FDCWD, file, 0);

		return ret;
	}

	open_in_foreground = 1;
	const int exit_status = open_file(file);
	open_in_foreground = 0;

	if (exit_status != FUNC_SUCCESS) {
		xerror("br: %s\n", errno != 0
			? strerror(errno) : _("Error opening temporary file"));

		if (unlinkat(XAT_FDCWD, file, 0) == -1)
			xerror("br: unlink: '%s': %s\n", file, strerror(errno));

		return exit_status;
	}

	return FUNC_SUCCESS;
}

/* Rename files in OLD_NAMES to those in NEW_NAMES. */
static int
rename_bulk_files(char **old_names, char **new_names, int *is_cwd,
	size_t *renamed, const size_t modified)
{
	int exit_status = FUNC_SUCCESS;

	for (size_t i = 0; new_names[i]; i++) {
		if (!old_names[i] || strcmp(old_names[i], new_names[i]) == 0)
			continue;

		const int ret = rename_file(old_names[i], new_names[i]);
		if (ret != 0) {
			if (ret != EEXIST)
				exit_status = ret;
			continue;
		}

		if (*is_cwd == 0 && (is_file_in_cwd(old_names[i])
		|| is_file_in_cwd(new_names[i])))
			*is_cwd = 1;

		(*renamed)++;
	}

	if (conf.autols == 1 && exit_status != 0 && modified > 1)
		press_any_key_to_continue(0);

	return exit_status;
}

/* Extract destiny filenames from the file pointed to by FP.
 *
 * If the number of destiny filenames does not equal TOTAL (the number of
 * source filenames) NULL is returned.
 *
 * Destiny filenames are checked for duplicates. If one is found, and the user
 * decides not to continue, NULL is returned.
 *
 * Otherwise, an array containing destiny filenames is returned.
 *
 * STATUS is set to FUNC_FAILURE in case of error. */
char **
get_new_names(FILE *fp, const size_t total, int *status)
{
	if (!fp)
		return (char **)NULL;

	char line[PATH_MAX + 1];
	*line = '\0';

	size_t n = 0, i = 0;

	while (fgets(line, (int)sizeof(line), fp)) {
		if (!*line || *line == '\n' || IS_BR_COMMENT(line))
			continue;
		n++;
	}

	fseek(fp, 0L, SEEK_SET);
	char **fnames = xnmalloc(n + 1, sizeof(char *));

	while (fgets(line, (int)sizeof(line), fp)) {
		if (!*line || *line == '\n' || IS_BR_COMMENT(line))
			continue;

		size_t len = strlen(line);
		if (line[len - 1] == '\n') {
			len--;
			line[len] = '\0';
		}

		fnames[i] = savestring(line, len);
		i++;
	}

	fnames[i] = (char *)NULL;

	if (i == 0)
		goto FREE_AND_EXIT;

	/* Check line mismatch */
	if (i != total) {
		xerror("%s\n", _("br: Line mismatch in temporary file"));
		*status = FUNC_FAILURE;
		goto FREE_AND_EXIT;
	}

	/* Check for duplicate entries */
	size_t dups = 0;
	for (i = 0; fnames[i]; i++) {
		for (n = i + 1; fnames[n]; n++) {
			if (strcmp(fnames[i], fnames[n]) == 0) {
				dups++;
				xerror(_("br: '%s' is duplicated\n"), fnames[i]);
			}
		}
	}

	if (dups > 0 && rl_get_y_or_n(_("Continue?"),
	conf.default_answer.overwrite) == 0)
		goto FREE_AND_EXIT;

	return fnames;

FREE_AND_EXIT:
	for (i = 0; fnames[i]; i++)
		free(fnames[i]);
	free(fnames);
	return (char **)NULL;
}

static void
unlink_and_close_tmpfile(FILE *fp, const char *tmpfile, int *status)
{
	if (unlinkat(XAT_FDCWD, tmpfile, 0) == -1) {
		*status = errno;
		err('w', PRINT_PROMPT, "br: unlink: '%s': %s\n",
			tmpfile, strerror(errno));
	}

	fclose(fp);
}

/* Rename a bulk of files (ARGS) at once.
 * Takes files to be renamed as arguments, and returns zero on success or one
 * on error.
 *
 * RENAMED is updated to the number of renamed files.
 * If reload_list is set to 1, the list of files is reloaded and a summary
 * message is displayed.
 *
 * The procedure is simple: filenames to be renamed are copied to a
 * temporary file, which is opened via the mime function and shown to
 * the user to modify it. Once the filenames have been modified and saved,
 * modifications are printed on the screen and the user is asked for
 * confirmation. */
int
bulk_rename(char **args, size_t *renamed, const size_t reload_list)
{
	*renamed = 0;

	if (virtual_dir == 1) {
		xerror(_("%s: br: Feature not allowed in virtual "
			"directories\n"), PROGRAM_NAME);
		return FUNC_SUCCESS;
	}

	if (!args || !args[1] || IS_HELP(args[1])) {
		puts(_(BULK_RENAME_USAGE));
		return FUNC_SUCCESS;
	}

	int exit_status = FUNC_SUCCESS;
	struct stat attra; /* Original temp file. */
	struct stat attrb; /* Edited temp file. */

	char tmpfile[PATH_MAX + 1];
	snprintf(tmpfile, sizeof(tmpfile), "%s/%s",
		xargs.stealth_mode == 1 ? P_tmpdir : tmp_dir, TMP_FILENAME);

	int fd = mkstemp(tmpfile);
	if (fd == -1) {
		xerror("br: mkstemp: '%s': %s\n", tmpfile, strerror(errno));
		return FUNC_FAILURE;
	}

	/* Write files to be renamed into a tmp file and store stat info in attra. */
	size_t written = 0;
	int ret = write_files_to_tmp(&args, tmpfile, fd, &attra, &written);
	if (ret != FUNC_SUCCESS)
		return ret;

	/* Open the tmp file with the associated text editor. */
	if ((ret = open_tmpfile(args[args_n], tmpfile)) != FUNC_SUCCESS)
		return ret;

	FILE *fp;
	if (!(fp = open_fread(tmpfile, &fd)))
		return err_open_tmp_file(tmpfile, fd);

	/* Compare the new modification time to the stored one: if they
	 * match, nothing was modified. */
	if (fstat(fd, &attrb) == -1) {
		xerror("br: '%s': %s\n", tmpfile, strerror(errno));
		goto CLOSE_AND_EXIT;
	}

	if (attra.st_mtime == attrb.st_mtime) {
		puts(_("br: Nothing to do"));
		goto CLOSE_AND_EXIT;
	}

	/* Load destiny names, checking for line mismatch and duplicates. */
	char **new_names = get_new_names(fp, written, &exit_status);

	unlink_and_close_tmpfile(fp, tmpfile, &exit_status);

	if (!new_names)
		return exit_status;

	char **old_names = args + 1;
	const size_t modified = print_and_count_modified_names(old_names, new_names);
	if (modified == 0)
		goto FREE_AND_EXIT;

	/* Ask the user for confirmation. */
	if (rl_get_y_or_n(_("Continue?"), conf.default_answer.bulk_rename) == 0)
		goto FREE_AND_EXIT;

	int is_cwd = 0;

	if ((ret = rename_bulk_files(old_names, new_names, &is_cwd,
	renamed, modified)) != FUNC_SUCCESS)
		exit_status = ret;

	if (sel_n > 0 && cwd_has_sel_files())
		/* Just in case a selected file in the current dir was renamed. */
		get_sel_files();

	if (reload_list == 1) {
		if (*renamed > 0 && is_cwd == 1 && conf.autols == 1)
			reload_dirlist();

		print_reload_msg(SET_SUCCESS_PTR, xs_cb,
			_("%zu file(s) renamed\n"), *renamed);
	}

FREE_AND_EXIT:
	for (size_t i = 0; new_names[i]; i++)
		free(new_names[i]);
	free(new_names);

	return exit_status;

CLOSE_AND_EXIT:
	unlink_and_close_tmpfile(fp, tmpfile, &exit_status);
	return exit_status;
}
