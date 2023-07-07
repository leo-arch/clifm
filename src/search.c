/* search.c -- functions for the search system */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <glob.h>

/* We need rl_line_buffer in case of no matches and no metacharacter */
#include <readline/readline.h>

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "exec.h"
#include "messages.h"
#include "misc.h"
#include "navigation.h"
#include "sort.h"

#define ERR_SKIP_REGEX 2

#if defined(__OpenBSD__) || defined(__sun)
/* OpenBSD/Solaris find(1) has neither -regex nor -iregex. We'll try to use
 * gfind(1) instead.*/
# define FIND_HAS_NO_REGEX
#endif /* __OpenBSD__ || __sun */

struct search_t {
	char *name;
	size_t len;
	int eln;
	int pad;
};

static int
exec_find(char *name, char *_path, char *method, char *pattern)
{
	if (follow_symlinks == 1) {
		char *cmd[] = {name, "-L", _path, method, pattern, NULL};
		return launch_execv(cmd, FOREGROUND, E_NOSTDERR);
	}

	char *cmd[] = {name, _path, method, pattern, NULL};
	return launch_execv(cmd, FOREGROUND, E_NOSTDERR);
}

#ifdef FIND_HAS_NO_REGEX
static char *
define_find_name(void)
{
	static int check = 1;
	static int have_gfind = 0;

	/* Let's run this only once. */
	if (check == 1) {
		check = 0;
		char *p = get_cmd_path("gfind");
		if (p)
			have_gfind = 1;
		free(p);
	}

	return (have_gfind == 1 ? "gfind" : "find");
}
#endif /* FIND_HAS_NO_REGEX */

static int
run_find(char *search_path, char *arg)
{
	char *_path = (search_path && *search_path) ? search_path : ".";
	char *name = "find";

#if defined(_BE_POSIX)
	/* POSIX find(1) only supports -name */
	char *method = "-name";
#else
	char *method = conf.search_strategy == REGEX_ONLY
		? (conf.case_sens_search == 1 ? "-regex" : "-iregex")
		: (conf.case_sens_search == 1 ? "-name" : "-iname");
# if defined(FIND_HAS_NO_REGEX)
	name = define_find_name();
	if (*name != 'g') /* GNU find (gfind) not found */
		method = conf.case_sens_search == 1 ? "-name" : "-iname";
# endif /* FIND_HAS_NO_REGEX */
#endif /* _BE_POSIX */

	int glob_char = check_glob_char(arg + 1, GLOB_REGEX);
	if (glob_char == 1)
		return exec_find(name, _path, method, arg + 1);

	int ret = EXIT_SUCCESS;

	size_t pattern_len = strlen(arg + 1) + 5;
	char *pattern = (char *)xnmalloc(pattern_len, sizeof(char));

#if !defined(_BE_POSIX)
	if (conf.search_strategy == REGEX_ONLY) {
# if !defined(FIND_HAS_NO_REGEX)
		snprintf(pattern, pattern_len, ".*%s.*", arg + 1);
# else
		if (*name == 'g') /* We have GNU find (gfind) */
			snprintf(pattern, pattern_len, ".*%s.*", arg + 1);
		else
			snprintf(pattern, pattern_len, "*%s*", arg + 1);
# endif /* FIND_HAS_NO_REGEX */
	} else {
		snprintf(pattern, pattern_len, "*%s*", arg + 1);
	}
#else
	snprintf(pattern, pattern_len, "*%s*", arg + 1);
#endif /* !_BE_POSIX */

	ret = exec_find(name, _path, method, pattern);
	free(pattern);

	return ret;
}

