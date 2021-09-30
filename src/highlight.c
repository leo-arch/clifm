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

/* Change the color of the word _LAST_WORD, at offset OFFSET, to COLOR
 * in the current input string */
void
change_word_color(const char *_last_word, const int offset, const char *color)
{
	UNUSED(_last_word);
	int bk = rl_point;
	fputs("\x1b[?25l", stdout);
	char *p = rl_copy_text(0, rl_end);
	rl_delete_text(offset, rl_end);
	rl_point = rl_end = offset;
	rl_redisplay();
	fputs(color, stdout);
	rl_insert_text(p);
	free(p);
	rl_point = bk;
	fputs("\x1b[?25h", stdout);
}

/* Get the appropriate color for C and print the color (returning a null
 * pointer) if SET_COLOR is set to 1; otherwise, just return a pointer
 * to the corresponding color. This function is used to colorize input,
 * history entries, and accepted suggestions */
char *
rl_highlight(const char *str, const size_t pos, const int flag)
{
	char *cl = (char *)NULL;
	// PREV is -1 when there is no previous char (STR[POS] is the first)
	char prev = pos ? str[pos - 1] : 0;
	char c = *(str + pos);

//	printf("'%zu:%zu' ", pos, strlen(str));
//	size_t len = strlen(str);
//	if (flag == SET_COLOR)
//		printf("'%c:%c(%zu:%zu:%d):%s'\n", c, prev, pos, len, rl_end, str);

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
	--m;

	while (m >= 0) {
		if (rl_line_buffer[m] == '\'')
			qn[_SINGLE]++;
		else if (rl_line_buffer[m] == '"') {
			qn[_DOUBLE]++;
		}
		--m;
	}

	if (prev != 0) {
		switch(prev) {
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
	}

	switch(c) {
	case ' ':
		if (cur_color != hq_c && cur_color != hc_c /*&& cur_color != hb_c */)
			cl = df_c;
		break;
	case '\'': /* fallthrough */
	case '/': cl = hd_c; break;
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
		if (prev == ' ' || prev == 0) {
			cl = hp_c;
		}
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
//	if (cur_color != df_c && cur_color != hw_c) {
	cur_color = df_c;
	fputs(df_c, stdout);
//	}

	int bk = rl_point;
	if (rl_point && rl_point != rl_end)
		rl_point--;

	// Get the current color up to the current cursor position
	size_t i = 0;
	char *cl = (char *)NULL;
	for (; i < (size_t)rl_point; i++) {
		cl = rl_highlight(rl_line_buffer, i, INFORM_COLOR);
		if (cl)
			cur_color = cl;
	}

	if (cl)
		fputs(cl, stdout);

	if (rl_point == 0 && rl_end == 0) {
		fputs("\x1b[?25h", stdout);
		return;
	}

	int point = rl_point;
	char *ss = rl_copy_text(rl_point? rl_point - 1 : 0, rl_end);
	rl_delete_text(rl_point, rl_end);
	rl_point = rl_end = point;

	// Loop through each char from cursor position onward and colorize it
	i = rl_point ? 1 : 0;
	for (;ss[i]; i++) {
		// Let's keep the color of wrong commands
/*		if (wrong_cmd_line && (sp < 0 || (int)i < sp)) {
			cur_color = hw_c;
			fputs(hw_c, stdout);
		} else { */
			rl_highlight(ss, i, SET_COLOR);
			
//		}
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
	rl_point = bk;
}
