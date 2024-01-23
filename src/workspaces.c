/* workspaces.c -- handle workspaces ('ws' command) */

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

#include <errno.h>
#include <stdio.h>
#include <string.h> /* strerror */
#include <unistd.h> /* access */

#include "aux.h"        /* xatoi */
#include "checks.h"     /* is_number */
#include "colors.h"     /* get_dir_color */
#include "listing.h"    /* reload_dirlist */
#include "messages.h"   /* WS_USAGE */
#include "misc.h"       /* xerror */
#include "navigation.h" /* xchdir */
#include "history.h"    /* add_to_dirhist */
#include "strings.h"    /* wc_xstrlen */

static size_t
get_longest_workspace_name(void)
{
	if (!workspaces)
		return 0;

	size_t longest_ws = 0;
	int i = MAX_WS;

	while (--i >= 0) {
		if (!workspaces[i].name)
			continue;

		const size_t l = wc_xstrlen(workspaces[i].name);
		if (l > longest_ws)
			longest_ws = l;
	}

	return longest_ws;
}

static char *
get_workspace_path_color(const uint8_t num)
{
	if (conf.colorize == 0)
		return df_c;

	if (!workspaces[num].path)
		/* Unset. DL (dividing line) defaults to gray: let's use this */
		return DEF_DL_C;

	struct stat a;
	if (lstat(workspaces[num].path, &a) == -1)
		return uf_c;

	if (check_file_access(a.st_mode, a.st_uid, a.st_gid) == 0)
		return nd_c;

	if (S_ISLNK(a.st_mode)) {
		char p[PATH_MAX + 1];
		*p = '\0';
		char *ret = realpath(workspaces[num].path, p);
		return (ret && *p) ? ln_c : or_c;
	}

	return get_dir_color(workspaces[num].path, a.st_mode, a.st_nlink, -1);
}

static int
list_workspaces(void)
{
	uint8_t i;
	int pad = (int)get_longest_workspace_name();

	for (i = 0; i < MAX_WS; i++) {
		char *path_color = get_workspace_path_color(i);

		if (i == cur_ws)
			printf("%s>%s ", mi_c, df_c);
		else
			fputs("  ", stdout);

		char *ws_color = df_c;

		if (workspaces[i].name) {
			printf("%s%d%s [%s%s%s]: %*s",
				ws_color, i + 1, df_c,
				ws_color, workspaces[i].name, df_c,
				pad - (int)wc_xstrlen(workspaces[i].name), "");
		} else {
			printf("%s%d%s: %*s",
				ws_color, i + 1, df_c,
				pad > 0 ? pad + 3 : 0, "");
		}

		printf("%s%s%s\n",
			path_color,
			workspaces[i].path ? workspaces[i].path : "unset",
			df_c);
	}

	return FUNC_SUCCESS;
}

static int
check_workspace_num(char *str, int *tmp_ws)
{
	const int istr = atoi(str);
	if (istr <= 0 || istr > MAX_WS) {
		xerror(_("ws: %d: No such workspace (valid workspaces: "
			"1-%d)\n"), istr, MAX_WS);
		return FUNC_FAILURE;
	}

	*tmp_ws = istr - 1;

	if (*tmp_ws == cur_ws) {
		xerror(_("ws: %d: Is the current workspace\n"), *tmp_ws + 1);
		return FUNC_SUCCESS;
	}

	return 2;
}

static void
save_workspace_opts(const int n)
{
	free(workspace_opts[n].filter.str);
	workspace_opts[n].filter.str = filter.str
		? savestring(filter.str, strlen(filter.str)) : (char *)NULL;
	workspace_opts[n].filter.rev = filter.rev;
	workspace_opts[n].filter.type = filter.type;
	workspace_opts[n].filter.env = filter.env;

	workspace_opts[n].color_scheme = cur_cscheme;
	workspace_opts[n].files_counter = conf.files_counter;
	workspace_opts[n].light_mode = conf.light_mode;
	workspace_opts[n].list_dirs_first = conf.list_dirs_first;
	workspace_opts[n].long_view = conf.long_view;
	workspace_opts[n].max_files = max_files;
	workspace_opts[n].max_name_len = conf.max_name_len;
	workspace_opts[n].only_dirs = conf.only_dirs;
	workspace_opts[n].pager = conf.pager;
	workspace_opts[n].show_hidden = conf.show_hidden;
	workspace_opts[n].sort = conf.sort;
	workspace_opts[n].sort_reverse = conf.sort_reverse;
}

static void
unset_ws_filter(void)
{
	free(filter.str);
	filter.str = (char *)NULL;
	filter.rev = 0;
	filter.type = FILTER_NONE;
	regfree(&regex_exp);
}

static void
set_ws_filter(const int n)
{
	filter.type = workspace_opts[n].filter.type;
	filter.rev = workspace_opts[n].filter.rev;
	filter.env = workspace_opts[n].filter.env;

	free(filter.str);
	regfree(&regex_exp);
	char *p = workspace_opts[n].filter.str;
	filter.str = savestring(p, strlen(p));

	if (filter.type != FILTER_FILE_NAME)
		return;

	if (regcomp(&regex_exp, filter.str, REG_NOSUB | REG_EXTENDED) != FUNC_SUCCESS)
		unset_ws_filter();
}

