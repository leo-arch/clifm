/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* selection.c -- files selection functions */

#include "helpers.h"

#include <errno.h>
#include <readline/tilde.h>
#include <string.h>
#include <unistd.h>
#if defined(__linux__) || defined(__HAIKU__) || defined(__APPLE__) \
|| defined(__sun) || defined(__CYGWIN__)
# ifdef __TINYC__
/* Silence a tcc warning. We don't use CTRL anyway */
#  undef CTRL
# endif /* __TINYC__ */
# include <sys/ioctl.h>
#endif /* __linux__ || __HAIKU__ || __APPLE__ || __sun || __CYGWIN__ */

#ifdef __sun
# include <sys/termios.h> /* TIOCGWINSZ */
#endif /* __sun */

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "file_operations.h"
#include "init.h"
#include "listing.h"
#include "messages.h"
#include "misc.h"
#include "navigation.h"
#include "properties.h" /* get_size_color() */
#include "readline.h"
#include "selection.h"
#include "selset.h" /* devino_set_contains/devino_set_insert */
#include "sort.h"
#include "xdu.h" /* dir_size() */
#include "xmatch.h" /* xglob(), xregex() */

/* Save selected elements into a tmp file. Returns 1 on success or 0
 * on error. This function allows the user to work with multiple
 * instances of the program: they can select some files in the
 * first instance and then execute a second one to operate on those
 * files as they wish. */
int
save_sel(void)
{
	if (selfile_ok == 0 || !sel_file)
		return (xargs.stealth_mode == 1 ? FUNC_SUCCESS : FUNC_FAILURE);

	if (sel_n == 0) {
		if (unlinkat(XAT_FDCWD, sel_file, 0) == -1 && errno != ENOENT) {
			xerror("sel: '%s': %s\n", sel_file, strerror(errno));
			return FUNC_FAILURE;
		}
		return FUNC_SUCCESS;
	}

	int fd = -1;
	FILE *fp = open_fwrite(sel_file, &fd);
	if (!fp) {
		xerror("sel: '%s': %s\n", sel_file, strerror(errno));
		return FUNC_FAILURE;
	}

	size_t buf_len = PATH_MAX * 3 + 7 + 1;
	char *buf = xnmalloc(buf_len, sizeof(char));
	for (size_t i = 0; i < sel_n; i++) {
		const char *name = sel_elements[i].name;
		if (!name)
			continue;

		/* At most, each char will become %XX, plus "file://" and a NUL byte. */
		size_t need = strlen(name) * 3 + 7 + 1;
		if (need > buf_len) {
			while (buf_len < need) buf_len *= 2;
			buf = xnrealloc(buf, buf_len, sizeof(char));
		}

		char *enc = url_encode(sel_elements[i].name, 1, buf);
		if (enc) {
			fputs(enc, fp);
			fputc('\n', fp);
			/* enc points to buf, no need to free(). */
		}
	}

	free(buf);
	fclose(fp);
	return FUNC_SUCCESS;
}

int
select_file(char *file)
{
	if (!file || !*file)
		return 0;

	if (sel_n == MAX_SEL) {
		xerror(_("sel: Cannot select any more files\n"));
		return 0;
	}

	static char buf[PATH_MAX + 1];
	char *tfile = file;
	struct stat a;
	int exists = 0, new_sel = 0;

	const size_t flen = strlen(file);
	if (flen > 1 && file[flen - 1] == '/')
		file[flen - 1] = '\0';

	if (lstat(file, &a) == -1) {
		xerror(_("sel: Cannot select file '%s': %s\n"), file, strerror(errno));
		return 0;
	}

	/* If we are in a virtual directory, dereference symlinks */
	if (virtual_dir == 1 && S_ISLNK(a.st_mode)
	&& is_file_in_cwd(file)) {
		*buf = '\0';
		const ssize_t ret = xreadlink(XAT_FDCWD, file, buf, sizeof(buf));
		if (ret == -1) {
			xerror(_("sel: Cannot select file '%s': %s\n"),
				file, strerror(errno));
			return 0;
		}
		tfile = buf;
		if (lstat(tfile, &a) == -1) {
			xerror(_("sel: Cannot select file '%s': %s\n"),
				file, strerror(errno));
			return 0;
		}
	}

	if (devino_set_contains(&sel_set, a.st_dev, a.st_ino)) {
		/* Only scan for names in case of dev/ino matches (mostly hardlinks
		 * or names in different devices). */
		filesn_t j = (filesn_t)sel_n;
		while (--j >= 0) {
			if (*tfile == *sel_elements[j].name
			&& strcmp(sel_elements[j].name, tfile) == 0) {
				exists = 1;
				break;
			}
		}
	}

	if (exists == 0) {
		sel_elements = xnrealloc(sel_elements, sel_n + 2, sizeof(struct sel_t));
		sel_elements[sel_n].name = strdup(tfile);
		sel_elements[sel_n++].size = (off_t)UNSET;

		sel_elements[sel_n].name = NULL;
		sel_elements[sel_n].size = (off_t)UNSET;

		new_sel++;
		devino_set_insert(&sel_set, a.st_dev, a.st_ino);
	} else {
		xerror(_("sel: '%s': Already selected\n"), tfile);
	}

	return new_sel;
}

