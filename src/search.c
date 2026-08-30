/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* search.c -- functions for the search system */

#include "helpers.h"

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef __sun
# include <sys/termios.h> /* TIOCGWINSZ */
#endif /* __sun */

/* We need rl_line_buffer in case of no matches and no metacharacter */
#include <readline/readline.h>

#include "aux.h"
#include "checks.h"
#include "colors.h"
#ifndef _NO_ICONS
# include "listing.h" /* print_file_icon() */
#endif
#include "messages.h"
#include "misc.h"
#include "navigation.h"
#include "sort.h"
#include "spawn.h"
#include "xmatch.h" /* xglob() */

#define IS_METHOD_PREFIX(s, l) ((s) && (l) > 3                       \
&& ((strncmp((s), "/gl:", 4) == 0) || (strncmp((s), "/re:", 4) == 0)))

#if (defined(__OpenBSD__) || defined(__sun)) && !defined(_BE_POSIX)
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
exec_find(const char *name, const char *_path, const char *method, const char *pattern)
{
	if (conf.follow_symlinks == 1) {
		const char *cmd[] = {name, "-L", _path, method, pattern, NULL};
		return launch_execv(cmd, FOREGROUND, E_NOSTDERR);
	}

	const char *cmd[] = {name, _path, method, pattern, NULL};
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
		if (is_cmd_in_path("gfind", NULL) == 1)
			have_gfind = 1;
	}

	return (have_gfind == 1 ? "gfind" : "find");
}
#endif /* FIND_HAS_NO_REGEX */

static int
run_find(char *search_path, const char *arg, const int regex)
{
	char *_path = (search_path && *search_path) ? search_path : ".";
	char *name = "find";

#if defined(_BE_POSIX)
	/* POSIX find(1) only supports -name */
	char *method = "-name";
#else
	char *method = regex == 1
		? (conf.ignore_case == 0 ? "-regex" : "-iregex")
		: (conf.ignore_case == 0 ? "-name" : "-iname");
# if defined(FIND_HAS_NO_REGEX)
	name = define_find_name();
	if (*name != 'g') /* GNU find (gfind) not found */
		method = conf.ignore_case == 0 ? "-name" : "-iname";
# endif /* FIND_HAS_NO_REGEX */
#endif /* _BE_POSIX */

	const int meta_char = check_metachar(arg, REGEX_MATCH);
	if (meta_char == 1)
		return exec_find(name, _path, method, arg);

	const size_t pattern_len = strlen(arg) + 5;
	char *pattern = xnmalloc(pattern_len, sizeof(char));

#if !defined(_BE_POSIX)
	if (regex == 1) {
# if !defined(FIND_HAS_NO_REGEX)
		snprintf(pattern, pattern_len, ".*%s.*", arg);
# else
		if (*name == 'g') /* We have GNU find (gfind) */
			snprintf(pattern, pattern_len, ".*%s.*", arg);
		else
			snprintf(pattern, pattern_len, "*%s*", arg);
# endif /* FIND_HAS_NO_REGEX */
	} else {
		snprintf(pattern, pattern_len, "*%s*", arg);
	}
#else
	snprintf(pattern, pattern_len, "*%s*", arg);
#endif /* !_BE_POSIX */

	const int ret = exec_find(name, _path, method, pattern);
	free(pattern);

	return ret;
}

/*
static char **
glob_sort_dirs(xglob_t *globbed_files, size_t *g)
{
	int *dirs = xnmalloc(globbed_files->gl_pathc + 1, sizeof(int));
	char **gfiles = xnmalloc(globbed_files->gl_pathc + 1, sizeof(char *));
	struct stat attr;
	size_t i, n = 0;

	for (i = 0; globbed_files->gl_pathv[i]; i++) {
		if (stat(globbed_files->gl_pathv[i], &attr) != -1
		&& S_ISDIR(attr.st_mode))
			dirs[i] = 1;
		else
			dirs[i] = 0;
	}

	if (conf.group_dirs == GROUP_DIRS_FIRST) {
		for (i = 0; globbed_files->gl_pathv[i]; i++) {
			if (dirs[i] == 1)
				gfiles[n++] = globbed_files->gl_pathv[i];
		}
		for (i = 0; globbed_files->gl_pathv[i]; i++) {
			if (dirs[i] == 0)
				gfiles[n++] = globbed_files->gl_pathv[i];
		}
	} else { // GROUP_DIRS_LAST
		for (i = 0; globbed_files->gl_pathv[i]; i++) {
			if (dirs[i] == 0)
				gfiles[n++] = globbed_files->gl_pathv[i];
		}
		for (i = 0; globbed_files->gl_pathv[i]; i++) {
			if (dirs[i] == 1)
				gfiles[n++] = globbed_files->gl_pathv[i];
		}
	}

	free(dirs);
	gfiles[n] = NULL;

	*g = n;
	return gfiles;
} */