static int
set_file_type_and_search_path(char **args, mode_t *file_type,
	char **search_path, const int invert)
{
	/* If there are two arguments, the one starting with '-' is the
	 * file type and the other is the path. */
	if (args[1] && args[2]) {
		if (*args[1] == '-') {
			*file_type = (mode_t)args[1][1];
			*search_path = args[2];
		} else if (*args[2] == '-') {
			*file_type = (mode_t)args[2][1];
			*search_path = args[1];
		} else {
			*search_path = args[1];
		}
	} else {
		/* If just one argument, '-' indicates file type. Else, we have a path. */
		if (args[1]) {
			if (*args[1] == '-')
				*file_type = (mode_t)args[1][1];
			else
				*search_path = args[1];
		}
	}

	if (*file_type == 0)
		return EXIT_SUCCESS;

	/* Convert file type into a macro that can be decoded by stat(). If
	 * file type is specified, matches will be checked against this value. */
	switch (*file_type) {
	case 'b': *file_type = invert == 1 ? DT_BLK : S_IFBLK; break;
	case 'c': *file_type = invert == 1 ? DT_CHR : S_IFCHR; break;
	case 'd': *file_type = invert == 1 ? DT_DIR : S_IFDIR; break;
#ifdef __sun
	case 'D': *file_type = invert == 1 ? DT_DOOR : S_IFDOOR; break;
#endif /* __sun */
	case 'f': *file_type = invert == 1 ? DT_REG : S_IFREG; break;
	case 'l': *file_type = invert == 1 ? DT_LNK : S_IFLNK; break;
	case 'p': *file_type = invert == 1 ? DT_FIFO : S_IFIFO; break;
	case 's': *file_type = invert == 1 ? DT_SOCK : S_IFSOCK; break;
	case 'x': run_find(*search_path, args[0]); return EXIT_SUCCESS;
	default:
		fprintf(stderr, _("search: '%c': Unrecognized file "
			"type\n"), (char)*file_type);
		break;
	}

	return EXIT_SUCCESS;
}