static int
select_matches(const struct fileinfo *finfo, const int matches)
{
	int i = matches;
	int new_sel = 0;

	while (--i >= 0) {
		char *name = finfo[i].name;
		if (!name)
			continue;

		if (*name == '/') { /* Absolute path */
			new_sel += select_file(name);
			continue;
		}

		/* Relative path */
		char *tmp = NULL;

		if (*workspaces[cur_ws].path == '/'
		&& !*(workspaces[cur_ws].path + 1)) {
			/* CWD is root */
			const size_t tmp_len = strlen(name) + 2;
			tmp = xnmalloc(tmp_len, sizeof(char));
			snprintf(tmp, tmp_len, "/%s", name);
		} else {
			const size_t tmp_len = strlen(workspaces[cur_ws].path)
				+ strlen(name) + 2;
			tmp = xnmalloc(tmp_len, sizeof(char));
			snprintf(tmp, tmp_len, "%s/%s",
				workspaces[cur_ws].path, name);
		}

		new_sel += select_file(tmp);
		free(tmp);
	}

	return new_sel;
}

/* Err removing the reference to find(1), used for recusrive selection. */
static void
xglob_err(const char *pattern)
{
	const char *p = pattern;
	const int is_find = (*p == '$' && p[1] == '('
	&& (strncmp(p + 2, "find ", 5) == 0 || strncmp(p + 2, "gfind ", 6) == 0));

	if (is_find == 0) {
		xerror(_("sel: '%s': No matches found\n"), pattern);
		return;
	}

	const char *lead_quote = strchr(p, '\'');
	const char *end_quote = lead_quote ? strrchr(lead_quote + 1, '\'') : NULL;
	if (!lead_quote || !end_quote) {
		xerror(_("sel: '%s': No matches found\n"), pattern);
		return;
	}

	const size_t pat_len = (size_t)(end_quote - lead_quote) + 1;
	char *buf = xnmalloc(pat_len + 1, sizeof(char));
	memcpy(buf, lead_quote, pat_len);
	buf[pat_len] = '\0';
	xerror(_("sel: %s: No matches found\n"), buf);
	free(buf);
}

static int
sel_glob(char *str, const char *sel_path, const mode_t filetype)
{
	if (!str || !*str)
		return (-1);

	xglob_t g = {0};
	char *pattern = str;

	char *gl_path = NULL;
	char *spath = (char *)sel_path;
	char *s = !sel_path ? strrchr(pattern, '/') : NULL;
	if (s && s[1]) {
		*s = '\0';
		if (*pattern == '~')
			gl_path = tilde_expand(pattern);
		else
			gl_path = strdup(pattern);
		spath = gl_path;
		*s = '/';
		pattern = s + 1;
	}

	int ret = xglob(spath ? spath : ".", pattern, &g, -1, filetype, 0, 0);
	if (ret != 0) {
		xglob_err(pattern);
		xglobfree(&g);
		free(gl_path);
		return (-1);
	}

	int new_sel = select_matches(g.gl_finfo, (int)g.gl_matches);

	xglobfree(&g);
	free(gl_path);

	return new_sel;
}

