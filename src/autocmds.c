/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* autocmds.c -- run cmds on a per-directory basis */

#include "helpers.h"

#include <fnmatch.h>  /* fnmatch */
#include <string.h>
#include <readline/tilde.h>

#include "aux.h"
#include "checks.h"   /* is_number */
#include "colors.h"   /* set_colors */
#include "listing.h"  /* reload_dirlist */
#include "messages.h" /* AUTO_USAGE macro */
#include "misc.h"     /* xerror (err) */
#include "sanitize.h" /* sanitize_cmd */
#include "sort.h"     /* num_to_sort_name */
#include "spawn.h"    /* launch_execl */

/* Some options (mf and mn) take UNSET (-1) as a valid value. Let's use
 * this macro to mark them as unset (no value). */
#define AC_UNSET (-2)

/* Size of the buffer used to store the list of autocommand options */
#define AC_BUF_SIZE PATH_MAX

/* The opts struct contains option values previous to any autocommand call */
void
reset_opts(void)
{
	opts.color_scheme = cur_cscheme;
	opts.file_counter = conf.file_counter;
	opts.light_mode = conf.light_mode;
	opts.max_files = conf.max_files;
	opts.full_dir_size = conf.full_dir_size;
	opts.long_view = conf.long_view;
	opts.show_hidden = conf.show_hidden;
	opts.max_name_len = conf.max_name_len;
	opts.only_dirs = conf.only_dirs;
	opts.pager = conf.pager;
	opts.sort = conf.sort;
	opts.sort_reverse = conf.sort_reverse;
	opts.filter = (struct filter_t){0};
}

void
update_autocmd_opts(const int opt)
{
	switch (opt) {
	case AC_COLOR_SCHEME: opts.color_scheme = cur_cscheme; break;
	case AC_FILE_COUNTER: opts.file_counter = conf.file_counter; break;
	case AC_FULL_DIR_SIZE: opts.full_dir_size = conf.full_dir_size; break;
	case AC_LIGHT_MODE: opts.light_mode = conf.light_mode; break;
	case AC_LONG_VIEW: opts.long_view = conf.long_view; break;
	case AC_SHOW_HIDDEN: opts.show_hidden = conf.show_hidden; break;
	case AC_MAX_FILES: opts.max_files = conf.max_files; break;
	case AC_MAX_NAME_LEN: opts.max_name_len = conf.max_name_len; break;
	case AC_ONLY_DIRS: opts.only_dirs = conf.only_dirs; break;
	case AC_SORT:
		opts.sort = conf.sort;
		opts.sort_reverse = conf.sort_reverse;
		break;
	case AC_PAGER: opts.pager = conf.pager; break;
	case AC_FILTER:
		free(opts.filter.str);
		if (!filter.str) {
			opts.filter = (struct filter_t){0};
		} else {
			opts.filter.str = savestring(filter.str, strlen(filter.str));
			opts.filter.type = filter.type;
			opts.filter.rev = filter.rev;
			opts.filter.env = filter.env;
		}
		break;
	default: break;
	}
}

