/* highlight.c -- a simple function to perform syntax highlighting */

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
#ifdef __OpenBSD__
typedef char *rl_cpvfunc_t;
#include <ereadline/readline/readline.h>
#else
#include <readline/readline.h>
#endif

/* Macros for single and double quotes */
#define _SINGLE 0
#define _DOUBLE 1

/* Get the appropriate color for C and print the color. This function is
 * used to colorize input, history entries, and accepted suggestions */
void
rl_highlight(unsigned char c)
{
	char prev = rl_line_buffer[rl_end ? rl_end - 1 : 0];

	if ((rl_end == 0 && c == BS)
	|| prev == '\\') {
		cur_color = df_c;
		fputs(df_c, stdout);
		return;
	}

	if (cur_color == hc_c)
		return;

	char *sp = strchr(rl_line_buffer, ' ');

	if (cur_color == hw_c && !sp)
		return;

	if (!sp)
		wrong_cmd_line = 0;

	if (c >= '0' && c <= '9' && (prev == ' '
	|| cur_color == hn_c || rl_end == 0)) {
		cur_color = hn_c;
		fputs(hn_c, stdout);
		return;
	}

	char *cl = (char *)NULL;
	int m = rl_point;
	size_t qn[2] = {0};
	m--;

	while (m >= 0) {
		if (rl_line_buffer[m] == '\'')
			qn[_SINGLE]++;
		else if (rl_line_buffer[m] == '"')
			qn[_DOUBLE]++;
		--m;
	}

	switch(rl_line_buffer[rl_end ? rl_end - 1 : 0]) {
	case ')': /* fallthrough */
	case ']': /* fallthrough */
	case '}': cl = df_c; break;
	case '\'':
		if (qn[_SINGLE] % 2 == 0)
			cl = df_c;
		break;
	case '"':
		if (qn[_DOUBLE] % 2 == 0)
			cl = df_c;
		break;
	}

	switch(c) {
	case ' ':
		if (cur_color != hq_c && cur_color != hc_c && cur_color != hb_c)
			cl = df_c;
		break;
	case '\'': /* fallthrough */
	case '"': cl = hq_c; break;
	case '\\': /* fallthrough */
	case ENTER: cl = df_c; break;
	case '~': /* fallthrough */
	case '*': cl = he_c; break;
	case '(': /* fallthrough */
	case ')': /* fallthrough */
	case '[': /* fallthrough */
	case ']': /* fallthrough */
	case '{': /* fallthrough */
	case '}': cl = hb_c; break;
	case '|': /* fallthrough */
	case '&': /* fallthrough */
	case ';': cl = hs_c; break;
	case '>': cl = hr_c; break;
	case '$': cl = hv_c; break;
	case '-':
		if (prev == SPACE)
			cl = hp_c;
		break;
	case '#': cl = hc_c; break;
	default:
		if (cur_color != hq_c && cur_color != hb_c && cur_color != hc_c
		&& cur_color != hv_c && cur_color != hp_c)
			cl = df_c;
		break;
	}

	if (cl && cl != cur_color) {
		cur_color = cl;
		fputs(cl, stdout);
	}
}