static int
sel_regex(char *str, const char *sel_path, const mode_t filetype)
{
	if (!str || !*str)
		return (-1);

	xregex_t rbuf = {0};
	char *pattern = str;

	char *re_path = NULL;
	char *spath = (char *)sel_path;
	char *s = !sel_path ? strrchr(pattern, '/') : NULL;
	if (s && s[1]) {
		*s = '\0';
		if (*pattern == '~')
			re_path = tilde_expand(pattern);
		else
			re_path = strdup(pattern);
		spath = re_path;
		*s = '/';
		pattern = s + 1;
	}

	int ret = xregex(spath ? spath : ".", pattern, &rbuf, -1, filetype, 0, 0);
	if (ret != 0) {
		xerror(_("sel: '%s': No matches found\n"), pattern);
		xregfree(&rbuf);
		free(re_path);
		return (-1);
	}

	int new_sel = select_matches(rbuf.re_finfo, (int)rbuf.re_matches);

	xregfree(&rbuf);
	free(re_path);

	return new_sel;
}

/* Convert file type into a macro that can be decoded by stat(3).
 * If file type is specified, matches will be checked against
 * this value. */
static int
convert_filetype(mode_t *filetype)
{
	switch (*filetype) {
	case 'b': *filetype = DT_BLK; break;
	case 'c': *filetype = DT_CHR; break;
	case 'd': *filetype = DT_DIR; break;
	case 'f': *filetype = DT_REG; break;
	case 'l': *filetype = DT_LNK; break;
	case 'p': *filetype = DT_FIFO; break;
	case 's': *filetype = DT_SOCK; break;
#ifdef SOLARIS_DOORS
	case 'O': *filetype = DT_DOOR; break;
	case 'P': *filetype = DT_PORT; break;
#endif /* SOLARIS_DOORS */
	default:
		xerror(_("sel: '%c': Unrecognized file type.\n"
			"Try 'sel --help' for more information.\n"), (char)*filetype);
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static char *
parse_sel_params(char ***args, int *ifiletype, mode_t *filetype, int *isel_path)
{
	char *sel_path = NULL;
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
				xerror("sel: '%s': Cannot expand tilde\n", (*args)[i]);
				*filetype = (mode_t)-1;
				return NULL;
			}

			free((*args)[i]);
			(*args)[i] = exp_path;
		}
	}

	if (*filetype != 0 && convert_filetype(filetype) == FUNC_FAILURE) {
		*filetype = (mode_t)-1;
		return NULL;
	}

	return sel_path;
}

static char *
build_sel_path(char *sel_path)
{
	char tmpdir[PATH_MAX + 1];
	xstrsncpy(tmpdir, sel_path, sizeof(tmpdir));

	if (*sel_path == '.' && xrealpath(sel_path, tmpdir) == NULL) {
		xerror("sel: '%s': %s\n", sel_path, strerror(errno));
		return NULL;
	}

	if (*sel_path == '~') {
		char *exp_path = tilde_expand(sel_path);
		if (!exp_path) {
			xerror("%s\n", _("sel: Error expanding path"));
			errno = 1;
			return NULL;
		}

		xstrsncpy(tmpdir, exp_path, sizeof(tmpdir));
		free(exp_path);
	}

	char *dir = NULL;
	if (*tmpdir != '/') {
		const size_t dir_len = strlen(workspaces[cur_ws].path)
			+ strnlen(tmpdir, sizeof(tmpdir)) + 2;
		dir = xnmalloc(dir_len, sizeof(char));
		snprintf(dir, dir_len, "%s/%s", workspaces[cur_ws].path, tmpdir);
	} else {
		dir = savestring(tmpdir, strnlen(tmpdir, sizeof(tmpdir)));
	}

	return dir;
}

static char *
check_sel_path(char **sel_path)
{
	const size_t len = strlen(*sel_path);
	if (len > 0 && (*sel_path)[len - 1] == '/')
		(*sel_path)[len - 1] = '\0';

	if (strchr(*sel_path, '\\')) {
		char *deq = unescape_str(*sel_path);
		if (deq) {
			xstrsncpy(*sel_path, deq, strlen(deq) + 1);
			free(deq);
		}
	}

	errno = 0;
	char *dir = build_sel_path(*sel_path);

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

	if (S_ISDIR(attr.st_mode)) {
		fputs(dn_c, stdout);
		fputs(_("Calculating file size... "), stdout); fflush(stdout);
		fputs(df_c, stdout);
#ifdef USE_DU1
		sel_elements[i].size = (off_t)(dir_size(sel_elements[i].name,
			0, status) * (xargs.si == 1 ? 1000 : 1024));
#else
		sel_elements[i].size = dir_size(sel_elements[i].name, 0, status);
#endif /* USE_DU1 */
		putchar('\r'); ERASE_TO_RIGHT; fflush(stdout);
	} else {
		sel_elements[i].size = (off_t)FILE_SIZE(attr);
	}

	return sel_elements[i].size;
}