static int
set_autocmd_regex_filter(const char *pattern)
{
	regfree(&regex_exp);
	const int ret = regcomp(&regex_exp, pattern, REG_NOSUB | REG_EXTENDED);
	if (ret != FUNC_SUCCESS) {
		regfree(&regex_exp);
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static void
copy_current_files_filter(void)
{
	free(opts.filter.str);
	opts.filter.str = (char *)NULL;

	if (!filter.str)
		return;

	opts.filter.str = savestring(filter.str, strlen(filter.str));
	opts.filter.rev = filter.rev;
	opts.filter.type = filter.type;
	opts.filter.env = filter.env;
}

static void
install_autocmd_files_filter(const size_t i)
{
	free(filter.str);
	if (strcmp(autocmds[i].filter.str, "unset") == 0) {
		filter.str = (char *)NULL;
		if (filter.type == FILTER_FILE_NAME)
			regfree(&regex_exp);
	} else {
		filter.str = savestring(autocmds[i].filter.str,
			strlen(autocmds[i].filter.str));
		filter.type = autocmds[i].filter.type;
		filter.rev = autocmds[i].filter.rev;
		filter.env = autocmds[i].filter.env;
		if (filter.type == FILTER_FILE_NAME)
			set_autocmd_regex_filter(filter.str);
	}
}

static void
save_current_options(void)
{
	opts.light_mode = conf.light_mode;
	opts.file_counter = conf.file_counter;
	opts.full_dir_size = conf.full_dir_size;
	opts.long_view = conf.long_view;
	opts.max_files = conf.max_files;
	opts.show_hidden = conf.show_hidden;
	opts.sort = conf.sort;
	opts.sort_reverse = conf.sort_reverse;
	opts.max_name_len = conf.max_name_len;
	opts.pager = conf.pager;
	opts.only_dirs = conf.only_dirs;
	opts.color_scheme = cur_cscheme;

	copy_current_files_filter();
}

static void
set_autocmd_options(const size_t i)
{
	if (autocmds[i].light_mode != UNSET)
		conf.light_mode = autocmds[i].light_mode;
	if (autocmds[i].file_counter != UNSET)
		conf.file_counter = autocmds[i].file_counter;
	if (autocmds[i].full_dir_size != UNSET)
		conf.full_dir_size = autocmds[i].full_dir_size;
	if (autocmds[i].long_view != UNSET)
		conf.long_view = autocmds[i].long_view;
	if (autocmds[i].show_hidden != UNSET)
		conf.show_hidden = autocmds[i].show_hidden;
	if (autocmds[i].only_dirs != UNSET)
		conf.only_dirs = autocmds[i].only_dirs;
	if (autocmds[i].pager != UNSET)
		conf.pager = autocmds[i].pager;
	if (autocmds[i].sort != UNSET)
		conf.sort = autocmds[i].sort;
	if (autocmds[i].sort_reverse != UNSET)
		conf.sort_reverse = autocmds[i].sort_reverse;
	if (autocmds[i].max_name_len != AC_UNSET)
		conf.max_name_len = autocmds[i].max_name_len;
	if (autocmds[i].max_files != AC_UNSET)
		conf.max_files = autocmds[i].max_files;
	if (autocmds[i].cmd) {
		if (xargs.secure_cmds == 0
		|| sanitize_cmd(autocmds[i].cmd, SNT_AUTOCMD) == FUNC_SUCCESS)
			launch_execl(autocmds[i].cmd);
	}

	/* Color scheme and files filter will be set later, to avoid
	 * setting them twice if there are multiple autocommands matching
	 * the current directory. */
}

static struct autocmds_t
gen_common_options(void)
{
	struct autocmds_t a;
	struct autocmds_t *b = autocmds;

	a.cmd = (char *)NULL;
	a.color_scheme = (char *)NULL;
	a.file_counter = UNSET;
	a.full_dir_size = UNSET;
	a.light_mode = UNSET;
	a.long_view = UNSET;
	a.max_files = AC_UNSET;
	a.max_name_len = AC_UNSET;
	a.only_dirs = UNSET;
	a.pager = UNSET;
	a.show_hidden = UNSET;
	a.sort = UNSET;
	a.sort_reverse = UNSET;
	a.filter = (struct filter_t){0};
	a.temp = 0;

	for (size_t i = 0; i < autocmds_n; i++) {
		if (b[i].match == 0)
			continue;

		if (b[i].color_scheme)
			a.color_scheme = b[i].color_scheme;
		if (b[i].filter.str) {
			a.filter.str = b[i].filter.str;
			a.filter.rev = b[i].filter.rev;
			a.filter.type = b[i].filter.type;
		}
		if (b[i].file_counter != UNSET)
			a.file_counter = b[i].file_counter;
		if (b[i].full_dir_size != UNSET)
			a.full_dir_size = b[i].full_dir_size;
		if (b[i].light_mode != UNSET)
			a.light_mode = b[i].light_mode;
		if (b[i].long_view != UNSET)
			a.long_view = b[i].long_view;
		if (b[i].max_files != AC_UNSET)
			a.max_files = b[i].max_files;
		if (b[i].max_name_len != AC_UNSET)
			a.max_name_len = b[i].max_name_len;
		if (b[i].only_dirs != UNSET)
			a.only_dirs = b[i].only_dirs;
		if (b[i].pager != UNSET)
			a.pager = b[i].pager;
		if (b[i].show_hidden != UNSET)
			a.show_hidden = autocmds[i].show_hidden;
		if (b[i].sort != UNSET)
			a.sort = b[i].sort;
		if (b[i].sort_reverse != UNSET)
			a.sort_reverse = b[i].sort_reverse;
	}

	return a;
}

static int
gen_opt_entry(char *buf, const char *name, const char *val, size_t *pos,
	const int long_msg)
{
	const int ret = snprintf(buf, AC_BUF_SIZE, "%s%s%s%s", *pos > 0 ? ", " : "",
		name, (long_msg == 1 && val) ? "=" : "",
		(long_msg == 1 && val) ? val : "");

	(*pos)++;
	return (ret > 0 ? ret : 0);
}

/* Write into BUF the list of autocmd options set in the struct A. */
static int
gen_autocmd_options_list(char *buf, struct autocmds_t *a, const int print_filter)
{
	int len = 0; /* Number of bytes currently consumed by buf. */
	size_t c = 0; /* Number of procesed options for the current autocommand. */
	/* PRINT_FILTER is 1 if coming from the 'auto list' command. */
	/* Should we print the message in long mode? */
	const int lm =
		(conf.autocmd_msg != AUTOCMD_MSG_SHORT || print_filter == 1);

	if (a->color_scheme != NULL)
		len += gen_opt_entry(buf + len, "cs", a->color_scheme, &c, lm);

	if (a->file_counter != UNSET)
		len += gen_opt_entry(buf + len, "fc", xitoa(a->file_counter), &c, lm);

	if (a->filter.str != NULL)
		len += gen_opt_entry(buf + len, "ft",
			print_filter == 1 ? a->filter.str : NULL, &c, lm);

	if (a->full_dir_size != UNSET)
		len += gen_opt_entry(buf + len, "fz", xitoa(a->full_dir_size), &c, lm);

	if (a->show_hidden != UNSET)
		len += gen_opt_entry(buf + len, "hf", xitoa(a->show_hidden), &c, lm);

	if (a->light_mode != UNSET)
		len += gen_opt_entry(buf + len, "lm", xitoa(a->light_mode), &c, lm);

	if (a->long_view != UNSET)
		len += gen_opt_entry(buf + len, "lv", xitoa(a->long_view), &c, lm);

	if (a->max_files != AC_UNSET)
		len += gen_opt_entry(buf + len, "mf",
			a->max_files == UNSET ? "unset" : xitoa(a->max_files), &c, lm);

	if (a->max_name_len != AC_UNSET)
		len += gen_opt_entry(buf + len, "mn",
			a->max_name_len == UNSET ? "unset" : xitoa(a->max_name_len), &c, lm);

	if (a->only_dirs != UNSET)
		len += gen_opt_entry(buf + len, "od", xitoa(a->only_dirs), &c, lm);

	if (a->pager != UNSET)
		len += gen_opt_entry(buf + len, "pg", xitoa(a->pager), &c, lm);

	if (a->sort != UNSET)
		len += gen_opt_entry(buf + len, "st", num_to_sort_name(a->sort, 0), &c, lm);

	if (a->sort_reverse != UNSET)
		len += gen_opt_entry(buf + len, "sr", xitoa(a->sort_reverse), &c, lm);

	return len;
}

static void
print_autocmd_options_list_full(void)
{
	for (size_t i = 0; i < autocmds_n; i++) {
		if (autocmds[i].match == 0)
			continue;

		char buf[AC_BUF_SIZE];
		const int len = gen_autocmd_options_list(buf, &autocmds[i], 0);
		if (len <= 0)
			continue;

		print_reload_msg(NULL, NULL, "Autocmd [");
		fputs(buf, stdout);
		printf("]%s\n", autocmds[i].temp == 1 ? "T" : "");
	}
}

void
print_autocmd_msg(void)
{
	if (conf.autocmd_msg == AUTOCMD_MSG_MINI) {
		print_reload_msg(NULL, NULL, "Autocmd\n");
		return;
	}

	if (conf.autocmd_msg == AUTOCMD_MSG_FULL) {
		print_autocmd_options_list_full();
		return;
	}

	char buf[AC_BUF_SIZE];
	struct autocmds_t a = gen_common_options();
	const int len = gen_autocmd_options_list(buf, &a, 0);

	if (len <= 0) /* No autocommand option set. Do not print any message */
		return;

	print_reload_msg(NULL, NULL, "Autocmd [");
	fputs(buf, stdout);
	fputs("]\n", stdout);
}

static int
run_autocmds(const size_t *matches, const size_t matches_n)
{
	save_current_options();
	autocmd_set = 1;

	int cs_last_index = UNSET;
	int ft_last_index = UNSET;

	for (size_t i = 0; i < matches_n; i++) {
		autocmds[matches[i]].match = 1;
		if (autocmds[matches[i]].color_scheme != NULL)
			cs_last_index = (int)matches[i];
		if (autocmds[matches[i]].filter.str != NULL)
			ft_last_index = (int)matches[i];
		set_autocmd_options(matches[i]);
	}

	if (cs_last_index != UNSET)
		set_colors(autocmds[cs_last_index].color_scheme, 0);
	if (ft_last_index != UNSET)
		install_autocmd_files_filter((size_t)ft_last_index);

	return 1;
}

static void
unset_autocmd_matches(void)
{
	for (size_t i = 0; i < autocmds_n; i++)
		autocmds[i].match = 0;
}

/* Check the current directory for matching autocommands and set options
 * accordingly.
 * Returns 1 if at least one matching autocommand is found, or 0 otherwise. */
int
check_autocmds(void)
{
	if (!autocmds || autocmds_n == 0)
		return 0;

	unset_autocmd_matches();

	size_t matches[256]; /* Store indices of matching entries */
	size_t matches_n = 0;

	for (size_t i = 0; i < autocmds_n && matches_n < sizeof(matches); i++) {
		if (!autocmds[i].pattern || !*autocmds[i].pattern)
			continue;

		const char *p = autocmds[i].pattern;
		const int rev = autocmds[i].pattern_rev;
		int found = 0;

		/* 1. Temporary autocommands (set via the 'auto' command). */
		if (autocmds[i].temp == 1) {
			found = (strcmp(p, workspaces[cur_ws].path) == 0);
			goto CHECK_MATCH;
		}

		/* 2. Workspaces (@wsN). */
		if (*p == '@' && p[1] == 'w' && p[2] == 's' && p[3]) {
			found = (p[3] - '0' == cur_ws + 1);
			goto CHECK_MATCH;
		}

		/* 3. Double asterisk: match everything starting with PATTERN
		 * (less double asterisk itself and ending slash). */
		const size_t plen = strlen(p);
		if (plen >= 3 && p[plen - 1] == '*' && p[plen - 2] == '*') {
			size_t n = 2;
			if (p[plen - 3] == '/')
				n++;

			if (plen - n == 0 || strncmp(p,
			workspaces[cur_ws].path, plen - n) == 0)
				found = 1;

			goto CHECK_MATCH;
		}

		/* 4. Glob expression or plain text for PATTERN */
		found = (fnmatch(p, workspaces[cur_ws].path, 0) == 0);

CHECK_MATCH:
		if ((rev == 0 && found == 0) || (rev == 1 && found == 1))
			continue;

		matches[matches_n] = i;
		matches_n++;
	}

	return (matches_n > 0 ? run_autocmds(matches, matches_n) : 0);
}

static int
set_autocmd_filter_type(const char c)
{
	if (c == '=')
		return FILTER_FILE_TYPE;

	if (c == '@')
		return FILTER_MIME_TYPE; /* UNIMPLEMENTED */

	return FILTER_FILE_NAME;
}

static void
revert_files_filter(void)
{
	filter.type = opts.filter.type;
	filter.rev = opts.filter.rev;
	filter.env = opts.filter.env;

	free(filter.str);
	filter.str = savestring(opts.filter.str, strlen(opts.filter.str));
	free(opts.filter.str);
	opts.filter.str = (char *)NULL;

	if (filter.type == FILTER_FILE_NAME)
		set_autocmd_regex_filter(filter.str);
}

static void
remove_files_filter(void)
{
	free(filter.str);
	filter = (struct filter_t){0};

	if (filter.type == FILTER_FILE_NAME)
		regfree(&regex_exp);
}

static int
set_autocmd_files_filter(const char *pattern, const size_t n)
{
	if (*pattern == 'u' && strcmp(pattern, "unset") == 0) {
		free(autocmds[n].filter.str);
		autocmds[n].filter = (struct filter_t){0};
		return FUNC_SUCCESS;
	}

	autocmds[n].filter.rev = (*pattern == '!');
	const char *p = pattern + (*pattern == '!');
	free(autocmds[n].filter.str);
	autocmds[n].filter.str = savestring(p, strlen(p));
	autocmds[n].filter.type = set_autocmd_filter_type(*p);
	autocmds[n].filter.env = 0;

	return FUNC_SUCCESS;
}

/* Revert back to options previous to autocommand */
void
revert_autocmd_opts(void)
{
	conf.light_mode = opts.light_mode;
	conf.file_counter = opts.file_counter;
	conf.full_dir_size = opts.full_dir_size;
	conf.long_view = opts.long_view;
	conf.max_files = opts.max_files;
	conf.show_hidden = opts.show_hidden;
	conf.max_name_len = opts.max_name_len;
	conf.pager = opts.pager;
	conf.sort = opts.sort;
	conf.only_dirs = opts.only_dirs;
	conf.sort_reverse = opts.sort_reverse;

	if (opts.color_scheme && opts.color_scheme != cur_cscheme)
		set_colors(opts.color_scheme, 0);

	if (opts.filter.str) {
		revert_files_filter();
	} else {
		if (filter.str) /* This is an autocmd filter. Remove it. */
			remove_files_filter();
	}

	autocmd_set = 0;
}

static int
set_autocmd_color_scheme(const char *name, const size_t n)
{
	if (!name || !*name || cschemes_n == 0)
		return FUNC_FAILURE;

	if (*name == 'u' && strcmp(name, "unset") == 0) {
		autocmds[n].color_scheme = (char *)NULL;
		return FUNC_SUCCESS;
	}

	for (size_t i = cschemes_n; i-- > 0;) {
		if (*color_schemes[i] == *name
		&& strcmp(color_schemes[i], name) == 0) {
			autocmds[n].color_scheme = color_schemes[i];
			return FUNC_SUCCESS;
		}
	}

	err(ERR_NO_LOG, PRINT_PROMPT, _("autocmd: '%s': Invalid value for 'cs'\n"),
		name);
	autocmds[n].color_scheme = (char *)NULL;
	return FUNC_FAILURE;
}

static int
set_autocmd_sort_by_name(const char *name, const size_t n)
{
	if (*name == 'u' && strcmp(name, "unset") == 0) {
		autocmds[n].sort = UNSET;
		return FUNC_SUCCESS;
	}

	for (size_t i = 0; i <= SORT_TYPES; i++) {
		if (*name != *sort_methods[i].name
		|| strcmp(name, sort_methods[i].name) != 0)
			continue;

		autocmds[n].sort = (conf.light_mode == 1
			&& !ST_IN_LIGHT_MODE(sort_methods[i].num))
			? conf.sort : sort_methods[i].num;
		return FUNC_SUCCESS;
	}

	return FUNC_FAILURE;
}

static int
set_autocmd_sort(const char *val, const size_t n)
{
	if (!val || !*val)
		return FUNC_FAILURE;

	if (!is_number(val)) {
		if (set_autocmd_sort_by_name(val, n) == FUNC_SUCCESS)
			return FUNC_SUCCESS;
		goto ERROR;
	}

	const int a = atoi(val);
	if (a >= 0 && a <= SORT_TYPES) {
		autocmds[n].sort = a;
		return FUNC_SUCCESS;
	}

ERROR:
	err(ERR_NO_LOG, PRINT_PROMPT, _("autocmd: '%s': Invalid value for 'st'\n"),
		val);
	return FUNC_FAILURE;
}

/* Store each autocommand option in OPT in the corresponding field of
 * the autocommand at index N. */
static int
fill_autocmd_opt(char *opt, const size_t n)
{
	if (!opt || !*opt)
		return FUNC_FAILURE;

	if (*opt == '!' && *(++opt)) {
		free(autocmds[n].cmd);
		autocmds[n].cmd = savestring(opt, strlen(opt));
		return FUNC_SUCCESS;
	}

	char *p = strchr(opt, '=');
	if (!p) {
		err(ERR_NO_LOG, PRINT_PROMPT, _("autocmd: '%s': Invalid option format "
			" (it must be 'OPTION=VALUE')\n"), opt);
		return FUNC_FAILURE;
	}

	*p = '\0';
	p++;

	/* All option names take exactly two characters. */
	if (!*opt || !opt[1] || opt[2])
		goto ERR_NAME;

	/* 'cs', 'ft', and 'st' take strings as values. */
	if (*opt == 'c' && opt[1] == 's')
		return set_autocmd_color_scheme(*p ? p : "unset", n);

	if (*opt == 'f' && opt[1] == 't')
		return set_autocmd_files_filter(*p ? p : "unset", n);

	if (*opt == 's' && opt[1] == 't')
		return set_autocmd_sort(*p ? p : "unset", n);

	/* The below options take only numbers (or 'unset') as values. */
	int a = 0;
	if (!*p) { /* 'OPTION=' amounts to 'OPTION=unset'. */
		a = UNSET;
	} else if (!is_number(p)) {
		if (*p != 'u' || strcmp(p, "unset") != 0)
			goto ERR_VAL;
		a = UNSET;
	} else {
		a = atoi(p);
	}

	if (*opt == 'm' && opt[1] == 'f') {
		autocmds[n].max_files = a == UNSET ? AC_UNSET : a;
		return FUNC_SUCCESS;
	}

	if (*opt == 'm' && opt[1] == 'n') {
		autocmds[n].max_name_len = a == UNSET ? AC_UNSET : a;
		return FUNC_SUCCESS;
	}

	if (a != UNSET && a != 1 && a != 0)
		goto ERR_VAL;

	if (*opt == 'f' && opt[1] == 'c')
		autocmds[n].file_counter = a;
	else if (*opt == 'f' && opt[1] == 'z')
		autocmds[n].full_dir_size = a;
	else if (*opt == 'h' && (opt[1] == 'f' || opt[1] == 'h'))
		autocmds[n].show_hidden = a;
	else if (*opt == 'l' && opt[1] == 'm')
		autocmds[n].light_mode = a;
	else if (*opt == 'l' && (opt[1] == 'v' || opt[1] == 'l'))
		autocmds[n].long_view = a;
	else if (*opt == 'o' && opt[1] == 'd')
		autocmds[n].only_dirs = a;
	else if (*opt == 'p' && opt[1] == 'g')
		autocmds[n].pager = a;
	else if (*opt == 's' && opt[1] == 'r')
		autocmds[n].sort_reverse = a;
	else
		goto ERR_NAME;

	return FUNC_SUCCESS;

ERR_NAME:
	err(ERR_NO_LOG, PRINT_PROMPT, _("autocmd: '%s': Invalid option name\n"),
		opt);
	return FUNC_FAILURE;

ERR_VAL:
	err(ERR_NO_LOG, PRINT_PROMPT, _("autocmd: '%s': Invalid value for '%s'\n"),
		p, opt);
	return FUNC_FAILURE;
}

static void
init_autocmd_opts(const size_t n)
{
	autocmds[n].cmd = (char *)NULL;
	autocmds[n].color_scheme = (char *)NULL;
	autocmds[n].file_counter = UNSET;
	autocmds[n].full_dir_size = UNSET;
	autocmds[n].light_mode = UNSET;
	autocmds[n].long_view = UNSET;
	autocmds[n].max_files = AC_UNSET;
	autocmds[n].max_name_len = AC_UNSET;
	autocmds[n].only_dirs = UNSET;
	autocmds[n].pager = UNSET;
	autocmds[n].show_hidden = UNSET;
	autocmds[n].sort = UNSET;
	autocmds[n].sort_reverse = UNSET;
	autocmds[n].filter = (struct filter_t){0};
	autocmds[n].temp = 0;
	autocmds[n].match = 0;
	autocmds[n].pattern_rev = 0;
}

/* Modify the options of the autocommand whose index number is N
 * according to the list of parameters found in ARG. */
static int
modify_autocmd(char *arg, const size_t n)
{
	char *p = arg;
	int exit_status = FUNC_FAILURE;

	while (1) {
		char *val = (char *)NULL;
		if (*arg == ',') {
			*arg = '\0';
			arg++;
			val = p;
			p = arg;
		} else if (!*arg) {
			val = p;
		} else {
			arg++;
			continue;
		}

		if (fill_autocmd_opt(val, n) == FUNC_SUCCESS)
			exit_status = FUNC_SUCCESS;

		if (!*arg)
			break;
	}

	return exit_status;
}

static void
save_autocmd_pattern(const char *p, const size_t buf_len, const size_t n)
{
	if (*p == '!') {
		p++;
		autocmds[n].pattern_rev = 1;
	}

	if (*p == '~' && user.home && *user.home) {
		if (!p[1] || (p[1] == '/' && !p[2])) {
			autocmds[n].pattern = savestring(user.home, user.home_len);
		} else if (p[1] == '/') {
			const size_t len = user.home_len + (strnlen(p, buf_len) - 2) + 2;
			autocmds[n].pattern = xnmalloc(len, sizeof(char));
			snprintf(autocmds[n].pattern, len, "%s/%s", user.home, p + 2);
		} else {
			autocmds[n].pattern = savestring(p, strnlen(p, buf_len));
		}
	} else {
		autocmds[n].pattern = savestring(p, strnlen(p, buf_len));
	}
}

/* Take an autocmd line and store parameters in a struct. */
int
parse_autocmd_line(char *cmd, const size_t buflen)
{
	if (!cmd || !*cmd)
		return FUNC_FAILURE;

	const size_t clen = strnlen(cmd, buflen);
	if (clen > 0 && cmd[clen - 1] == '\n')
		cmd[clen - 1] = '\0';

	char *p = strchr(cmd, ' ');
	if (!p || !p[1])
		return FUNC_FAILURE;

	*p = '\0';

	autocmds = xnrealloc(autocmds, autocmds_n + 1, sizeof(struct autocmds_t));
	init_autocmd_opts(autocmds_n);
	save_autocmd_pattern(cmd, buflen, autocmds_n);

	p++;

	if (modify_autocmd(p, autocmds_n) == FUNC_FAILURE) {
		/* No valid option found for this autocmd: remove it. */
		free(autocmds[autocmds_n].pattern);
		if (autocmds_n > 0) {
			autocmds =
				xnrealloc(autocmds, autocmds_n, sizeof(struct autocmds_t));
		} else {
			free(autocmds);
			autocmds = (struct autocmds_t *)NULL;
		}

		return FUNC_FAILURE;
	}

	autocmds_n++;
	return FUNC_SUCCESS;
}

static int
unset_tmp_autocmds(void)
{
	size_t found = 0;

	for (size_t i = 0; i < autocmds_n; i++) {
		if (autocmds[i].temp == 1
		&& *autocmds[i].pattern == *workspaces[cur_ws].path
		&& strcmp(autocmds[i].pattern, workspaces[cur_ws].path) == 0) {
			*autocmds[i].pattern = '\0';
			found++;
		}
	}

	if (found == 0) {
		xerror(_("auto: No temporary autocommand defined for "
			"the current directory\n"));
		return FUNC_FAILURE;
	}

	return FUNC_SUCCESS;
}

static int
autocmd_dirlist_reload(void)
{
	dir_changed = 1;
	if (conf.autols == 1)
		reload_dirlist();
	return FUNC_SUCCESS;
}

static size_t
get_longest_pattern(void)
{
	size_t len = 0;

	for (size_t i = 0; i < autocmds_n; i++) {
		if (!autocmds[i].pattern || !*autocmds[i].pattern)
			continue;

		const size_t l = strlen(autocmds[i].pattern)
			+ (size_t)autocmds[i].pattern_rev;
		if (l > len)
			len = l;
	}

	return len;
}

static int
print_autocmds_list(void)
{
	if (autocmds_n == 0) {
		fputs("auto: No autocommand defined\n", stdout);
		return FUNC_SUCCESS;
	}

	char buf[AC_BUF_SIZE];
	const int longest_pattern = (int)get_longest_pattern();

	for (size_t i = 0; i < autocmds_n; i++) {
		if (!autocmds[i].pattern || !*autocmds[i].pattern
		|| gen_autocmd_options_list(buf, &autocmds[i], 1) <= 0)
			continue;

		printf("%s%s%s%s%-*s %s%s%s %s%s\n",
			xs_cb, autocmds[i].match == 1 ? SET_MISC_PTR : " ", df_c,
			autocmds[i].pattern_rev == 1 ? "!" : "",
			longest_pattern, autocmds[i].pattern,
			mi_c, SET_MSG_PTR, df_c,
			buf, autocmds[i].temp == 1 ? " [temp]" : "");
	}

	return FUNC_SUCCESS;
}

static int
reload_dir_ignoring_autocmds(void)
{
	if (autocmds_n == 0) {
		fputs("auto: No autocommand defined\n", stdout);
		return FUNC_SUCCESS;
	}

	revert_autocmd_opts();
	unset_autocmd_matches();
	dir_changed = 0;
	reload_dirlist();
	return FUNC_SUCCESS;
}

/* 'auto' command: add a temporary autocommand for the current directory. */
int
add_autocmd(char **args)
{
	if (!args || !args[0] || !*args[0] || IS_HELP(args[0])) {
		puts(AUTO_USAGE);
		return FUNC_SUCCESS;
	}

	if (*args[0] == 'n' && strcmp(args[0], "none") == 0)
		return reload_dir_ignoring_autocmds();

	if (*args[0] == 'l' && strcmp(args[0], "list") == 0)
		return print_autocmds_list();

	if (!workspaces || !workspaces[cur_ws].path || !*workspaces[cur_ws].path) {
		xerror(_("auto: The current directory is undefined\n"));
		return FUNC_FAILURE;
	}

	if (*args[0] == 'u' && strcmp(args[0], "unset") == 0) {
		return (unset_tmp_autocmds() == FUNC_FAILURE ? FUNC_FAILURE
			: autocmd_dirlist_reload());
	}

	for (size_t i = 0; i < autocmds_n; i++) {
		if (autocmds[i].temp == 1
		&& strcmp(workspaces[cur_ws].path, autocmds[i].pattern) == 0) {
			/* Add option to an existing autocmd */
			return modify_autocmd(args[0], i) == FUNC_SUCCESS
				? autocmd_dirlist_reload() : FUNC_FAILURE;
		}
	}

	/* No autocommand found for this target (current directory). Let's
	 * create a new entry for this autocommand. */

	const size_t cwd_len = strlen(workspaces[cur_ws].path);
	if (cwd_len == 0)
		return FUNC_FAILURE;

	const size_t cmd_len = strlen(args[0]);
	const size_t len = cwd_len + 1 + cmd_len + 1;
	char *str = xnmalloc(len, sizeof(char));
	snprintf(str, len, "%s %s", workspaces[cur_ws].path, args[0]);

	const int ret = parse_autocmd_line(str, len);

	free(str);

	if (ret == FUNC_FAILURE)
		return FUNC_FAILURE;

	/* If parse_autocmd_line returns successfully, it is warranted that
	 * autocmds_n will be greater than zero. */
	/* Mark the last entry as set via the 'auto' command. */
	autocmds[autocmds_n - 1].temp = 1;

	return autocmd_dirlist_reload();
}
