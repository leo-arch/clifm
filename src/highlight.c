/* highlight.c Function performing syntax highlighting */

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

#ifndef _NO_HIGHLIGHT

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

static char *
get_cur_color(const int point)
{
	int m = point; /* POINT is here rl_point */
	int sep = -1, sp = -1, t = -1;
	char *c = (char *)NULL;
	m--;

	while (m >= 0) {
		switch(rl_line_buffer[m]) {
		case ' ': sp = m; break;
		case '&': // fallthrough
		case '|': // fallthrough
		case ';': sep = m; break;
		case '\'': // fallthrough
		case '"': c = hq_c; t = m; break;
		case '-': c = hp_c; t = m; break;
		case '#': c = hc_c; t = m; break;
		case '$': c = hv_c; t = m; break;
		case '(': // fallthrough
		case '[': // fallthrough
		case '{': c = hb_c; t = m; break;
		default: c = df_c; break;
		}

		if (t != -1)
			break;

		--m;
	}

/*	int n = 0;
	while (n < m) {
		if (n > sp && n > sep) {
			switch(rl_line_buffer[n]) {
			case '\'': // fallthrough
			case '"': return hq_c;
			case '(': // fallthrough
			case '[': // fallthrough
			case '{': return hb_c;
			case '#': return hc_c;
			default: break;
			}
		}

		n++;
	} */

	if (t != -1) {
		if (c == hc_c || c == hq_c) {
			if (t > sep)
				return c;
			else
				return df_c;
		} else {
			if (t > sep && t > sp)
				return c;
			else
				return df_c;
		}
	}

	return df_c;
}

static char *
get_highlight_color(const unsigned char c, const size_t *qn, const int point)
{
	if (c >= '0' && c <= '9' && cur_color != hq_c && cur_color != hc_c
	&& cur_color != hb_c) {
		// Colorize numbers only if first word or previous char is space
		if (rl_line_buffer[point ? point - 1 : 0] == ' '
		|| rl_line_buffer[point ? point - 1 : 0] == '-' || rl_end == 0)
			return hn_c;
		return (char *)NULL;
	}

	char *cl = cur_color;
	char *p = (char *)NULL;
	static int open_quote = 0;

	switch(c) {
//	case '/': p = hv_c; break;
	case '{': // fallthrough
	case '}': // fallthrough
	case '(': // fallthrough
	case ')': // fallthrough
	case '[': // fallthrough
	case ']': p = hb_c; break;

	case '#': p = hc_c; break;
	case '~': // fallthrough
	case '-':
		if (rl_line_buffer[point ? point - 1 : 0] == ' ')
			p = (c == '~' ? he_c : hp_c);
		break;
	case '*': p = he_c; break;

	case '\'': // fallthrough
	case '"':
		if ((qn[(c == '\'' ? _SINGLE : _DOUBLE)] + 1) % 2 == 0) {
			open_quote = 0;
			p = df_c;
		} else {
			p = hq_c;
			open_quote = 1;
		}
		break;

	case '>': p = hr_c; break;
	case '|': // fallthrough
	case ';': // fallthrough
	case '&': p = hs_c; break;
	case '$': p = hv_c; break;

	case ENTER: p = df_c; break;

	case ' ': return (char *)NULL;
		// It works, but open_quote should be !open_quote
		if (open_quote && cl != hc_c)
			p = df_c;
		break;

	default:
		if (point < rl_end)
			p = get_cur_color(point);
		else if (open_quote && cl != hv_c && cl != hp_c && cl != hc_c)
			p = df_c;
		break;
	}

	return p;
}

void
rl_highlight(const unsigned char c)
{
	if (rl_readline_state & RL_STATE_MOREINPUT)
		return;

	if (c < 32 && c != BS && c != ENTER)
		return;

	if (rl_end == 1 && (c == BS || c == 127)) {
		cur_color = df_c;
		return;
	}

/*	if (rl_end == 0 || rl_line_buffer[rl_end ? rl_end - 1 : 0] == ' ') {
		if (c == '/') {
			cur_color = sf_c;
			fputs(sf_c, stdout);
			return;
		}
		int i = (int)files;
		while (--i >= 0) {
			if (c == *file_info[i].name) {
				cur_color = sf_c;
				fputs(sf_c, stdout);
				return;
			}
		}
	}

	if (cur_color == sf_c && c == ' ') {
		cur_color = df_c;
		fputs("\x1b[0m", stdout);
		fputs(df_c, stdout);
		return;
	} */

	// Restore default color
	switch(rl_line_buffer[rl_end ? rl_end - 1 : 0]) {
//	case '/': // fallthrough
	case ' ': // fallthrough
	case ')': // fallthrough
	case '}': // fallthrough
	case ']': // fallthrough
	case '|': // fallthrough
	case '*': // fallthrough
	case ';': // fallthrough
	case '&': // fallthrough
	case '>':
		if (rl_point == rl_end && cur_color != hc_c && cur_color != hq_c) {
			fputs(df_c, stdout);
			cur_color = df_c;
		}
		break;
	default: break;
	}

	if ((c < '0' || c > '9') && cur_color == hn_c) {
		fputs(df_c, stdout);
		cur_color = df_c;
	} else if ((c == BS || c == 127)
	&& rl_line_buffer[rl_end ? rl_end - 1 : 0] == '#') {
		fputs(df_c, stdout);
		cur_color = df_c;
	}

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

	char *p = rl_line_buffer;
	char *cl = (char *)NULL;
	int bk = rl_point;
	rl_point = 0;

	for (rl_point = 0; p[rl_point]; rl_point++)
		cl = get_highlight_color((unsigned char)p[rl_point], qn, bk);

	if (cl != hc_c && cl != hb_c)
		cl = get_highlight_color(c, qn, bk);

	rl_point = bk;

	int skip = 0;
	if ((c == '\'' && qn[_SINGLE] % 2 != 0)
	|| (c == '"' && qn[_DOUBLE] % 2 != 0))
		skip = 1;

	if (!skip && cl && cl != cur_color) {
		cur_color = cl;
		fputs(cl, stdout);
	}

	return;
}
#endif