static int
print_sel_results(const int new_sel, const char *pattern, const int error)
{
	if (new_sel > 0 && xargs.stealth_mode != 1 && sel_file
	&& save_sel() != FUNC_SUCCESS) {
		err('e', PRINT_PROMPT, _("sel: Error writing files "
			"into the selections file\n"));
		return FUNC_FAILURE;
	}

	if (new_sel <= 0) {
		if (pattern && error == 0)
			fputs(_("sel: No matches found\n"), stderr);
		return FUNC_FAILURE;
	}

	get_sel_files();
	if (sel_n == 0) {
		fputs(_("sel: No matches found\n"), stderr);
		return FUNC_FAILURE;
	}

	if (conf.autols == 1) {
		if (error > 0)
			press_any_key_to_continue(0);
		reload_dirlist();
	}

	print_reload_msg(SET_SUCCESS_PTR, xs_cb,
		_("%d %s selected\n"), new_sel, FILE_STR(new_sel));
	print_reload_msg(NULL, NULL, _("%zu total selected %s\n"), sel_n,
		FILE_STR(sel_n));

	return FUNC_SUCCESS;
}

static char *
build_sel_filename(const char *dir, const char *name)
{
	char *f = NULL;
	size_t flen = 0;

	if (!dir) {
		if (*workspaces[cur_ws].path == '/'
		&& !*(workspaces[cur_ws].path + 1)) {
			flen = strlen(name) + 2;
			f = xnmalloc(flen, sizeof(char));
			snprintf(f, flen, "/%s", name);

			return f;
		}

		flen = strlen(workspaces[cur_ws].path) + strlen(name) + 2;
		f = xnmalloc(flen, sizeof(char));
		snprintf(f, flen, "%s/%s", workspaces[cur_ws].path, name);

		return f;
	}

	flen = strlen(dir) + strlen(name) + 2;
	f = xnmalloc(flen, sizeof(char));
	snprintf(f, flen, "%s/%s", dir, name);

	return f;
}

static int
select_filename(char *arg, const char *dir, int *errors, const int unescape)
{
	int new_sel = 0;

	if (unescape == 1 && strchr(arg, '\\')) {
		char *deq_str = unescape_str(arg);
		if (deq_str) {
			xstrsncpy(arg, deq_str, strlen(deq_str) + 1);
			free(deq_str);
		}
	}

	char *name = arg;
	if (*name == '.' && *(name + 1) == '/')
		name += 2;

	if (*arg != '/') {
		char *tmp = build_sel_filename(dir, name);
		struct stat attr;
		if (lstat(tmp, &attr) == -1) {
			xerror("sel: '%s': %s\n", arg, strerror(errno));
			(*errors)++;
		} else {
			const int r = select_file(tmp);
			new_sel += r;
			if (r == 0)
				(*errors)++;
		}
		free(tmp);
		return new_sel;
	}

	struct stat a;
	if (lstat(arg, &a) == -1) {
		xerror("sel: '%s': %s\n", name, strerror(errno));
		(*errors)++;
	} else {
		const int r = select_file(name);
		new_sel += r;
		if (r == 0)
			(*errors)++;
	}

	return new_sel;
}

static int
select_pattern(char *arg, const char *dir, const mode_t filetype,
	int *err, const int matching_style)
{
	int ret = 0;
	if (matching_style == GLOB_MATCH) {
		ret = sel_glob(arg, dir, filetype);
	} else if (matching_style == REGEX_MATCH) {
		ret = sel_regex(arg, dir, filetype);
	} else {
		if (conf.matching_style == GLOB_MATCH)
			ret = sel_glob(arg, dir, filetype);
		else
			ret = sel_regex(arg, dir, filetype);
	}

	if (ret == -1) /* Error message already printed. */
		(*err)++;

	return ret;
}

/* Reconstruct the list of selections excluding those located in the current
 * directory. */
