/* highlight.c -- a simple function to perform syntax highlighting */

/*
 * This file is part of CliFM
 * 
 * Copyright (C) 2016-2022, L. Abramovich <johndoe.arch@outlook.com>
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

#include "strings.h"
#include "checks.h"
#include "aux.h"

/* Macros for single and double quotes */
#define _SINGLE 0
#define _DOUBLE 1

/* Change the color of the word _LAST_WORD, at offset OFFSET, to COLOR
 * in the current input string */
/*void
change_word_color(const char *_last_word, const int offset, const char *color)
{
	UNUSED(_last_word);
	fputs("\x1b[?25l", stdout);
	char *p = rl_copy_text(offset, rl_end);
	rl_delete_text(offset, rl_end);
	rl_point = rl_end = offset;
	rl_redisplay();
	fputs(color, stdout);
	rl_insert_text(p);
	rl_redisplay();
	free(p);
	fputs(tx_c, stdout);
	fputs("\x1b[?25h", stdout);
} */

/* Get the appropriate color for the character at position POS in the string STR
 * and print the color if SET_COLOR is set to 1 (in which case NULL is returned);
 * otherwise, just return a pointer to the corresponding color.
 * This function is used to colorize input, history entries, and accepted suggestions */
char *
rl_highlight(char *str, const size_t pos, const int flag)
{
	char *cl = (char *)NULL;
	/* PREV is 0 when there is no previous char (STR[POS] is the first one) */
	char prev = pos ? str[pos - 1] : 0;
	char c = *(str + pos);

	if (wrong_cmd == 1 && cur_color == hw_c && rl_end == 0) {
		fputs(tx_c, stdout); fflush(stdout);
		rl_redisplay();
	}

	if ((rl_end == 0 && c == BS) || prev == '\\') {
		if (prev == '\\')
			goto END;
		cl = tx_c;
		goto END;
	}

	if (cur_color == hc_c)
		goto END;

	char *sp = strchr(rl_line_buffer, ' ');
	if (cur_color == hw_c && !sp)
		goto END;

	if (*rl_line_buffer != ';' && *rl_line_buffer != ':'
	&& cur_color != hq_c && c >= '0' && c <= '9') {
/*		if (prev == ' ' || cur_color == hn_c || rl_end == 1) {
			cl = hn_c;
			goto END;
		} */
// TESTING! HIGHLIGHT
		if (prev == ' ' || prev == 0 || cur_color == hn_c || rl_end == 1) {
			char *a = strchr(str + pos, ' ');
			if (a) {
				*a = '\0';
				if (is_number(str + pos))
					cl = hn_c;
				*a = ' ';
			} else {
				cl = hn_c;
			}
			goto END;
// TESTING! HIGHLIGHT
		} else {
			char cc = c;
			*(str + pos) = '\0';
			int ret = is_internal_f(str);
			*(str + pos) = cc;
			if (ret) {
				cl = hn_c;
				goto END;
			}
		}
	}

	size_t qn[2] = {0};
	size_t m = 0;
	for (; m < (size_t)rl_point; m++) {
		if (rl_line_buffer[m] == '\'') {
			if (qn[_DOUBLE] == 1 || (m && rl_line_buffer[m - 1] == '\\'))
				continue;
			qn[_SINGLE]++;
			if (qn[_SINGLE] > 2)
				qn[_SINGLE] = 1;
		} else {
			if (rl_line_buffer[m] == '"') {
				if (qn[_SINGLE] == 1 || (m && rl_line_buffer[m - 1] == '\\'))
					continue;
				qn[_DOUBLE]++;
				if (qn[_DOUBLE] > 2)
					qn[_DOUBLE] = 1;
			}
		}
	}

	if (prev != 0) {
		switch(prev) {
		case ')': /* fallthrough */
		case ']': /* fallthrough */
		case '}': cl = tx_c; break;
		case '\'':
			if (cur_color == hq_c && qn[_SINGLE] == 2)
				cl = tx_c;
			break;
		case '"':
			if (cur_color == hq_c && qn[_DOUBLE] == 2)
				cl = tx_c;
			break;
		default: break;
		}
	}

	switch(c) {
	case ' ':
		if (cur_color != hq_c && cur_color != hc_c)
			cl = tx_c;
		break;
	case '/': cl = (cur_color != hq_c) ? hd_c : (char *)NULL; break;
	case '\'': /* fallthrough */
	case '"': cl = hq_c; break;
	case '\\': /* fallthrough */
	case ENTER: cl = tx_c; break;
	case '~': /* fallthrough */
	case '*': cl = (cur_color != hq_c) ? he_c : (char *)NULL; break;
	case '=': /* fallthrough */
	case '(': /* fallthrough */
	case ')': /* fallthrough */
	case '[': /* fallthrough */
	case ']': /* fallthrough */
	case '{': /* fallthrough */
	case '}': cl = (cur_color != hq_c) ? hb_c : (char *)NULL; break;
	case '|': /* fallthrough */
	case '&': /* fallthrough */
	case ';': cl = (cur_color != hq_c) ? hs_c : (char *)NULL; break;
	case '<': /* fallthrough */
	case '>': cl = (cur_color != hq_c) ? hr_c : (char *)NULL; break;
	case '$': cl = (cur_color != hq_c) ? hv_c : (char *)NULL; break;
	case '-':
		if (prev == ' ' || prev == 0)
			cl = (cur_color != hq_c) ? hp_c : (char *)NULL;
		break;
	case '#':
		if (prev == ' ' || prev == 0)
			cl = (cur_color != hq_c) ? hc_c : (char *)NULL;
		else
			cl = tx_c;
		break;
	default:
		if (cur_color != hq_c && cur_color != hc_c
		&& cur_color != hv_c && cur_color != hp_c)
			cl = tx_c;
		break;
	}

	if (cur_color == hq_c) {
		if (qn[_SINGLE] == 1)
			cl = (char *)NULL;
		else if (qn[_DOUBLE] == 1)
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
	/* Hide the cursor to minimize flickering */
	fputs(HIDE_CURSOR, stdout);

	/* Set text color to default */
	if (cur_color != tx_c && cur_color != hw_c && cur_color != hn_c) {
		cur_color = tx_c;
		fputs(tx_c, stdout);
	}

	int bk_point = rl_point;
	if (rl_point > 0 && rl_point != rl_end)
		rl_point--;

	/* Get the current color up to the current cursor position */
	size_t i;
	char *cl = (char *)NULL;
	for (i = 0; i < (size_t)rl_point; i++) {
		cl = rl_highlight(rl_line_buffer, i, INFORM_COLOR);
		if (cl)
			cur_color = cl;
	}

	if (cl)
		fputs(cl, stdout);

	if (rl_point == 0 && rl_end == 0) {
		fputs(UNHIDE_CURSOR, stdout);
		return;
	}

	int end_bk = rl_end;
	int start = rl_point > 0 ? rl_point - 1 : 0;
	char *ss = rl_copy_text(start, rl_end);
	rl_delete_text(start, rl_end);
	rl_point = rl_end = start;
// TESTING HIGHLIGHT!
	if (start == 0 && end_bk > 1)
		/* First char of a non-empty recolored line (recovering from wrong cmd) */
		rl_redisplay();
// TESTING HIGHLIGHT!
	i = 0;

/*	int point = rl_point;
	int copy_start = point > 0 ? point - 1 : 0;
	int start = point;
	char *ss = rl_copy_text(copy_start, rl_end);
	rl_delete_text(start, rl_end);
	rl_point = rl_end = start;
	i = point ? 1 : 0; */

	size_t l = 0;

	if (!ss || !*ss)
		goto EXIT;

	// Loop through each char from cursor position onward and colorize it
	char t[PATH_MAX];
	for (;ss[i]; i++) {
		rl_highlight(ss, i, SET_COLOR);
		/* Redisplay the current char with the appropriate color */
		if (ss[i] < 0) {
			t[l] = ss[i];
			l++;
			if (ss[i + 1] >= 0) {
				t[l] = '\0';
				l = 0;
				rl_insert_text(t);
				rl_redisplay();
			}
			continue;
		}
		t[0] = (char)ss[i];
		t[1] = '\0';
		rl_insert_text(t);
		rl_redisplay();
	}

EXIT:
	free(ss);
	rl_point = bk_point;
	fputs(UNHIDE_CURSOR, stdout);
}
