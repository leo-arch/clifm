/* autocmds.c -- functions to cmds on a per directory basis */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>

#include "aux.h"
#include "colors.h"
#include "exec.h"
#include "listing.h"
#include "strings.h"

void
reset_opts(void)
{
	opts.color_scheme = cur_cscheme;
	opts.files_counter = files_counter;
	opts.light_mode = light_mode;
	opts.max_files = max_files;
	opts.long_view = long_view;
	opts.show_hidden = show_hidden;
	opts.max_name_len = max_name_len;
	opts.pager = pager;
	opts.sort = sort;
}

int
check_autocmds(void)
{
	int i = (int)autocmds_n, found = 0;
	while (--i >= 0) {
		if (!autocmds[i].pattern)
			continue;

		int rev = 0;
		char *p = autocmds[i].pattern;
		if (*p == '!') {
			++p;
			rev = 1;
		}

		glob_t g;
		int ret = glob(p, GLOB_NOSORT | GLOB_NOCHECK
				| GLOB_TILDE | GLOB_BRACE, NULL, &g);

		if (ret != EXIT_SUCCESS) {
			globfree(&g);
			continue;
		}

		size_t j = 0;
		for (; j < g.gl_pathc; j++) {
			if (*ws[cur_ws].path == *g.gl_pathv[j]
			&& strcmp(ws[cur_ws].path, g.gl_pathv[j]) == 0) {
				found = 1;
				break;
			}
		}
		globfree(&g);

		if (!rev) {
			if (!found)
				continue;
		} else if (found) {
			continue;
		}

		if (!autocmd_set) {
			// Backup current options
			opts.light_mode = light_mode;
			opts.files_counter = files_counter;
			opts.long_view = long_view;
			opts.max_files = max_files;
			opts.show_hidden = show_hidden;
			opts.sort = sort;
			opts.max_name_len = max_name_len;
			opts.pager = pager;
			if (autocmds[i].color_scheme)
				opts.color_scheme = cur_cscheme;
			autocmd_set = 1;
		}

		// Set options for current directory
		if (autocmds[i].light_mode != -1)
			light_mode = autocmds[i].light_mode;
		if (autocmds[i].files_counter != -1)
			files_counter = autocmds[i].files_counter;
		if (autocmds[i].long_view != -1)
			long_view = autocmds[i].long_view;
		if (autocmds[i].show_hidden != -1)
			show_hidden = autocmds[i].show_hidden;
		if (autocmds[i].pager != -1)
			pager = autocmds[i].pager;
		if (autocmds[i].sort != -1)
			sort = autocmds[i].sort;
		if (autocmds[i].max_name_len != -1)
			max_name_len = autocmds[i].max_name_len;
		if (autocmds[i].max_files != -2)
			max_files = autocmds[i].max_files;
		if (autocmds[i].color_scheme)
			set_colors(autocmds[i].color_scheme, 0);
		if (autocmds[i].cmd) {
//			if (*autocmds[i].cmd != '!') {
			launch_execle(autocmds[i].cmd);
/*			} else {
//				size_t old_args_n = args_n;
				free_dirlist();
				char **cmd = parse_input_str(autocmds[i].cmd + 1);
				if (!cmd)
					break;
				exec_cmd(cmd);
//				args_n = old_args_n;
				for (j = 0; cmd[j]; j++)
					free(cmd[j]);
				free(cmd);
			} */
		}

		break;
	}

	return found;
}

void
revert_autocmd_opts(void)
{
	light_mode = opts.light_mode;
	files_counter = opts.files_counter;
	long_view = opts.long_view;
	max_files = opts.max_files;
	show_hidden = opts.show_hidden;
	max_name_len = opts.max_name_len;
	pager = opts.pager;
	sort = opts.sort;
	if (opts.color_scheme && opts.color_scheme != cur_cscheme)
		set_colors(opts.color_scheme, 0);
	autocmd_set = 0;
}

void
free_autocmds(void)
{
	int i = (int)autocmds_n;
	while (--i >= 0) {
		free(autocmds[i].pattern);
		free(autocmds[i].cmd);
	}
	free(autocmds);
	autocmds = (struct autocmds_t *)NULL;
	autocmds_n = autocmd_set = 0;

	opts.color_scheme = (char *)NULL;
}

static void
set_autocmd_opt(char *opt)
{
	if (!opt || !*opt)
		return;

	if (*opt == '!' && *(++opt)) {
		free(autocmds[autocmds_n].cmd);
		autocmds[autocmds_n].cmd = savestring(opt, strlen(opt));
		return;
	}

	char *p = strchr(opt, '=');
	if (!p || !*(++p))
		return;

	*(p - 1) = '\0';
	if (*opt == 'c' && opt[1] == 's') {
		int i = (int)cschemes_n;
		while (--i >= 0) {
			if (*color_schemes[i] == *p && strcmp(color_schemes[i], p) == 0) {
				autocmds[autocmds_n].color_scheme = color_schemes[i];
				break;
			}
		}
	} else if (*opt == 'f' && opt[1] == 'c')
		autocmds[autocmds_n].files_counter = atoi(p);
	else if (*opt == 'h' && opt[1] == 'f')
		autocmds[autocmds_n].show_hidden = atoi(p);
	else if (*opt == 'l' && opt[1] == 'm')
		autocmds[autocmds_n].light_mode = atoi(p);
	else if (*opt == 'l' && opt[1] == 'v')
		autocmds[autocmds_n].long_view = atoi(p);
	else if (*opt == 'm' && opt[1] == 'f')
		autocmds[autocmds_n].max_files = atoi(p);
	else if (*opt == 'm' && opt[1] == 'n')
		autocmds[autocmds_n].max_name_len = atoi(p);
	else if (*opt == 'p' && opt[1] == 'g')
		autocmds[autocmds_n].pager = atoi(p);
	else if (*opt == 's' && opt[1] == 't')
		autocmds[autocmds_n].sort = atoi(p);
}

static void
init_autocmd_opts()
{
	autocmds[autocmds_n].cmd = (char *)NULL;
	autocmds[autocmds_n].color_scheme = opts.color_scheme;
	autocmds[autocmds_n].files_counter = opts.files_counter;
	autocmds[autocmds_n].light_mode = opts.light_mode;
	autocmds[autocmds_n].long_view = opts.long_view;
	autocmds[autocmds_n].max_files = max_files;
	autocmds[autocmds_n].max_name_len = max_name_len;
	autocmds[autocmds_n].pager = opts.pager;
	autocmds[autocmds_n].show_hidden = opts.show_hidden;
	autocmds[autocmds_n].sort = sort;
}

void
parse_autocmd_line(char *cmd)
{
	if (!cmd || !*cmd)
		return;

	size_t clen = strlen(cmd);
	if (cmd[clen - 1] == '\n')
		cmd[clen - 1] = '\0';

	char *p = strchr(cmd, ' ');
	if (!p || !*(p + 1))
		return;

	*p = '\0';

	autocmds = (struct autocmds_t *)xrealloc(autocmds,
				(autocmds_n + 1) * sizeof(struct autocmds_t));
	autocmds[autocmds_n].pattern = savestring(cmd, strlen(cmd));

	init_autocmd_opts();

	char *q = ++p;
	while (1) {
		char *val = (char *)NULL;
		if (*p == ',') {
			*(p++) = '\0';
			val = q;
			q = p;
		} else if (!*p) {
			val = q;
		} else {
			++p;
			continue;
		}

		set_autocmd_opt(val);

		if (!*p)
			break;
	}

	autocmds_n++;
}