static void
deselect_files_in_cwd(void)
{
	/* Free data about selected files. It will be reconstructed later
	 * by get_sel_files(). */
	free(sel_devino);
	sel_devino = NULL;

	struct sel_t *sel = NULL;
	size_t n = 0;

	for (size_t i = 0; i < sel_n; i++) {
		if (!is_file_in_cwd(sel_elements[i].name)) {
			sel = xnrealloc(sel, n + 2, sizeof(sel[0]));
			sel[n].name =
				savestring(sel_elements[i].name, strlen(sel_elements[i].name));
			sel[n++].size = (off_t)UNSET;
		}
	}

	if (n == 0) { /* All selections are in the current directory. */
		deselect_all();
		return;
	}

	sel[n].name = NULL;
	sel[n].size = (off_t)UNSET;

	for (size_t i = 0; i < sel_n; i++)
		free(sel_elements[i].name);
	free(sel_elements);
	sel_n = n;
	sel_elements = sel;
}

static void
print_inversion_results(const int new_sel, const int desel, const int errors)
{
	if (new_sel > 0) {
		print_sel_results(new_sel, NULL, errors);
		return;
	}

	save_sel();

	if (errors == 0 && conf.autols == 1)
		reload_dirlist();

	print_reload_msg(SET_SUCCESS_PTR, xs_cb,
		_("%zu %s deselected\n"), desel, FILE_STR(desel));
	print_reload_msg(NULL, NULL, _("%zu total selected %s\n"), sel_n,
		FILE_STR(sel_n));
}

/* Invert the list of selections in the current directory. */
static int
invert_selection(void)
{
	if (sel_n > 0)
		deselect_files_in_cwd();

	int new_sel = 0;
	int errors = 0;
	int desel = 0;

	for (filesn_t i = 0; i < g_files_num; i++) {
		if (file_info[i].sel == 0) {
			new_sel += select_filename(file_info[i].name, NULL, &errors, 0);
			file_info[i].sel = 1;
		} else {
			file_info[i].sel = 0;
			desel++;
		}
	}

	print_inversion_results(new_sel, desel, errors);

	return (errors != 0);
}

static char *
set_pattern_style(char *pattern, int *style)
{
	if (!pattern || !*pattern)
		return pattern;

	char *p = pattern;
	if (IS_GLOB_PREFIX(p)) {
		if (style) *style = GLOB_MATCH;
		return p + 3;
	} else {
		if (IS_REGEX_PREFIX(p)) {
			if (style) *style = REGEX_MATCH;
			return p + 3;
		}
	}

	return p;
}

int
sel_function(char **args)
{
	if (!args) return FUNC_FAILURE;

	if (!args[1] || IS_HELP(args[1])) {
		puts(_(SEL_USAGE));
		return FUNC_SUCCESS;
	}

	if (*args[1] == '-' && (strcmp(args[1], "-i") == 0
	|| strcmp(args[1], "--invert") == 0))
		return invert_selection();

	mode_t filetype = 0;
	int i, ifiletype = 0, isel_path = 0, new_sel = 0, error = 0, f = 0;

	char *dir = NULL;
	char *pattern = NULL;
	char *sel_path = parse_sel_params(&args, &ifiletype, &filetype, &isel_path);

	if (sel_path) {
		dir = check_sel_path(&sel_path);
		if (!dir || !*dir) {
			free(dir);
			return errno;
		}
	} else {
		if (filetype == (mode_t)-1) { /* parse_sel_params() error */
			free(dir);
			return FUNC_FAILURE;
		}
	}

	for (i = 1; args[i]; i++) {
		if (i == ifiletype || i == isel_path)
			continue;
		f++;

		pattern = NULL;
		int matching_style = UNSET;
		if (check_regex(args[i]) == FUNC_SUCCESS)
			pattern = set_pattern_style(args[i], &matching_style);

		if (!pattern) {
			new_sel += select_filename(args[i], dir, &error, 1);
		} else {
			new_sel +=
				select_pattern(pattern, dir, filetype, &error, matching_style);
		}
	}

	if (f == 0)
		fputs(_("Missing parameter. Try 's --help'\n"), stderr);
	free(dir);

	return print_sel_results(new_sel, pattern, error);
}

