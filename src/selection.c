/* selection.c -- files selection functions */

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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <readline/readline.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__) || defined(__HAIKU__)
#ifdef __TINYC__
/* Silence a tcc warning. We don't use CTRL anyway */
#undef CTRL
#endif
#include <sys/ioctl.h>
#endif

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "listing.h"
#include "misc.h"
#include "navigation.h"
#include "readline.h"
#include "selection.h"
#include "sort.h"
#include "messages.h"

/* Save selected elements into a tmp file. Returns 1 if success and 0
 * if error. This function allows the user to work with multiple
 * instances of the program: he/she can select some files in the
 * first instance and then execute a second one to operate on those
 * files as he/she whises. */
int
save_sel(void)
{
	if (!selfile_ok || !config_ok || !sel_file)
		return EXIT_FAILURE;

	if (sel_n == 0) {
		if (unlink(sel_file) == -1) {
			fprintf(stderr, "%s: sel: %s: %s\n", PROGRAM_NAME,
			    sel_file, strerror(errno));
			return EXIT_FAILURE;
		} else {
			return EXIT_SUCCESS;
		}
	}

	int fd;
	FILE *fp = open_fstream_w(sel_file, &fd);
	if (!fp) {
		_err(0, NOPRINT_PROMPT, "%s: sel: %s: %s\n", PROGRAM_NAME,
		    sel_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i;
	for (i = 0; i < sel_n; i++) {
		fputs(sel_elements[i], fp);
		fputc('\n', fp);
	}

	close_fstream(fp, fd);
	return EXIT_SUCCESS;
}

static int
select_file(char *file)
{
	if (!file || !*file)
		return 0;

	/* Check if the selected element is already in the selection
	 * box */
	int exists = 0, new_sel = 0, j;

	j = (int)sel_n;
	while (--j >= 0) {
		if (*file == *sel_elements[j] && strcmp(sel_elements[j], file) == 0) {
			exists = 1;
			break;
		}
	}

	if (!exists) {
		sel_elements = (char **)xrealloc(sel_elements, (sel_n + 2)
											* sizeof(char *));
		sel_elements[sel_n++] = savestring(file, strlen(file));
		sel_elements[sel_n] = (char *)NULL;
		new_sel++;
	} else {
		fprintf(stderr, _("%s: sel: %s: Already selected\n"),
		    PROGRAM_NAME, file);
	}

	return new_sel;
}

static int
sel_glob(char *str, const char *sel_path, mode_t filetype)
{
	if (!str || !*str)
		return -1;

	glob_t gbuf;
	char *pattern = str;
	int invert = 0, ret = -1;

	if (*pattern == '!') {
		pattern++;
		invert = 1;
	}

	ret = glob(pattern, 0, NULL, &gbuf);

	if (ret == GLOB_NOSPACE || ret == GLOB_ABORTED) {
		globfree(&gbuf);
		return -1;
	}

	if (ret == GLOB_NOMATCH) {
		globfree(&gbuf);
		return 0;
	}

	char **matches = (char **)NULL;
	int i, k = 0;
	struct dirent **ent = (struct dirent **)NULL;

	if (invert) {
		if (!sel_path) {
			matches = (char **)xnmalloc(files + 2, sizeof(char *));

			i = (int)files;
			while (--i >= 0) {
				if (filetype && file_info[i].type != filetype)
					continue;

				int found = 0;
				int j = (int)gbuf.gl_pathc;
				while (--j >= 0) {
					if (*file_info[i].name == *gbuf.gl_pathv[j]
					&& strcmp(file_info[i].name, gbuf.gl_pathv[j]) == 0) {
						found = 1;
						break;
					}
				}

				if (!found)
					matches[k++] = file_info[i].name;
			}
		} else {
			ret = scandir(sel_path, &ent, skip_files, xalphasort);
			if (ret == -1) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
				    sel_path, strerror(errno));
				globfree(&gbuf);
				return -1;
			}

			matches = (char **)xnmalloc((size_t)ret + 2, sizeof(char *));

			i = ret;
			while (--i >= 0) {
#if !defined(_DIRENT_HAVE_D_TYPE)
				mode_t type;
				struct stat attr;
				if (lstat(ent[i]->d_name, &attr) == -1)
					continue;
				switch (attr.st_mode & S_IFMT) {
				case S_IFBLK: type = DT_BLK; break;
				case S_IFCHR: type = DT_CHR; break;
				case S_IFDIR: type = DT_DIR; break;
				case S_IFIFO: type = DT_FIFO; break;
				case S_IFLNK: type = DT_LNK; break;
				case S_IFREG: type = DT_REG; break;
				case S_IFSOCK: type = DT_SOCK; break;
				default: type = DT_UNKNOWN; break;
				}
				if (filetype && type != filetype)
#else
				if (filetype && ent[i]->d_type != filetype)
#endif
					continue;

				int j = (int)gbuf.gl_pathc;
				while (--j >= 0) {
					if (*ent[i]->d_name == *gbuf.gl_pathv[j]
					&& strcmp(ent[i]->d_name, gbuf.gl_pathv[j]) == 0)
						break;
				}

				if (!gbuf.gl_pathv[j])
					matches[k++] = ent[i]->d_name;
			}
		}
	}

	else {
		matches = (char **)xnmalloc(gbuf.gl_pathc + 2,
		    sizeof(char *));
		mode_t t = 0;
		if (filetype) {

			switch (filetype) {
			case DT_DIR: t = S_IFDIR; break;
			case DT_REG: t = S_IFREG; break;
			case DT_LNK: t = S_IFLNK; break;
			case DT_SOCK: t = S_IFSOCK; break;
			case DT_FIFO: t = S_IFIFO; break;
			case DT_BLK: t = S_IFBLK; break;
			case DT_CHR: t = S_IFCHR; break;
			}
		}

		i = (int)gbuf.gl_pathc;
		while (--i >= 0) {
			/* We need to run stat(3) here, so that the d_type macros
			 * won't work: convert them into st_mode macros */
			if (filetype) {
				struct stat attr;
				if (lstat(gbuf.gl_pathv[i], &attr) == -1)
					continue;
				if ((attr.st_mode & S_IFMT) != t)
					continue;
			}

			if (*gbuf.gl_pathv[i] == '.' && (!gbuf.gl_pathv[i][1]
			|| (gbuf.gl_pathv[i][1] == '.' && !gbuf.gl_pathv[i][2])))
				continue;

			matches[k++] = gbuf.gl_pathv[i];
		}
	}

	matches[k] = (char *)NULL;
	int new_sel = 0;

	i = k;
	while (--i >= 0) {
		if (!matches[i])
			continue;

		if (!sel_path) {
			if (*matches[i] == '/') {
				new_sel += select_file(matches[i]);
			} else {
				char *tmp = (char *)NULL;
				if (*ws[cur_ws].path == '/' && !*(ws[cur_ws].path + 1)) {
					tmp = (char *)xnmalloc(strlen(matches[i]) + 2,
								sizeof(char));
					sprintf(tmp, "/%s", matches[i]);
				} else {
					tmp = (char *)xnmalloc(strlen(ws[cur_ws].path)
								+ strlen(matches[i]) + 2, sizeof(char));
					sprintf(tmp, "%s/%s", ws[cur_ws].path, matches[i]);
				}
				new_sel += select_file(tmp);
				free(tmp);
			}
		} else {
			char *tmp = (char *)xnmalloc(strlen(sel_path)
						+ strlen(matches[i]) + 2, sizeof(char));
			sprintf(tmp, "%s/%s", sel_path, matches[i]);
			new_sel += select_file(tmp);
			free(tmp);
		}
	}

	free(matches);
	globfree(&gbuf);

	if (invert && sel_path) {
		i = ret;
		while (--i >= 0)
			free(ent[i]);
		free(ent);
	}

	return new_sel;
}