/* If the pattern string contains no metacharacters, change it to "*PATTERN*". */
static char *
build_glob_query(char **pattern)
{
	search_flags &= ~NO_GLOB_CHAR;

	/* If the query string already contains metacharacters, return it as is. */
	if (check_metachar(*pattern, REGEX_MATCH) == 1)
		return *pattern;

	search_flags |= NO_GLOB_CHAR;

	const size_t len = strlen(*pattern);
	char *tmp = strdup(*pattern);
	*pattern = xnrealloc(*pattern, len + 3, sizeof(char));
	snprintf(*pattern, len + 3, "*%s*", tmp);
	free(tmp);

	return *pattern;
}

static int
err_glob_no_match(const char *arg)
{
	char *input = (conf.autocd == 1 && !arg
		&& (search_flags & NO_GLOB_CHAR) && rl_line_buffer
		&& !IS_METHOD_PREFIX(rl_line_buffer, rl_end))
			? strrchr(rl_line_buffer, '/') : NULL;

	if (input && input != rl_line_buffer) {
		/* Input string contains two slashes: it looks like a path, so let's
		 * err like it was. */
		char *p = unescape_str(rl_line_buffer);
		xerror("cd: '%s': %s\n", p ? p : rl_line_buffer, strerror(ENOENT));
		free(p);
		return FUNC_FAILURE;
	}

	fputs(_("search: No matches found\n"), stderr);
	return FUNC_FAILURE;
}

/* Original string is either "/QUERY" or "/!QUERY". Let's extract QUERY.
 * If the query string contains no metacharacters, change it to ".*QUERY.*" */
static char *
build_regex_query(char **query, int *regex_found)
{
	*regex_found = check_regex(*query);
	if (*regex_found == FUNC_SUCCESS)
		return *query;

	const size_t len = strlen(*query);
	char *tmp = strdup(*query);
	*query = xnrealloc(*query, len + 5, sizeof(char));
	snprintf(*query, len + 5, ".*%s.*", tmp);
	free(tmp);

	return *query;
}

static void
err_regex_no_match(const int regex_found, const char *arg)
{
	char *input = (conf.autocd == 1 && !arg && (regex_found == FUNC_FAILURE
			|| (search_flags & NO_GLOB_CHAR)) && rl_line_buffer
			&& !IS_METHOD_PREFIX(rl_line_buffer, rl_end))
			? strrchr(rl_line_buffer, '/') : NULL;

	if (input && input != rl_line_buffer) {
		/* Input string contains at least two slashes. It looks like a path:
		 * let's err like it was. */
		char *p = unescape_str(rl_line_buffer);
		xerror("cd: '%s': %s\n", p ? p : rl_line_buffer, strerror(ENOENT));
		free(p);
	} else {
		fputs(_("search: No matches found\n"), stderr);
	}

	search_flags &= ~NO_GLOB_CHAR;
}

static void
set_search_params(const char *query, char **pattern, int *style)
{
	*style = conf.matching_style;

	if (!query || !*query || !query[1])
		return;

	if (query[1] == '!') {
		*pattern = strdup(query[2] ? query + 2 : query + 1);
		return;
	}

	const char *pat = query + 1; /* Skip leading slash */
	if (IS_GLOB_PREFIX(pat)) {
		pat += 3;
		*style = GLOB_MATCH;
	} else if (IS_REGEX_PREFIX(pat)) {
		pat += 3;
		*style = REGEX_MATCH;
	}

	*pattern = strdup(pat);
}