void
list_selected_files(const int clear_screen)
{
	if (sel_n == 0) {
		puts(_("sel: No selected files"));
		return;
	}

	if (conf.clear_screen > 0 && clear_screen)
		CLEAR;

	if (conf.pager == 0)
		HIDE_CURSOR;

	printf(_("%s%sSelection Box%s\n"), df_c, BOLD, df_c);

	int reset_pager = 0;

	putchar('\n');

	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
		w.ws_row = 24;

	const int t_lines = (w.ws_row > 0 ? (int)w.ws_row : DEFAULT_WIN_ROWS) - 2;

	size_t counter = 0;
	size_t i;
	off_t total = 0;

	const size_t t = tab_offset;
	tab_offset = 0;
	const uint8_t epad = DIGINUM(sel_n);
	int status = 0;

	flags |= IN_SELBOX_SCREEN;
	for (i = 0; i < sel_n; i++) {
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
			/* Stop paging (and set a flag to reenable the pager later) */
			case 99: /* fallthrough */  /* 'c' */
			case 112: /* fallthrough */ /* 'p' */
			case 113:
				conf.pager = 0; reset_pager = 1; /* 'q' */
				break;
			/* If another key is pressed, go back one position.
			 * Otherwise, some filenames won't be listed.*/
			default:
				i--;
				continue;
			}
		}

		counter++;

		printf("%s%*zu%s ", el_c, epad, i + 1, df_c);
		colors_list(sel_elements[i].name, NO_ELN, NO_PAD, PRINT_NEWLINE, 1);

		int ret = 0;
		const off_t s = get_sel_file_size(i, &ret);
		if (ret != 0)
			status = ret;

		if (s != (off_t)-1)
			total += s;
	}
	flags &= ~IN_SELBOX_SCREEN;
	tab_offset = t;

	const char *human_size = construct_human_size(total);
	char err_str[sizeof(xf_cb) + 6]; *err_str = '\0';
	if (status != 0)
		snprintf(err_str, sizeof(err_str), "%s%c%s", xf_cb, DU_ERR_CHAR, NC);

	char s[MAX_SHADE_LEN]; *s = '\0';
	if (conf.colorize == 1)
		get_color_size(total, s, sizeof(s));

	printf(_("\n%sTotal size: %s%s%s%s\n"), df_c, err_str, s, human_size, df_c);

	if (conf.pager == 0)
		UNHIDE_CURSOR;

	if (reset_pager == 1)
		conf.pager = 1;
}

static int
edit_selfile(void)
{
	if (!sel_file || !*sel_file || sel_n == 0)
		return FUNC_FAILURE;

	struct stat attr;
	if (stat(sel_file, &attr) == -1)
		goto ERROR;

	const time_t prev_mtime = attr.st_mtime;

	if (open_file(sel_file) != FUNC_SUCCESS) {
		xerror("%s\n", _("sel: Cannot open the selections file"));
		return FUNC_FAILURE;
	}

	/* Compare new and old modification times: if they match, nothing was modified */
	if (stat(sel_file, &attr) == -1)
		goto ERROR;

	if (prev_mtime == attr.st_mtime)
		return FUNC_SUCCESS;

	const int ret = get_sel_files();
	if (conf.autols == 1)
		reload_dirlist();

	print_reload_msg(SET_SUCCESS_PTR, xs_cb, _("%zu %s selected\n"),
		sel_n, FILE_STR(sel_n));
	return ret;

ERROR:
	xerror("sel: '%s': %s\n", sel_file, strerror(errno));
	return FUNC_FAILURE;
}