static int
chdir_search_path(char **search_path, const char *arg)
{
	if (strchr(*search_path, '\\')) {
		char *deq_dir = dequote_str(*search_path, 0);
		if (!deq_dir) {
			xerror(_("search: %s: Error dequoting file name\n"), arg);
			return EXIT_FAILURE;
		}

		xstrsncpy(*search_path, deq_dir, strlen(deq_dir) + 1);
		free(deq_dir);
	}

	size_t path_len = strlen(*search_path);
	if (path_len > 1 && (*search_path)[path_len - 1] == '/')
		(*search_path)[path_len - 1] = '\0';

	/* If search path is the current directory. */
	if ((*(*search_path) == '.' && !(*search_path)[1]) ||
	    ((*search_path)[1] == workspaces[cur_ws].path[1]
	    && strcmp(*search_path, workspaces[cur_ws].path) == 0)) {
			*search_path = (char *)NULL;
	} else {
		if (xchdir(*search_path, NO_TITLE) == -1) {
			xerror("search: %s: %s\n", *search_path, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

static char **
glob_sort_dirs(glob_t globbed_files, size_t *g)
{
	int *dirs = (int *)xnmalloc(globbed_files.gl_pathc + 1, sizeof(int));
	char **gfiles = (char **)xnmalloc(globbed_files.gl_pathc + 1, sizeof(char *));
	struct stat attr;
	size_t i, n = 0;

	for (i = 0; globbed_files.gl_pathv[i]; i++) {
		if (stat(globbed_files.gl_pathv[i], &attr) != -1
		&& S_ISDIR(attr.st_mode))
			dirs[i] = 1;
		else
			dirs[i] = 0;
	}

	for (i = 0; globbed_files.gl_pathv[i]; i++) {
		if (dirs[i] == 1) {
			gfiles[n] = globbed_files.gl_pathv[i];
			n++;
		}
	}

	for (i = 0; globbed_files.gl_pathv[i]; i++) {
		if (dirs[i] == 0) {
			gfiles[n] = globbed_files.gl_pathv[i];
			n++;
		}
	}

	free(dirs);
	gfiles[n] = (char *)NULL;

	*g = n;
	return gfiles;
}

static struct search_t *
get_glob_matches(char **gfiles, const char *search_path,
	const mode_t file_type, const size_t g)
{
	struct search_t *matches = (struct search_t *)xnmalloc(
		g + 1, sizeof(struct search_t));

	size_t i;
	int n = 0;
	struct stat attr;

	for (i = 0; gfiles[i]; i++) {
		if (SELFORPARENT(gfiles[i]))
			continue;

		if (file_type != 0) {
			/* Simply skip all files not matching file_type. */
			if (lstat(gfiles[i], &attr) == -1
			|| (attr.st_mode & S_IFMT) != file_type)
				continue;
		}

		matches[n].name = savestring(gfiles[i], strlen(gfiles[i]));

		/* Get the longest file name in the list. */
		/* If not in CWD, we only need to know the file's length (no ELN) */
		if (search_path) {
			/* This will be passed to colors_list(): -1 means no ELN */
			matches[n].eln = -1;
			matches[n].len = wc_xstrlen(matches[n].name);
			n++;
			continue;
		}

		/* No search_path */
		/* If searching in CWD, take into account the file's ELN
		 * when calculating its length. */
		size_t j, f = 0;
		for (j = 0; file_info[j].name; j++) {
			if (!matches[n].name || *matches[n].name != *file_info[j].name
			|| strcmp(matches[n].name, file_info[j].name) != 0)
				continue;

			f = 1;
			matches[n].eln = (int)(j + 1);
			matches[n].len = wc_xstrlen(file_info[j].name)
				+ (size_t)file_info[j].eln_n + 1;
		}

		if (f == 0) {
			matches[n].eln = -1;
			matches[n].len = 0;
		}

		n++;
	}

	matches[n].name = (char *)NULL;
	return matches;
}

static struct search_t *
get_non_matches_from_search_path(const char *search_path, char **gfiles,
		const mode_t file_type)
{
	struct dirent **ent = (struct dirent **)NULL;
	int dir_entries = scandir(search_path, &ent, skip_files, xalphasort);
	if (dir_entries == -1)
		return (struct search_t *)NULL;

	int i, j, n = 0;
	struct search_t *matches = (struct search_t *)xnmalloc(
		(size_t)dir_entries + 1, sizeof(struct search_t));

	for (i = 0; i < dir_entries; i++) {
		int f = 0;
		for (j = 0; gfiles[j]; j++) {
			if (*ent[i]->d_name == *gfiles[j]
			&& strcmp(ent[i]->d_name, gfiles[j]) == 0) {
				f = 1;
				break;
			}
		}

		if (f == 1)
			continue;

#if !defined(_DIRENT_HAVE_D_TYPE)
		mode_t type;
		struct stat attr;
		if (lstat(ent[i]->d_name, &attr) == -1)
			continue;

		type = get_dt(attr.st_mode);
		if (file_type && type != file_type)
#else
		if (file_type && ent[i]->d_type != file_type)
#endif /* !_DIRENT_HAVE_D_TYPE */
			continue;

		matches[n].eln = -1;
		matches[n].len = wc_xstrlen(ent[i]->d_name);
		matches[n].name = strdup(ent[i]->d_name);
		n++;
	}

	i = dir_entries;
	while (--i >= 0)
		free(ent[i]);
	free(ent);

	matches[n].name = (char *)NULL;
	return matches;
}

static struct search_t *
get_glob_matches_invert(char **gfiles, const char *search_path,
	const mode_t file_type)
{
	if (search_path)
		return get_non_matches_from_search_path(search_path, gfiles, file_type);

	int i, j, n = 0;
	struct search_t *matches =
		(struct search_t *)xnmalloc(files + 1, sizeof(struct search_t));

	for (i = 0; file_info[i].name; i++) {
		int f = 0;

		for (j = 0; gfiles[j]; j++) {
			if (*gfiles[j] == *file_info[i].name
			&& strcmp(gfiles[j], file_info[i].name) == 0) {
				f = 1;
				break;
			}
		}

		if (f == 1 || (file_type && file_info[i].type != file_type))
			continue;

		matches[n].eln = (int)(i + 1);
		matches[n].len = wc_xstrlen(file_info[i].name)
			+ (size_t)file_info[i].eln_n + 1;

		matches[n].name = strdup(file_info[i].name);
		n++;
	}

	matches[n].name = (char *)NULL;
	return matches;
}

static void
get_glob_longest(struct search_t *matches, int *longest_eln,
	size_t *longest_match, int *eln_pad)
{
	int search_path = (*eln_pad == -1);

	size_t i;
	for (i = 0; matches[i].name; i++) {
		if (matches[i].len <= *longest_match)
			continue;

		*longest_match = matches[i].len;
		*longest_eln = matches[i].eln;
	}

	if (search_path == 1) {
		*longest_eln = -1;
		return;
	}

	if (conf.icons == 1)
		*longest_match += 3;

	int largest = 0;
	for (i = 0; matches[i].name; i++) {
		if (matches[i].eln > largest)
			largest = matches[i].eln;
	}

	*eln_pad = DIGINUM(largest);
	*longest_match += (size_t)(*eln_pad - DIGINUM(*longest_eln));
}

/* Original string is either "/QUERY" or "/!QUERY". Let's extract QUERY.
 * If the query string contains no metacharacters, change it to "*QUERY*" */
static char *
construct_glob_query(char **arg, const int invert)
{
	search_flags &= ~NO_GLOB_CHAR;
	char *query = *arg + (invert == 1 ? 2 : 1);

	/* If the query string already contains metacharacters, return it as is. */
	if (check_glob_char(query, GLOB_REGEX) == 1)
		return query;

	search_flags |= NO_GLOB_CHAR;
	if (conf.search_strategy != GLOB_ONLY) {
		/* Let's return here to perform a regex search. */
		return (char *)NULL;
	}

	/* Search strategy is glob-only */
	size_t len = strlen(*arg);

	char *q = savestring(query, len - (invert == 1 ? 2 : 1));
	*arg = (char *)xrealloc(*arg, (len + 3) * sizeof(char));

	snprintf((*arg) + 1, len + 2, "*%s*", q);
	free(q);

	return (*arg) + 1;
}

static int
calculate_glob_output_columns(const size_t flongest, const int found)
{
	int columns_n = 0;

	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	unsigned short tcols = w.ws_col;

	if (flongest == 0 || flongest > tcols)
		columns_n = 1;
	else
		columns_n = (int)(tcols / (flongest + 1));

	if (columns_n > found)
		columns_n = found;

	return columns_n;
}

static int
print_glob_matches(struct search_t *matches, const char *search_path)
{
	int found = 0;
	for (found = 0; matches && matches[found].name; found++);
	if (found == 0)
		return 0;

	int eln_pad = search_path ? -1 : 0;
	int longest_eln = -1;
	size_t flongest = 0;

	get_glob_longest(matches, &longest_eln, &flongest, &eln_pad);

	int columns_n = calculate_glob_output_columns(flongest, found);

	/* colors_list() makes use of TAB_OFFSET. We don't want it here. */
	size_t tab_offset_bk = tab_offset;
	tab_offset = 0;

	int last_column = 0, i;

	for (i = 0; matches[i].name; i++) {
		if ((i + 1) % columns_n == 0)
			last_column = 1;
		else
			last_column = 0;

		if (!search_path) {
			/* Print ELN, file indicator, and icon. */
			int index = matches[i].eln - 1;
			char ind_chr = file_info[index].sel == 1 ? SELFILE_CHR : ' ';
			char *ind_chr_color = file_info[index].sel == 1 ? li_cb : "";

			printf("%s%*d%s%s%c%s%s%s%s%c", el_c, eln_pad, matches[i].eln, df_c,
				ind_chr_color, ind_chr, df_c,
				conf.icons == 1 ? file_info[index].icon_color : "",
				conf.icons == 1 ? file_info[index].icon : "",
				df_c, conf.icons == 1 ? ' ' : 0);
		}

		/* Print file name. */
		int name_pad = (last_column == 1 || i == (found - 1)) ? NO_PAD :
		    (int)(flongest - matches[i].len - ( search_path ? 0
		    : (size_t)(eln_pad - DIGINUM(matches[i].eln)) ) + 1);

		if (name_pad < 0)
			name_pad = 0;

		colors_list(matches[i].name, NO_ELN, name_pad,
		    (last_column == 1 || i == found - 1) ? 1 : NO_NEWLINE);
	}

	tab_offset = tab_offset_bk;

	print_reload_msg(_("Matches found: %d%s\n"), found,
		conf.search_strategy != GLOB_ONLY ? " (glob)" : "");

	return found;
}

/* List matching file names in the specified directory. */
static int
search_glob(char **args)
{
	if (!args || !args[0])
		return EXIT_FAILURE;

	int invert = (args[0][1] == '!');

	char *search_query = (char *)NULL, *search_path = (char *)NULL;
	mode_t file_type = 0;

	if (set_file_type_and_search_path(args, &file_type,
	&search_path, invert) == EXIT_FAILURE) {
		return ERR_SKIP_REGEX;
	}

	if (file_type == 'x') /* Recursive search via find(1) */
		return EXIT_SUCCESS;

	/* If we have a path ("/str /path"), chdir into it, since glob(3)
	 * works on CWD. */
	if (search_path && *search_path
	&& chdir_search_path(&search_path, args[1]) == EXIT_FAILURE)
		return ERR_SKIP_REGEX;

	search_query = construct_glob_query(&args[0], invert);
	if (!search_query && conf.search_strategy != GLOB_ONLY)
		return EXIT_FAILURE;

	/* Get matches, if any. */
	glob_t globbed_files;
	int ret = glob(search_query, GLOB_BRACE, NULL, &globbed_files);
	if (ret != 0) {
		globfree(&globbed_files);

		/* Go back to the directory we came from */
		if (search_path && xchdir(workspaces[cur_ws].path, NO_TITLE) == -1)
			xerror("search: %s: %s\n", workspaces[cur_ws].path, strerror(errno));

		return EXIT_FAILURE;
	}

	/* We have matches */

	char **gfiles = globbed_files.gl_pathv;
	size_t g = globbed_files.gl_pathc;

	/* glob(3) doesn't sort directories first. Let's do it ourselves */
	if (conf.list_dirs_first == 1)
		gfiles = glob_sort_dirs(globbed_files, &g);

	/* We need to store pointers to matching file names in array of pointers,
	 * just as the file name length (to construct the columned output), and,
	 * if searching in CWD, its index (ELN) in the dirlist array as well */
	struct search_t *list = (struct search_t *)NULL;

	if (invert == 0)
		list = get_glob_matches(gfiles, search_path, file_type, g);
	else
		list = get_glob_matches_invert(gfiles, search_path, file_type);

	globfree(&globbed_files);

	int matches = print_glob_matches(list, search_path);

	/* Free stuff */
	size_t i;
	for (i = 0; list[i].name; i++)
		free(list[i].name);
	free(list);

	if (conf.list_dirs_first == 1)
		free(gfiles);

	/* If needed, go back to the directory we came from */
	if (search_path && xchdir(workspaces[cur_ws].path, NO_TITLE) == -1) {
		xerror("search: %s: %s\n", workspaces[cur_ws].path, strerror(errno));
		return EXIT_FAILURE;
	}

	return (matches == 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}

/* Original string is either "/QUERY" or "/!QUERY". Let's extract QUERY.
 * If the query string contains no metacharacters, change it to ".*QUERY.*" */
static char *
construct_regex_query(char **arg, const int invert, int *regex_found)
{
	char *query = *arg + (invert == 1 ? 2 : 1);

	*regex_found = check_regex(query);
	if (*regex_found == EXIT_SUCCESS)
		return query;

	size_t len = strlen(*arg);

	char *q = savestring(query, len - (invert == 1 ? 2 : 1));
	*arg = (char *)xrealloc(*arg, (len + 5) * sizeof(char));

	snprintf(*arg + 1, len + 4, ".*%s.*", q);
	free(q);

	return *arg + 1;
}

static void
err_regex_no_match(const int regex_found, const char *arg)
{
	char *input = (conf.autocd == 1 && !arg && (regex_found == EXIT_FAILURE
			|| (search_flags & NO_GLOB_CHAR)) && rl_line_buffer)
			? strrchr(rl_line_buffer, '/') : (char *)NULL;

	if (input && input != rl_line_buffer) {
		/* Input string contains at least two slashes. It looks like a path:
		 * let's err like it was. */
		xerror("cd: %s: %s\n", rl_line_buffer, strerror(ENOENT));
	} else if (search_flags & NO_GLOB_CHAR) {
		fputs(_("search: No matches found\n"), stderr);
	} else {
		fputs(_("No matches found\n"), stderr);
	}

	search_flags &= ~NO_GLOB_CHAR;
}

static void
free_regex_dirlist(struct dirent ***dirlist, const int tmp_files)
{
	if (!dirlist)
		return;

	int i = tmp_files;
	while (--i >= 0)
		free((*dirlist)[i]);

	free(*dirlist);

	if (xchdir(workspaces[cur_ws].path, NO_TITLE) == -1)
		xerror("search: %s: %s\n", workspaces[cur_ws].path, strerror(errno));
}

static int
check_regex_file_type(struct dirent **reg_dirlist, const int index,
	const mode_t file_type)
{
	if (reg_dirlist) { /* A search path has been provided. */
#if !defined(_DIRENT_HAVE_D_TYPE)
		mode_t type;
		struct stat attr;
		if (lstat(reg_dirlist[index]->d_name, &attr) == -1)
			return EXIT_FAILURE;

		switch (attr.st_mode & S_IFMT) {
		case S_IFBLK: type = DT_BLK; break;
		case S_IFCHR: type = DT_CHR; break;
		case S_IFDIR: type = DT_DIR; break;
# ifdef __sun
		case S_IFDOOR: type = DT_DOOR; break;
# endif /* __sun */
		case S_IFIFO: type = DT_FIFO; break;
		case S_IFLNK: type = DT_LNK; break;
		case S_IFREG: type = DT_REG; break;
		case S_IFSOCK: type = DT_SOCK; break;
		default: type = DT_UNKNOWN; break;
		}

		if (type != file_type)
#else
		if (reg_dirlist[index]->d_type != file_type)
#endif /* !_DIRENT_HAVE_D_TYPE */
			return EXIT_FAILURE;
	} else { /* Searching in CWD. */
		if (file_info[index].type != file_type)
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static struct search_t
load_entry_info(struct dirent **reg_dirlist, const int index)
{
	char *name = reg_dirlist ? reg_dirlist[index]->d_name
		: file_info[index].name;

	struct search_t list;
	list.name = name;
	list.eln = reg_dirlist ? -1 : index + 1;
	list.len = wc_xstrlen(name);
	list.len += (!reg_dirlist && conf.icons == 1) ? 3 : 0;
	list.len += reg_dirlist ? 0 : (size_t)(DIGINUM(list.eln) + 1);

	return list;
}

static size_t
get_regex_largest(struct search_t *list, const int total, int *elnpad)
{
	int i = total;
	size_t largest_file = 0;
	int largest_file_eln = -1;
	int largest_eln = -1;

	while (--i >= 0) {
		if (list[i].len > largest_file) {
			largest_file = list[i].len;
			largest_file_eln = list[i].eln;
		}
		if (list[i].eln != -1 && list[i].eln > largest_eln)
			largest_eln = list[i].eln;
	}

	*elnpad = DIGINUM(largest_eln);
	largest_file += (size_t)(*elnpad - DIGINUM(largest_file_eln));

	return largest_file;
}

static size_t
calc_columns(const size_t largest_file, const size_t matches)
{
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	unsigned short termcols = w.ws_col;

	size_t columns;
	if (largest_file == 0 || largest_file > termcols)
		columns = 1;
	else
		columns = (size_t)termcols / (largest_file + 1);

	if (columns > matches)
		columns = matches;

	return columns;
}

static void
print_regex_entry(struct search_t list, const int namepad, const int elnpad,
	const int newline)
{
	if (list.eln != -1) {
		// Print ELN, file indicator, and icon.
		int index = list.eln - 1;
		char ind_chr = file_info[index].sel == 1 ? SELFILE_CHR : ' ';
		char *ind_chr_color = file_info[index].sel == 1 ? li_cb : "";

		printf("%s%*d%s%s%c%s%s%s%s%c", el_c, elnpad,
			list.eln, df_c, ind_chr_color, ind_chr, df_c,
			conf.icons == 1 ? file_info[index].icon_color : "",
			conf.icons == 1 ? file_info[index].icon : "",
			df_c, conf.icons == 1 ? ' ' : 0);
	}

	colors_list(list.name, NO_ELN, namepad, newline);
}

static size_t
print_regex_matches(const mode_t file_type, struct dirent **reg_dirlist,
	int *regex_index)
{
	/* colors_list() makes use of TAB_OFFSET. We don't need it here. */
	size_t tab_offset_bk = tab_offset;
	tab_offset = 0;

	size_t total; /* Total number of matches (without file type filter) */
	for (total = 0; regex_index[total] > -1; total++);

	struct search_t *list =
		(struct search_t *)xnmalloc(total + 1, sizeof(struct search_t));

	size_t matches = 0; /* Number of filtered matches */

	int i = (int)total;
	while (--i >= 0) {
		int index = regex_index[i];

		if (file_type != 0 && check_regex_file_type(reg_dirlist,
		index, file_type) == EXIT_FAILURE)
			continue;

		list[matches] = load_entry_info(reg_dirlist, index);
		matches++;
	}

	size_t count = 0;
	if (matches == 0) {
		fputs(_("search: No matches found\n"), stderr);
		goto END;
	}

	int eln_pad = -1;
	size_t largest_file = get_regex_largest(list, (int)matches, &eln_pad);
	size_t columns = calc_columns(largest_file, matches);

	size_t last_col = 0, cur_col = 0;

	i = (int)matches;
	while (--i >= 0) {
		cur_col++;
		count++;

		if (cur_col == columns) {
			last_col = 1;
			cur_col = 0;
		} else {
			last_col = 0;
		}

		/* Calculate how much right pad we need for the current entry */
		int name_pad = (last_col == 1 || count == matches) ? NO_PAD :
			(int)(largest_file - list[i].len - (list[i].eln == -1 ? 0
			: (size_t)(eln_pad - DIGINUM(list[i].eln))) + 1);

		if (name_pad < 0)
			name_pad = 0;

		int newline = (last_col == 1 || count == matches);
		print_regex_entry(list[i], name_pad, eln_pad, newline);
	}

	print_reload_msg("Matches found: %zu\n", count);

END:
	free(list);
	tab_offset = tab_offset_bk;
	return count;
}

/* List matching (or non-marching if INVERT is set to 1) file names
 * in the specified directory. */
static int
search_regex(char **args)
{
	if (!args || !args[0])
		return EXIT_FAILURE;

	int invert = (args[0][1] == '!');
	char *search_query = (char *)NULL, *search_path = (char *)NULL;
	mode_t file_type = 0;

	if (set_file_type_and_search_path(args, &file_type,
	&search_path, 1) == EXIT_FAILURE)
		return EXIT_FAILURE;

	if (file_type == 'x') /* Recursive search via find(1) */
		return EXIT_SUCCESS;

	struct dirent **reg_dirlist = (struct dirent **)NULL;
	int tmp_files = - 1;

	if (search_path && *search_path) {
		if (chdir_search_path(&search_path, args[1]) == EXIT_FAILURE)
			return EXIT_FAILURE;

		tmp_files = scandir(".", &reg_dirlist, skip_files, xalphasort);
		if (tmp_files == -1) {
			xerror("search: %s: %s\n", search_path, strerror(errno));

			if (xchdir(workspaces[cur_ws].path, NO_TITLE) == -1)
				xerror("search: %s: %s\n", workspaces[cur_ws].path, strerror(errno));

			return EXIT_FAILURE;
		}
	}

	int regex_found = 0;
	search_query = construct_regex_query(&args[0], invert, &regex_found);

	/* Get matches */
	size_t i;

	regex_t regex_files;
	int reg_flags = conf.case_sens_search == 1 ? (REG_NOSUB | REG_EXTENDED)
		: (REG_NOSUB | REG_EXTENDED | REG_ICASE);
	int ret = regcomp(&regex_files, search_query, reg_flags);

	if (ret != EXIT_SUCCESS) {
		xerror(_("'%s': Invalid regular expression\n"), search_query);
		regfree(&regex_files);
		free_regex_dirlist(&reg_dirlist, tmp_files);
		return EXIT_FAILURE;
	}

	size_t found = 0;
	int *regex_index = (int *)xnmalloc((search_path ? (size_t)tmp_files
		: files) + 2, sizeof(int));
	size_t max = (search_path && *search_path) ? (size_t)tmp_files : files;

	for (i = 0; i < max; i++) {
		char *name = (search_path && *search_path) ? reg_dirlist[i]->d_name
		: file_info[i].name;

		if (regexec(&regex_files, name, 0, NULL, 0) == EXIT_SUCCESS) {
			if (invert == 0) {
				regex_index[found] = (int)i;
				found++;
			}
		} else {
			if (invert == 1) {
				regex_index[found] = (int)i;
				found++;
			}
		}
	}

	regex_index[found] = -1; /* Mark end of array */
	regfree(&regex_files);

	if (found == 0) {
		err_regex_no_match(regex_found, args[1]);
		free(regex_index);
		free_regex_dirlist(&reg_dirlist, tmp_files);
		return EXIT_FAILURE;
	}

	/* We have matches: print them. */
	size_t matches = print_regex_matches(file_type, reg_dirlist, regex_index);

	free(regex_index);
	free_regex_dirlist(&reg_dirlist, tmp_files);

	return matches == 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int
err_glob_no_match(const char *arg)
{
	char *input = (conf.autocd == 1 && !arg
		&& (search_flags & NO_GLOB_CHAR)
		&& rl_line_buffer) ? strrchr(rl_line_buffer, '/')
		: (char *)NULL;

	if (input && input != rl_line_buffer) {
		/* Input string contains two slashes: it looks like a path, so let's
		 * err like it was. */
		xerror("cd: %s: %s\n", rl_line_buffer, strerror(ENOENT));
		return EXIT_FAILURE;
	}

	fputs(_("search: No matches found\n"), stderr);
	return EXIT_FAILURE;
}

/* We have three search strategies:
 * 1 - Only glob
 * 2 - Only regex
 * 3 - Glob-regex */
int
search_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(SEARCH_USAGE);
		return EXIT_SUCCESS;
	}

	if (conf.search_strategy == REGEX_ONLY)
		return search_regex(args);

	int ret = search_glob(args);
	if (ret != EXIT_FAILURE)
		return (ret == ERR_SKIP_REGEX ? 1 : ret);

	if (conf.search_strategy == GLOB_ONLY)
		return err_glob_no_match(args[1]);

	if (!(search_flags & NO_GLOB_CHAR))
		fputs(_("Glob: No matches found. Trying regex...\n"), stderr);

	return search_regex(args);
}
