/* selection.c -- files selection functions */

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
#include <limits.h>

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
#include "file_operations.h"
#include "init.h"

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
		}
		return EXIT_SUCCESS;
	}

	FILE *fp = fopen(sel_file, "w");
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

	fclose(fp);
	return EXIT_SUCCESS;
}

static int
select_file(char *file)
{
	if (!file || !*file)
		return 0;

	int exists = 0, new_sel = 0, j;
	size_t flen = strlen(file);
	if (file[flen - 1] == '/')
		file[flen - 1] = '\0';

	int __free = 0;
	char *p = home_tilde(file, &__free);

	/* Check if the selected element is already in the selection box */
	j = (int)sel_n;
	while (--j >= 0) {
		if (*p == *sel_elements[j] && strcmp(sel_elements[j], p) == 0) {
			exists = 1;
			break;
		}
	}

	if (!exists) {
		sel_elements = (char **)xrealloc(sel_elements, (sel_n + 2)
											* sizeof(char *));
		sel_elements[sel_n] = savestring(p, strlen(p));
		sel_n++;
		sel_elements[sel_n] = (char *)NULL;
		new_sel++;
	} else {
		fprintf(stderr, _("%s: sel: %s: Already selected\n"),
		    PROGRAM_NAME, file);
	}

	if (__free == 1)
		free(p);
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

				if (!found) {
					matches[k] = file_info[i].name;
					k++;
				}
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
				type = get_dt(attr.st_mode);
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

				if (j == -1) {
					matches[k] = ent[i]->d_name;
					k++;
				}
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
			default: break;
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

			matches[k] = gbuf.gl_pathv[i];
			k++;
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
				if (*workspaces[cur_ws].path == '/'
				&& !*(workspaces[cur_ws].path + 1)) {
					tmp = (char *)xnmalloc(strlen(matches[i]) + 2,
								sizeof(char));
					sprintf(tmp, "/%s", matches[i]);
				} else {
					tmp = (char *)xnmalloc(strlen(workspaces[cur_ws].path)
								+ strlen(matches[i]) + 2, sizeof(char));
					sprintf(tmp, "%s/%s", workspaces[cur_ws].path, matches[i]);
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
			if (*workspaces[cur_ws].path == '/'
			&& !*(workspaces[cur_ws].path + 1)) {
				snprintf(tmp_path, PATH_MAX - 1, "/%s", file_info[i].name);
			} else {
				snprintf(tmp_path, PATH_MAX - 1, "%s/%s", workspaces[cur_ws].path,
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
			return (-1);
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

/* Convert file type into a macro that can be decoded by stat().
 * If file type is specified, matches will be checked against
 * this value */
static inline mode_t
convert_filetype(mode_t *filetype)
{
	switch (*filetype) {
	case 'd': *filetype = DT_DIR; break;
	case 'r': *filetype = DT_REG; break;
	case 'l': *filetype = DT_LNK; break;
	case 's': *filetype = DT_SOCK; break;
	case 'f': *filetype = DT_FIFO; break;
	case 'b': *filetype = DT_BLK; break;
	case 'c': *filetype = DT_CHR; break;
	default:
		fprintf(stderr, _("%s: '%c': Unrecognized file type\n"),
		    PROGRAM_NAME, (char)*filetype);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static inline char *
parse_sel_params(char ***args, int *ifiletype, mode_t *filetype, int *isel_path)
{
	char *sel_path = (char *)NULL;
	int i;
	for (i = 1; (*args)[i]; i++) {
		if (*(*args)[i] == '-') {
			*ifiletype = i;
			*filetype = (mode_t)*((*args)[i] + 1);
		}

		if (*(*args)[i] == ':') {
			*isel_path = i;
			sel_path = (*args)[i] + 1;
		}

		if (*(*args)[i] == '~') {
			char *exp_path = tilde_expand((*args)[i]);
			if (!exp_path) {
				fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, (*args)[i],
				    strerror(errno));
				return (char *)NULL;
			}

			free((*args)[i]);
			(*args)[i] = exp_path;
		}
	}

	if (*filetype != 0 && convert_filetype(filetype) == EXIT_FAILURE)
		return (char *)NULL;

	return sel_path;
}

static inline char *
construct_sel_path(char *sel_path)
{
	char tmpdir[PATH_MAX];
	xstrsncpy(tmpdir, sel_path, (size_t)PATH_MAX);

	if (*sel_path == '.' && realpath(sel_path, tmpdir) == NULL) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, sel_path, strerror(errno));
		return (char *)NULL;
	}

	if (*sel_path == '~') {
		char *exp_path = tilde_expand(sel_path);
		if (!exp_path) {
			fprintf(stderr, _("%s: Error expanding path\n"), PROGRAM_NAME);
			return (char *)NULL;
		}
		strcpy(tmpdir, exp_path);
		free(exp_path);
	}

	char *dir = (char *)NULL;
	if (*tmpdir != '/') {
		dir = (char *)xnmalloc(strlen(workspaces[cur_ws].path)
				+ strlen(tmpdir) + 2, sizeof(char));
		sprintf(dir, "%s/%s", workspaces[cur_ws].path, tmpdir);
	} else {
		dir = savestring(tmpdir, strlen(tmpdir));
	}

	return dir;
}

static inline char *
check_sel_path(char **sel_path)
{
	size_t len = strlen(*sel_path);
	if ((*sel_path)[len - 1] == '/')
		(*sel_path)[len - 1] = '\0';

	if (strchr(*sel_path, '\\')) {
		char *deq = dequote_str(*sel_path, 0);
		if (deq) {
			strcpy(*sel_path, deq);
			free(deq);
		}
	}

	char *dir = construct_sel_path(*sel_path);
	if (!dir)
		return (char *)NULL;

	if (access(dir, X_OK) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, dir, strerror(errno));
		free(dir);
		return (char *)NULL;
	}

	if (xchdir(dir, NO_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, dir, strerror(errno));
		free(dir);
		return (char *)NULL;
	}

	return dir;
}

static inline void
print_total_size(void)
{
	char *human_size = get_size_unit(total_sel_size);
	printf(_("%s%sTotal size%s: %s\n"), df_c, BOLD, df_c, human_size);
	free(human_size);
}

static inline void
print_selected_files(int print_header, int print_total)
{
	if (print_header == 1) {
		printf(_("%sSelection Box%s\n"), BOLD, df_c);
		putchar('\0');
	}

	size_t t = tab_offset, i;
	tab_offset = 0;
	for (i = 0; i < sel_n; i++)
		colors_list(sel_elements[i], (int)i + 1, NO_PAD, PRINT_NEWLINE);
	tab_offset = t;

	if (print_total == 1) {
		putchar('\n');
		print_total_size();
	}
}

static inline int
print_sel_results(int new_sel, char *sel_path, char *pattern)
{
	if (new_sel > 0 && xargs.stealth_mode != 1 && sel_file
	&& save_sel() != EXIT_SUCCESS) {
		_err('e', PRINT_PROMPT, _("%s: Error writing selected files "
			"to the selections file\n"), PROGRAM_NAME);
	}

	if (sel_path && xchdir(workspaces[cur_ws].path, NO_TITLE) == -1) {
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, workspaces[cur_ws].path,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	if (new_sel <= 0) {
		if (pattern) fprintf(stderr, _("%s: No matches found\n"), PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Get total size of sel files */
	struct stat attr;
	int i = (int)sel_n;
	while (--i >= 0) {
		if (lstat(sel_elements[i], &attr) != -1)
			total_sel_size += FILE_SIZE;
	}

	/* Print entries */
	if (sel_n > 10) {
		printf(_("%zu files are now in the Selection Box\n"), sel_n);
	} else if (sel_n > 0) {
		printf(_("%zu selected file(s):\n\n"), sel_n);
		print_selected_files(0, 0);
		putchar('\n');
	}

	print_total_size();
	return EXIT_SUCCESS;
}

static inline char *
construct_sel_filename(const char *sel_path, const char *dir, const char *name)
{
	char *f = (char *)NULL;

	if (!sel_path) {
		if (*workspaces[cur_ws].path == '/'
		&& !*(workspaces[cur_ws].path + 1)) {
			f = (char *)xnmalloc(strlen(name) + 2, sizeof(char));
			sprintf(f, "/%s", name);
			return f;
		}

		f = (char *)xnmalloc(strlen(workspaces[cur_ws].path)
					+ strlen(name) + 2, sizeof(char));
		sprintf(f, "%s/%s", workspaces[cur_ws].path, name);
		return f;
	}

	f = (char *)xnmalloc(strlen(dir) + strlen(name) + 2, sizeof(char));
	sprintf(f, "%s/%s", dir, name);
	return f;
}

static inline int
select_filename(char *arg, const char *sel_path, char *dir)
{
	int new_sel = 0;

	if (strchr(arg, '\\')) {
		char *deq_str = dequote_str(arg, 0);
		if (deq_str) {
			strcpy(arg, deq_str);
			free(deq_str);
		}
	}

	char *name = arg;
	if (*name == '.' && *(name + 1) == '/')
		name += 2;

	if (*arg != '/') {
		char *tmp = construct_sel_filename(sel_path, dir, name);
		struct stat attr;
		if (lstat(tmp, &attr) == -1)
			fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, arg, strerror(errno));
		else
			new_sel += select_file(tmp);
		free(tmp);
		return new_sel;
	}

	struct stat a;
	if (lstat(arg, &a) == -1)
		fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, name, strerror(errno));
	else
		new_sel += select_file(name);

	return new_sel;
}

static inline int
select_pattern(char *arg, char *sel_path, char *dir, mode_t filetype)
{
	/* GLOB */
	int ret = sel_glob(arg, sel_path ? dir : NULL, filetype ? filetype : 0);

	/* If glob failed, try REGEX */
	if (ret <= 0)
		return sel_regex(arg, sel_path ? dir : NULL, filetype);

	return ret;
}

int
sel_function(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (!args[1] || IS_HELP(args[1])) {
		puts(_(SEL_USAGE));
		return EXIT_SUCCESS;
	}

	mode_t filetype = 0;
	int i, ifiletype = 0, isel_path = 0, new_sel = 0;

	char *dir = (char *)NULL, *pattern = (char *)NULL;
	char *sel_path = parse_sel_params(&args, &ifiletype, &filetype, &isel_path);

	if (sel_path)
		dir = check_sel_path(&sel_path);

	for (i = 1; args[i]; i++) {
		if (i == ifiletype || i == isel_path)
			continue;

		if (check_regex(args[i]) == EXIT_SUCCESS) {
			pattern = args[i];
			if (*pattern == '!')
				pattern++;
		}

		if (!pattern)
			new_sel += select_filename(args[i], sel_path, dir);
		else
			new_sel += select_pattern(args[i], sel_path, dir, filetype);
	}

	free(dir);
	return print_sel_results(new_sel, sel_path, pattern);
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

		size_t t = tab_offset;
		tab_offset = 0;

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

		tab_offset = t;

		char *human_size = get_size_unit(total_sel_size);
		printf(_("\n%s%sTotal size%s: %s\n"), df_c, BOLD, df_c, human_size);
		free(human_size);
	}

	if (reset_pager)
		pager = 1;
}

static int
edit_selfile(void)
{
	if (!sel_file || !*sel_file || sel_n == 0)
		return EXIT_FAILURE;

	struct stat attr;
	if (stat(sel_file, &attr) == -1)
		goto ERROR;

	time_t mtime_bfr = (time_t)attr.st_mtime;

	if (open_file(sel_file) != EXIT_SUCCESS) {
		fprintf(stderr, "%s: Could not open the selections file\n", PROGRAM_NAME);
		return EXIT_FAILURE;
	}

	/* Compare the new modification time to the stored one: if they
	 * match, nothing was modified */
	if (stat(sel_file, &attr) == -1)
		goto ERROR;

	if (mtime_bfr == (time_t)attr.st_mtime)
		return EXIT_SUCCESS;

	return get_sel_files();

ERROR:
	fprintf(stderr, "sel: %s: %s\n", sel_file, strerror(errno));
	return EXIT_FAILURE;
}

static int
desel_entries(char **desel_elements, size_t desel_n, int all)
{
	/* If a valid ELN and not asterisk... */
	/* Store the full path of all the elements to be deselected in a new
	 * array (desel_path). We need to do this because after the first
	 * rearragement of the sel array, that is, after the removal of the
	 * first element, the index of the next elements changed, and cannot
	 * thereby be found by their index. The only way to find them is
	 * comparing string by string */
	char **desel_path = (char **)NULL;

	int i = (int)desel_n;

	if (all == 0) {
		desel_path = (char **)xnmalloc(desel_n, sizeof(char *));
		while (--i >= 0) {
			int desel_int = atoi(desel_elements[i]);
			if (desel_int == INT_MIN) {
				desel_path[i] = (char *)NULL;
				continue;
			}
			desel_path[i] = savestring(sel_elements[desel_int - 1],
				strlen(sel_elements[desel_int - 1]));
		}
	} else { /* Deselect all selected files */
		desel_path = desel_elements;
	}

	/* Search the sel array for the path of the element to deselect and
	 * store its index */
	struct stat attr;

	int desel_index = -1, err = 0, err_printed = 0, dn = (int)desel_n;
	i = (int)desel_n;
	while (--i >= 0) {
		int j, k;

		if (!desel_path[i])
			continue;

		k = (int)sel_n;
		while (--k >= 0) {
			if (strcmp(sel_elements[k], desel_path[i]) == 0) {
				/* Sustract size from total size */
				if (lstat(sel_elements[k], &attr) != -1) {
					if (S_ISDIR(attr.st_mode))
						total_sel_size -= dir_size(sel_elements[k]);
					else
						total_sel_size -= FILE_SIZE;
				}

				desel_index = k;
				break;
			}
		}

		if (desel_index == -1) {
			dn--;
			if (all) {
				err_printed = 1;
				fprintf(stderr, _("%s: %s: No such selected file\n"),
					PROGRAM_NAME, desel_path[i]);
			}
			continue;
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

	if (desel_index == -1) {
		err = 1;
		goto FREE;
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

FREE:
	/* Deallocate local arrays */
	i = (int)desel_n;
	while (--i >= 0) {
		if (all == 0)
			free(desel_path[i]);
		free(desel_elements[i]);
	}
	if (all == 0)
		free(desel_path);
	else if (err_printed)
		printf(_("%d file(s) deselected. "), dn);
	else
		printf(_("%s: %d file(s) deselected. "), PROGRAM_NAME, dn);
	printf(_("%zu file(s) currently selected\n"), sel_n);
	free(desel_elements);

	if (err)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

/* Deselect all selected files */
static inline int
deselect_all(void)
{
	int i = (int)sel_n;
	while (--i >= 0)
		free(sel_elements[i]);

	sel_n = 0;
	total_sel_size = 0;

	return save_sel();
}

/* Deselect files passed as parameters to the desel command
 * Returns zero on success and 1 on error */
static inline int
deselect_from_args(char **args)
{
	char **ds = (char **)xnmalloc(args_n + 1, sizeof(char *));
	size_t j, k = 0;

	for (j = 1; j <= args_n; j++) {
		char *pp = normalize_path(args[j], strlen(args[j]));
		if (!pp)
			continue;
		ds[k] = savestring(pp, strlen(pp));
		k++;
		free(pp);
	}
	ds[k] = (char *)NULL;

	if (desel_entries(ds, args_n, 1) == EXIT_FAILURE)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static inline char **
get_desel_input(size_t *n)
{
	printf(_("\n%sEnter 'q' to quit or 'e' to edit the selections file\n"), df_c);

	char *line = NULL;
	while (!line)
		line = rl_no_hist(_("File(s) to be deselected (ex: 1 2-6, or *): "));

	char **entries = get_substr(line, ' ');
	free(line);

	if (!entries)
		return (char **)NULL;

	size_t i;
	for (i = 0; entries[i]; i++)
		(*n)++;

	return entries;
}

static inline void
free_desel_elements(size_t desel_n, char ***desel_elements)
{
	int i = (int)desel_n;
	while (--i >= 0)
		free((*desel_elements)[i]);
	free(*desel_elements);
}

static inline int
handle_alpha_entry(int i, size_t desel_n, char **desel_elements)
{
	if (*desel_elements[i] == 'e' && !desel_elements[i][1]) {
		free_desel_elements(desel_n, &desel_elements);
		return edit_selfile();
	}

	if (*desel_elements[i] == 'q' && !desel_elements[i][1]) {
		free_desel_elements(desel_n, &desel_elements);
		return EXIT_SUCCESS;
	}

	if (*desel_elements[i] == '*' && !desel_elements[i][1]) {
		free_desel_elements(desel_n, &desel_elements);
		int exit_status = deselect_all();
		if (autols) {
			free_dirlist();
			if (list_dir() != EXIT_SUCCESS)
				exit_status = EXIT_FAILURE;
		}
		return exit_status;
	}

	printf(_("desel: '%s': Invalid entry\n"), desel_elements[i]);
	free_desel_elements(desel_n, &desel_elements);
	return EXIT_FAILURE;
}

static inline int
valid_desel_eln(int i, size_t desel_n, char **desel_elements)
{
	int n = atoi(desel_elements[i]);

	if (n <= 0 || (size_t)n > sel_n) {
		printf(_("desel: '%s': Invalid ELN\n"), desel_elements[i]);
		free_desel_elements(desel_n, &desel_elements);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static inline int
end_deselect(int err, char ***args)
{
	int exit_status = EXIT_SUCCESS;

	size_t argsbk = args_n;
	if (args_n > 0) {
		size_t i;
		for (i = 1; i <= args_n; i++) {
			free((*args)[i]);
			(*args)[i] = (char *)NULL;
		}
		args_n = 0;
	}

	if (!err && save_sel() != 0)
		exit_status = EXIT_FAILURE;

	/* If there is still some selected file, reload the desel screen */
	if (sel_n && argsbk == 0)
		if (deselect(*args) != 0)
			exit_status = EXIT_FAILURE;

	return exit_status;
}

int
deselect(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (sel_n == 0) {
		puts(_("desel: There are no selected files"));
		return EXIT_SUCCESS;
	}

	int err = 0;

	if (args[1] && *args[1]) {
		if (strcmp(args[1], "*") == 0 || strcmp(args[1], "a") == 0
		|| strcmp(args[1], "all") == 0) {
			return deselect_all();
		} else {
			err = deselect_from_args(args);
			return end_deselect(err, &args);
		}
	}

	print_selected_files(1, 1);

	size_t desel_n = 0;
	char **desel_elements = get_desel_input(&desel_n);

	int i = (int)desel_n;
	while (--i >= 0) {
		if (!is_number(desel_elements[i]))
			return handle_alpha_entry(i, desel_n, desel_elements);

		if (valid_desel_eln(i, desel_n, desel_elements) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}

	desel_entries(desel_elements, desel_n, 0);
	return end_deselect(err, &args);
}