static int
deselect_entries(char **desel_path, const size_t desel_n, int *error,
	const int desel_screen)
{
	int dn = (int)desel_n;
	int i = (int)desel_n;

	while (--i >= 0) {
		int j, desel_index = -1;

		if (!desel_path[i])
			continue;

		/* Search the sel array for the path of the entry to be deselected and
		 * store its index. */
		size_t k = sel_n;
		for (; k-- > 0;) {
			if (strcmp(sel_elements[k].name, desel_path[i]) == 0) {
				desel_index = k > INT_MAX ? INT_MAX : (int)k;
				break;
			}
		}

		if (desel_index == -1) {
			dn--;
			*error = 1;
			if (desel_screen == 0) {
				xerror(_("%s: '%s': No such selected file\n"),
					PROGRAM_NAME, desel_path[i]);
			}
			continue;
		}

		/* Once the index was found, rearrange the sel array removing the
		 * deselected element (actually, moving each string after it to
		 * the previous position). */
		for (j = desel_index; j < (int)(sel_n - 1); j++) {
			const size_t len = strlen(sel_elements[j + 1].name);
			sel_elements[j].name =
				xnrealloc(sel_elements[j].name, len + 1, sizeof(char));

			xstrsncpy(sel_elements[j].name, sel_elements[j + 1].name, len + 1);
			sel_elements[j].size = sel_elements[j + 1].size;
		}
	}

	/* Free the last DN entries from the old sel array. They won't
	 * be used anymore, since they contain the same value as the last
	 * non-deselected element due to the above array rearrangement. */
	for (i = 1; i <= dn; i++) {
		const int sel_index = (int)sel_n - i;

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
	char **desel_path = NULL;
	size_t i = desel_n;

	/* Get entries to be deselected */
	if (desel_screen == 1) { /* Coming from the deselect screen */
		desel_path = xnmalloc(desel_n, sizeof(char *));
		for (; i-- > 0;) {
			const int desel_int = xatoi(desel_elements[i]);
			if (desel_int == INT_MIN) {
				desel_path[i] = NULL;
				continue;
			}
			desel_path[i] = savestring(sel_elements[desel_int - 1].name,
				strlen(sel_elements[desel_int - 1].name));
		}
	} else {
		desel_path = desel_elements;
	}

	int error = 0;
	const int dn = deselect_entries(desel_path, desel_n, &error, desel_screen);

	/* Update the number of selected files according to the number of
	 * deselected files. */
	sel_n = (sel_n - (size_t)dn);

	if ((int)sel_n < 0)
		sel_n = 0;

	if (sel_n > 0)
		sel_elements = xnrealloc(sel_elements, sel_n, sizeof(struct sel_t));

	/* Deallocate local arrays. */
	i = desel_n;
	for (; i-- > 0;) {
		if (desel_screen == 1)
			free(desel_path[i]);
		free(desel_elements[i]);
	}

	if (desel_screen == 1) {
		free(desel_path);
	} else {
		if (error == 1) {
			print_reload_msg(SET_SUCCESS_PTR, xs_cb,
				_("%d %s deselected\n"), dn, FILE_STR(dn));
			print_reload_msg(NULL, NULL,
				_("%zu total selected %s\n"), sel_n, FILE_STR(sel_n));
		}
	}
	free(desel_elements);

	if (error == 1) {
		save_sel();
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

/* Deselect all selected files */
int
deselect_all(void)
{
	size_t i = sel_n;
	for (; i-- > 0;) {
		free(sel_elements[i].name);
		sel_elements[i].name = NULL;
		sel_elements[i].size = (off_t)UNSET;
	}

	sel_n = 0;

	return save_sel();
}

/* Deselect files passed as parameters to the desel command
 * Returns zero on success or 1 on error. */
static int
deselect_from_args(char **args)
{
	char **ds = xnmalloc(args_n + 1, sizeof(char *));
	size_t i, j = 0;

	for (i = 1; i <= args_n; i++) {
		char *ptr = normalize_path(args[i], strlen(args[i]));
		if (!ptr)
			continue;

		ds[j++] = savestring(ptr, strlen(ptr));

		free(ptr);
	}

	ds[j] = NULL;

	return (desel_entries(ds, j, 0) == FUNC_FAILURE
		? FUNC_FAILURE : FUNC_SUCCESS);
}

/* Desel screen: take user input and return an array of input substrings,
 * updating N to the number of substrings. */
static char **
get_desel_input(size_t *n)
{
	printf(_("\n%sEnter 'q' to quit or 'e' to edit the selections file\n"
		"File(s) to be deselected (e.g.: 1 2-6, or *):\n"), df_c);

	char dp[(MAX_COLOR * 2) + 7];
	snprintf(dp, sizeof(dp), "\001%s\002>\001%s\002 ", mi_c, tx_c);
	char *line = NULL;
	while (!line)
		line = rl_no_hist(dp, 0);

	/* get_substr() will try to expand ranges, in which case a range with
	 * no second field is expanded from the value of the first field to the
	 * ELN of the last listed file in the CWD (value taken from the global
	 * variable FILES). But, since we are deselecting files, FILES must be the
	 * number of selected files (SEL_N), and not that of listed files in
	 * the CWD. */
	const filesn_t files_bk = g_files_num;
	g_files_num = (filesn_t)sel_n;
	char **entries = get_substr(line, ' ', 1);
	g_files_num = files_bk;
	free(line);

	if (!entries)
		return NULL;

	for (*n = 0; entries[*n]; (*n)++);

	return entries;
}

static inline void
free_desel_elements(const size_t desel_n, char ***desel_elements)
{
	size_t i = desel_n;
	for (; i-- > 0;)
		free((*desel_elements)[i]);
	free(*desel_elements);
}

static int
handle_alpha_entry(const size_t i, const size_t desel_n, char **desel_elements)
{
	if (*desel_elements[i] == 'e' && !desel_elements[i][1]) {
		free_desel_elements(desel_n, &desel_elements);
		return edit_selfile();
	}

	if (*desel_elements[i] == 'q' && !desel_elements[i][1]) {
		free_desel_elements(desel_n, &desel_elements);
		if (conf.autols == 1)
			reload_dirlist();
		return FUNC_SUCCESS;
	}

	if (*desel_elements[i] == '*' && !desel_elements[i][1]) {
		free_desel_elements(desel_n, &desel_elements);
		const int exit_status = deselect_all();
		if (conf.autols == 1)
			reload_dirlist();
		return exit_status;
	}

	printf(_("desel: '%s': Invalid entry\n"), desel_elements[i]);
	free_desel_elements(desel_n, &desel_elements);
	return FUNC_FAILURE;
}

static int
valid_desel_eln(const size_t i, const size_t desel_n, char **desel_elements)
{
	const int n = xatoi(desel_elements[i]);

	if (n <= 0 || (size_t)n > sel_n) {
		printf(_("desel: %s: Invalid ELN\n"), desel_elements[i]);
		free_desel_elements(desel_n, &desel_elements);
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static int
end_deselect(const int err, char ***args)
{
	int exit_status = FUNC_SUCCESS;
	const size_t argsbk = args_n;
	size_t desel_files = 0;

	if (args_n > 0) {
		size_t i;
		for (i = 1; i <= args_n; i++) {
			free((*args)[i]);
			(*args)[i] = NULL;
			desel_files++;
		}
		args_n = 0;
	}

	if (!err && save_sel() != 0)
		exit_status = FUNC_FAILURE;

	get_sel_files();

	/* There is still some selected file and we are in the desel
	 * screen: reload this screen. */
	if (sel_n > 0 && argsbk == 0)
		return deselect(*args);

	if (err)
		return FUNC_FAILURE;

	if (conf.autols == 1 && exit_status == FUNC_SUCCESS)
		reload_dirlist();

	if (argsbk > 0) {
		print_reload_msg(SET_SUCCESS_PTR, xs_cb,
			_("%zu %s deselected\n"), desel_files, FILE_STR(desel_files));
		print_reload_msg(NULL, NULL, _("%zu total selected %s\n"),
			sel_n, FILE_STR(sel_n));
	} else {
		print_reload_msg(NULL, NULL, _("%zu selected %s\n"),
			sel_n, FILE_STR(sel_n));
	}

	return exit_status;
}

static int
handle_desel_args(char **args)
{
	if (strcmp(args[1], "*") == 0 || strcmp(args[1], "a") == 0
	|| strcmp(args[1], "all") == 0) {
		const size_t n = sel_n;
		const int ret = deselect_all();

		if (conf.autols == 1)
			reload_dirlist();

		if (ret == FUNC_SUCCESS) {
			print_reload_msg(SET_SUCCESS_PTR, xs_cb,
				_("%zu %s deselected\n"), n, FILE_STR(n));
			print_reload_msg(NULL, NULL, _("0 total selected files\n"));
		}
		return ret;

	} else {
		const int error = deselect_from_args(args);
		return end_deselect(error, &args);
	}
}

int
deselect(char **args)
{
	if (!args)
		return FUNC_FAILURE;

	if (sel_n == 0) {
		puts(_("desel: No selected files"));
		return FUNC_SUCCESS;
	}

	if (args[1] && *args[1])
		return handle_desel_args(args);

	/* No arguments: print the deselection screen. */
	list_selected_files(1);

	size_t desel_n = 0;
	char **desel_elements = get_desel_input(&desel_n);

	size_t i = desel_n;
	for (; i-- > 0;) {
		if (!is_number(desel_elements[i])) /* Only for 'q', 'e', and '*' */
			return handle_alpha_entry(i, desel_n, desel_elements);

		if (valid_desel_eln(i, desel_n, desel_elements) == FUNC_FAILURE)
			return FUNC_FAILURE;
	}

	desel_entries(desel_elements, desel_n, 1);
	return end_deselect(0, &args);
}