static void
set_workspace_opts(const int n)
{
	if (workspace_opts[n].color_scheme
	&& workspace_opts[n].color_scheme != cur_cscheme)
		set_colors(workspace_opts[n].color_scheme, 0);

	if (workspace_opts[n].filter.str && *workspace_opts[n].filter.str) {
		set_ws_filter(n);
	} else {
		if (filter.str)
			unset_ws_filter();
	}

	conf.light_mode = workspace_opts[n].light_mode;
	conf.list_dirs_first = workspace_opts[n].list_dirs_first;
	conf.long_view = workspace_opts[n].long_view;
	conf.files_counter = workspace_opts[n].files_counter;
	max_files = workspace_opts[n].max_files;
	conf.max_name_len = workspace_opts[n].max_name_len;
	conf.only_dirs = workspace_opts[n].only_dirs;
	conf.pager = workspace_opts[n].pager;
	conf.show_hidden = workspace_opts[n].show_hidden;
	conf.sort = workspace_opts[n].sort;
	conf.sort_reverse = workspace_opts[n].sort_reverse;
}

static int
switch_workspace(const int tmp_ws)
{
	/* If new workspace has no path yet, copy the path of the current workspace */
	if (!workspaces[tmp_ws].path) {
		workspaces[tmp_ws].path = savestring(workspaces[cur_ws].path,
		    strlen(workspaces[cur_ws].path));
	} else if (tmp_ws != cur_ws) {
		if (access(workspaces[tmp_ws].path, R_OK | X_OK) != FUNC_SUCCESS) {
			xerror("ws: '%s': %s\n", workspaces[tmp_ws].path, strerror(errno));
			if (conf.autols == 1) press_any_key_to_continue(0);
			free(workspaces[tmp_ws].path);
			workspaces[tmp_ws].path = savestring(workspaces[cur_ws].path,
				strlen(workspaces[cur_ws].path));
		}
	} else {
		xerror(_("ws: %d: Is the current workspace\n"), tmp_ws + 1);
		return FUNC_SUCCESS;
	}

	if (xchdir(workspaces[tmp_ws].path, SET_TITLE) == -1) {
		xerror("ws: '%s': %s\n", workspaces[tmp_ws].path, strerror(errno));
		return FUNC_FAILURE;
	}

	if (conf.private_ws_settings == 1)
		save_workspace_opts(cur_ws);

	prev_ws = cur_ws;
	cur_ws = tmp_ws;
	dir_changed = 1;

	if (conf.colorize == 1 && xargs.eln_use_workspace_color == 1)
		set_eln_color();

	if (conf.private_ws_settings == 1)
		set_workspace_opts(cur_ws);

	if (conf.autols == 1)
		reload_dirlist();

	add_to_dirhist(workspaces[cur_ws].path);
	return FUNC_SUCCESS;
}

/* Return the workspace number corresponding to the workspace name NAME,
 * or -1 if no workspace is named NAME, if error, or if NAME is already
 * the current workspace */
static int
get_workspace_by_name(char *name, const int check_current)
{
	if (!workspaces || !name || !*name)
		return (-1);

	/* CHECK_CURRENT is zero when coming from unset_workspace(), in which
	 * case name is already unescapeed. */
	char *p = check_current == 1 ? unescape_str(name, 0) : (char *)NULL;
	char *q = p ? p : name;

	int n = MAX_WS;
	while (--n >= 0) {
		if (!workspaces[n].name || *workspaces[n].name != *q
		|| strcmp(workspaces[n].name, q) != 0)
			continue;
		if (n == cur_ws && check_current == 1) {
			xerror(_("ws: %s: Is the current workspace\n"), q);
			free(p);
			return (-1);
		}
		free(p);
		return n;
	}

	xerror(_("ws: %s: No such workspace\n"), q);
	free(p);
	return (-1);
}

static int
unset_workspace(char *str)
{
	int n = -1;

	char *name = unescape_str(str, 0);
	if (!name) {
		xerror("ws: '%s': Error unescaping name\n", str);
		return FUNC_FAILURE;
	}

	if (!is_number(name)) {
		if ((n = get_workspace_by_name(name, 0)) == -1)
			goto ERROR;
		n++;
	} else {
		n = atoi(name);
	}

	if (n < 1 || n > MAX_WS) {
		xerror(_("ws: '%s': No such workspace (valid workspaces: "
			"1-%d)\n"), name, MAX_WS);
		goto ERROR;
	}

	n--;
	if (n == cur_ws) {
		xerror(_("ws: '%s': Is the current workspace\n"), name);
		goto ERROR;
	}

	if (!workspaces[n].path) {
		xerror(_("ws: '%s': Already unset\n"), name);
		goto ERROR;
	}

	printf(_("ws: '%s': Workspace unset\n"), name);
	free(name);

	free(workspaces[n].path);
	workspaces[n].path = (char *)NULL;

	return FUNC_SUCCESS;

ERROR:
	free(name);
	return FUNC_FAILURE;
}

int
handle_workspaces(char **args)
{
	if (!args[0] || !*args[0])
		return list_workspaces();

	if (IS_HELP(args[0])) {
		puts(_(WS_USAGE));
		return FUNC_SUCCESS;
	}

	if (args[1] && strcmp(args[1], "unset") == 0)
		return unset_workspace(args[0]);

	int tmp_ws = 0;

	if (is_number(args[0])) {
		const int ret = check_workspace_num(args[0], &tmp_ws);
		if (ret != 2)
			return ret;
	} else if (*args[0] == '+' && !args[0][1]) {
		if ((cur_ws + 1) >= MAX_WS)
			return FUNC_FAILURE;
		tmp_ws = cur_ws + 1;
	} else if (*args[0] == '-' && !args[0][1]) {
			if ((cur_ws - 1) < 0)
				return FUNC_FAILURE;
			tmp_ws = cur_ws - 1;
	} else {
		tmp_ws = get_workspace_by_name(args[0], 1);
		if (tmp_ws == -1)
			return FUNC_FAILURE;
	}

	return switch_workspace(tmp_ws);
}
