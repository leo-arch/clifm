/* file_operations.c -- control multiple file operations */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2021, L. Abramovich <johndoe.arch@outlook.com>
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
#include <readline/readline.h>

#include "archives.h"
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

int
xchmod(const char *file, mode_t mode)
{
	/* Set or unset S_IXUSR, S_IXGRP, and S_IXOTH */
	(0100 & mode) ? (mode &= ~0111) : (mode |= 0111);

	if (chmod(file, mode) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int
open_function(char **cmd)
{
	if (!cmd)
		return EXIT_FAILURE;

	if (strchr(cmd[1], '\\')) {
		char *deq_path = (char *)NULL;
		deq_path = dequote_str(cmd[1], 0);

		if (!deq_path) {
			fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
			    PROGRAM_NAME, cmd[1]);
			return EXIT_FAILURE;
		}

		strcpy(cmd[1], deq_path);
		free(deq_path);
	}

	char *file = cmd[1];

	/* Check file existence */
	struct stat file_attrib;

	if (stat(file, &file_attrib) == -1) {
		fprintf(stderr, "%s: open: %s: %s\n", PROGRAM_NAME, cmd[1],
		    strerror(errno));
		return EXIT_FAILURE;
	}

	/* Check file type: only directories, symlinks, and regular files
	 * will be opened */

	char no_open_file = 1, file_type[128];
	/* Reserve a good amount of bytes for filetype: it cannot be
		  * known beforehand how many bytes the TRANSLATED string will
		  * need */

	switch ((file_attrib.st_mode & S_IFMT)) {
	case S_IFBLK:
		/* Store filetype to compose and print the error message, if
		 * necessary */
		strcpy(file_type, _("block device"));
		break;

	case S_IFCHR:
		strcpy(file_type, _("character device"));
		break;

	case S_IFSOCK:
		strcpy(file_type, _("socket"));
		break;

	case S_IFIFO:
		strcpy(file_type, _("FIFO/pipe"));
		break;

	case S_IFDIR:
		return cd_function(file);

	case S_IFREG:

		/* If an archive/compressed file, call archiver() */
		if (is_compressed(file, 1) == 0) {
			char *tmp_cmd[] = {"ad", file, NULL};
			return archiver(tmp_cmd, 'd');
		}

		no_open_file = 0;
		break;

	default:
		strcpy(file_type, _("unknown file type"));
		break;
	}

	/* If neither directory nor regular file nor symlink (to directory
	 * or regular file), print the corresponding error message and
	 * exit */
	if (no_open_file) {
		fprintf(stderr, _("%s: %s (%s): Cannot open file. Try "
			"'APPLICATION FILENAME'.\n"), PROGRAM_NAME, cmd[1], file_type);
		return EXIT_FAILURE;
	}

	/* At this point we know the file to be openend is either a regular
	 * file or a symlink to a regular file. So, just open the file */

	if (!cmd[2] || (*cmd[2] == '&' && !cmd[2][1])) {

		if (opener) {
			char *tmp_cmd[] = {opener, file, NULL};

			int ret = launch_execve(tmp_cmd,
			    strcmp(cmd[args_n], "&") == 0 ? BACKGROUND
							  : FOREGROUND,
			    E_NOSTDERR);

			if (ret != EXIT_SUCCESS)
				return EXIT_FAILURE;

			return EXIT_SUCCESS;
		}

		else if (!(flags & FILE_CMD_OK)) {
			fprintf(stderr, _("%s: file: Command not found. Specify an "
					"application to open the file\nUsage: "
					"open ELN/FILENAME [APPLICATION]\n"), PROGRAM_NAME);
			return EXIT_FAILURE;
		}

		else {
			int ret = mime_open(cmd);

			/* The return value of mime_open could be zero
			 * (EXIT_SUCCESS), if success, one (EXIT_FAILURE) if error
			 * (in which case the following error message should be
			 * printed), and -1 if no access permission, in which case
			 * no error message should be printed, since the
			 * corresponding message is printed by mime_open itself */
			if (ret == EXIT_FAILURE) {
				fputs("Add a new entry to the mimelist file ('mime "
				      "edit' or F6) or run 'open FILE APPLICATION'\n", stderr);
				return EXIT_FAILURE;
			}

			return EXIT_SUCCESS;
		}
	}

	/* If some application was specified to open the file */
	char *tmp_cmd[] = {cmd[2], file, NULL};

	int ret = launch_execve(tmp_cmd, (cmd[args_n]
	&& strcmp(cmd[args_n], "&") == 0) ? BACKGROUND : FOREGROUND, E_NOSTDERR);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* Relink symlink to new path */
int
edit_link(char *link)
{
	if (!link || !*link)
		return EXIT_FAILURE;

	/* Dequote the filename, if necessary */
	if (strchr(link, '\\')) {
		char *tmp = dequote_str(link, 0);

		if (!tmp) {
			fprintf(stderr, _("%s: %s: Error dequoting file\n"),
			    PROGRAM_NAME, link);
			return EXIT_FAILURE;
		}

		strcpy(link, tmp);
		free(tmp);
	}

	/* Check we have a valid symbolic link */
	struct stat file_attrib;

	if (lstat(link, &file_attrib) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, link,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	if ((file_attrib.st_mode & S_IFMT) != S_IFLNK) {
		fprintf(stderr, _("%s: %s: Not a symbolic link\n"),
		    PROGRAM_NAME, link);
		return EXIT_FAILURE;
	}

	/* Get file pointed to by symlink and report to the user */
	char *real_path = realpath(link, NULL);

	if (!real_path)
		printf(_("%s%s%s currently pointing to nowhere (broken link)\n"),
		    or_c, link, df_c);
	else {
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

	/* If an ELN, replace by the corresponding filename */
	if (is_number(new_path)) {
		int i_new_path = atoi(new_path) - 1;
		if (file_info[i_new_path].name) {
			new_path = (char *)xrealloc(new_path,
			    (strlen(file_info[i_new_path].name) + 1) * sizeof(char));
			strcpy(new_path, file_info[i_new_path].name);
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
			fprintf(stderr, _("%s: %s: Error dequoting file\n"),
			    PROGRAM_NAME, new_path);
			free(new_path);
			return EXIT_FAILURE;
		}

		strcpy(new_path, tmp);
		free(tmp);
	}

	/* Check new_path existence and warn the user if it does not
	 * exist */
	if (lstat(new_path, &file_attrib) == -1) {

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
			}

			else {
				free(answer);
				free(new_path);
				return EXIT_SUCCESS;
			}
		}
	}

	/* Finally, relink the symlink to new_path */
	char *cmd[] = {"ln", "-sfn", new_path, link, NULL};
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

int
copy_function(char **comm)
{
	log_function(comm);

	if (!is_sel)
		return run_and_refresh(comm);

	char *tmp_cmd = (char *)NULL;
	size_t total_len = 0, i = 0;

	for (i = 0; i <= args_n; i++)
		total_len += strlen(comm[i]);

	tmp_cmd = (char *)xcalloc(total_len + (i + 1) + 2, sizeof(char));

	for (i = 0; i <= args_n; i++) {
		strcat(tmp_cmd, comm[i]);
		strcat(tmp_cmd, " ");
	}

	if (sel_is_last)
		strcat(tmp_cmd, ".");

	int ret = 0;
	ret = launch_execle(tmp_cmd);
	free(tmp_cmd);

	if (ret != EXIT_SUCCESS)
		return EXIT_FAILURE;

	if (copy_n_rename) { /* pp */
		char **tmp = (char **)xnmalloc(sel_n + 3, sizeof(char *));
		tmp[0] = savestring("br", 2);

		size_t j;
		for (j = 0; j < sel_n; j++) {
			size_t arg_len = strlen(sel_elements[j]);

			if (sel_elements[j][arg_len - 1] == '/')
				sel_elements[j][arg_len - 1] = '\0';

			if (*comm[args_n] == '~') {
				char *exp_dest = tilde_expand(comm[args_n]);
				comm[args_n] = xrealloc(comm[args_n],
				    (strlen(exp_dest) + 1) * sizeof(char));
				strcpy(comm[args_n], exp_dest);
				free(exp_dest);
			}

			size_t dest_len = strlen(comm[args_n]);
			if (comm[args_n][dest_len - 1] == '/') {
				comm[args_n][dest_len - 1] = '\0';
			}

			char dest[PATH_MAX];
			strcpy(dest, (sel_is_last || strcmp(comm[args_n], ".") == 0)
					 ? ws[cur_ws].path
					 : comm[args_n]);

			char *ret_val = strrchr(sel_elements[j], '/');

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

		copy_n_rename = 0;

		return EXIT_SUCCESS;
	}

	/* If 'mv sel' and command is successful deselect everything,
	 * since sel files are note there anymore */
	if (*comm[0] == 'm' && comm[0][1] == 'v'
	&& (!comm[0][2] || comm[0][2] == ' ')) {

		for (i = 0; i < sel_n; i++)
			free(sel_elements[i]);

		sel_n = 0;
		save_sel();
	}

	if (cd_lists_on_the_fly) {
		free_dirlist();
		list_dir();
	}

	return EXIT_SUCCESS;
}

int
remove_file(char **args)
{
	int cwd = 0, exit_status = EXIT_SUCCESS;

	char **rm_cmd = (char **)xnmalloc(args_n + 4, sizeof(char *));
	int i, j = 3, dirs = 0;

	for (i = 1; args[i]; i++) {

		/* Check if at least one file is in the current directory. If not,
		 * there is no need to refresh the screen */
		if (!cwd) {
			char *ret = strchr(args[i], '/');
			/* If there's no slash, or if slash is the last char and
			 * the file is not root "/", we have a file in CWD */
			if (!ret || (!*(ret + 1) && ret != args[i]))
				cwd = 1;
		}

		char *tmp = (char *)NULL;
		if (strchr(args[i], '\\')) {
			tmp = dequote_str(args[i], 0);

			if (tmp) {
				/* Start storing filenames in 3: 0 is for 'rm', and 1
				 * and 2 for parameters, including end of parameters (--) */
				rm_cmd[j++] = savestring(tmp, strlen(tmp));
				free(tmp);
			} else {
				fprintf(stderr, "%s: %s: Error dequoting filename\n",
				    PROGRAM_NAME, args[i]);
				continue;
			}
		}

		else
			rm_cmd[j++] = savestring(args[i], strlen(args[i]));

		struct stat attr;
		if (!dirs && lstat(rm_cmd[j - 1], &attr) != -1
		&& (attr.st_mode & S_IFMT) == S_IFDIR)
			dirs = 1;
	}

	rm_cmd[j] = (char *)NULL;

	rm_cmd[0] = savestring("rm", 2);
	if (dirs)
		rm_cmd[1] = savestring("-dIr", 4);
	else
		rm_cmd[1] = savestring("-I", 2);
	rm_cmd[2] = savestring("--", 2);

	if (launch_execve(rm_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;
	else {
		if (cwd && cd_lists_on_the_fly && strcmp(args[1], "--help") != 0
		&& strcmp(args[1], "--version") != 0) {
			free_dirlist();
			exit_status = list_dir();
		}
	}

	for (i = 0; rm_cmd[i]; i++)
		free(rm_cmd[i]);

	free(rm_cmd);

	return exit_status;
}

/* Rename a bulk of files (ARGS) at once. Takes files to be renamed
 * as arguments, and returns zero on success and one on error. The
 * procedude is quite simple: filenames to be renamed are copied into
 * a temporary file, which is opened via the mime function and shown
 * to the user to modify it. Once the filenames have been modified and
 * saved, modifications are printed on the screen and the user is
 * asked whether to perform the actual bulk renaming (via mv) or not.
 * I took this bulk rename method, just because it is quite simple and
 * KISS, from the fff filemanager. So, thanks fff! BTW, this method
 * is also implemented by ranger and nnn */
int
bulk_rename(char **args)
{
	if (!args[1])
		return EXIT_FAILURE;

	int exit_status = EXIT_SUCCESS;

	char BULK_FILE[PATH_MAX] = "";
	if (xargs.stealth_mode == 1)
		sprintf(BULK_FILE, "/tmp/.clifm_bulk_rename");
	else
		sprintf(BULK_FILE, "%s/.bulk_rename", TMP_DIR);

	FILE *bulk_fp;

	bulk_fp = fopen(BULK_FILE, "w+");
	if (!bulk_fp) {
		_err('e', PRINT_PROMPT, "bulk: '%s': %s\n", BULK_FILE, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i, arg_total = 0;

	/* Copy all files to be renamed to the bulk file */
	for (i = 1; args[i]; i++) {

		/* Dequote filename, if necessary */
		if (strchr(args[i], '\\')) {
			char *deq_file = dequote_str(args[i], 0);

			if (!deq_file) {
				fprintf(stderr, _("bulk: %s: Error dequoting "
						  "filename\n"),
				    args[i]);
				continue;
			}

			strcpy(args[i], deq_file);
			free(deq_file);
		}

		fprintf(bulk_fp, "%s\n", args[i]);
	}

	arg_total = i;

	fclose(bulk_fp);

	/* Store the last modification time of the bulk file. This time
	 * will be later compared to the modification time of the same
	 * file after shown to the user */
	struct stat file_attrib;
	stat(BULK_FILE, &file_attrib);
	time_t mtime_bfr = (time_t)file_attrib.st_mtime;

	/* Open the bulk file via the mime function */
	char *cmd[] = {"mm", BULK_FILE, NULL};
	mime_open(cmd);

	/* Compare the new modification time to the stored one: if they
	 * match, nothing was modified */
	stat(BULK_FILE, &file_attrib);

	if (mtime_bfr == (time_t)file_attrib.st_mtime) {

		puts(_("bulk: Nothing to do"));

		if (unlink(BULK_FILE) == -1) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			    BULK_FILE, strerror(errno));
			exit_status = EXIT_FAILURE;
		}

		return exit_status;
	}

	bulk_fp = fopen(BULK_FILE, "r");

	if (!bulk_fp) {
		_err('e', PRINT_PROMPT, "bulk: '%s': %s\n", BULK_FILE,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	/* Go back to the beginning of the bulk file */
	fseek(bulk_fp, 0, SEEK_SET);

	/* Make sure there are as many lines in the bulk file as files
	 * to be renamed */
	size_t file_total = 1;
	char tmp_line[256];

	while (fgets(tmp_line, (int)sizeof(tmp_line), bulk_fp))
		file_total++;

	if (arg_total != file_total) {

		fputs(_("bulk: Line mismatch in rename file\n"), stderr);

		fclose(bulk_fp);

		if (unlink(BULK_FILE) == -1)
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			    BULK_FILE, strerror(errno));

		return EXIT_FAILURE;
	}

	/* Go back to the beginning of the bulk file, again */
	fseek(bulk_fp, 0L, SEEK_SET);

	size_t line_size = 0;
	char *line = (char *)NULL;
	ssize_t line_len = 0;
	int modified = 0;

	i = 1;

	/* Print what would be done */
	while ((line_len = getline(&line, &line_size, bulk_fp)) > 0) {

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		if (strcmp(args[i], line) != 0) {
			printf("%s %s->%s %s\n", args[i], mi_c, df_c, line);
			modified++;
		}

		i++;
	}

	/* If no filename was modified */
	if (!modified) {
		puts(_("bulk: Nothing to do"));

		if (unlink(BULK_FILE) == -1) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			    BULK_FILE, strerror(errno));
			exit_status = EXIT_FAILURE;
		}

		free(line);
		fclose(bulk_fp);

		return exit_status;
	}

	/* Ask the user for confirmation */
	char *answer = (char *)NULL;

	while (!answer) {
		answer = readline(_("Continue? [y/N] "));

		if (strlen(answer) > 1) {
			free(answer);
			answer = (char *)NULL;
			continue;
		}

		switch (*answer) {
		case 'y': /* fallthrough */
		case 'Y':
			break;

		case 'n': /* fallthrough */
		case 'N': /* fallthrough */
		case '\0':
			free(answer);
			free(line);
			fclose(bulk_fp);
			return EXIT_SUCCESS;

		default:
			free(answer);
			answer = (char *)NULL;
			break;
		}
	}

	free(answer);

	/* Once again */
	fseek(bulk_fp, 0L, SEEK_SET);

	i = 1;

	/* Compose the mv commands and execute them */
	while ((line_len = getline(&line, &line_size, bulk_fp)) > 0) {

		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		if (strcmp(args[i], line) != 0) {
			char *tmp_cmd[] = {"mv", args[i], line, NULL};

			if (launch_execve(tmp_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}

		i++;
	}

	free(line);

	fclose(bulk_fp);

	if (unlink(BULK_FILE) == -1) {
		_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
		    BULK_FILE, strerror(errno));
		exit_status = EXIT_FAILURE;
	}

	if (cd_lists_on_the_fly) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	return exit_status;
}

/* Export files in CWD (if FILENAMES is NULL), or files in FILENAMES,
 * into a temporary file. Return the address of this empt file if
 * success (it must be freed) or NULL in case of error */
char *export(char **filenames, int open)
{
	char *rand_ext = gen_rand_str(6);

	if (!rand_ext)
		return (char *)NULL;

	char *tmp_file = (char *)xnmalloc(strlen(TMP_DIR) + 14, sizeof(char));
	sprintf(tmp_file, "%s/.clifm%s", TMP_DIR, rand_ext);
	free(rand_ext);

	FILE *fp = fopen(tmp_file, "w");

	if (!fp) {
		free(tmp_file);
		return (char *)NULL;
	}

	size_t i;

	/* If no argument, export files in CWD */
	if (!filenames[1]) {
		for (i = 0; file_info[i].name; i++)
			fprintf(fp, "%s\n", file_info[i].name);
	}

	else {
		for (i = 1; filenames[i]; i++) {
			if (*filenames[i] == '.' && (!filenames[i][1] || (filenames[i][1] == '.' && !filenames[i][2])))
				continue;

			fprintf(fp, "%s\n", filenames[i]);
		}
	}

	fclose(fp);

	if (!open)
		return tmp_file;

	char *cmd[] = {"mime", tmp_file, NULL};

	int ret = mime_open(cmd);

	if (ret == EXIT_SUCCESS)
		return tmp_file;

	else {
		free(tmp_file);
		return (char *)NULL;
	}
}

int
batch_link(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1] || (*args[1] == '-' && strcmp(args[1], "--help") == 0)) {
		puts(_("Usage: bl [FILE(s)]"));
		return EXIT_SUCCESS;
	}

	char *suffix = (char *)NULL;
	while (!suffix) {
		suffix = rl_no_hist(_("Enter links suffix ('n' for none): "));

		if (!suffix)
			continue;

		if (!*suffix) {
			free(suffix);
			continue;
		}
	}

	size_t i;

	int exit_status = EXIT_SUCCESS;
	char tmp[NAME_MAX];

	for (i = 1; args[i]; i++) {
		char *linkname = (char *)NULL;

		if (*suffix == 'n' && !suffix[1])
			linkname = args[i];

		else {
			snprintf(tmp, NAME_MAX, "%s%s", args[i], suffix);
			linkname = tmp;
		}

		char *ptr = strrchr(linkname, '/');

		if (symlink(args[i], ptr ? ++ptr : linkname) == -1) {
			exit_status = EXIT_FAILURE;
			fprintf(stderr, _("%s: %s: Cannot create symlink: %s\n"),
			    PROGRAM_NAME, ptr ? ptr : linkname, strerror(errno));
		}
	}

	if (exit_status == EXIT_SUCCESS && cd_lists_on_the_fly) {
		free_dirlist();

		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	free(suffix);
	return exit_status;
}
