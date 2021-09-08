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
#include <time.h>
#include <readline/readline.h>

#include <fcntl.h>

#ifndef _NO_ARCHIVING
#include "archives.h"
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

static void
clear_selbox(void)
{
	size_t i;
	for (i = 0; i < sel_n; i++)
		free(sel_elements[i]);
	sel_n = 0;
	save_sel();	
}

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
		char *cmd[] = {opener, file, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	} else {
#ifndef _NO_LIRA
		char *cmd[] = {"mm", file, NULL};
		exit_status = mime_open(cmd);
#else
		/* Fallback to (xdg-)open */
#ifdef __HAIKU__
		char *cmd[] = {"open", file, NULL};
#else
		char *cmd[] = {"xdg-open", file, NULL};
#endif /* __HAIKU__ */
		if (launch_execve(cmd, FOREGROUND, E_NOSTDERR) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
#endif /* _NO_LIRA */
	}

	return exit_status;
}

/* Toggle executable bit on file */
int
xchmod(const char *file, mode_t mode)
{
	/* Set or unset S_IXUSR, S_IXGRP, and S_IXOTH */
	(0100 & mode) ? (mode &= (mode_t)~0111) : (mode |= 0111);

	log_function(NULL);

	if (chmod(file, mode) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, file, strerror(errno));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* Create a duplicate of a file/dir using rsync or cp */
int
dup_file(char *source, char *dest)
{
	if (!source || !*source)
		return EXIT_FAILURE;

	log_function(NULL);

	if (strchr(source, '\\')) {
		char *deq_str = dequote_str(source, 0);
		if (!deq_str) {
			fprintf(stderr, "%s: %s: Error dequoting file name\n",
				PROGRAM_NAME, source);
			return EXIT_FAILURE;
		}
		strcpy(source, deq_str);
		free(deq_str);
	}

	if (dest) {
		if (strchr(dest, '\\')) {
			char *deq_str = dequote_str(dest, 0);
			if (!deq_str) {
				fprintf(stderr, "%s: %s: Error dequoting file name\n",
					PROGRAM_NAME, source);
				return EXIT_FAILURE;
			}
			strcpy(dest, deq_str);
			free(deq_str);
		}
	}

	int exit_status =  EXIT_SUCCESS;
	int free_dest = 0;

	/* If no dest, use source as file name: source.copy, and, if already
	 * exists, source.copy.YYYYMMDDHHMMSS */
	if (!dest) {
		size_t source_len = strlen(source);
		if (strcmp(source, "/") != 0 && source[source_len - 1] == '/')
			source[source_len - 1] = '\0';

		char *tmp = strrchr(source, '/');
		char *source_name;

		if (tmp && *(tmp + 1))
			source_name = tmp + 1;
		else
			source_name = source;
			
		free_dest = 1;
		dest = (char *)xnmalloc(strlen(source_name) + 6, sizeof(char));
		sprintf(dest, "%s.copy", source_name);

		struct stat attr;
		if (stat(dest, &attr) == EXIT_SUCCESS) {
			time_t rawtime = time(NULL);
			struct tm tm;
			localtime_r(&rawtime, &tm);
			char date[64] = "";
			strftime(date, sizeof(date), "%b %d %H:%M:%S %Y", &tm);

			char suffix[68] = "";
			snprintf(suffix, 67, "%d%d%d%d%d%d", tm.tm_year + 1900,
				tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
				tm.tm_sec);

			char tmp_dest[PATH_MAX];
			xstrsncpy(tmp_dest, dest, PATH_MAX);
			dest = (char *)xrealloc(dest, (strlen(tmp_dest) + strlen(suffix) + 2)
									* sizeof(char));
			sprintf(dest, "%s.%s", tmp_dest, suffix);
		}
	}

	char *rsync_path = get_cmd_path("rsync");
	if (rsync_path) {
		char *cmd[] = {"rsync", "-aczvAXHS", "--progress", source, dest, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
		free(rsync_path);
	} else {
		char *cmd[] = {"cp", "-a", source, dest, NULL};
		if (launch_execve(cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	if (free_dest)
		free(dest);
	return exit_status;
}

int
create_file(char **cmd)
{
	if (cmd[1] && *cmd[1] == '-' && strcmp(cmd[1], "--help") == 0) {
		puts(_(NEW_USAGE));
		return EXIT_FAILURE;
	}

	log_function(NULL);

	int exit_status = EXIT_SUCCESS;
#ifdef __HAIKU__
	int file_in_cwd = 0;
#endif
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
	size_t i;
	for (i = 1; cmd[i]; i++) {
		if (strchr(cmd[i], '\\')) {
			char *deq_str = dequote_str(cmd[i], 0);
			if (!deq_str) {
				_err('w', PRINT_PROMPT, _("%s: %s: Error dequoting filename\n"),
					PROGRAM_NAME, cmd[i]);
				continue;
			}

			strcpy(cmd[i], deq_str);
			free(deq_str);
		}

		if (*cmd[i] == '~') {
			char *exp_path = tilde_expand(cmd[i]);
			if (exp_path) {
				cmd[i] = (char *)xrealloc(cmd[i], (strlen(exp_path) + 1)
											* sizeof(char));
				strcpy(cmd[i], exp_path);
				free(exp_path);
			}
		}

#ifdef __HAIKU__
		/* If at least one filename lacks a slash (or it is the last char,
		 * in which case we have a directory in CWD), we are creating a
		 * file in CWD, and thereby we need to update the screen */
		char *ret = strrchr(cmd[i], '/');
		if (!ret || !*(ret + 1))
			file_in_cwd = 1;
#endif
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
		size_t cmd_len = strlen(cmd[i]);
		/* Filenames ending with a slash are taken as dir names */
		if (cmd[i][cmd_len - 1] == '/')
			ndirs[cndirs++] = cmd[i];
		else
			nfiles[cnfiles++] = cmd[i];
	}

	ndirs[cndirs] = (char *)NULL;
	nfiles[cnfiles] = (char *)NULL;

	/* Execute commands */
	if (cnfiles > 1) {
		if (launch_execve(nfiles, FOREGROUND, 0) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	if (cndirs > 2) {
		if (launch_execve(ndirs, FOREGROUND, 0) != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}

	free(nfiles[0]);
	free(ndirs[0]);
	free(ndirs[1]);
	free(nfiles);
	free(ndirs);
	if (free_cmd) {
		for (i = 0; cmd[i]; i++)
			free(cmd[i]);
		free(cmd);
	}

#ifdef __HAIKU__
	if (exit_status == EXIT_SUCCESS && cd_lists_on_the_fly && file_in_cwd) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}
#endif

	return exit_status;
}

int
open_function(char **cmd)
{
	if (!cmd)
		return EXIT_FAILURE;

	if (*cmd[0] == 'o' && (!cmd[0][1] || strcmp(cmd[0], "open") == 0)) {
		if (strchr(cmd[1], '\\')) {
			char *deq_path = dequote_str(cmd[1], 0);
			if (!deq_path) {
				fprintf(stderr, _("%s: %s: Error dequoting filename\n"),
					PROGRAM_NAME, cmd[1]);
				return EXIT_FAILURE;
			}

			strcpy(cmd[1], deq_path);
			free(deq_path);
		}
	}

	char *file = cmd[1];

	/* Check file existence */
	struct stat attr;
	if (stat(file, &attr) == -1) {
		fprintf(stderr, "%s: open: %s: %s\n", PROGRAM_NAME, cmd[1],
		    strerror(errno));
		return EXIT_FAILURE;
	}

	/* Check file type: only directories, symlinks, and regular files
	 * will be opened */

	char no_open_file = 1, file_type[128];
	/* Reserve a good amount of bytes for file type: it cannot be
		  * known beforehand how many bytes the TRANSLATED string will
		  * need */

	switch ((attr.st_mode & S_IFMT)) {
		/* Store file type to compose and print the error message, if
		 * necessary */
	case S_IFBLK: strcpy(file_type, _("block device")); break;
	case S_IFCHR: strcpy(file_type, _("character device")); break;
	case S_IFSOCK: strcpy(file_type, _("socket")); break;
	case S_IFIFO: strcpy(file_type, _("FIFO/pipe")); break;
	case S_IFDIR: return cd_function(file, CD_PRINT_ERROR);
	case S_IFREG:
#ifndef _NO_ARCHIVING
		/* If an archive/compressed file, call archiver() */
		if (is_compressed(file, 1) == 0) {
			char *tmp_cmd[] = {"ad", file, NULL};
			return archiver(tmp_cmd, 'd');
		}
#endif
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

	log_function(NULL);

	/* Dequote the file name, if necessary */
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
	if (!real_path) {
		printf(_("%s%s%s currently pointing to nowhere (broken link)\n"),
		    or_c, link, df_c);
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
			} else {
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
	log_function(NULL);

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

	if (copy_n_rename) { /* vv */
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
	&& (!comm[0][2] || comm[0][2] == ' '))
		clear_selbox();

#ifdef __HAIKU__
	if (cd_lists_on_the_fly) {
		free_dirlist();
		list_dir();
	}
#endif

	return EXIT_SUCCESS;
}

int
remove_file(char **args)
{
	int cwd = 0, exit_status = EXIT_SUCCESS;

	log_function(NULL);

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
				/* Start storing file names in 3: 0 is for 'rm', and 1
				 * and 2 for parameters, including end of parameters (--) */
				rm_cmd[j++] = savestring(tmp, strlen(tmp));
				free(tmp);
			} else {
				fprintf(stderr, "%s: %s: Error dequoting file name\n",
				    PROGRAM_NAME, args[i]);
				continue;
			}
		} else {
			rm_cmd[j++] = savestring(args[i], strlen(args[i]));
		}

		struct stat attr;
		if (!dirs && lstat(rm_cmd[j - 1], &attr) != -1
		&& (attr.st_mode & S_IFMT) == S_IFDIR)
			dirs = 1;
	}

	rm_cmd[j] = (char *)NULL;

	rm_cmd[0] = savestring("rm", 2);
	if (dirs)
#if defined(__NetBSD__) || defined(__OpenBSD__)
		rm_cmd[1] = savestring("-r", 2);
#else
		rm_cmd[1] = savestring("-dIr", 4);
#endif
	else
#if defined(__NetBSD__) || defined(__OpenBSD__)
		rm_cmd[1] = savestring("-f", 2);
#else
		rm_cmd[1] = savestring("-I", 2);
#endif
	rm_cmd[2] = savestring("--", 2);

	if (launch_execve(rm_cmd, FOREGROUND, E_NOFLAG) != EXIT_SUCCESS)
		exit_status = EXIT_FAILURE;
#ifdef __HAIKU__
	else {
		if (cwd && cd_lists_on_the_fly && strcmp(args[1], "--help") != 0
		&& strcmp(args[1], "--version") != 0) {
			free_dirlist();
			exit_status = list_dir();
		}
	}
#endif

	if (is_sel && exit_status == EXIT_SUCCESS)
		clear_selbox();

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
 * asked whether to perform the actual bulk renaming (via mv) or not.
 * I took this bulk rename method, just because it is quite simple and
 * KISS, from the fff filemanager. So, thanks fff! BTW, this method
 * is also implemented by ranger and nnn */
int
bulk_rename(char **args)
{
	if (!args[1])
		return EXIT_FAILURE;

	log_function(NULL);

	int exit_status = EXIT_SUCCESS;

	char bulk_file[PATH_MAX];
	if (xargs.stealth_mode == 1)
		sprintf(bulk_file, "/tmp/.clifm_bulk_rename");
	else
		sprintf(bulk_file, "%s/.bulk_rename", tmp_dir);

	int fd;
	FILE *fp = open_fstream_w(bulk_file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "bulk: '%s': %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i, arg_total = 0;

	/* Copy all files to be renamed to the bulk file */
	for (i = 1; args[i]; i++) {
		/* Dequote file name, if necessary */
		if (strchr(args[i], '\\')) {
			char *deq_file = dequote_str(args[i], 0);
			if (!deq_file) {
				fprintf(stderr, _("bulk: %s: Error dequoting "
						"file name\n"), args[i]);
				continue;
			}
			strcpy(args[i], deq_file);
			free(deq_file);
		}

		fprintf(fp, "%s\n", args[i]);
	}

	arg_total = i;
	close_fstream(fp, fd);

	fp = open_fstream_r(bulk_file, &fd);
	if (!fp) {
		_err('e', PRINT_PROMPT, "bulk: '%s': %s\n", bulk_file, strerror(errno));
		return EXIT_FAILURE;
	}

	/* Store the last modification time of the bulk file. This time
	 * will be later compared to the modification time of the same
	 * file after shown to the user */
	struct stat attr;
	fstat(fd, &attr);
	time_t mtime_bfr = (time_t)attr.st_mtime;

	/* Open the bulk file */
	if (open_file(bulk_file) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	/* Compare the new modification time to the stored one: if they
	 * match, nothing was modified */
	fstat(fd, &attr);
	if (mtime_bfr == (time_t)attr.st_mtime) {
		puts(_("bulk: Nothing to do"));
		if (unlinkat(fd, bulk_file, 0) == -1) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			    bulk_file, strerror(errno));
			exit_status = EXIT_FAILURE;
		}
		close_fstream(fp, fd);
		return exit_status;
	}

	/* Go back to the beginning of the bulk file */
	fseek(fp, 0, SEEK_SET);

	/* Make sure there are as many lines in the bulk file as files
	 * to be renamed */
	size_t file_total = 1;
	char tmp_line[256];
	while (fgets(tmp_line, (int)sizeof(tmp_line), fp))
		file_total++;

	if (arg_total != file_total) {
		fputs(_("bulk: Line mismatch in rename file\n"), stderr);
		if (unlinkat(fd, bulk_file, 0) == -1)
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			    bulk_file, strerror(errno));
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
		puts(_("bulk: Nothing to do"));
		if (unlinkat(fd, bulk_file, 0) == -1) {
			_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
			    bulk_file, strerror(errno));
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
		if (strlen(answer) > 1) {
			free(answer);
			answer = (char *)NULL;
			continue;
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
		_err('e', PRINT_PROMPT, "%s: '%s': %s\n", PROGRAM_NAME,
		    bulk_file, strerror(errno));
		exit_status = EXIT_FAILURE;
	}
	close_fstream(fp, fd);

#ifdef __HAIKU__
	if (cd_lists_on_the_fly) {
		free_dirlist();
		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}
#endif

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

	char *tmp_file = (char *)xnmalloc(strlen(tmp_dir) + 14, sizeof(char));
	sprintf(tmp_file, "%s/.clifm%s", tmp_dir, rand_ext);
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
	} else {
		for (i = 1; filenames[i]; i++) {
			if (*filenames[i] == '.' && (!filenames[i][1] || (filenames[i][1] == '.' && !filenames[i][2])))
				continue;
			fprintf(fp, "%s\n", filenames[i]);
		}
	}

	fclose(fp);

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

	if (!args[1] || (*args[1] == '-' && strcmp(args[1], "--help") == 0)) {
		puts(_(BL_USAGE));
		return EXIT_SUCCESS;
	}

	log_function(NULL);

	char *suffix = (char *)NULL;
	while (!suffix) {
		suffix = rl_no_hist(_("Enter links suffix ('n' for none): "));
		if (!suffix)
			continue;
		if (!*suffix) {
			free(suffix);
			suffix = (char *)NULL;
			continue;
		}
	}

	size_t i;
	int exit_status = EXIT_SUCCESS;
	char tmp[NAME_MAX];

	for (i = 1; args[i]; i++) {
		char *linkname = (char *)NULL;

		if (*suffix == 'n' && !suffix[1]) {
			linkname = args[i];
		} else {
			snprintf(tmp, NAME_MAX, "%s%s", args[i], suffix);
			linkname = tmp;
		}

		char *ptr = strrchr(linkname, '/');
		if (symlinkat(args[i], AT_FDCWD, ptr ? ++ptr : linkname) == -1) {
			exit_status = EXIT_FAILURE;
			fprintf(stderr, _("%s: %s: Cannot create symlink: %s\n"),
			    PROGRAM_NAME, ptr ? ptr : linkname, strerror(errno));
		}
	}

#ifdef __HAIKU__
	if (exit_status == EXIT_SUCCESS && cd_lists_on_the_fly) {
		free_dirlist();

		if (list_dir() != EXIT_SUCCESS)
			exit_status = EXIT_FAILURE;
	}
#endif

	free(suffix);
	return exit_status;
}
