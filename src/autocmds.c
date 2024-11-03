/* autocmds.c -- run cmds on a per directory basis */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
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

#include <string.h>
#include <readline/tilde.h>

#include "aux.h"
#include "checks.h"   /* is_number */
#include "colors.h"
#include "listing.h"  /* reload_dirlist */
#include "messages.h" /* AUTO_USAGE macro */
#include "misc.h"     /* xerror (err) */
#include "sanitize.h"
#include "spawn.h"

/* The opts struct contains option values previous to any autocommand call */
void
reset_opts(void)
{
	opts.color_scheme = cur_cscheme;
	opts.files_counter = conf.files_counter;
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

/* Run autocommands for the current directory */
int
check_autocmds(void)
{
	if (!autocmds || autocmds_n == 0)
		return FUNC_SUCCESS;

	size_t i;
	int found = 0;

	for (i = 0; i < autocmds_n; i++) {
		if (!autocmds[i].pattern || !*autocmds[i].pattern)
			continue;

		int rev = 0;
		char *p = autocmds[i].pattern;
		if (*p == '!') {
			++p;
			rev = 1;
		}

		/* Check workspaces (@wsN)*/
		if (*autocmds[i].pattern == '@' && autocmds[i].pattern[1] == 'w'
		&& autocmds[i].pattern[2] == 's' && autocmds[i].pattern[3]
		/* char '1' - 48 (or '0') == int 1 */
		&& autocmds[i].pattern[3] - 48 == cur_ws + 1) {
			found = 1;
			goto RUN_AUTOCMD;
		}

		/* Double asterisk: match everything starting with PATTERN
		 * (less double asterisk itself and ending slash). */
		size_t plen = strlen(p), n = 0;
		if (!rev && plen >= 3 && p[plen - 1] == '*' && p[plen - 2] == '*') {
			n = 2;
			if (p[plen - 3] == '/')
				n++;

			if (*p == '~') {
				p[plen - n] = '\0';
				char *path = tilde_expand(p);
				if (!path)
					continue;

				p[plen - n] = (n == 2 ? '*' : '/');
				const size_t tlen = strlen(path);
				const int ret = strncmp(path, workspaces[cur_ws].path, tlen);
				free(path);

				if (ret == 0) {
					found = 1;
					goto RUN_AUTOCMD;
				}

			} else { /* We have an absolute path */
				/* If (plen - n) == 0 we have "/\**", that is, match everything:
				 * no need to perform any check. */
				if (plen - n == 0 || strncmp(autocmds[i].pattern,
				workspaces[cur_ws].path, plen - n) == 0) {
					found = 1;
					goto RUN_AUTOCMD;
				}
			}
		}

		/* Glob expression or plain text for PATTERN */
		glob_t g;
		const int ret = glob(p, GLOB_NOSORT | GLOB_NOCHECK | GLOB_TILDE
		| GLOB_BRACE, NULL, &g);

		if (ret != FUNC_SUCCESS) {
			globfree(&g);
			continue;
		}

		size_t j;
		for (j = 0; j < g.gl_pathc; j++) {
			if (*workspaces[cur_ws].path == *g.gl_pathv[j]
			&& strcmp(workspaces[cur_ws].path, g.gl_pathv[j]) == 0) {
				found = 1;
				break;
			}
		}
		globfree(&g);

		if (rev == 0) {
			if (found == 0)
				continue;
		} else {
			if (found == 1)
				continue;
		}

RUN_AUTOCMD:
		if (autocmd_set == 0) {
			/* Backup current options, only if there was no autocmd for
			 * this directory */
			opts.light_mode = conf.light_mode;
			opts.files_counter = conf.files_counter;
			opts.full_dir_size = conf.full_dir_size;
			opts.long_view = conf.long_view;
			opts.max_files = conf.max_files;
			opts.show_hidden = conf.show_hidden;
			opts.sort = conf.sort;
			opts.sort_reverse = conf.sort_reverse;
			opts.max_name_len = conf.max_name_len;
			opts.pager = conf.pager;
			opts.only_dirs = conf.only_dirs;

			if (autocmds[i].color_scheme && cur_cscheme)
				opts.color_scheme = cur_cscheme;
			else
				opts.color_scheme = (char *)NULL;

			copy_current_files_filter();

			autocmd_set = 1;
		}

		/* Set options for current directory */
		if (autocmds[i].light_mode != -1)
			conf.light_mode = autocmds[i].light_mode;
		if (autocmds[i].files_counter != -1)
			conf.files_counter = autocmds[i].files_counter;
		if (autocmds[i].full_dir_size != -1)
			conf.full_dir_size = autocmds[i].full_dir_size;
		if (autocmds[i].long_view != -1)
			conf.long_view = autocmds[i].long_view;
		if (autocmds[i].show_hidden != -1)
			conf.show_hidden = autocmds[i].show_hidden;
		if (autocmds[i].only_dirs != -1)
			conf.only_dirs = autocmds[i].only_dirs;
		if (autocmds[i].pager != -1)
			conf.pager = autocmds[i].pager;
		if (autocmds[i].sort != -1)
			conf.sort = autocmds[i].sort;
		if (autocmds[i].sort_reverse != -1)
			conf.sort_reverse = autocmds[i].sort_reverse;
		if (autocmds[i].max_name_len != -1)
			conf.max_name_len = autocmds[i].max_name_len;
		if (autocmds[i].max_files != -2)
			conf.max_files = autocmds[i].max_files;
		if (autocmds[i].color_scheme)
			set_colors(autocmds[i].color_scheme, 0);
		if (autocmds[i].cmd) {
			if (xargs.secure_cmds == 0
			|| sanitize_cmd(autocmds[i].cmd, SNT_AUTOCMD) == FUNC_SUCCESS)
				launch_execl(autocmds[i].cmd);
		}
		if (autocmds[i].filter.str)
			install_autocmd_files_filter(i);

		break;
	}

	return found;
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

static void
set_autocmd_files_filter(const char *pattern, const size_t n)
{
	if (*pattern == 'u' && strcmp(pattern, "unset") == 0) {
		free(autocmds[n].filter.str);
		autocmds[n].filter = (struct filter_t){0};
		return;
	}

	autocmds[n].filter.rev = (*pattern == '!');
	const char *p = pattern + (*pattern == '!');
	free(autocmds[n].filter.str);
	autocmds[n].filter.str = savestring(p, strlen(p));
	autocmds[n].filter.type = set_autocmd_filter_type(*p);
	autocmds[n].filter.env = 0;
}

/* Revert back to options previous to autocommand */
void
revert_autocmd_opts(void)
{
	conf.light_mode = opts.light_mode;
	conf.files_counter = opts.files_counter;
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
set_autocmd_sort_by_name(const char *name)
{
	size_t i;
	for (i = 0; i <= SORT_TYPES; i++) {
		if (*name != *sort_methods[i].name
		|| strcmp(name, sort_methods[i].name) != 0)
			continue;

		return (conf.light_mode == 1 && !ST_IN_LIGHT_MODE(sort_methods[i].num))
			? DEF_SORT : sort_methods[i].num;
	}

	return DEF_SORT;
}

static void
set_autocmd_color_scheme(const char *name, const size_t n)
{
	if (!name || !*name || cschemes_n == 0)
		return;

	if (*name == 'u' && strcmp(name, "unset") == 0) {
		autocmds[n].color_scheme = (char *)NULL;
		return;
	}

	int i = (int)cschemes_n;
	while (--i >= 0) {
		if (*color_schemes[i] == *name
		&& strcmp(color_schemes[i], name) == 0) {
			autocmds[n].color_scheme = color_schemes[i];
			return;
		}
	}

	autocmds[n].color_scheme = (char *)NULL;
}

/* Store each autocommand option in the corresponding field of the
 * autocmds struct. */
static void
set_autocmd_opt(char *opt, const size_t n)
{
	if (!opt || !*opt)
		return;

	if (*opt == '!' && *(++opt)) {
		free(autocmds[n].cmd);
		autocmds[n].cmd = savestring(opt, strlen(opt));
		return;
	}

	char *p = strchr(opt, '=');
	if (!p || !*(++p))
		return;

	*(p - 1) = '\0';
	if (*opt == 'c' && opt[1] == 's') {
		set_autocmd_color_scheme(p, n);
	} else if (*opt == 'f' && opt[1] == 't') {
		set_autocmd_files_filter(p, n);
	} else {
		int a = is_number(p) ? atoi(p) : -1;
		if (a == INT_MIN)
			a = 0;
		if (*opt == 'f' && opt[1] == 'c')
			autocmds[n].files_counter = a;
		if (*opt == 'f' && opt[1] == 'z')
			autocmds[n].full_dir_size = a;
		else if (*opt == 'h' && opt[1] == 'f')
			autocmds[n].show_hidden = a;
		else if (*opt == 'l' && opt[1] == 'm')
			autocmds[n].light_mode = a;
		else if (*opt == 'l' && opt[1] == 'v')
			autocmds[n].long_view = a;
		else if (*opt == 'm' && opt[1] == 'f')
			autocmds[n].max_files = a;
		else if (*opt == 'm' && opt[1] == 'n')
			autocmds[n].max_name_len = a;
		else if (*opt == 'o' && opt[1] == 'd')
			autocmds[n].only_dirs = a;
		else if (*opt == 'p' && opt[1] == 'g')
			autocmds[n].pager = a;
		else if (*opt == 's' && opt[1] == 't')
			autocmds[n].sort =
				IS_DIGIT(*p) ? a : set_autocmd_sort_by_name(p);
		else {
			if (*opt == 's' && opt[1] == 'r')
				autocmds[n].sort_reverse = a;
		}
	}
}

static void
init_autocmd_opts(const size_t n)
{
	autocmds[n].cmd = (char *)NULL;
	autocmds[n].color_scheme = cur_cscheme;
	autocmds[n].files_counter = conf.files_counter;
	autocmds[n].full_dir_size = conf.full_dir_size;
	autocmds[n].light_mode = conf.light_mode;
	autocmds[n].long_view = conf.long_view;
	autocmds[n].max_files = conf.max_files;
	autocmds[n].max_name_len = conf.max_name_len;
	autocmds[n].only_dirs = conf.only_dirs;
	autocmds[n].pager = conf.pager;
	autocmds[n].show_hidden = conf.show_hidden;
	autocmds[n].sort = conf.sort;
	autocmds[n].sort_reverse = conf.sort_reverse;
	autocmds[n].filter = (struct filter_t){0};
	autocmds[n].temp = 0;
}

/* Modify the options of the autocommand whose index number is N
 * according to the list of parameters found in ARG. */
static int
modify_autocmd(char *arg, const size_t n)
{
	char *p = arg;

	while (1) {
		char *val = (char *)NULL;
		if (*arg == ',') {
			*(arg) = '\0';
			arg++;
			val = p;
			p = arg;
		} else if (!*arg) {
			val = p;
		} else {
			++arg;
			continue;
		}

		set_autocmd_opt(val, n);

		if (!*arg)
			break;
	}

	return FUNC_SUCCESS;
}

/* Take an autocmd line (from the config file) and store parameters
 * in a struct. */
void
parse_autocmd_line(char *cmd, const size_t buflen)
{
	if (!cmd || !*cmd)
		return;

	const size_t clen = strnlen(cmd, buflen);
	if (clen > 0 && cmd[clen - 1] == '\n')
		cmd[clen - 1] = '\0';

	char *p = strchr(cmd, ' ');
	if (!p || !*(p + 1))
		return;

	*p = '\0';

	autocmds = xnrealloc(autocmds, autocmds_n + 1, sizeof(struct autocmds_t));
	autocmds[autocmds_n].pattern = savestring(cmd, strnlen(cmd, buflen));

	init_autocmd_opts(autocmds_n);

	p++;

	modify_autocmd(p, autocmds_n);

	autocmds_n++;
}

static int
unset_autocmd(void)
{
	size_t found = 0;
	size_t i;
	for (i = 0; i < autocmds_n; i++) {
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
	reload_dirlist();
	return FUNC_SUCCESS;
}

int
add_autocmd(char **args)
{
	if (!args || !args[0] || !*args[0] || IS_HELP(args[0])) {
		puts(AUTO_USAGE);
		return FUNC_SUCCESS;
	}

	if (!workspaces || !workspaces[cur_ws].path || !*workspaces[cur_ws].path) {
		xerror(_("auto: The current directory is undefined\n"));
		return FUNC_FAILURE;
	}

	if (*args[0] == 'u' && strcmp(args[0], "unset") == 0) {
		return (unset_autocmd() == FUNC_FAILURE ? FUNC_FAILURE
			: autocmd_dirlist_reload());
	}

	size_t i;
	for (i = 0; i < autocmds_n; i++) {
		if (autocmds[i].temp == 1
		&& strcmp(workspaces[cur_ws].path, autocmds[i].pattern) == 0) {
			/* Add option to an existing autocmd */
			modify_autocmd(args[0], i);
			return autocmd_dirlist_reload();
		}
	}

	/* No autocommand found for this target (current directory). Let's
	 * create a new entry for this autocommand. */

	const size_t cwd_len = workspaces[cur_ws].path
		? strlen(workspaces[cur_ws].path) : 0;
	if (cwd_len == 0)
		return FUNC_FAILURE;

	const size_t cmd_len = strlen(args[0]);
	const size_t len = cwd_len + 1 + cmd_len + 1;
	char *str = xnmalloc(len, sizeof(char));
	snprintf(str, len, "%s %s", workspaces[cur_ws].path, args[0]);

	parse_autocmd_line(str, len);

	free(str);

	if (autocmds_n > 1) {
		/* Move the entry added by parse_autocmd_line() to the first place
		 * in the autocmds array. */
		autocmds = xnrealloc(autocmds, autocmds_n + 1, sizeof(struct autocmds_t));
		memmove(autocmds + 1, autocmds, autocmds_n * sizeof(struct autocmds_t));
		memmove(autocmds, autocmds + autocmds_n, sizeof(struct autocmds_t));
		autocmds = xnrealloc(autocmds, autocmds_n, sizeof(struct autocmds_t));
	}

	autocmds[0].temp = 1; /* Mark this entry as set via the 'auto' command. */

	return autocmd_dirlist_reload();
}