static int
sel_regex(char *str, const char *sel_path, mode_t filetype)
{
	if (!str || !*str)
		return -1;

	char *pattern = str;

	int invert = 0;
	if (*pattern == '!') {
		pattern++;
		invert = 1;
	}

	regex_t regex;
	if (regcomp(&regex, pattern, REG_NOSUB | REG_EXTENDED) != EXIT_SUCCESS) {
		fprintf(stderr, _("%s: sel: %s: Invalid regular "
				"expression\n"), PROGRAM_NAME, str);

		regfree(&regex);
		return -1;
	}

	int new_sel = 0, i;

	if (!sel_path) { /* Check pattern (STR) against files in CWD */
		i = (int)files;
		while (--i >= 0) {
			if (filetype && file_info[i].type != filetype)
				continue;

			char tmp_path[PATH_MAX];
			if (*ws[cur_ws].path == '/' && !*(ws[cur_ws].path + 1)) {
				snprintf(tmp_path, PATH_MAX - 1, "/%s", file_info[i].name);
			} else {
				snprintf(tmp_path, PATH_MAX - 1, "%s/%s", ws[cur_ws].path,
						file_info[i].name);
			}

			if (regexec(&regex, file_info[i].name, 0, NULL, 0) == EXIT_SUCCESS) {
				if (!invert)
					new_sel += select_file(tmp_path);
			} else if (invert) {
				new_sel += select_file(tmp_path);
			}
		}
	} else { /* Check pattern against files in SEL_PATH */
		struct dirent **list = (struct dirent **)NULL;
		int filesn = scandir(sel_path, &list, skip_files, xalphasort);

		if (filesn == -1) {
			fprintf(stderr, "sel: %s: %s\n", sel_path, strerror(errno));
			return -1;
		}

		mode_t t = 0;
		if (filetype) {

			switch (filetype) {
			case DT_DIR: t = S_IFDIR; break;
			case DT_REG: t = S_IFREG; break;
			case DT_LNK: t = S_IFLNK; break;
			case DT_SOCK: t = S_IFSOCK; break;
			case DT_FIFO: t = S_IFIFO; break;
			case DT_BLK: t = S_IFBLK; break;
			case DT_CHR: t = S_IFCHR; break;
			}
		}

		i = (int)filesn;
		while (--i >= 0) {
			if (filetype) {
				struct stat attr;
				if (lstat(list[i]->d_name, &attr) != -1) {
					if ((attr.st_mode & S_IFMT) != t) {
						free(list[i]);
						continue;
					}
				}
			}

			char *tmp_path = (char *)xnmalloc(strlen(sel_path)
							+ strlen(list[i]->d_name) + 2, sizeof(char));
			sprintf(tmp_path, "%s/%s", sel_path, list[i]->d_name);

			if (regexec(&regex, list[i]->d_name, 0, NULL, 0) == EXIT_SUCCESS) {
				if (!invert)
					new_sel += select_file(tmp_path);
			} else if (invert) {
				new_sel += select_file(tmp_path);
			}

			free(tmp_path);
			free(list[i]);
		}

		free(list);
	}

	regfree(&regex);
	return new_sel;
}

