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

/* Get the appropriate color for C and print the color (returning a null
 * pointer) if SET_COLOR is set to 1; otherwise, just return a pointer
 * to the corresponding color. This function is used to colorize input,
 * history entries, and accepted suggestions */
char *
rl_highlight(unsigned char c, const int flag)
{
	char *cl = (char *)NULL;
	char prev = rl_line_buffer[rl_end ? rl_end - 1 : 0];

	if ((rl_end == 0 && c == BS) || prev == '\\') {
		if (prev == '\\')
			goto END;
		cl = df_c;
		goto END;
	}

	if (cur_color == hc_c)
		goto END;

	char *sp = strchr(rl_line_buffer, ' ');

	if (cur_color == hw_c && !sp)
		goto END;

	if (!sp)
		wrong_cmd_line = 0;

	if (c >= '0' && c <= '9' && (prev == ' '
	|| cur_color == hn_c || rl_end == 0)) {
		cl = hn_c;
		goto END;
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

	switch(rl_line_buffer[rl_end ? rl_end - 1 : 0]) {
	case ')': /* fallthrough */
	case ']': /* fallthrough */
	case '}': cl = df_c; break;
	case '\'':
		if (cur_color == hq_c && qn[_SINGLE] % 2 == 0)
			cl = df_c;
		break;
	case '"':
		if (cur_color == hq_c && qn[_DOUBLE] % 2 == 0)
			cl = df_c;
		break;
	default: break;
	}

	switch(c) {
	case ' ':
		if (cur_color != hq_c && cur_color != hc_c /*&& cur_color != hb_c */)
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
		if (prev == ' ')
			cl = hp_c;
		break;
	case '#': cl = hc_c; break;
	default:
		if (cur_color != hq_c && /*cur_color != hb_c &&*/ cur_color != hc_c
		&& cur_color != hv_c && cur_color != hp_c)
			cl = df_c;
		break;
	}

	if (cur_color == hq_c) {
		if (qn[_SINGLE] % 2 != 0)
			cl = (char *)NULL;
		else if (qn[_DOUBLE] % 2 != 0)
			cl = (char *)NULL;
	}

END:
	if (flag == SET_COLOR) {
		if (cl && cl != cur_color) {
			cur_color = cl;
			fputs(cl, stdout);
		}
		return (char *)NULL;
	}

	if (!cl)
		return cur_color;
	return cl;
}

/* Recolorize current input line starting from rl_point */
void
recolorize_line(void)
{
	// Hide the cursor to minimize flickering
	fputs("\x1b[?25l", stdout);

	// Set text color to default
	cur_color = df_c;
	fputs(df_c, stdout);

	// Get the current color up to the current cursor position
	size_t i = 0;
	char *cl = (char *)NULL;
	for (; i < (size_t)rl_point; i++) {
		cl = rl_highlight((unsigned char)rl_line_buffer[i], INFORM_COLOR);
		if (cl)
			cur_color = cl;
	}

	if (cl)
		fputs(cl, stdout);

	int sp = strcntchr(rl_line_buffer, ' ');
	int bk = rl_point - 1;
	char *ss = rl_copy_text(bk, rl_end);
	rl_delete_text(bk, rl_end);
	rl_point = rl_end = bk;

	// Loop through each char from cursor position onward and colorize it
	i = 0;
	for (;ss[i]; i++) {
		// Let's keep the color of wrong commands
		if (wrong_cmd_line && rl_point < sp) {
			cur_color = hw_c;
			fputs(hw_c, stdout);
		} else {
			rl_highlight((unsigned char)ss[i], SET_COLOR);
		}

		// Redisplay the current char with the appropriate color
		char t[2];
		t[0] = (char)ss[i];
		t[1] = '\0';
		rl_insert_text(t);
		rl_redisplay();
	}

	// Unhide the cursor
	fputs("\x1b[?25h", stdout);
	free(ss);
	rl_point = ++bk;
}