static int
set_file_type_and_search_path(char **args, mode_t *file_type,
	char **search_path, const char *query, const int regex)
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

	/* Starting path is the same as the current dir. Ignore it. */
	if (*search_path && strcmp(*search_path, workspaces[cur_ws].path) == 0)
		*search_path = NULL;

	if (*file_type == 0)
		return FUNC_SUCCESS;

	/* Convert file type into a macro that can be decoded by stat(). If
	 * file type is specified, matches will be checked against this value. */
	switch (*file_type) {
	case 'b': *file_type = DT_BLK; break;
	case 'c': *file_type = DT_CHR; break;
	case 'd': *file_type = DT_DIR; break;
#ifdef SOLARIS_DOORS
	case 'O': *file_type = DT_DOOR; break;
	case 'P': *file_type = DT_PORT; break;
#endif /* SOLARIS_DOORS */
	case 'f': *file_type = DT_REG; break;
	case 'l': *file_type = DT_LNK; break;
	case 'p': *file_type = DT_FIFO; break;
	case 's': *file_type = DT_SOCK; break;
	case 'r': /* Fallthrough */
	case 'x': run_find(*search_path, query, regex); return FUNC_SUCCESS;
	default:
		fprintf(stderr, _("search: '%c': Unrecognized file "
			"type\n"), (char)*file_type);
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static inline size_t
calc_item_len(const struct fileinfo *f)
{
	/* ELN is zero if no ELN should be printed (not current dir) */
	return ((f->eln > 0 ? (size_t)f->eln_n + 1 : 0)
		+ (conf.icons == 1 ? (size_t)ICON_LEN : 0)
		+ f->len + (size_t)f->dir
		+ (f->filesn > 0 ? DIGINUM(f->filesn) : 0));
}

/* Test a column count and optionally return the width of each column. */
static int
layout_fits(struct fileinfo *finfo, const size_t count, const size_t columns,
	const size_t spacing, size_t widths[])
{
	const int hl = (conf.listing_mode == HORLIST);
	size_t rows = (count + columns - 1) / columns;
	size_t total_width = 0;
	size_t screen_width = (size_t)term_cols;

	for (size_t col = 0; col < columns; col++) {
		size_t longest = 0;

		for (size_t row = 0; row < rows; row++) {
			const size_t index = hl ? (row * columns + col) : (col * rows + row);
			if (index >= count)
				continue;

			size_t len = calc_item_len(&finfo[index]);
			if (len > longest)
				longest = len;
		}

		widths[col] = longest;

		/* Add spacing after every column except the last one. */
		if (col + 1 < columns)
			total_width += longest + spacing;
		else
			total_width += longest;

		if (total_width > screen_width)
			return 0;
	}

	return total_width <= screen_width;
}

static void
print_matches(struct fileinfo *finfo, const size_t count, const char *dir)
{
	if (count == 0)
		return;

	const size_t spacing = 2; // COLUMNS_GAP
	const int hl = (conf.listing_mode == HORLIST);
	const int conf_icons = conf.icons;
	size_t *widths = xnmalloc(count, sizeof(*widths));
	size_t columns = 1;

	/* Find the greatest number of columns that fits. */
	while (columns < count) {
		columns++;
		if (layout_fits(finfo, count, columns, spacing, widths) == 0) {
			layout_fits(finfo, count, --columns, spacing, widths);
			break;
		}
	}

	size_t rows = (count + columns - 1) / columns;

	for (size_t row = 0; row < rows; row++) {
		for (size_t col = 0; col < columns; col++) {
			const size_t index = hl ? (row * columns + col) : (col * rows + row);
			if (index >= count)
				continue;

			const size_t len = calc_item_len(&finfo[index]);
			size_t padding = 0;
			if (col + 1 < columns)
				padding = widths[col] - len + spacing;

			if (!dir) { /* Print ELN */
				printf("%s%*zd%s ",
					el_c, (int)finfo[index].eln_n, finfo[index].eln, df_c);
			}

			/* Print the remaining line */
			printf("%s%s%s%s%s%s%s%s%s%s%s%*s",
				conf_icons ? finfo[index].icon_color : "",
				conf_icons ? finfo[index].icon : "", df_c,
				conf_icons ? " " : "",

				finfo[index].color, finfo[index].name, df_c,
				finfo[index].dir ? fc_c : "",
				finfo[index].dir ? "/" : "",
				finfo[index].filesn > 0 ? xitoa(finfo[index].filesn) : "",
				df_c,
				(int)padding, "");

		}

		putchar('\n');
	}

	free(widths);
}

static filesn_t
get_longest_eln(const struct fileinfo *finfo, const size_t total)
{
	filesn_t l = 0;

	for (size_t i = 0; i < total; i++) {
		if (finfo[i].eln > l)
			l = finfo[i].eln;
	}

	return l;
}

static int
search_glob(char **args, char **pattern)
{
	char *search_query = NULL;
	char *search_path = NULL;
	mode_t file_type = 0;
	int ret = FUNC_FAILURE;

	if (set_file_type_and_search_path(args, &file_type,
	&search_path, *pattern, 0) != FUNC_SUCCESS) {
		return FUNC_FAILURE;
	}

	if (file_type == 'x' || file_type == 'r') /* Recursive search via find(1) */
		return FUNC_SUCCESS;

	search_query = build_glob_query(pattern);
	if (!search_query)
		return FUNC_FAILURE;

	xglob_t g = {0};
	ret = xglob(search_path ? search_path : ".", search_query, &g, -1, file_type, 1, 1);
	if (ret != 0 || g.gl_matches == 0) {
		xglobfree(&g);
		return err_glob_no_match(args[1]);
	}

	// Remove once filenames are truncated in the output
	if (!search_path) {
		const int eln_len =
			(int)DIGINUM(get_longest_eln(g.gl_finfo, g.gl_matches));
		for (size_t i = 0; i < g.gl_matches; i++) {
			g.gl_finfo[i].eln_n = eln_len;
			g.gl_finfo[i].len = g.gl_finfo[i].utf8
				? wc_xstrlen(g.gl_finfo[i].name) : g.gl_finfo[i].bytes;
		}
	}

	print_matches(g.gl_finfo, g.gl_matches, search_path);

	xglobfree(&g);
	return FUNC_SUCCESS;
}

static int
search_regex(char **args, char **pattern)
{
	char *search_query = NULL;
	char *search_path = NULL;
	mode_t file_type = 0;
	int ret = FUNC_FAILURE;

	if (set_file_type_and_search_path(args, &file_type,
	&search_path, *pattern, 0) != FUNC_SUCCESS) {
		return FUNC_FAILURE;
	}

	if (file_type == 'x' || file_type == 'r') /* Recursive search via find(1) */
		return FUNC_SUCCESS;

	int found = 0;
	search_query = build_regex_query(pattern, &found);
	if (!search_query)
		return FUNC_FAILURE;

	xregex_t r = {0};
	ret = xregex(search_path ? search_path : ".", search_query, &r, -1, file_type, 1, 1);
	if (ret != 0 || r.re_matches == 0) {
		xregfree(&r);
		err_regex_no_match(found, args[1]);
		return FUNC_FAILURE;
	}

	// Remove once filenames are truncated in the output
	if (!search_path) {
		const int eln_len =
			(int)DIGINUM(get_longest_eln(r.re_finfo, r.re_matches));
		for (size_t i = 0; i < r.re_matches; i++) {
			r.re_finfo[i].eln_n = eln_len;
			r.re_finfo[i].len = r.re_finfo[i].utf8
				? wc_xstrlen(r.re_finfo[i].name) : r.re_finfo[i].bytes;
		}
	}

	print_matches(r.re_finfo, r.re_matches, search_path);

	xregfree(&r);
	return FUNC_SUCCESS;
}

/* Search for files in ARGS[1] (current directory if NULL) using ARGS[0]
 * as pattern. */
int
search_function(char **args)
{
	if (args[1] && IS_HELP(args[1])) {
		puts(SEARCH_USAGE);
		return FUNC_SUCCESS;
	}

	char *pattern = NULL;
	int matching_style = 0;
	int ret = 0;

	set_search_params(args[0], &pattern, &matching_style);
	if (!pattern)
		return FUNC_FAILURE;

	if (matching_style == REGEX_MATCH)
		ret = search_regex(args, &pattern);
	else /* matching_style == GLOB_MATCH */
		ret = search_glob(args, &pattern);

	free(pattern);

	return ret;
}