int
sel_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1] || (*args[1] == '-' && args[1][1] == '-'
	&& strcmp(args[1], "--help") == 0)) {
		puts(_(SEL_USAGE));
		return EXIT_SUCCESS;
	}

	char *sel_path = (char *)NULL;
	mode_t filetype = 0;
	int i, ifiletype = 0, isel_path = 0, new_sel = 0;

	for (i = 1; args[i]; i++) {
		if (*args[i] == '-') {
			ifiletype = i;
			filetype = (mode_t) * (args[i] + 1);
		}

		if (*args[i] == ':') {
			isel_path = i;
			sel_path = args[i] + 1;
		}

		if (*args[i] == '~') {
			char *exp_path = tilde_expand(args[i]);
			if (!exp_path) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, args[i],
				    strerror(errno));
				return EXIT_FAILURE;
			}

			args[i] = (char *)xrealloc(args[i], (strlen(exp_path) + 1) *
								sizeof(char));
			strcpy(args[i], exp_path);
			free(exp_path);
		}
	}

	if (filetype) {
		/* Convert file type into a macro that can be decoded by stat().
		 * If file type is specified, matches will be checked against
		 * this value */
		switch (filetype) {
		case 'd': filetype = DT_DIR; break;
		case 'r': filetype = DT_REG; break;
		case 'l': filetype = DT_LNK; break;
		case 's': filetype = DT_SOCK; break;
		case 'f': filetype = DT_FIFO; break;
		case 'b': filetype = DT_BLK; break;
		case 'c': filetype = DT_CHR; break;
		default:
			fprintf(stderr, _("%s: '%c': Unrecognized file type\n"),
			    PROGRAM_NAME, (char)filetype);
			return EXIT_FAILURE;
		}
	}

	char dir[PATH_MAX];

	if (sel_path) {
		size_t sel_path_len = strlen(sel_path);
		if (sel_path[sel_path_len - 1] == '/')
			sel_path[sel_path_len - 1] = '\0';

		char *tmpdir = xnmalloc(PATH_MAX + 1, sizeof(char));

		if (strchr(sel_path, '\\')) {
			char *deq_str = dequote_str(sel_path, 0);
			if (deq_str) {
				strcpy(sel_path, deq_str);
				free(deq_str);
			}
		}

		strcpy(tmpdir, sel_path);

		if (*sel_path == '.') {
			if (realpath(sel_path, tmpdir) == NULL) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, sel_path,
						strerror(errno));
				return EXIT_FAILURE;
			}
		}

		if (*sel_path == '~') {
			char *exp_path = tilde_expand(sel_path);
			if (!exp_path) {
				fprintf(stderr, _("%s: Error expanding path\n"), PROGRAM_NAME);
				free(tmpdir);
				return EXIT_FAILURE;
			}
			strcpy(tmpdir, exp_path);
			free(exp_path);
		}

		if (*tmpdir != '/') {
			snprintf(dir, PATH_MAX, "%s/%s", ws[cur_ws].path, tmpdir);
		} else
			strcpy(dir, tmpdir);

		free(tmpdir);

		if (access(dir, X_OK) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, dir,
			    strerror(errno));
			return EXIT_FAILURE;
		}

		if (xchdir(dir, NO_TITLE) == -1) {
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, dir,
			    strerror(errno));
			return EXIT_FAILURE;
		}
	}

	char *pattern = (char *)NULL;

	for (i = 1; args[i]; i++) {
		if (i == ifiletype || i == isel_path)
			continue;
		/*      int invert = 0; */

		if (check_regex(args[i]) == EXIT_SUCCESS) {
			pattern = args[i];
			if (*pattern == '!') {
				pattern++;
				/*  invert = 1; */
			}
		}

		if (!pattern) {
			if (strchr(args[i], '\\')) {
				char *deq_str = dequote_str(args[i], 0);
				if (deq_str) {
					strcpy(args[i], deq_str);
					free(deq_str);
				}
			}

			char *tmp = (char *)NULL;

			if (*args[i] != '/') {
				if (!sel_path) {
					if (*ws[cur_ws].path == '/' && !*(ws[cur_ws].path + 1)) {
						tmp = (char *)xnmalloc(strlen(args[i]) + 2,
									sizeof(char));
						sprintf(tmp, "/%s", args[i]);
					} else {
						tmp = (char *)xnmalloc(strlen(ws[cur_ws].path)
									+ strlen(args[i]) + 2, sizeof(char));
						sprintf(tmp, "%s/%s", ws[cur_ws].path, args[i]);
					}
				} else {
					tmp = (char *)xnmalloc(strlen(dir) + strlen(args[i])
													+ 2, sizeof(char));
					sprintf(tmp, "%s/%s", dir, args[i]);
				}

				struct stat fattr;
				if (lstat(tmp, &fattr) == -1) {
					fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
					    args[i], strerror(errno));
				} else {
					new_sel += select_file(tmp);
				}
				free(tmp);
			} else {
				struct stat fattr;
				if (lstat(args[i], &fattr) == -1) {
					fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME,
					    args[i], strerror(errno));
				} else {
					new_sel += select_file(args[i]);
				}
			}
		} else {
			/* We have a pattern */
			/* GLOB */
			int ret = -1;
			ret = sel_glob(args[i], sel_path ? dir : NULL,
			    filetype ? filetype : 0);

			/* If glob failed, try REGEX */
			if (ret <= 0) {
				ret = sel_regex(args[i], sel_path ? dir : NULL,
				    filetype);
				if (ret > 0)
					new_sel += ret;
			} else {
				new_sel += ret;
			}
		}
	}

	if (new_sel > 0 && xargs.stealth_mode != 1) {
		if (sel_file && save_sel() != EXIT_SUCCESS) {
			_err('e', PRINT_PROMPT, _("%s: Error writing selected files "
				"to the selections file\n"), PROGRAM_NAME);
		}
	}

	if (sel_path && xchdir(ws[cur_ws].path, NO_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, ws[cur_ws].path,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	if (new_sel <= 0) {
		if (pattern)
			fprintf(stderr, _("%s: No matches found\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get total size of sel files */
	struct stat sattr;

	i = (int)sel_n;
	while (--i >= 0) {
		if (lstat(sel_elements[i], &sattr) != -1) {
			/*          if ((sattr.st_mode & S_IFMT) == S_IFDIR) {
				off_t dsize = dir_size(sel_elements[i]);
				total_sel_size += dsize;
			}
			else */
			total_sel_size += sattr.st_size;
		}
	}

	/* Print entries */
	if (sel_n > 10) {
		printf(_("%zu files are now in the Selection Box\n"), sel_n);
	} else if (sel_n > 0) {
		printf(_("%zu selected %s:\n\n"), sel_n, (sel_n == 1) ? _("file")
				: _("files"));

		for (i = 0; i < (int)sel_n; i++)
			colors_list(sel_elements[i], (int)i + 1, NO_PAD,
			    PRINT_NEWLINE);
	}

	/* Print total size */
	char *human_size = get_size_unit(total_sel_size);

	if (sel_n > 10)
		printf(_("Total size: %s\n"), human_size);
	else if (sel_n > 0)
		printf(_("\n%s%sTotal size%s: %s\n"), df_c, BOLD, df_c, human_size);

	free(human_size);
	return EXIT_SUCCESS;
}

void
show_sel_files(void)
{
	if (clear_screen)
		CLEAR;

	printf(_("%s%sSelection Box%s\n"), df_c, BOLD, df_c);

	int reset_pager = 0;

	if (sel_n == 0) {
		puts(_("Empty"));
	} else {
		putchar('\n');
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		size_t counter = 0;
		int t_rows = (int)w.ws_row;
		t_rows -= 2;
		size_t i;

		for (i = 0; i < sel_n; i++) {
			/* if (pager && counter > (term_rows-2)) { */
			if (pager && counter > (size_t)t_rows) {
				switch (xgetchar()) {
				/* Advance one line at a time */
				case 66: /* fallthrough */ /* Down arrow */
				case 10: /* fallthrough */ /* Enter */
				case 32: /* Space */
					break;
				/* Advance one page at a time */
				case 126:
					counter = 0; /* Page Down */
					break;
				/* Stop paging (and set a flag to reenable the pager
				 * later) */
				case 99: /* fallthrough */  /* 'c' */
				case 112: /* fallthrough */ /* 'p' */
				case 113:
					pager = 0, reset_pager = 1; /* 'q' */
					break;
				/* If another key is pressed, go back one position.
				 * Otherwise, some file names won't be listed.*/
				default:
					i--;
					continue;
					break;
				}
			}

			counter++;
			colors_list(sel_elements[i], (int)i + 1, NO_PAD, PRINT_NEWLINE);
		}

		char *human_size = get_size_unit(total_sel_size);
		printf(_("\n%s%sTotal size%s: %s\n"), df_c, BOLD, df_c, human_size);
		free(human_size);
	}

	if (reset_pager)
		pager = 1;
}

int
deselect(char **comm)
{
	if (!comm)
		return EXIT_FAILURE;

	if (comm[1] && (strcmp(comm[1], "*") == 0 || strcmp(comm[1], "a") == 0
	|| strcmp(comm[1], "all") == 0)) {

		if (sel_n > 0) {
			int i = (int)sel_n;

			while (--i >= 0)
				free(sel_elements[i]);

			sel_n = 0;
			total_sel_size = 0;

			if (save_sel() != 0)
				return EXIT_FAILURE;
			else
				return EXIT_SUCCESS;
		} else {
			puts(_("desel: There are no selected files"));
			return EXIT_SUCCESS;
		}
	}

	register int i;

/*	if (clear_screen)
		CLEAR; */

	printf(_("%sSelection Box%s\n"), BOLD, df_c);

	if (sel_n == 0) {
		puts(_("Empty"));
		return EXIT_SUCCESS;
	}

	putchar('\0');

	for (i = 0; i < (int)sel_n; i++)
		colors_list(sel_elements[i], (int)i + 1, NO_PAD, PRINT_NEWLINE);

	char *human_size = get_size_unit(total_sel_size);
	printf(_("\n%s%sTotal size%s: %s\n"), df_c, BOLD, df_c, human_size);
	free(human_size);

	printf(_("\n%sEnter '%c' to quit.\n"), df_c, 'q');
	size_t desel_n = 0;
	char *line = NULL, **desel_elements = (char **)NULL;

	while (!line)
		line = rl_no_hist(_("File(s) to be deselected (ex: 1 2-6, or *): "));

	desel_elements = get_substr(line, ' ');
	free(line);

	if (!desel_elements)
		return EXIT_FAILURE;

	for (i = 0; desel_elements[i]; i++)
		desel_n++;

	i = (int)desel_n;
	while (--i >= 0) { /* Validation */
		/* If not a number */
		if (!is_number(desel_elements[i])) {
			if (strcmp(desel_elements[i], "q") == 0) {
				i = (int)desel_n;
				while (--i >= 0)
					free(desel_elements[i]);
				free(desel_elements);
				return EXIT_SUCCESS;
			} else if (strcmp(desel_elements[i], "*") == 0) {
				/* Clear the sel array */
				i = (int)sel_n;
				while (--i >= 0)
					free(sel_elements[i]);

				sel_n = 0;
				total_sel_size = 0;

				i = (int)desel_n;
				while (--i >= 0)
					free(desel_elements[i]);

				int exit_status = EXIT_SUCCESS;

				if (save_sel() != 0)
					exit_status = EXIT_FAILURE;

				free(desel_elements);

				if (cd_lists_on_the_fly) {
					free_dirlist();
					if (list_dir() != EXIT_SUCCESS)
						exit_status = EXIT_FAILURE;
				}

				return exit_status;
			} else {
				printf(_("desel: '%s': Invalid element\n"), desel_elements[i]);
				int j = (int)desel_n;
				while (--j >= 0)
					free(desel_elements[j]);
				free(desel_elements);
				return EXIT_FAILURE;
			}
		}

		/* If a number, check it's a valid ELN */
		else {
			int atoi_desel = atoi(desel_elements[i]);
			if (atoi_desel == 0 || (size_t)atoi_desel > sel_n) {
				printf(_("desel: '%s': Invalid ELN\n"), desel_elements[i]);
				int j = (int)desel_n;
				while (--j >= 0)
					free(desel_elements[j]);
				free(desel_elements);
				return EXIT_FAILURE;
			}
		}
	}

	/* If a valid ELN and not asterisk... */
	/* Store the full path of all the elements to be deselected in a new
	 * array (desel_path). I need to do this because after the first
	 * rearragement of the sel array, that is, after the removal of the
	 * first element, the index of the next elements changed, and cannot
	 * thereby be found by their index. The only way to find them is to
	 * compare string by string */
	char **desel_path = (char **)NULL;
	desel_path = (char **)xnmalloc(desel_n, sizeof(char *));

	i = (int)desel_n;
	while (--i >= 0) {
		int desel_int = atoi(desel_elements[i]);
		desel_path[i] = savestring(sel_elements[desel_int - 1],
		    strlen(sel_elements[desel_int - 1]));
	}

	/* Search the sel array for the path of the element to deselect and
	 * store its index */
	struct stat desel_attrib;

	i = (int)desel_n;
	while (--i >= 0) {
		int j, k, desel_index = 0;

		k = (int)sel_n;
		while (--k >= 0) {
			if (strcmp(sel_elements[k], desel_path[i]) == 0) {
				/* Sustract size from total size */
				if (lstat(sel_elements[k], &desel_attrib) != -1) {
					if ((desel_attrib.st_mode & S_IFMT) == S_IFDIR)
						total_sel_size -= dir_size(sel_elements[k]);
					else
						total_sel_size -= desel_attrib.st_size;
				}

				desel_index = k;
				break;
			}
		}

		/* Once the index was found, rearrange the sel array removing the
		 * deselected element (actually, moving each string after it to
		 * the previous position) */
		for (j = desel_index; j < (int)(sel_n - 1); j++) {
			sel_elements[j] = (char *)xrealloc(sel_elements[j],
			    (strlen(sel_elements[j + 1]) + 1) * sizeof(char));
			strcpy(sel_elements[j], sel_elements[j + 1]);
		}
	}

	/* Free the last DESEL_N elements from the old sel array. They won't
	 * be used anymore, for they contain the same value as the last
	 * non-deselected element due to the above array rearrangement */
	for (i = 1; i <= (int)desel_n; i++)
		if (((int)sel_n - i) >= 0 && sel_elements[(int)sel_n - i])
			free(sel_elements[(int)sel_n - i]);

	/* Reallocate the sel array according to the new size */
	sel_n = (sel_n - desel_n);

	if ((int)sel_n < 0) {
		sel_n = 0;
		total_sel_size = 0;
	}

	if (sel_n)
		sel_elements = (char **)xrealloc(sel_elements, sel_n * sizeof(char *));

	/* Deallocate local arrays */
	i = (int)desel_n;
	while (--i >= 0) {
		free(desel_path[i]);
		free(desel_elements[i]);
	}
	free(desel_path);
	free(desel_elements);

	if (args_n > 0) {
		for (i = 1; i <= (int)args_n; i++)
			free(comm[i]);
		comm = (char **)xrealloc(comm, 1 * sizeof(char *));
		args_n = 0;
	}

	int exit_status = EXIT_SUCCESS;

	if (save_sel() != 0)
		exit_status = EXIT_FAILURE;

	/* If there is still some selected file, reload the desel screen */
	if (sel_n)
		if (deselect(comm) != 0)
			exit_status = EXIT_FAILURE;

	return exit_status;
}
