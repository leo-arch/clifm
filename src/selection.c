/* selection.c -- files selection functions */

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
#include <glob.h>
#include <stdio.h>
#include <readline/tilde.h>
#include <string.h>
#include <unistd.h>
#if defined(__linux__) || defined(__HAIKU__) || defined(__APPLE__) \
|| defined(__sun) || defined(__CYGWIN__)
# ifdef __TINYC__
/* Silence a tcc warning. We don't use CTRL anyway */
#  undef CTRL
# endif
# include <sys/ioctl.h>
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
/*
char **new_selections = (char **)NULL;
size_t new_seln = 0; */

/* Save selected elements into a tmp file. Returns 1 if success and 0
 * if error. This function allows the user to work with multiple
 * instances of the program: he/she can select some files in the
 * first instance and then execute a second one to operate on those
 * files as he/she wishes. */
int
save_sel(void)
{
	if (selfile_ok == 0 || config_ok == 0 || !sel_file)
		return EXIT_FAILURE;

	if (sel_n == 0) {
		if (unlink(sel_file) == -1) {
			xerror("sel: %s: %s\n", sel_file, strerror(errno));
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}

	int fd = 0;
	FILE *fp = open_fwrite(sel_file, &fd);
	if (!fp) {
		xerror("sel: %s: %s\n", sel_file, strerror(errno));
		return EXIT_FAILURE;
	}

	size_t i;
	for (i = 0; i < sel_n; i++) {
		fputs(sel_elements[i].name, fp);
		fputc('\n', fp);
	}

	fclose(fp);
	return EXIT_SUCCESS;
}

int
select_file(char *file)
{
	if (!file || !*file)
		return 0;

	int exists = 0, new_sel = 0, j;
	size_t flen = strlen(file);
	if (flen > 1 && file[flen - 1] == '/')
		file[flen - 1] = '\0';

	/* Check if FILE is already in the selection box */
	j = (int)sel_n;
	while (--j >= 0) {
		if (*file == *sel_elements[j].name
		&& strcmp(sel_elements[j].name, file) == 0) {
			exists = 1;
			break;
		}
	}

	if (exists == 0) {
		sel_elements = (struct sel_t *)xrealloc(sel_elements,
			(sel_n + 2) * sizeof(struct sel_t));
		sel_elements[sel_n].name = savestring(file, strlen(file));
		sel_elements[sel_n].size = (off_t)UNSET;
		sel_n++;
		sel_elements[sel_n].name = (char *)NULL;
		sel_elements[sel_n].size = (off_t)UNSET;

		new_sel++;
	} else {
		xerror(_("sel: %s: Already selected\n"), file);
	}

	return new_sel;
}

static char **
load_matches_invert_cwd(glob_t gbuf, const mode_t filetype, int *matches)
{
	char **list = (char **)xnmalloc(files + 2, sizeof(char *));

	int i = (int)files;
	while (--i >= 0) {
		if (filetype != 0 && file_info[i].type != filetype)
			continue;

		int j = (int)gbuf.gl_pathc;
		while (--j >= 0) {
			if (*file_info[i].name == *gbuf.gl_pathv[j]
			&& strcmp(file_info[i].name, gbuf.gl_pathv[j]) == 0)
				break;
		}

		if (j == -1) {
			list[*matches] = file_info[i].name;
			(*matches)++;
		}
	}

	return list;
}

static char **
load_matches_invert_nocwd(glob_t gbuf, struct dirent **ent,
	const mode_t filetype, int *matches, const int ret)
{
	char **list = (char **)xnmalloc((size_t)ret + 2, sizeof(char *));

	int i = ret;
	while (--i >= 0) {
#if !defined(_DIRENT_HAVE_D_TYPE)
		struct stat attr;
		if (lstat(ent[i]->d_name, &attr) == -1)
			continue;
		mode_t type = get_dt(attr.st_mode);
		if (filetype != 0 && type != filetype)
#else
		if (filetype != 0 && ent[i]->d_type != filetype)
#endif
			continue;

		int j = (int)gbuf.gl_pathc;
		while (--j >= 0) {
			if (*ent[i]->d_name == *gbuf.gl_pathv[j]
			&& strcmp(ent[i]->d_name, gbuf.gl_pathv[j]) == 0)
				break;
		}

		if (j == -1) {
			list[*matches] = ent[i]->d_name;
			(*matches)++;
		}
	}

	return list;
}

static mode_t
convert_filetype_mask(const mode_t filetype)
{
	switch (filetype) {
	case DT_DIR:  return S_IFDIR;
	case DT_REG:  return S_IFREG;
	case DT_LNK:  return S_IFLNK;
	case DT_SOCK: return S_IFSOCK;
	case DT_FIFO: return S_IFIFO;
	case DT_BLK:  return S_IFBLK;
	case DT_CHR:  return S_IFCHR;
	default: return 0;
	}
}

static char **
load_matches(glob_t gbuf, const mode_t filetype, int *matches)
{
	char **list = (char **)xnmalloc(gbuf.gl_pathc + 2, sizeof(char *));
	mode_t type = convert_filetype_mask(filetype);

	int i = (int)gbuf.gl_pathc;
	while (--i >= 0) {
		char *basename = strrchr(gbuf.gl_pathv[i], '/');
		if (!basename && SELFORPARENT(gbuf.gl_pathv[i]))
			continue;
		if (basename && basename[1] && SELFORPARENT(basename + 1))
			continue;

		if (filetype != 0) {
			struct stat attr;
			if (lstat(gbuf.gl_pathv[i], &attr) == -1
			|| (attr.st_mode & S_IFMT) != type)
				continue;
		}

		list[*matches] = gbuf.gl_pathv[i];
		(*matches)++;
	}

	return list;
}

static int
select_matches(char **list, const char *sel_path, const int matches)
{
	int i = matches;
	int new_sel = 0;

	while (--i >= 0) {
		if (!list[i])
			continue;

		if (sel_path) {
			size_t tmp_len = strlen(sel_path) + strlen(list[i]) + 2;
			char *tmp = (char *)xnmalloc(tmp_len, sizeof(char));
			snprintf(tmp, tmp_len, "%s/%s", sel_path, list[i]);

			new_sel += select_file(tmp);

			free(tmp);
			continue;
		}

		/* CWD */
		if (*list[i] == '/') { /* Absolute path */
			new_sel += select_file(list[i]);
			continue;
		}

		/* Relative path */
		char *tmp = (char *)NULL;

		if (*workspaces[cur_ws].path == '/'
		&& !*(workspaces[cur_ws].path + 1)) {
			/* CWD is root */
			size_t tmp_len = strlen(list[i]) + 2;
			tmp = (char *)xnmalloc(tmp_len, sizeof(char));
			snprintf(tmp, tmp_len, "/%s", list[i]);
		} else {
			size_t tmp_len = strlen(workspaces[cur_ws].path)
				+ strlen(list[i]) + 2;
			tmp = (char *)xnmalloc(tmp_len, sizeof(char));
			snprintf(tmp, tmp_len, "%s/%s",
				workspaces[cur_ws].path, list[i]);
		}

		new_sel += select_file(tmp);
		free(tmp);
	}

	return new_sel;
}

static int
sel_glob(char *str, const char *sel_path, const mode_t filetype)
{
	if (!str || !*str)
		return (-1);

	glob_t gbuf;
	char *pattern = str;
	int invert = 0;

	if (*pattern == '!') {
		pattern++;
		invert = 1;
	}

	int ret = glob(pattern, GLOB_BRACE, NULL, &gbuf);
	if (ret != EXIT_SUCCESS) {
		globfree(&gbuf);
		return (-1);
	}

	char **list = (char **)NULL;
	int matches = 0;
	struct dirent **ent = (struct dirent **)NULL;

	if (invert == 1) {
		if (!sel_path) {
			list = load_matches_invert_cwd(gbuf, filetype, &matches);
		} else {
			ret = scandir(sel_path, &ent, skip_files, xalphasort);
			if (ret == -1) {
				xerror("sel: %s: %s\n", sel_path, strerror(errno));
				globfree(&gbuf);
				return (-1);
			}

			list = load_matches_invert_nocwd(gbuf, ent, filetype, &matches, ret);
		}
	} else {
		list = load_matches(gbuf, filetype, &matches);
	}

	list[matches] = (char *)NULL;

	int new_sel = select_matches(list, sel_path, matches);

	free(list);
	globfree(&gbuf);

	if (invert == 1 && sel_path) {
		int i = ret;
		while (--i >= 0)
			free(ent[i]);
		free(ent);
	}

	return new_sel;
}

static int
sel_regex_cwd(regex_t regex, const mode_t filetype, const int invert)
{
	int new_sel = 0;
	int i = (int)files;

	while (--i >= 0) {
		if (filetype != 0 && file_info[i].type != filetype)
			continue;

		char tmp_path[PATH_MAX];
		if (*workspaces[cur_ws].path == '/'
		&& !*(workspaces[cur_ws].path + 1)) {
			snprintf(tmp_path, sizeof(tmp_path), "/%s", file_info[i].name);
		} else {
			snprintf(tmp_path, sizeof(tmp_path), "%s/%s",
				workspaces[cur_ws].path, file_info[i].name);
		}

		if (regexec(&regex, file_info[i].name, 0, NULL, 0) == EXIT_SUCCESS) {
			if (invert == 0)
				new_sel += select_file(tmp_path);
		} else if (invert == 1) {
			new_sel += select_file(tmp_path);
		}
	}

	return new_sel;
}

static int
sel_regex_nocwd(regex_t regex, const char *sel_path, const mode_t filetype,
	const int invert)
{
	int new_sel = 0;
	struct dirent **list = (struct dirent **)NULL;
	int filesn = scandir(sel_path, &list, skip_files, xalphasort);

	if (filesn == -1) {
		xerror("sel: %s: %s\n", sel_path, strerror(errno));
		return (-1);
	}

	mode_t type = convert_filetype_mask(filetype);

	int i = (int)filesn;
	while (--i >= 0) {
		if (filetype != 0) {
			struct stat attr;
			if (lstat(list[i]->d_name, &attr) != -1
			&& (attr.st_mode & S_IFMT) != type) {
				free(list[i]);
				continue;
			}
		}

		size_t tmp_len = strlen(sel_path) + strlen(list[i]->d_name) + 2;
		char *tmp_path = (char *)xnmalloc(tmp_len, sizeof(char));
		snprintf(tmp_path, tmp_len, "%s/%s", sel_path, list[i]->d_name);

		if (regexec(&regex, list[i]->d_name, 0, NULL, 0) == EXIT_SUCCESS) {
			if (invert == 0)
				new_sel += select_file(tmp_path);
		} else if (invert == 1) {
			new_sel += select_file(tmp_path);
		}

		free(tmp_path);
		free(list[i]);
	}

	free(list);

	return new_sel;
}

static int
sel_regex(char *str, const char *sel_path, const mode_t filetype)
{
	if (!str || !*str)
		return (-1);

	regex_t regex;
	char *pattern = str;
	int invert = 0;
	int new_sel = 0;

	if (*pattern == '!') {
		pattern++;
		invert = 1;
	}

	int reg_flags = conf.case_sens_list == 1 ? (REG_NOSUB | REG_EXTENDED)
			: (REG_NOSUB | REG_EXTENDED | REG_ICASE);

	if (regcomp(&regex, pattern, reg_flags) != EXIT_SUCCESS) {
		xerror(_("sel: %s: Invalid regular expression\n"), str);
		regfree(&regex);
		return (-1);
	}

	if (!sel_path) { /* Check pattern (STR) against files in CWD */
		new_sel = sel_regex_cwd(regex, filetype, invert);
	} else { /* Check pattern against files in SEL_PATH */
		new_sel = sel_regex_nocwd(regex, sel_path, filetype, invert);
		if (new_sel == -1)
			return (-1);
	}

	regfree(&regex);
	return new_sel;
}

/* Convert file type into a macro that can be decoded by stat(3).
 * If file type is specified, matches will be checked against
 * this value. */
static mode_t
convert_filetype(mode_t *filetype)
{
	switch (*filetype) {
	case 'b': *filetype = DT_BLK; break;
	case 'c': *filetype = DT_CHR; break;
	case 'd': *filetype = DT_DIR; break;
	case 'f': *filetype = DT_REG; break;
	case 'l': *filetype = DT_LNK; break;
	case 's': *filetype = DT_SOCK; break;
	case 'p': *filetype = DT_FIFO; break;
	default:
		xerror(_("%s: '%c': Unrecognized file type\n"),
		    PROGRAM_NAME, (char)*filetype);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static char *
parse_sel_params(char ***args, int *ifiletype, mode_t *filetype, int *isel_path)
{
	char *sel_path = (char *)NULL;
	int i;
	for (i = 1; (*args)[i]; i++) {
		if (*(*args)[i] == '-' && (*args)[i][1] && !(*args)[i][2]) {
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
				xerror("sel: %s: %s\n", (*args)[i], strerror(errno));
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

static char *
construct_sel_path(char *sel_path)
{
	char tmpdir[PATH_MAX];
	xstrsncpy(tmpdir, sel_path, sizeof(tmpdir) - 1);

	if (*sel_path == '.' && realpath(sel_path, tmpdir) == NULL) {
		xerror("sel: %s: %s\n", sel_path, strerror(errno));
		return (char *)NULL;
	}

	if (*sel_path == '~') {
		char *exp_path = tilde_expand(sel_path);
		if (!exp_path) {
			xerror("%s\n", _("sel: Error expanding path"));
			return (char *)NULL;
		}

		xstrsncpy(tmpdir, exp_path, sizeof(tmpdir) - 1);
		free(exp_path);
	}

	char *dir = (char *)NULL;
	if (*tmpdir != '/') {
		size_t dir_len = strlen(workspaces[cur_ws].path) + strlen(tmpdir) + 2;
		dir = (char *)xnmalloc(dir_len, sizeof(char));
		snprintf(dir, dir_len, "%s/%s", workspaces[cur_ws].path, tmpdir);
	} else {
		dir = savestring(tmpdir, strlen(tmpdir));
	}

	return dir;
}

static char *
check_sel_path(char **sel_path)
{
	size_t len = strlen(*sel_path);
	if (len > 0 && (*sel_path)[len - 1] == '/')
		(*sel_path)[len - 1] = '\0';

	if (strchr(*sel_path, '\\')) {
		char *deq = dequote_str(*sel_path, 0);
		if (deq) {
			xstrsncpy(*sel_path, deq, strlen(deq));
			free(deq);
		}
	}

	char *dir = construct_sel_path(*sel_path);
	if (!dir)
		return (char *)NULL;

	if (access(dir, X_OK) == -1) {
		xerror("sel: %s: %s\n", dir, strerror(errno));
		free(dir);
		return (char *)NULL;
	}

	if (xchdir(dir, NO_TITLE) == -1) {
		xerror("sel: %s: %s\n", dir, strerror(errno));
		free(dir);
		return (char *)NULL;
	}

	return dir;
}

static off_t
get_sel_file_size(const size_t i, int *status)
{
	*status = 0;

	if (sel_elements[i].size != (off_t)UNSET)
		return sel_elements[i].size;

	struct stat attr;
	if (lstat(sel_elements[i].name, &attr) == -1)
		return (off_t)-1;

	int base = xargs.si == 1 ? 1000 : 1024;
	if (S_ISDIR(attr.st_mode)) {
		sel_elements[i].size = (off_t)(dir_size(sel_elements[i].name,
			0, status) * base);
	} else {
		sel_elements[i].size = (off_t)FILE_SIZE;
	}

	return sel_elements[i].size;
}

static void
print_total_size(const off_t total, const int status)
{
	char *human_size = get_size_unit(total);

	char err[sizeof(xf_c) + 6]; *err = '\0';
	if (status != 0)
		snprintf(err, sizeof(err), "%s%c%s", xf_c, DU_ERR_CHAR, NC);

	printf(_("%s%sTotal size%s: %s%s\n"), df_c, BOLD, df_c, err, human_size);
	free(human_size);
}

static void
print_selected_files(void)
{
	if (conf.clear_screen == 1)
		CLEAR;

	printf(_("%sSelection Box%s\n\n"), BOLD, df_c);

	size_t t = tab_offset, i;
	off_t total = 0;
	tab_offset = 0;

	int status = 0;
	uint8_t epad = DIGINUM(sel_n);

	flags |= IN_SELBOX_SCREEN;
	for (i = 0; i < sel_n; i++) {
		printf("%s%*zu%s ", el_c, epad, i + 1, df_c);
		colors_list(sel_elements[i].name, NO_ELN, NO_PAD, PRINT_NEWLINE);

		printf(_("Calculating file size... ")); fflush(stdout);
		int ret = 0;
		off_t s = get_sel_file_size(i, &ret);
		if (ret != 0)
			status = ret;
		putchar('\r'); ERASE_TO_RIGHT; fflush(stdout);

		if (s != (off_t)-1)
			total += s;
	}
	flags &= ~IN_SELBOX_SCREEN;
	tab_offset = t;

	putchar('\n');
	print_total_size(total, status);
}

static int
print_sel_results(const int new_sel, const char *sel_path,
	const char *pattern, const int err)
{
	if (new_sel > 0 && xargs.stealth_mode != 1 && sel_file
	&& save_sel() != EXIT_SUCCESS) {
		_err('e', PRINT_PROMPT, _("sel: Error writing selected files "
			"into the selections file\n"));
		return EXIT_FAILURE;
	}

	if (sel_path && xchdir(workspaces[cur_ws].path, NO_TITLE) == -1) {
		xerror("sel: %s: %s\n", workspaces[cur_ws].path, strerror(errno));
		return EXIT_FAILURE;
	}

	if (new_sel <= 0) {
		if (pattern && err == 0)
			fprintf(stderr, _("sel: No matches found\n"));
		return EXIT_FAILURE;
	}

	get_sel_files();

	if (conf.autols == 1 && err == 0)
		reload_dirlist();

	print_reload_msg(_("%d file(s) selected\n"), new_sel);
	print_reload_msg(_("%zu total selected file(s)\n"), sel_n);

	return EXIT_SUCCESS;
}

static char *
construct_sel_filename(const char *dir, const char *name)
{
	char *f = (char *)NULL;
	size_t flen = 0;

	if (!dir) {
		if (*workspaces[cur_ws].path == '/'
		&& !*(workspaces[cur_ws].path + 1)) {
			flen = strlen(name) + 2;
			f = (char *)xnmalloc(flen, sizeof(char));
			snprintf(f, flen, "/%s", name);

			return f;
		}

		flen = strlen(workspaces[cur_ws].path) + strlen(name) + 2;
		f = (char *)xnmalloc(flen, sizeof(char));
		snprintf(f, flen, "%s/%s", workspaces[cur_ws].path, name);

		return f;
	}

	flen = strlen(dir) + strlen(name) + 2;
	f = (char *)xnmalloc(flen, sizeof(char));
	snprintf(f, flen, "%s/%s", dir, name);

	return f;
}

static int
select_filename(char *arg, char *dir, int *err)
{
	int new_sel = 0;

	if (strchr(arg, '\\')) {
		char *deq_str = dequote_str(arg, 0);
		if (deq_str) {
			xstrsncpy(arg, deq_str, strlen(deq_str));
			free(deq_str);
		}
	}

	char *name = arg;
	if (*name == '.' && *(name + 1) == '/')
		name += 2;

	if (*arg != '/') {
		char *tmp = construct_sel_filename(dir, name);
		struct stat attr;
		if (lstat(tmp, &attr) == -1) {
			xerror("sel: %s: %s\n", arg, strerror(errno));
			(*err)++;
		} else {
			int r = select_file(tmp);
			new_sel += r;
			if (r == 0)
				(*err)++;
		}
		free(tmp);
		return new_sel;
	}

	struct stat a;
	if (lstat(arg, &a) == -1) {
		xerror("sel: %s: %s\n", name, strerror(errno));
		(*err)++;
	} else {
		int r = select_file(name);
		new_sel += r;
		if (r == 0)
			(*err)++;
	}

	return new_sel;
}

static int
select_pattern(char *arg, const char *dir, const mode_t filetype, int *err)
{
	int ret = sel_glob(arg, dir, filetype);

	/* Glob failed. Try REGEX. */
	if (ret == -1)
		ret = sel_regex(arg, dir, filetype);

	if (ret == -1)
		(*err)++;

	return ret;
}

int
sel_function(char **args)
{
	if (!args) return EXIT_FAILURE;

	if (!args[1] || IS_HELP(args[1])) {
		puts(_(SEL_USAGE));
		return EXIT_SUCCESS;
	}

	mode_t filetype = 0;
	int i, ifiletype = 0, isel_path = 0, new_sel = 0, err = 0, f = 0;

	char *dir = (char *)NULL, *pattern = (char *)NULL;
	char *sel_path = parse_sel_params(&args, &ifiletype, &filetype, &isel_path);

	if (sel_path)
		dir = check_sel_path(&sel_path);

	for (i = 1; args[i]; i++) {
		if (i == ifiletype || i == isel_path)
			continue;
		f++;

		pattern = (char *)NULL;
		if (check_regex(args[i]) == EXIT_SUCCESS) {
			pattern = args[i];
			if (*pattern == '!')
				pattern++;
		}

		if (!pattern)
			new_sel += select_filename(args[i], dir, &err);
		else
			new_sel += select_pattern(args[i], dir, filetype, &err);
	}

	if (f == 0)
		fprintf(stderr, _("Missing parameter. Try 's --help'\n"));
	free(dir);

	return print_sel_results(new_sel, sel_path, pattern, err);
}

void
show_sel_files(void)
{
	if (sel_n == 0) {
		puts(_("sel: No selected files"));
		return;
	}

	if (conf.clear_screen == 1)
		CLEAR;

	printf(_("%s%sSelection Box%s\n"), df_c, BOLD, df_c);

	int reset_pager = 0;

	putchar('\n');
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	size_t counter = 0;
	int t_lines = (int)w.ws_row;
	t_lines -= 2;
	size_t i;
	off_t total = 0;

	size_t t = tab_offset;
	tab_offset = 0;
	uint8_t epad = DIGINUM(sel_n);
	int status = 0;

	flags |= IN_SELBOX_SCREEN;
	for (i = 0; i < sel_n; i++) {
		/* if (pager && counter > (term_lines-2)) { */
		if (conf.pager && counter > (size_t)t_lines) {
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
				conf.pager = 0, reset_pager = 1; /* 'q' */
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

		printf("%s%*zu%s ", el_c, epad, i + 1, df_c);
		colors_list(sel_elements[i].name, NO_ELN, NO_PAD, PRINT_NEWLINE);

		printf(_("Calculating file size... ")); fflush(stdout);
		int ret = 0;
		off_t s = get_sel_file_size(i, &ret);
		if (ret != 0)
			status = ret;
		putchar('\r'); ERASE_TO_RIGHT; fflush(stdout);
		if (s != (off_t)-1)
			total += s;
	}
	flags &= ~IN_SELBOX_SCREEN;
	tab_offset = t;

	char *human_size = get_size_unit(total);
	char err[sizeof(xf_c) + 6]; *err = '\0';
	if (status != 0)
		snprintf(err, sizeof(err), "%s%c%s", xf_c, DU_ERR_CHAR, NC);

	printf(_("\n%s%sTotal size%s: %s%s\n"), df_c, BOLD, df_c, err, human_size);
	free(human_size);

	if (reset_pager == 1)
		conf.pager = 1;
}

static int
edit_selfile(void)
{
	if (!sel_file || !*sel_file || sel_n == 0)
		return EXIT_FAILURE;

	struct stat attr;
	if (stat(sel_file, &attr) == -1)
		goto ERROR;

	time_t mtime_old = (time_t)attr.st_mtime;

	if (open_file(sel_file) != EXIT_SUCCESS) {
		xerror("%s\n", _("sel: Could not open the selections file"));
		return EXIT_FAILURE;
	}

	/* Compare new and old modification times: if they match, nothing was modified */
	if (stat(sel_file, &attr) == -1)
		goto ERROR;

	if (mtime_old == (time_t)attr.st_mtime)
		return EXIT_SUCCESS;

	int ret = get_sel_files();
	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(_("%zu file(s) selected\n"), sel_n);
	return ret;

ERROR:
	xerror("sel: %s: %s\n", sel_file, strerror(errno));
	return EXIT_FAILURE;
}

static int
deselect_entries(char **desel_path, const size_t desel_n, int *err,
	const int desel_screen)
{
	int dn = (int)desel_n;
	int i = (int)desel_n;

	while (--i >= 0) {
		int j, k, desel_index = -1;

		if (!desel_path[i])
			continue;

		/* Search the sel array for the path of the entry to be deselected and
		 * store its index. */
		k = (int)sel_n;
		while (--k >= 0) {
			if (strcmp(sel_elements[k].name, desel_path[i]) == 0) {
				desel_index = k;
				break;
			}
		}

		if (desel_index == -1) {
			dn--;
			*err = 1;
			if (desel_screen == 0) {
				xerror(_("%s: %s: No such selected file\n"),
					PROGRAM_NAME, desel_path[i]);
			}
			continue;
		}

		/* Once the index was found, rearrange the sel array removing the
		 * deselected element (actually, moving each string after it to
		 * the previous position). */
		for (j = desel_index; j < (int)(sel_n - 1); j++) {
			size_t len = strlen(sel_elements[j + 1].name);
			sel_elements[j].name =
				(char *)xrealloc(sel_elements[j].name, (len + 1) * sizeof(char));

			xstrsncpy(sel_elements[j].name, sel_elements[j + 1].name, len);
			sel_elements[j].size = sel_elements[j + 1].size;
		}
	}

	/* Free the last DN entries from the old sel array. They won't
	 * be used anymore, since they contain the same value as the last
	 * non-deselected element due to the above array rearrangement. */
	for (i = 1; i <= dn; i++) {
		int sel_index = (int)sel_n - i;

		if (sel_index >= 0 && sel_elements[sel_index].name) {
			free(sel_elements[sel_index].name);
			sel_elements[sel_index].size = (off_t)UNSET;
		}
	}

	return dn;
}

static int
desel_entries(char **desel_elements, const size_t desel_n, const int desel_screen)
{
	char **desel_path = (char **)NULL;
	int i = (int)desel_n;

	/* Get entries to be deselected */
	if (desel_screen == 1) { /* Coming from the deselect screen */
		desel_path = (char **)xnmalloc(desel_n, sizeof(char *));
		while (--i >= 0) {
			int desel_int = atoi(desel_elements[i]);
			if (desel_int == INT_MIN) {
				desel_path[i] = (char *)NULL;
				continue;
			}
			desel_path[i] = savestring(sel_elements[desel_int - 1].name,
				strlen(sel_elements[desel_int - 1].name));
		}
	} else {
		desel_path = desel_elements;
	}

	int err = 0;
	int dn = deselect_entries(desel_path, desel_n, &err, desel_screen);

	/* Update the number of selected files according to the number of
	 * deselected files. */
	sel_n = (sel_n - (size_t)dn);

	if ((int)sel_n < 0)
		sel_n = 0;

	if (sel_n > 0) {
		sel_elements = (struct sel_t *)xrealloc(sel_elements,
			sel_n * sizeof(struct sel_t));
	}

	/* Deallocate local arrays. */
	i = (int)desel_n;
	while (--i >= 0) {
		if (desel_screen == 1)
			free(desel_path[i]);
		free(desel_elements[i]);
	}

	if (desel_screen == 1) {
		free(desel_path);
	} else if (err == 1) {
		print_reload_msg(_("%d file(s) deselected\n"), dn);
		print_reload_msg(_("%zu total selected file(s)\n"), sel_n);
	}
	free(desel_elements);

	if (err == 1) {
		save_sel();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* Deselect all selected files */
int
deselect_all(void)
{
	int i = (int)sel_n;
	while (--i >= 0) {
		free(sel_elements[i].name);
		sel_elements[i].name = (char *)NULL;
		sel_elements[i].size = (off_t)UNSET;
	}

	sel_n = 0;

	return save_sel();
}

/* Deselect files passed as parameters to the desel command
 * Returns zero on success or 1 on error. */
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

	if (desel_entries(ds, args_n, 0) == EXIT_FAILURE)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static char **
get_desel_input(size_t *n)
{
	printf(_("\n%sEnter 'q' to quit or 'e' to edit the selections file\n"
		"File(s) to be deselected (ex: 1 2-6, or *):\n"), df_c);

	char dp[(MAX_COLOR * 2) + 7];
	snprintf(dp, sizeof(dp), "\001%s\002>\001%s\002 ", mi_c, tx_c);
	char *line = NULL;
	while (!line)
		line = rl_no_hist(dp);

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
free_desel_elements(const size_t desel_n, char ***desel_elements)
{
	int i = (int)desel_n;
	while (--i >= 0)
		free((*desel_elements)[i]);
	free(*desel_elements);
}

static inline int
handle_alpha_entry(const int i, const size_t desel_n, char **desel_elements)
{
	if (*desel_elements[i] == 'e' && !desel_elements[i][1]) {
		free_desel_elements(desel_n, &desel_elements);
		return edit_selfile();
	}

	if (*desel_elements[i] == 'q' && !desel_elements[i][1]) {
		free_desel_elements(desel_n, &desel_elements);
		if (conf.autols == 1)
			reload_dirlist();
		return EXIT_SUCCESS;
	}

	if (*desel_elements[i] == '*' && !desel_elements[i][1]) {
		free_desel_elements(desel_n, &desel_elements);
		int exit_status = deselect_all();
		if (conf.autols == 1)
			reload_dirlist();
		return exit_status;
	}

	printf(_("desel: %s: Invalid entry\n"), desel_elements[i]);
	free_desel_elements(desel_n, &desel_elements);
	return EXIT_FAILURE;
}

static inline int
valid_desel_eln(const int i, const size_t desel_n, char **desel_elements)
{
	int n = atoi(desel_elements[i]);

	if (n <= 0 || (size_t)n > sel_n) {
		printf(_("desel: %s: Invalid ELN\n"), desel_elements[i]);
		free_desel_elements(desel_n, &desel_elements);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static inline int
end_deselect(const int err, char ***args)
{
	int exit_status = EXIT_SUCCESS;

	size_t argsbk = args_n, desel_files = 0;
	if (args_n > 0) {
		size_t i;
		for (i = 1; i <= args_n; i++) {
			free((*args)[i]);
			(*args)[i] = (char *)NULL;
			desel_files++;
		}
		args_n = 0;
	}

	if (!err && save_sel() != 0)
		exit_status = EXIT_FAILURE;

	get_sel_files();

	/* There is still some selected file and we are in the desel
	 * screen: reload this screen */
	if (sel_n && argsbk == 0 && deselect(*args) != 0)
		exit_status = EXIT_FAILURE;

	if (err)
		return EXIT_FAILURE;

	if (conf.autols == 1 && exit_status == EXIT_SUCCESS)
		reload_dirlist();

	if (argsbk > 0) {
		print_reload_msg(_("%zu file(s) deselected\n"), desel_files);
		print_reload_msg(_("%zu total selected file(s)\n"), sel_n);
	} else {
		print_reload_msg(_("%zu selected file(s)\n"), sel_n);
	}

	return exit_status;
}

int
deselect(char **args)
{
	if (!args)
		return EXIT_FAILURE;

	if (sel_n == 0) {
		puts(_("desel: No selected files"));
		return EXIT_SUCCESS;
	}

	int err = 0;

	if (args[1] && *args[1]) {
		if (strcmp(args[1], "*") == 0 || strcmp(args[1], "a") == 0
		|| strcmp(args[1], "all") == 0) {
			size_t n = sel_n;
			int ret = deselect_all();
			if (conf.autols == 1)
				reload_dirlist();
			if (ret == EXIT_SUCCESS) {
				print_reload_msg(_("%zu file(s) deselected\n"), n);
				print_reload_msg(_("0 total selected file(s)\n"));
			}
			return ret;
		} else {
			err = deselect_from_args(args);
			return end_deselect(err, &args);
		}
	}

	print_selected_files();

	size_t desel_n = 0;
	char **desel_elements = get_desel_input(&desel_n);

	int i = (int)desel_n;
	while (--i >= 0) {
		if (!is_number(desel_elements[i])) /* Only for 'q', 'e', and '*' */
			return handle_alpha_entry(i, desel_n, desel_elements);

		if (valid_desel_eln(i, desel_n, desel_elements) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}

	desel_entries(desel_elements, desel_n, 1);
	return end_deselect(err, &args);
}
