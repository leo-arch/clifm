/* suggestions.c -- functions to manage the suggestions system */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
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
#ifndef _NO_SUGGESTIONS

#include "helpers.h"

#include <stdio.h>
#include <string.h>
#include <strings.h> /* str(n)casecmp() */
#include <unistd.h>
#include <pwd.h>

#if defined(__linux__)
# include <sys/capability.h>
#endif /* __linux__ */

#if defined(__OpenBSD__)
typedef char *rl_cpvfunc_t;
# include <ereadline/readline/readline.h>
#else
# include <readline/readline.h>
#endif /* __OpenBSD__ */

#include "aux.h"
#include "checks.h"
#include "colors.h"
#include "fuzzy_match.h"
#include "jump.h"
#include "messages.h"
#include "navigation.h" /* fastback() */
#include "readline.h"
#include "builtins.h"
#include "prompt.h"

#ifndef _NO_HIGHLIGHT
# include "highlight.h"
#endif /* !_NO_HIGHLIGHT */

#define NO_MATCH      0
#define PARTIAL_MATCH 1
#define FULL_MATCH    2

#define CHECK_MATCH 0
#define PRINT_MATCH 1

#define BAEJ_OFFSET 1

static char *last_word = (char *)NULL;
static int last_word_offset = 0;
static int point_is_first_word = 0;

/*
#ifndef _NO_HIGHLIGHT
// Change the color of the word _LAST_WORD, at offset OFFSET, to COLOR
// in the current input string
static void
change_word_color(const char *_last_word, const int offset, const char *color)
{
	int bk = rl_point;
	fputs("\x1b[?25l", stdout);
	rl_delete_text(offset, rl_end);
	rl_point = rl_end = offset;
	rl_redisplay();
	fputs(color, stdout);
	rl_insert_text(_last_word);
	rl_point = bk;
	fputs("\x1b[?25h", stdout);
}
#endif // !_NO_HIGHLIGHT */

static char *
check_int_cmd_desc(const char *s, const size_t l)
{
	if (!s || !*s)
		return (char *)NULL;

	if (l == 1) {
		if (*s == 'b') return B_DESC;
		if (*s == 'c') return C_DESC;
		if (*s == 'd') return D_DESC;
		if (*s == 'f') return F_DESC;
		if (*s == 'j') return J_DESC;
		if (*s == 'l') return L_DESC;
		if (*s == 'm') return M_DESC;
		if (*s == 'n') return N_DESC;
		if (*s == 'o') return O_DESC;
		if (*s == 'p') return P_DESC;
		if (*s == 'q') return Q_DESC;
		if (*s == 'Q') return QU_DESC;
		if (*s == 'r') return R_DESC;
		if (*s == 's') return SEL_DESC;
		if (*s == 't') return TRASH_DESC;
		if (*s == 'u') return U_DESC;
//		if (*s == 'v') return V_DESC;
		if (*s == 'x') return X_DESC;
		if (*s == 'X') return XU_DESC;
	}

	else if (l == 2) {
		if (*s == 'a') {
			if (*(s + 1) == 'c') return AC_DESC;
			if (*(s + 1) == 'd') return AD_DESC;
			if (*(s + 1) == 'o') return AO_DESC;
		}

		else if (*s == 'b') {
			if (*(s + 1) == 'b') return BB_DESC;
			if (*(s + 1) == 'd') return BD_DESC;
			if (*(s + 1) == 'l') return BL_DESC;
			if (*(s + 1) == 'm') return BM_DESC;
			if (*(s + 1) == 'r') return BR_DESC;
		}

		else if (*s == 'c') {
			if (*(s + 1) == 'd') return CD_DESC;
			if (*(s + 1) == 'l') return CL_DESC;
			if (*(s + 1) == 's') return CS_DESC;
		}

		else if (*s == 'd') {
			if (*(s + 1) == 's') return DS_DESC;
			if (*(s + 1) == 'h') return DH_DESC;
		}

		else if (*s == 'f') {
			if (*(s + 1) == 'c') return FC_DESC;
			if (*(s + 1) == 'f') return FF_DESC;
			if (*(s + 1) == 's') return FS_DESC;
			if (*(s + 1) == 't') return FT_DESC;
			if (*(s + 1) == 'z') return FZ_DESC;
		}

		else if (*s == 'h' && (*(s + 1) == 'f' || *(s + 1) == 'h'))
			return HF_DESC;

		else if (*s == 'k') {
			if (*(s + 1) == 'b') return KB_DESC;
		}

		else if (*s == 'l') {
			if (*(s + 1) == 'l' || *(s + 1) == 'v') return LL_DESC;
			if (*(s + 1) == 'm') return LM_DESC;
		}

		else if (*s == 'm') {
			if (*(s + 1) == 'd') return MD_DESC;
			if (*(s + 1) == 'f') return MF_DESC;
			if (*(s + 1) == 'm') return MM_DESC;
			if (*(s + 1) == 'p') return MP_DESC;
		}

		else if (*s == 'l') {
			if (*(s + 1) == 'e') return LE_DESC;
		}

		else if (*s == 'o') {
			if (*(s + 1) == 'c') return OC_DESC;
			if (*(s + 1) == 'w') return OW_DESC;
		}

		else if (*s == 'p') {
			if (*(s + 1) == 'c') return PC_DESC;
			if (*(s + 1) == 'f') return PF_DESC;
			if (*(s + 1) == 'g') return PG_DESC;
//			if (*(s + 1) == 'r') return P_DESC;
			if (*(s + 1) == 'p') return PP_DESC;
		}

		else if (*s == 'r') {
			if (*(s + 1) == 'f') return RF_DESC;
			if (*(s + 1) == 'l') return RL_DESC;
			if (*(s + 1) == 'r') return RR_DESC;
		}

		else if (*s == 's') {
			if (*(s + 1) == 'b') return SB_DESC;
			if (*(s + 1) == 't') return ST_DESC;
		}

		else if (*s == 't') {
			if (*(s + 1) == 'a') return TA_DESC;
			if (*(s + 1) == 'd') return TD_DESC;
			if (*(s + 1) == 'e') return TE_DESC;
			if (*(s + 1) == 'l') return TL_DESC;
			if (*(s + 1) == 'm') return TM_DESC;
			if (*(s + 1) == 'n') return TN_DESC;
//			if (*(s + 1) == 'r') return TRASH_DESC;
			if (*(s + 1) == 'u') return TU_DESC;
			if (*(s + 1) == 'y') return TY_DESC;
		}

		else if (*s == 'v' && *(s + 1) == 'v')
			return VV_DESC;

		else if (*s == 'w') {
			if (*(s + 1) == 's') return WS_DESC;
		}

	} else if (l == 3) {
		if (*s == 'a' && *(s + 1) == 'c' && *(s + 2) == 'd')
			return ACD_DESC;
		if (*s == 'c' && *(s + 1) == 'm' && *(s + 2) == 'd')
			return CMD_DESC;
//		if (*s == 'c' && *(s + 1) == 'w' && *(s + 2) == 'd')
//			return CWD_DESC;
		if (*s == 'd' && *(s + 1) == 'u' && *(s + 2) == 'p')
			return D_DESC;
		if (*s == 'e' && *(s + 1) == 'x') {
			if (*(s + 2) == 'p') return EXP_DESC;
			if (*(s + 2) == 't') return EXT_DESC;
		}
		if (*s == 'l' && *(s + 1) == 'o' && *(s + 2) == 'g')
			return LOG_DESC;
		if (*s == 'm' && *(s + 1) == 's' && *(s + 2) == 'g')
			return MSG_DESC;
		if (*s == 'n' && *(s + 1) == 'e') {
			if (*(s + 2) == 't') return NET_DESC;
			if (*(s + 2) == 'w') return N_DESC;
		}
		if (*s == 'p' && *(s + 1) == 'i' && *(s + 2) == 'n')
			return PIN_DESC;
		if (*s == 's' && *(s + 1) == 'e' && *(s + 2) == 'l')
			return SEL_DESC;
		if (*s == 't' && *(s + 1) == 'a' && *(s + 2) == 'g')
			return TAG_DESC;
		if (*s == 'v' && *(s + 1) == 'e' && *(s + 2) == 'r')
			return VER_DESC;
	}

	else if (l == 4) {
		if (*s == 'b' && strcmp(s + 1, "ack") == 0)
			return B_DESC;
		if (*s == 'b' && strcmp(s + 1, "ulk") == 0)
			return BR_DESC;
		if (*s == 'e' && strcmp(s + 1, "dit") == 0)
			return EDIT_DESC;
//		if (*s == 'j' && strcmp(s + 1, "ump") == 0)
//			return J_DESC;
//		if (*s == 'e' && strcmp(s + 1, "xit") == 0)
//			return Q_DESC;
		if (*s == 'm' && strcmp(s + 1, "ime") == 0)
			return MM_DESC;
		if (*s == 'o' && strcmp(s + 1, "pen") == 0)
			return O_DESC;
//		if (*s == 'p' && strcmp(s + 1, "ath") == 0)
//			return CWD_DESC;
//		if (*s == 'p' && strcmp(s + 1, "rof") == 0)
//			return PF_DESC;
		if (*s == 'p' && strcmp(s + 1, "rop") == 0)
			return P_DESC;
//		if (*s == 'q' && strcmp(s + 1, "uit") == 0)
//			return Q_DESC;
		if (*s == 's' && strcmp(s + 1, "ort") == 0)
			return ST_DESC;
		if (*s == 't' && strcmp(s + 1, "ips") == 0)
			return TIPS_DESC;
		if (*s == 'v' && strcmp(s + 1, "iew") == 0)
			return VIEW_DESC;
	}

	else if (l == 5) {
		if (*s == 'a' && strcmp(s + 1, "lias") == 0)
			return ALIAS_DESC;
		if (*s == 'd' && strcmp(s + 1, "esel") == 0)
			return DS_DESC;
		if (*s == 'f' && strcmp(s + 1, "orth") == 0)
			return F_DESC;
		if (*s == 'i' && strcmp(s + 1, "cons") == 0)
			return ICONS_DESC;
		if (*s == 'm' && strcmp(s + 1, "edia") == 0)
			return MEDIA_DESC;
		if (*s == 'p' && strcmp(s + 1, "ager") == 0)
			return PG_DESC;
//		if (*s == 'p' && strcmp(s + 1, "aste") == 0)
//			return V_DESC;
		if (*s == 's' && strcmp(s + 1, "tats") == 0)
			return STATS_DESC;
		if (*s == 't' && strcmp(s + 1, "rash") == 0)
			return TRASH_DESC;
		if (*s == 'u' && strcmp(s + 1, "ndel") == 0)
			return U_DESC;
		if (*s == 'u' && strcmp(s + 1, "npin") == 0)
			return UNPIN_DESC;
	}

	else if (l == 6) {
		if (*s == 'a' && strcmp(s + 1, "utocd") == 0)
			return ACD_DESC;
		if (*s == 'b' && strcmp(s + 1, "leach") == 0)
			return BB_DESC;
		if (*s == 'c' && strcmp(s + 1, "olors") == 0)
			return COLORS_DESC;
		if (*s == 'c' && strcmp(s + 1, "onfig") == 0)
			return CONFIG_DESC;
		if (*s == 'f' && strcmp(s + 1, "ilter") == 0)
			return FT_DESC;
		if (*s == 'h' && strcmp(s + 1, "idden") == 0)
			return HF_DESC;
		if (*s == 'o' && strcmp(s + 1, "pener") == 0)
			return OPENER_DESC;
		if (*s == 'p' && strcmp(s + 1, "rompt") == 0)
			return PROMPT_DESC;
		if (*s == 'r' && strcmp(s + 1, "eload") == 0)
			return RL_DESC;
		if (*s == 's' && strcmp(s + 1, "elbox") == 0)
			return SB_DESC;
		if (*s == 's' && strcmp(s + 1, "plash") == 0)
			return SPLASH_DESC;
	}

	else if (l == 7) {
		if (*s == 'a' && strcmp(s + 1, "ctions") == 0)
			return ACTIONS_DESC;
		if (*s == 'c' && strcmp(s + 1, "olumns") == 0)
			return CL_DESC;
		if (*s == 'h' && strcmp(s + 1, "istory") == 0)
			return HIST_DESC;
		if (*s == 'p' && strcmp(s + 1, "rofile") == 0)
			return PF_DESC;
		if (*s == 'r' && strcmp(s + 1, "efresh") == 0)
			return RF_DESC;
		if (*s == 'u' && strcmp(s + 1, "ntrash") == 0)
			return U_DESC;
		if (*s == 'v' && strcmp(s + 1, "ersion") == 0)
			return VER_DESC;
	}

	else if (l == 8) {
		if (*s == 'c' && strcmp(s + 1, "ommands") == 0)
			return CMD_DESC;
		if (*s == 'k' && strcmp(s + 1, "eybinds") == 0)
			return KB_DESC;
		if (*s == 'm' && strcmp(s + 1, "essages") == 0)
			return MSG_DESC;
	}

	else if (l == 9) {
		if (*s == 'a' && strcmp(s + 1, "uto-open") == 0)
			return AO_DESC;
		if (*s == 'b' && strcmp(s + 1, "ookmarks") == 0)
			return BM_DESC;
	}

	else if (l == 10 && *s == 'd' && strcmp(s + 1, "irs-first") == 0)
			return FF_DESC;

	else if (l == 11 && *s == 'm' && strcmp(s + 1, "ountpoints") == 0)
		return MP_DESC;

	else if (l == 12 && *s == 'c' && strcmp(s + 1, "olorschemes") == 0)
		return CS_DESC;

	return (char *)NULL;
}

int
recover_from_wrong_cmd(void)
{
	/* Check rl_dispathing to know whether we are called from a keybind,
	 * in which case we should skip this check. */
	if (rl_line_buffer && (rl_dispatching == 0
	|| (words_num > 1 && point_is_first_word == 0))) {
		char *p = (strrchr(rl_line_buffer, ' '));
		if (p && p != rl_line_buffer && *(p - 1) != '\\' && *(p + 1) != ' ')
			return EXIT_FAILURE;
	}

	fputs(NC, stdout);
	cur_color = (char *)NULL;
	rl_restore_prompt();
	rl_clear_message();

#ifndef _NO_HIGHLIGHT
	if (conf.highlight == 1) {
		int p = rl_point;
		rl_point = 0;
		recolorize_line();
		rl_point = p;
	}
#endif /* !_NO_HIGHLIGHT */
	wrong_cmd = 0;

	return EXIT_SUCCESS;
}

/* This function is only used before running a keybind command. We don't
 * want the suggestion buffer after running a keybind */
void
free_suggestion(void)
{
	free(suggestion_buf);
	suggestion_buf = (char *)NULL;
	suggestion.printed = 0;
	suggestion.nlines = 0;
}

void
clear_suggestion(const int sflag)
{
	if (rl_end > rl_point) {
		MOVE_CURSOR_RIGHT(rl_end - rl_point);
		fflush(stdout);
	}

	ERASE_TO_RIGHT_AND_BELOW;

	if (rl_end > rl_point) {
		MOVE_CURSOR_LEFT(rl_end - rl_point);
		fflush(stdout);
	}

	suggestion.printed = 0;
	if (sflag == CS_FREEBUF) {
		free(suggestion_buf);
		suggestion_buf = (char *)NULL;
	}
}

/* THIS FUNCTION SHOULD BE REMOVED */
void
remove_suggestion_not_end(void)
{
//	if (conf.highlight != 0) {
//		MOVE_CURSOR_RIGHT(rl_end - rl_point);
//		fflush(stdout);
//	}

//	ERASE_TO_RIGHT;
	clear_suggestion(CS_FREEBUF);
//	MOVE_CURSOR_LEFT(rl_end - rl_point);
//	fflush(stdout);
}

static inline void
restore_cursor_position(const size_t slines)
{
	if (slines > 1)
		MOVE_CURSOR_UP((int)slines - 1);
	MOVE_CURSOR_LEFT(term_cols);
	if (conf.highlight == 0 && rl_point < rl_end)
		curcol -= (rl_end - rl_point);
	MOVE_CURSOR_RIGHT(curcol > 0 ? curcol - 1 : curcol);
}

static inline size_t
calculate_suggestion_lines(int *baej, const size_t suggestion_len)
{
	size_t cuc = (size_t)curcol; /* Current cursor column position*/

	if (suggestion.type == BOOKMARK_SUG || suggestion.type == ALIAS_SUG
	|| suggestion.type == ELN_SUG || suggestion.type == JCMD_SUG
	|| suggestion.type == JCMD_SUG_NOACD || suggestion.type == BACKDIR_SUG
	|| suggestion.type == SORT_SUG || suggestion.type == WS_NUM_SUG
	|| suggestion.type == FUZZY_FILENAME || suggestion.type == DIRHIST_SUG
	|| suggestion.type == FASTBACK_SUG) {
		/* 3 = 1 (one char forward) + 2 (" >") */
		cuc += 3;
		flags |= BAEJ_SUGGESTION;
		*baej = 1;
	}

	size_t cucs = cuc + suggestion_len;
	if (conf.highlight == 0 && rl_point < rl_end)
		cucs += (size_t)(rl_end - rl_point - 1);
	/* slines: amount of lines we need to print the suggestion, including
	 * the current line */
	size_t slines = 1;

	if (cucs > term_cols) {
		slines = cucs / (size_t)term_cols;
		const int cucs_rem = (int)cucs % term_cols;
		if (cucs_rem > 0)
			slines++;
	}

	return slines;
}

static inline char *
truncate_name(const char *str)
{
	char *wname = (char *)NULL;

	if (suggestion.type == ELN_SUG || suggestion.type == COMP_SUG
	|| suggestion.type == FILE_SUG) {
		const size_t wlen = wc_xstrlen(str);
		if (wlen == 0)
			wname = replace_invalid_chars(str);
	}

	return wname;
}

static inline void
set_cursor_position(const int baej)
{
	/* If not at the end of the line, move the cursor there */
	/* rl_end and rl_point are not updated: they do not include
	 * the last typed char. However, since we only care here about
	 * the difference between them, it doesn't matter: the result
	 * is the same (7 - 4 == 6 - 3 == 3) */
	if (rl_end > rl_point && conf.highlight == 0) {
		MOVE_CURSOR_RIGHT(rl_end - rl_point);
		fflush(stdout);
	}

	ERASE_TO_RIGHT;

	if (baej == 1)
		SUGGEST_BAEJ(BAEJ_OFFSET, sp_c);
}

static inline int
check_conditions(const size_t offset, const size_t wlen, int *baej,
	size_t *slines)
{
	if (offset > wlen)
		return EXIT_FAILURE;

	/* Do not print suggestions bigger than what the current terminal
	 * window size can hold. If length is zero (invalid wide char), or if
	 * it equals ARG_MAX, in which case we most probably have a truncated
	 * suggestion (mbstowcs will convert only up to ARG_MAX chars), exit */
//	size_t suggestion_len = wc_xstrlen(str + offset);
	const size_t suggestion_len = wlen - offset;
	if (suggestion_len == 0 || suggestion_len == ARG_MAX
	|| (int)suggestion_len > (term_cols * term_lines) - curcol)
		return EXIT_FAILURE;

	*slines = calculate_suggestion_lines(baej, suggestion_len - 1);

	if (*slines > (size_t)term_lines || (xargs.vt100 == 1 && *slines > 1))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static inline void
make_suggestion(const char *str, const size_t offset, const char *color)
{
	if (suggestion.type == FUZZY_FILENAME
	|| (suggestion.type == COMP_SUG && (flags & BAEJ_SUGGESTION)))
		color = sz_c;

	char *wname = truncate_name(str);
	fputs(color, stdout);
//	fputs((wname ? wname : str) + offset - (offset ? 1 : 0), stdout);
	fputs((wname ? wname : str) + offset, stdout);
	fflush(stdout);
	free(wname);
}

/* Clear the line, print the suggestion (STR) at OFFSET in COLOR, and
 * move the cursor back to the original position.
 * OFFSET marks the point in STR that is already typed: the suggestion
 * will be printed starting from this point */
void
print_suggestion(const char *str, size_t offset, char *color)
{
	if (!str || !*str)
		return;

	if (wrong_cmd == 1) {
		if (words_num > 1)
			return;
		recover_from_wrong_cmd();
	}

	if (words_num == 1 && rl_end > 0 && rl_line_buffer
	&& rl_line_buffer[rl_end - 1] == ' ' && (rl_end == 1
	|| rl_line_buffer[rl_end - 2] != '\\') && suggestion.type != HIST_SUG) {
		// We have "cmd     " (with one or more trailing spaces)
		suggestion.printed = 0;
		if (suggestion_buf)
			clear_suggestion(CS_FREEBUF);
		return;
	}

	HIDE_CURSOR;

	if (suggestion.printed && str != suggestion_buf)
		clear_suggestion(CS_FREEBUF);

	int baej = 0; /* Bookmark/backdir, alias, ELN, or jump (and fuzzy matches) */
	flags &= ~BAEJ_SUGGESTION;

	/* Let's check for baej suggestions, mostly in case of fuzzy matches */
	size_t wlen = last_word ? strlen(last_word) : 0;
	/* An alias name can be the same as the beginning of the alias definition,
	 * so that this check must always be true in case of aliases */
	if (suggestion.type == ALIAS_SUG || (last_word && cur_comp_type == TCMP_PATH
	&& (conf.case_sens_path_comp ? strncmp(last_word, str, wlen)
	: strncasecmp(last_word, str, wlen)) != 0) ) {
		flags |= BAEJ_SUGGESTION;
		baej = 1;
		offset = 0;
	}

	if (conf.highlight == 0)
		rl_redisplay();
	curcol = prompt_offset + (rl_line_buffer
			? (int)wc_xstrlen(rl_line_buffer) : 0);

	if (term_cols > 0) {
		while (curcol > term_cols)
			curcol -= term_cols;
	}

	const size_t str_len = wc_xstrlen(str);
	size_t slines = 0;
	if (check_conditions(offset, str_len, &baej, &slines) == EXIT_FAILURE) {
		UNHIDE_CURSOR;
		return;
	} else {
		if (baej == 1) {
			flags |= BAEJ_SUGGESTION;
			offset = 0;
		}
	}

	/* In some cases (accepting first suggested word), we might want to
	 * reprint the suggestion buffer, in which case it would be already stored */
	if (str != suggestion_buf) {
		/* Store the suggestion (used later by rl_accept_suggestion (keybinds.c) */
		free(suggestion_buf);
		suggestion_buf = savestring(str, strlen(str));
	}

	set_cursor_position(baej);
	make_suggestion(str, offset, color);

	restore_cursor_position(slines);

	/* Store the amount of lines taken by the current command line (plus the
	 * suggestion's length) to be able to correctly remove it later (via the
	 * clear_suggestion function) */
	suggestion.nlines = slines;
	/* Store the suggestion color, in case we need to reprint it */
	suggestion.color = color;

	UNHIDE_CURSOR;
}

static inline char *
get_reg_file_color(const char *filename, const struct stat *attr,
	int *free_color)
{
	if (conf.light_mode == 1) return fi_c;
	if (access(filename, R_OK) == -1) return nf_c;
	if (attr->st_mode & S_ISUID) return su_c;
	if (attr->st_mode & S_ISGID) return sg_c;

#ifdef LINUX_FILE_CAPS
	const cap_t cap = cap_get_file(filename);
	if (cap) {
		cap_free(cap);
		return ca_c;
	}
#endif /* LINUX_FILE_CAPS */
	if (attr->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		return (FILE_SIZE_PTR(attr) == 0) ? ee_c : ex_c;

	if (FILE_SIZE_PTR(attr) == 0) return ef_c;
	if (attr->st_nlink > 1)	return mh_c;

	const char *ext = check_ext == 1 ? strrchr(filename, '.') : (char *)NULL;
	if (!ext || ext == filename)
		return fi_c;

	size_t color_len = 0;
	char *extcolor = get_ext_color(ext, &color_len);
	if (!extcolor)
		return fi_c;

	color_len += 4;
	char *ext_color = xnmalloc(color_len, sizeof(char));
	snprintf(ext_color, color_len, "\x1b[%sm", extcolor);

	*free_color = 1;
	return ext_color;
}

/* Used by the check_completions function to get file names color
 * according to file type. */
static char *
get_comp_color(const char *filename, const struct stat *attr, int *free_color)
{
	char *color = (char *)NULL;

	switch (attr->st_mode & S_IFMT) {
	case S_IFDIR:
		if (conf.light_mode == 1) return di_c;
		if (access(filename, R_OK | X_OK) != 0)
			return nd_c;
		color = get_dir_color(filename, attr->st_mode, attr->st_nlink, -1);
		break;

	case S_IFREG:
		color = get_reg_file_color(filename, attr, free_color);
		break;

	case S_IFLNK: {
		if (conf.light_mode == 1) return ln_c;
		char *linkname = realpath(filename, (char *)NULL);
		if (linkname) {
			free(linkname);
			return ln_c;
		}
		return or_c;
		}

	case S_IFSOCK: return so_c;
	case S_IFBLK: return bd_c;
	case S_IFCHR: return cd_c;
	case S_IFIFO: return pi_c;
	default: return no_c;
	}

	return color;
}

static inline int
skip_leading_dot_slash(char **str, size_t *len)
{
	int dot_slash = 0;

	if (*len >= 2 && *(*str) == '.' && *(*str + 1) == '/') {
		dot_slash = 1;
		(*str) += 2;
		(*len) -= 2;
	}

	return dot_slash;
}

static inline int
remove_trailing_slash(char **str, size_t *len)
{
	if (*len == 0)
		return 0;

	if ((*str)[*len - 1] == '/') {
		(*len)--;
		(*str)[*len] = '\0';
		return 1;
	}

	return 0;
}

static inline void
skip_trailing_spaces(char **str, size_t *len)
{
//	if (*len == 0)
//		return;

//	while (*len > 0 && (*str)[*len - 1] == ' ') {
	while (*len > 0 && (*str)[*len - 1] == ' '
	&& (*len == 1 || (*str)[*len - 2] != '\\') ) {
		(*len)--;
		(*str)[*len] = '\0';
	}
}

static inline void
skip_leading_backslashes(char **str, size_t *len)
{
	if (*len == 0)
		return;

	while (*(*str) == '\\') {
		++(*str);
		--(*len);
	}
}

static void
match_print(char *match, const size_t len, char *color, const int append_slash)
{
	char t[PATH_MAX + 2];
	*t = '\0';

	if (append_slash == 1)
		snprintf(t, sizeof(t), "%s/", match);

	char *tmp = escape_str(*t ? t : match);
	if (!tmp || !*tmp) {
		print_suggestion(match, len, color);
		return;
	}

	char *q;
	if (cur_comp_type == TCMP_PATH && *tmp == '\\' && *(tmp + 1) == '~')
		q = tmp + 1;
	else
		q = tmp;

	print_suggestion(q, len, color);
	free(tmp);
}

static inline int
print_match(char *match, const size_t len)
{
	int append_slash = 0, free_color = 0;

	char *p = (char *)NULL, *_color = (char *)NULL;
	char *color = (conf.suggest_filetype_color == 1) ? no_c : sf_c;

	if (*match == '~')
		p = tilde_expand(match);

	struct stat attr;
	if (lstat(p ? p : match, &attr) != -1) {
		if (S_ISDIR(attr.st_mode)
		|| (S_ISLNK(attr.st_mode) && get_link_ref(p ? p : match) == S_IFDIR)) {
			/* Do not append slash if suggesting the root dir */
			append_slash = (*match == '/' && !*(match + 1)) ? 0 : 1;
			suggestion.filetype = DT_DIR;
		}

		if (conf.suggest_filetype_color == 1) {
			_color = get_comp_color(p ? p : match, &attr, &free_color);
			if (_color)
				color = _color;
			else
				free_color = 0;
		}
	} else {
		suggestion.filetype = DT_DIR;
	}

	free(p);
	suggestion.type = COMP_SUG;
	match_print(match, len, color, append_slash);

	if (free_color)
		free(color);
	return PARTIAL_MATCH;
}

static int
get_print_status(const char *str, const char *match, const size_t len)
{
	if (suggestion.printed && suggestion_buf)
		clear_suggestion(CS_FREEBUF);

	if ((len > 0 && str[len - 1] == '/') || strlen(match) == len)
		return FULL_MATCH;

	return PARTIAL_MATCH;
}

static int
check_completions(char *str, size_t len, const int print)
{
	if (!str || !*str)
		return NO_MATCH;

	skip_trailing_spaces(&str, &len);
	skip_leading_backslashes(&str, &len);

	if (len == 0)
		return NO_MATCH;

	if (conf.fuzzy_match != 0 && words_num == 1 && *str != '/'
	&& is_internal_c(str))
		return NO_MATCH;

	int printed = NO_MATCH;
	suggestion.filetype = DT_REG;
	cur_comp_type = TCMP_NONE;

	if (print == 0 && words_num == 1) {
		// First (and only) word followed by a space
		struct stat a;
		if (lstat(str, &a) == 0) {
			cur_comp_type = TCMP_PATH;
			return FULL_MATCH;
		}
		return NO_MATCH;
	}

	*fz_match = '\0';
	flags |= STATE_SUGGESTING;
	char *match = my_rl_path_completion(str, 0);
	flags &= ~STATE_SUGGESTING;
	if (!match && !*fz_match)
		return NO_MATCH;

	if (print == 0 && match) {
		int ret = get_print_status(str, match, len);
		free(match);
		cur_comp_type = TCMP_PATH;
		return ret;
	}

	cur_comp_type = TCMP_PATH; /* Required by print_match() */
	printed = print_match(match ? match : fz_match, len);
	*fz_match = '\0';

	cur_comp_type = printed == NO_MATCH ? TCMP_NONE : TCMP_PATH;
	free(match);

	return printed;
}

static inline void
print_directory_suggestion(const filesn_t i, const size_t len, char *color)
{
	if (conf.suggest_filetype_color == 1)
		color = file_info[i].color;

	suggestion.filetype = DT_DIR;

	char name[NAME_MAX + 2];
	snprintf(name, sizeof(name), "%s/", file_info[i].name);

	char *tmp = escape_str(name);
	if (tmp) {
		print_suggestion(tmp, len, color);
		free(tmp);
		return;
	}

	print_suggestion(name, len, color);
}

static inline void
print_reg_file_suggestion(char *str, const filesn_t i, size_t len,
	char *color, const int dot_slash)
{
	if (conf.suggest_filetype_color)
		color = file_info[i].color;

	suggestion.filetype = DT_REG;

	char *tmp = escape_str(file_info[i].name);
	if (tmp) {
		char *s = str;
		while (*s) {
			if (is_quote_char(*s))
				len++;
			s++;
		}

		if (dot_slash) { /* Reinsert './', removed to check file name */
			char t[NAME_MAX + 2];
			snprintf(t, sizeof(t), "./%s", tmp);
			print_suggestion(t, len + 2, color);
		} else {
			print_suggestion(tmp, len, color);
		}

		free(tmp);
		return;
	}

	if (dot_slash) {
		char t[NAME_MAX + 2];
		snprintf(t, sizeof(t), "./%s", file_info[i].name);
		print_suggestion(t, len + 2, color);
		return;
	}

	print_suggestion(file_info[i].name, len, color);
}

static int
check_filenames(char *str, size_t len, const int first_word,
	const size_t full_word)
{
	char *color = (conf.suggest_filetype_color == 1) ? no_c : sf_c;

	skip_leading_backslashes(&str, &len);
	int dot_slash = skip_leading_dot_slash(&str, &len);
	skip_trailing_spaces(&str, &len);
	int removed_slash = remove_trailing_slash(&str, &len);

	filesn_t fuzzy_index = -1;
	int fuzzy_str_type = (conf.fuzzy_match == 1 && contains_utf8(str) == 1)
		? FUZZY_FILES_UTF8 : FUZZY_FILES_ASCII;
	int best_fz_score = 0;

	filesn_t i;

	for (i = 0; i < files; i++) {
		if (!file_info[i].name)	continue;

		if (removed_slash == 1 && (file_info[i].dir != 1
		|| len != file_info[i].len))
			continue;

		if (full_word) {
			if ((conf.case_sens_path_comp ? strcmp(str, file_info[i].name)
			: strcasecmp(str, file_info[i].name)) == 0)
				return FULL_MATCH;
			continue;
		}

		if (len == 0) continue;
		if (first_word == 1 && ( (file_info[i].dir == 1 && conf.autocd == 0)
		|| (file_info[i].dir == 0 && conf.auto_open == 0) ) )
			continue;

		if (words_num > 1 && rl_line_buffer && *rl_line_buffer == 'c'
		&& rl_line_buffer[1] == 'd' && rl_line_buffer[2] == ' '
		&& file_info[i].dir == 0)
			continue;

		/* No fuzzy matching if not at the end of the line */
		if (conf.fuzzy_match == 0 || rl_point < rl_end) {
			if (conf.case_sens_path_comp ? (*str == *file_info[i].name
			&& strncmp(str, file_info[i].name, len) == 0)
			: (TOUPPER(*str) == TOUPPER(*file_info[i].name)
			&& strncasecmp(str, file_info[i].name, len) == 0)) {
				if (file_info[i].len == len) return FULL_MATCH;

				suggestion.type = FILE_SUG;

				if (file_info[i].dir)
					print_directory_suggestion(i, len, color);
				else
					print_reg_file_suggestion(str, i, len, color, dot_slash);

				return PARTIAL_MATCH;
			}
		}

		/* ############### FUZZY MATCHING ################## */
		else {
			int s = fuzzy_match(str, file_info[i].name, len, fuzzy_str_type);
			if (s > best_fz_score) {
				fuzzy_index = i;
				if (s == TARGET_BEGINNING_BONUS)
					break;
				best_fz_score = s;
			}
		}
	}

	if (fuzzy_index > -1) { /* We have a fuzzy match */
		cur_comp_type = TCMP_PATH;

		/* i < files == we have a full match (TARGET_BEGINNING_BONUS) */
		suggestion.type = i < files ? FILE_SUG : FUZZY_FILENAME;

		if (file_info[fuzzy_index].dir)
			print_directory_suggestion(fuzzy_index, len, color);
		else
			print_reg_file_suggestion(str, fuzzy_index, len, color,
				dot_slash);

		return PARTIAL_MATCH;
	}

	if (removed_slash == 1) /* We removed the final slash: reinsert it */
		str[len] = '/';

	return NO_MATCH;
}

static int
check_history(const char *str, const size_t len)
{
	if (!history || !str || !*str || len == 0)
		return NO_MATCH;

	int i = (int)current_hist_n;
	while (--i >= 0) {
		if (!history[i].cmd || TOUPPER(*str) != TOUPPER(*history[i].cmd))
			continue;

		if (len > 1 && *(history[i].cmd + 1)
		&& TOUPPER(*(str + 1)) != TOUPPER(*(history[i].cmd + 1)))
			continue;

		if ((conf.case_sens_path_comp ? strncmp(str, history[i].cmd, len)
		: strncasecmp(str, history[i].cmd, len)) == 0) {
			if (history[i].len > len) {
				suggestion.type = HIST_SUG;
				print_suggestion(history[i].cmd, len, sh_c);
				return PARTIAL_MATCH;
			}
			return FULL_MATCH;
		}
	}

	return NO_MATCH;
}

static int
check_builtins(const char *str, const size_t len, const int print)
{
	char **b = (char **)NULL;

	switch (shell) {
	case SHELL_NONE: return NO_MATCH;
	case SHELL_BASH: b = bash_builtins; break;
	case SHELL_DASH: b = dash_builtins; break;
	case SHELL_FISH: b = fish_builtins; break;
	case SHELL_KSH:  b = ksh_builtins; break;
	case SHELL_TCSH: b = tcsh_builtins; break;
	case SHELL_ZSH:  b = zsh_builtins; break;
	default: return NO_MATCH;
	}

	size_t i;
	for (i = 0; b[i]; i++) {
		if (*str != *b[i])
			continue;

		if (!print) {
			if (strcmp(str, b[i]) == 0)	return FULL_MATCH;
			continue;
		}

		if (strncmp(b[i], str, len) != 0)
			continue;

		const size_t blen = strlen(b[i]);
		if (blen > len) {
			suggestion.type = CMD_SUG;
			print_suggestion(b[i], len, sb_c);
			return PARTIAL_MATCH;
		}
		return FULL_MATCH;
	}

	return NO_MATCH;
}

static inline int
print_cmd_suggestion(const size_t i, const size_t len)
{
	if (is_internal_c(bin_commands[i])) {
		if (strlen(bin_commands[i]) > len) {
			suggestion.type = CMD_SUG;
			print_suggestion(bin_commands[i], len, sx_c);
			return PARTIAL_MATCH;
		}
		return FULL_MATCH;
	}

	if (conf.ext_cmd_ok) {
		if (strlen(bin_commands[i]) > len) {
			suggestion.type = CMD_SUG;
			print_suggestion(bin_commands[i], len, sc_c);
			return PARTIAL_MATCH;
		}
		return FULL_MATCH;
	}

	return NO_MATCH;
}

static inline int
print_internal_cmd_suggestion(char *str, const size_t len, const int print)
{
	/* Check internal command with fused parameter */
	char *p = (char *)NULL;
	size_t j;
	for (j = 0; str[j]; j++) {
		if (str[j] >= '1' && str[j] <= '9') {
			p = str + j;
			break;
		}
	}

	if (!p || p == str)
		return check_builtins(str, len, print);

	*p = '\0';
	if (!is_internal_c(str))
		return NO_MATCH;

	return FULL_MATCH;
}

/* Check STR against a list of command names, both internal and in PATH */
static int
check_cmds(char *str, size_t len, const int print)
{
	if (len == 0 || !str || !*str)
		return NO_MATCH;

	char *cmd = str;
	if (cmd && *cmd == '\\' && *(cmd + 1)) {
		cmd++;
		len--;
	}

	size_t i;
	for (i = 0; bin_commands[i]; i++) {
		if (!bin_commands[i] || *cmd != *bin_commands[i])
			continue;

		if (!print) {
			if (strcmp(cmd, bin_commands[i]) == 0)
				return FULL_MATCH;
			continue;
		}

		/* Let's check the 2nd char as well before calling strcmp() */
		if (len > 1 && *(bin_commands[i] + 1)
		&& *(cmd + 1) != *(bin_commands[i] + 1))
			continue;

		if (strncmp(cmd, bin_commands[i], len) != 0)
			continue;

		int ret = print_cmd_suggestion(i, len);
		if (ret == NO_MATCH)
			continue;
		return ret;
	}

	return print_internal_cmd_suggestion(cmd, len, print);
}

static int
check_jumpdb(const char *str, const size_t len, const int print)
{
	char *color = (conf.suggest_filetype_color == 1) ? di_c : sf_c;

	int i = (int)jump_n;
	while (--i >= 0) {
		if (!jump_db[i].path || TOUPPER(*str) != TOUPPER(*jump_db[i].path)
		|| jump_db[i].rank == JUMP_ENTRY_PURGED)
			continue;
		if (len > 1 && *(jump_db[i].path + 1)
		&& TOUPPER(*(str + 1)) != TOUPPER(*(jump_db[i].path + 1)))
			continue;
		if (!print) {
			if ((conf.case_sens_path_comp ? strcmp(str, jump_db[i].path)
			: strcasecmp(str, jump_db[i].path)) == 0)
				return FULL_MATCH;
			continue;
		}

		if (len && (conf.case_sens_path_comp
		? strncmp(str, jump_db[i].path, len)
		: strncasecmp(str, jump_db[i].path, len)) == 0) {
			if (jump_db[i].len <= len) return FULL_MATCH;

			suggestion.type = FILE_SUG;
			suggestion.filetype = DT_DIR;
			char tmp[PATH_MAX + 2];
			*tmp = '\0';

			if (jump_db[i].len > 0
			&& jump_db[i].path[jump_db[i].len - 1] != '/')
				snprintf(tmp, sizeof(tmp), "%s/", jump_db[i].path);

			print_suggestion(*tmp ? tmp : jump_db[i].path, len, color);
			return PARTIAL_MATCH;
		}
	}

	return NO_MATCH;
}

static int
check_int_params(const char *str, const size_t len)
{
	if (len == 0)
		return NO_MATCH;

	size_t i;
	for (i = 0; param_str[i].name; i++) {
		if (*str == *param_str[i].name && param_str[i].len > len
		&& strncmp(str, param_str[i].name, len) == 0) {
			suggestion.type = INT_CMD;
			print_suggestion(param_str[i].name, len, sx_c);
			return PARTIAL_MATCH;
		}
	}

	return NO_MATCH;
}

static int
check_eln(const char *str, const int print)
{
	if (!str || !*str)
		return NO_MATCH;

	filesn_t n = xatof(str);
	if ( n < 1 || n > files || !file_info[n - 1].name
	|| ( words_num == 1 && ( (file_info[n - 1].dir == 1 && conf.autocd == 0)
	|| (file_info[n - 1].dir == 0 && conf.auto_open == 0) ) ) )
		return NO_MATCH;

	if (print == 0)
		return FULL_MATCH;

	n--;
	char *color = sf_c;
	suggestion.type = ELN_SUG;
	if (conf.suggest_filetype_color)
		color = file_info[n].color;

	char tmp[NAME_MAX + 1];
	*tmp = '\0';
	if (file_info[n].dir) {
		snprintf(tmp, sizeof(tmp), "%s/", file_info[n].name);
		suggestion.filetype = DT_DIR;
	} else {
		suggestion.filetype = DT_REG;
	}

	print_suggestion(!*tmp ? file_info[n].name : tmp, 0, color);

	return PARTIAL_MATCH;
}

static int
check_aliases(const char *str, const size_t len, const int print)
{
	if (!aliases_n)
		return NO_MATCH;

	char *color = sc_c;

	int i = (int)aliases_n;
	while (--i >= 0) {
		if (!aliases[i].name)
			continue;
		char *p = aliases[i].name;
		if (TOUPPER(*p) != TOUPPER(*str))
			continue;

		if (!print) {
			if ((conf.case_sens_path_comp ? strcmp(p, str)
			: strcasecmp(p, str)) == 0)
				return FULL_MATCH;
			continue;
		}

		if ((conf.case_sens_path_comp ? strncmp(p, str, len)
		: strncasecmp(p, str, len)) != 0)
			continue;
		if (!aliases[i].cmd || !*aliases[i].cmd)
			continue;

		suggestion.type = ALIAS_SUG;

		print_suggestion(aliases[i].cmd, 0, color);
		return PARTIAL_MATCH;
	}

	return NO_MATCH;
}

/* Get a match from the jump database and print the suggestion */
static int
check_jcmd(char *line)
{
	if (suggestion_buf)
		clear_suggestion(CS_FREEBUF);

	/* Split line into an array of substrings */
	char **substr = line ? get_substr(line, ' ') : (char **)NULL;
	if (!substr)
		return NO_MATCH;

	/* Check the jump database for a match. If a match is found, it will
	 * be stored in jump_suggestion (global) */
	dirjump(substr, SUG_JUMP);

	size_t i;
	for (i = 0; substr[i]; i++)
		free(substr[i]);
	free(substr);

	if (!jump_suggestion)
		return NO_MATCH;

	suggestion.type = JCMD_SUG;
	suggestion.filetype = DT_DIR;

	print_suggestion(jump_suggestion, 0,
		conf.suggest_filetype_color ? di_c : sf_c);

	if (conf.autocd == 0)
		suggestion.type = JCMD_SUG_NOACD;

	free(jump_suggestion);
	jump_suggestion = (char *)NULL;
	return PARTIAL_MATCH;
}

/* Check if we must suggest --help for internal commands */
static int
check_help(char *full_line, const char *_last_word)
{
	size_t len = strlen(_last_word);
	if (strncmp(_last_word, "--help", len) != 0)
		return NO_MATCH;

	char *ret = strchr(full_line, ' ');
	if (!ret)
		return NO_MATCH;

	*ret = '\0';
	int retval = is_internal_c(full_line);
	*ret = ' ';

	if (!retval)
		return NO_MATCH;

	suggestion.type = INT_HELP_SUG;
	print_suggestion("--help", len, sx_c);
	return PARTIAL_MATCH;
}

static int
check_users(const char *str, const size_t len)
{
#if defined(__ANDROID__)
	UNUSED(str); UNUSED(len);
	return NO_MATCH;
#else
	struct passwd *p;
	while ((p = getpwent())) {
		if (!p->pw_name) break;
		if (len == 0 || (*str == *p->pw_name
		&& strncmp(str, p->pw_name, len) == 0)) {
			suggestion.type = USER_SUG;
			char t[NAME_MAX + 1];
			snprintf(t, sizeof(t), "~%s", p->pw_name);
			print_suggestion(t, len + 1, sf_c);
			endpwent();
			return PARTIAL_MATCH;
		}
	}

	endpwent();
	return NO_MATCH;
#endif /* __ANDROID__ */
}

static int
check_variables(const char *str, const size_t len)
{
	size_t i;
	for (i = 0; environ[i]; i++) {
		if (TOUPPER(*environ[i]) != TOUPPER(*str)
		|| strncasecmp(str, environ[i], len) != 0)
			continue;

		char *ret = strchr(environ[i], '=');
		if (!ret)
			continue;

		*ret = '\0';
		suggestion.type = VAR_SUG;
		char t[NAME_MAX + 1];
		snprintf(t, sizeof(t), "$%s", environ[i]);
		print_suggestion(t, len + 1, sh_c);
		*ret = '=';
		return PARTIAL_MATCH;
	}

	if (usrvar_n == 0)
		return NO_MATCH;

	for (i = 0; usr_var[i].name; i++) {
		if (TOUPPER(*str) != TOUPPER(*usr_var[i].name)
		|| strncasecmp(str, usr_var[i].name, len) != 0)
			continue;

		suggestion.type = CMD_SUG;
		char t[NAME_MAX + 1];
		snprintf(t, sizeof(t), "$%s", usr_var[i].name);
		print_suggestion(t, len + 1, sh_c);
		return PARTIAL_MATCH;
	}

	return NO_MATCH;
}

static int
is_last_word(void)
{
	int lw = 1;

	if (rl_point >= rl_end)
		return lw;

	char *p = strchr(rl_line_buffer + rl_point, ' ');
	if (!p)
		return lw;

	while (*(++p)) {
		if (*p != ' ') {
			lw = 0;
			break;
		}
	}

	return lw;
}

/* Return the number of words found in the current readline buffer */
static size_t
count_words(size_t *start_word, size_t *full_word)
{
	size_t words = 0, w = 0, first_non_space = 0;
	char q = 0;
	char *b = rl_line_buffer;
	for (; b[w]; w++) {
		/* Keep track of open quotes */
		if (b[w] == '\'' || b[w] == '"')
			q = q == b[w] ? 0 : b[w];

		if (!first_non_space && b[w] != ' ') {
			words = 1;
			*start_word = w;
			first_non_space = 1;
			continue;
		}
		if (w > 0 && b[w] == ' ' && b[w - 1] != '\\') {
			if (!*full_word && b[w - 1] != '|'
			&& b[w - 1] != ';' && b[w - 1] != '&')
				*full_word = w; /* Index of the end of the first full word (cmd) */
			if (b[w + 1] && b[w + 1] != ' ') {
				words++;
			}
		}
		/* If a process separator char is found, reset variables so that we
		 * can start counting again for the new command */
		if (!q && cur_color != hq_c && w > 0 && b[w - 1] != '\\'
		&& ((b[w] == '&' && b[w - 1] == '&') || b[w] == '|' || b[w] == ';')) {
			words = first_non_space = *full_word = 0;
		}
	}

	return words;
}

static void
turn_it_wrong(void)
{
	char *b = rl_copy_text(0, rl_end);
	if (!b) return;

	fputs(wp_c, stdout);
	fflush(stdout);
	cur_color = wp_c;
	int bk = rl_point;

	rl_delete_text(0, rl_end);
	rl_point = rl_end = 0;
	rl_redisplay();
	rl_insert_text(b);

	free(b);
	rl_point = bk;
}

/* Switch to the warning prompt
 * FC is first char and LC last char */
static void
print_warning_prompt(const char fc, const unsigned char lc)
{
	if (conf.warning_prompt == 0 || wrong_cmd == 1
	|| fc == ';' || fc == ':' || fc == '#' || fc == '@'
	|| fc == '$' || fc == '\'' || fc == '"')
		return;

	if (suggestion.printed || suggestion_buf)
		clear_suggestion(CS_FREEBUF);

	wrong_cmd = 1;
	rl_save_prompt();

	char *decoded_prompt = decode_prompt(conf.wprompt_str);
	rl_set_prompt(decoded_prompt);
	free(decoded_prompt);

	if (conf.highlight == 1
	&& ( (rl_point < rl_end && words_num > 1)
	|| (lc == ' ' && words_num == 1) ) )
		turn_it_wrong();
}

#ifndef _NO_TAGS
static inline int
check_tags(const char *str, const size_t len, const int type)
{
	if (!str || !*str || len == 0 || tags_n == 0 || !tags)
		return NO_MATCH;

	size_t i;
	for (i = 0; tags[i]; i++) {
		if (*str != *tags[i] || strncmp(str, tags[i], len) != 0)
			continue;
		suggestion.type = type;
		print_suggestion(tags[i], len, sf_c);
		return PARTIAL_MATCH;
	}

	return NO_MATCH;
}
#endif /* _NO_TAGS */

static int
check_sort_methods(const char *str, const size_t len)
{
	if (len == 0) {
		if (suggestion.printed)
			clear_suggestion(CS_FREEBUF);
		return NO_MATCH;
	}

	int a = atoi(str);
	if (a < 0 || a > SORT_TYPES) {
		if (suggestion.printed)
			clear_suggestion(CS_FREEBUF);
		return NO_MATCH;
	}

	suggestion.type = SORT_SUG;
	print_suggestion(sort_methods[a].name, 0, sf_c);
	return PARTIAL_MATCH;
}

static int
check_prompts(char *word, const size_t len)
{
	int i = (int)prompts_n;

	char *q = (char *)NULL, *w = word;
	size_t l = len;

	if (strchr(word, '\\')) {
		q = unescape_str(word, 0);
		w = q ? q : word;
		l = w == q ? strlen(w) : len;
	}

	while (--i >= 0) {
		if (TOUPPER(*w) == TOUPPER(*prompts[i].name)
		&& (conf.case_sens_list ? strncmp(prompts[i].name, w, l)
		: strncasecmp(prompts[i].name, w, l)) == 0) {
			suggestion.type = PROMPT_SUG;
			char *p = escape_str(prompts[i].name);
			print_suggestion(p ? p : prompts[i].name, len, sx_c);
			free(p);
			free(q);
			return PARTIAL_MATCH;
		}
	}

	free(q);
	return NO_MATCH;
}

/* Get the word after LAST_SPACE (last non-escaped space in rl_line_buffer,
 * returned by get_last_space()), store it in LAST_WORD (global), and
 * set LAST_WORD_OFFSET (global) to the index of the beginning of this last
 * word in rl_line_buffer */
static void
get_last_word(const char *last_space)
{
	const char *tmp = (last_space && *(last_space + 1)) ? last_space + 1
			: (rl_line_buffer ? rl_line_buffer : (char *)NULL);
	if (tmp) {
		size_t len = strlen(tmp);
		last_word = xnrealloc(last_word, len + 1, sizeof(char));
		xstrsncpy(last_word, tmp, len + 1);
	} else {
		last_word = xnrealloc(last_word, 1, sizeof(char));
		*last_word = '\0';
	}

	last_word_offset = (last_space && *(last_space + 1) && rl_line_buffer)
		? (int)((last_space + 1) - rl_line_buffer) : 0;
}

static int
check_workspaces(char *word, size_t wlen)
{
	if (!word || !*word || !workspaces)
		return NO_MATCH;

	if (*word >= '1' && *word <= MAX_WS + '0' && !*(word + 1)) {
		const int a = atoi(word);
		if (a > 0 && workspaces[a - 1].name) {
			suggestion.type = WS_NUM_SUG;
			print_suggestion(workspaces[a - 1].name, 0, sf_c);
			return PARTIAL_MATCH;
		}

		return NO_MATCH;
	}

	char *q = (char *)NULL, *w = word;
	size_t l = wlen;

	if (strchr(word, '\\')) {
		q = unescape_str(word, 0);
		w = q ? q : word;
		l = w == q ? strlen(w) : wlen;
	}

	int i = MAX_WS;
	while (--i >= 0) {
		if (!workspaces[i].name)
			continue;

		if (TOUPPER(*w) == TOUPPER(*workspaces[i].name)
		&& strncasecmp(w, workspaces[i].name, l) == 0) {
			suggestion.type = WS_NAME_SUG;
			char *p = escape_str(workspaces[i].name);
			print_suggestion(p ? p : workspaces[i].name, wlen, sf_c);
			free(p);
			free(q);
			return PARTIAL_MATCH;
		}
	}

	free(q);
	return NO_MATCH;
}

static int
check_fastback(char *w)
{
	if (!w || !*w)
		return NO_MATCH;

	char *f = fastback(w);
	if (!f)
		return NO_MATCH;

	if (!*f) {
		free(f);
		return NO_MATCH;
	}

	suggestion.type = FASTBACK_SUG;
	suggestion.filetype = DT_DIR;
	cur_comp_type = TCMP_PATH;
	char *e = escape_str(f);
	print_suggestion(e, 0, sf_c);
	free(f);
	free(e);

	return PARTIAL_MATCH;
}

#ifndef _NO_PROFILES
static int
check_profiles(char *word, const size_t len)
{
	if (!word || !*word || !profile_names)
		return NO_MATCH;

	char *q = (char *)NULL, *w = word;
	size_t l = len;

	if (strchr(word, '\\')) {
		q = unescape_str(word, 0);
		w = q ? q : word;
		l = w == q ? strlen(w) : len;
	}

	size_t i;
	for (i = 0; profile_names[i]; i++) {
		if (conf.case_sens_list == 0
		? (TOUPPER(*w) == TOUPPER(*profile_names[i])
		&& strncasecmp(w, profile_names[i], l) == 0)
		: (*w == *profile_names[i]
		&& strncmp(w, profile_names[i], l) == 0)) {
			suggestion.type = PROFILE_SUG;
			char *p = escape_str(profile_names[i]);
			print_suggestion(p ? p : profile_names[i], len, sx_c);
			free(p);
			free(q);
			return PARTIAL_MATCH;
		}
	}

	free(q);
	return NO_MATCH;
}
#endif /* !_NO_PROFILES */

static int
check_remotes(char *word, const size_t len)
{
	if (!word || !*word || !remotes)
		return NO_MATCH;

	char *q = (char *)NULL, *w = word;
	size_t l = len;

	if (strchr(word, '\\')) {
		q = unescape_str(word, 0);
		w = q ? q : word;
		l = w == q ? strlen(w) : len;
	}

	size_t i;
	for (i = 0; remotes[i].name; i++) {
		if (conf.case_sens_list == 0
		? (TOUPPER(*w) == TOUPPER(*remotes[i].name)
		&& strncasecmp(w, remotes[i].name, l) == 0)
		: (*w == *remotes[i].name
		&& strncmp(w, remotes[i].name, l) == 0)) {
			suggestion.type = NET_SUG;
			char *p = escape_str(remotes[i].name);
			print_suggestion(p ? p : remotes[i].name, len, sx_c);
			free(p);
			free(q);
			return PARTIAL_MATCH;
		}
	}

	free(q);
	return NO_MATCH;
}

static int
check_color_schemes(char *word, const size_t len)
{
	if (!word || !*word || !color_schemes)
		return NO_MATCH;

	char *q = (char *)NULL, *w = word;
	size_t l = len;

	if (strchr(word, '\\')) {
		q = unescape_str(word, 0);
		w = q ? q : word;
		l = w == q ? strlen(w) : len;
	}

	size_t i;
	for (i = 0; color_schemes[i]; i++) {
		if (conf.case_sens_list == 0
		? (TOUPPER(*w) == TOUPPER(*color_schemes[i])
		&& strncasecmp(w, color_schemes[i], l) == 0)
		: (*w == *color_schemes[i]
		&& strncmp(w, color_schemes[i], l) == 0)) {
			suggestion.type = CSCHEME_SUG;
			char *p = escape_str(color_schemes[i]);
			print_suggestion(p ? p : color_schemes[i], len, sx_c);
			free(p);
			free(q);
			return PARTIAL_MATCH;
		}
	}

	free(q);
	return NO_MATCH;
}

static int
check_bookmark_names(char *word, const size_t len)
{
	if (!word || !*word || !bookmarks)
		return NO_MATCH;

	size_t prefix = (*word == 'b' && *(word + 1) == ':') ? 2 : 0;
	char *q = (char *)NULL, *w = word + prefix;
	size_t l = len - prefix;

	if (strchr(word + prefix, '\\')) {
		q = unescape_str(word + prefix, 0);
		w = q ? q : word + prefix;
		l = w == q ? strlen(w) : len;
	}

	size_t i;
	for (i = 0; i < bm_n; i++) {
		if (!bookmarks[i].name || !*bookmarks[i].name)
			continue;

		if (conf.case_sens_list == 0
		? (TOUPPER(*w) == TOUPPER(*bookmarks[i].name)
		&& strncasecmp(w, bookmarks[i].name, l) == 0)
		: (*w == *bookmarks[i].name
		&& strncmp(w, bookmarks[i].name, l) == 0)) {
			if (prefix == 2 && !*(bookmarks[i].name + l)) // full match
				break;

			char *p = escape_str(bookmarks[i].name);

			suggestion.type = prefix == 2 ? BM_PREFIX_SUG : BM_NAME_SUG;
			print_suggestion(p ? p : bookmarks[i].name, len - prefix, sx_c);

			free(p);
			free(q);
			return PARTIAL_MATCH;
		}
	}

	free(q);
	return NO_MATCH;
}

static int
check_backdir(void)
{
	if (!workspaces || !workspaces[cur_ws].path)
		return NO_MATCH;

	/* Remove the last component of the current path name (CWD):
	 * we want to match only PARENT directories */
	char bk_cwd[PATH_MAX + 1];
	xstrsncpy(bk_cwd, workspaces[cur_ws].path, sizeof(bk_cwd));

	char *q = strrchr(bk_cwd, '/');
	if (q)
		*q = '\0';

	char *lb = rl_line_buffer + 3;
	char *ds = strchr(lb, '\\') ? unescape_str(lb, 0) : lb;

	/* Find the query string in the list of parent directories */
	char *p = conf.case_sens_path_comp == 1 ? strstr(bk_cwd, ds)
		: xstrcasestr(bk_cwd, ds);
	if (p) {
		char *pp = strchr(p, '/');
		if (pp)
			*pp = '\0';

		suggestion.type = BACKDIR_SUG;
		print_suggestion(bk_cwd, 0, sf_c);
		if (ds && ds != lb)
			free(ds);
		return PARTIAL_MATCH;
	}

	if (ds && ds != lb)
		free(ds);

	return NO_MATCH;
}

static int
check_dirhist(char *word, const size_t len)
{
	int fuzzy_str_type = (conf.fuzzy_match == 1 && contains_utf8(word) == 1)
		? FUZZY_FILES_UTF8 : FUZZY_FILES_ASCII;
	int best_fz_score = 0, fuzzy_index = -1;

	int i = dirhist_total_index;
	while (--i >= 0) {
		if (!old_pwd[i] || !*old_pwd[i] || *old_pwd[i] == KEY_ESC)
			continue;

		if (conf.fuzzy_match == 0 || rl_point < rl_end) {
			if (strstr(old_pwd[i], word)) {
				suggestion.type = DIRHIST_SUG;
				print_suggestion(old_pwd[i], 0, sf_c);
				return PARTIAL_MATCH;
			}
		} else {
			int s = fuzzy_match(word, old_pwd[i], len, fuzzy_str_type);
			if (s > best_fz_score) {
				fuzzy_index = (int)i;
				if (s == TARGET_BEGINNING_BONUS)
					break;
				best_fz_score = s;
			}
		}
	}

	if (fuzzy_index == -1)
		return NO_MATCH;

	cur_comp_type = TCMP_DIRHIST;
	suggestion.type = DIRHIST_SUG;

	print_suggestion(old_pwd[fuzzy_index], 0, sf_c);

	return PARTIAL_MATCH;
}

/* Check for available suggestions. Returns zero if true, one if not,
 * and -1 if C was inserted before the end of the current line.
 * If a suggestion is found, it will be printed by print_suggestion() */
int
rl_suggestions(const unsigned char c)
{
	if (*rl_line_buffer == '#' || cur_color == hc_c) {
		/* No suggestion at all if comment */
		if (suggestion.printed)
			clear_suggestion(CS_FREEBUF);
		return EXIT_SUCCESS;
	}

	int printed = 0, zero_offset = 0;
	last_word_offset = 0;
	cur_comp_type = TCMP_NONE;

	if (rl_end == 0 && rl_point == 0) {
		free(suggestion_buf);
		suggestion_buf = (char *)NULL;
		if (wrong_cmd)
			recover_from_wrong_cmd();
		return EXIT_SUCCESS;
	}

	suggestion.full_line_len = (size_t)rl_end + 1;
	char *last_space = get_last_chr(rl_line_buffer, ' ', rl_end);

	/* Reset the wrong cmd flag whenever we have a new word or a new line */
	if (rl_end == 0 || c == '\n') {
		if (wrong_cmd)
			recover_from_wrong_cmd();
	}

	/* We need a copy of the complete line */
	char *full_line = rl_line_buffer;

	/* A copy of the last entered word */
	get_last_word(last_space);

	/* Count words */
	size_t full_word = 0, start_word = 0;
	words_num = count_words(&start_word, &full_word);

	/* And a copy of the first word as well */
	char *first_word = (char *)NULL;
	if (full_word) {
		rl_line_buffer[full_word] = '\0';
		char *q = rl_line_buffer + start_word;
		first_word = savestring(q, strlen(q));
		rl_line_buffer[full_word] = ' ';
	}

	char *word = (words_num == 1 && c != ' ' && first_word)
		? first_word : last_word;
	size_t wlen = strlen(word);

	/* If more than one word and the cursor is on the first word,
	 * jump to the check command name section */
	point_is_first_word = 0;
	if (words_num >= 2 && rl_point <= (int)full_word + 1) {
		point_is_first_word = 1;
		goto CHECK_FIRST_WORD;
	}

	/* If not on the first word and not at the end of the last word, do nothing */
	int lw = is_last_word();
	if (!lw) {
		if (suggestion.printed == 1 && suggestion.nlines > 1)
			clear_suggestion(CS_FREEBUF);
		goto SUCCESS;
	}

	if (c == '=' && words_num == 1 && wrong_cmd == 1) {
		recover_from_wrong_cmd();
		goto SUCCESS;
	}

	/* '~' or '~/' */
	if (word && *word == '~' && (!word[1] || (word[1] == '/' && !word[2]))) {
		if (wrong_cmd)
			recover_from_wrong_cmd();

		if (suggestion.printed == 1 && suggestion_buf
		&& suggestion.type == HIST_SUG && (!rl_line_buffer
		|| strncmp(suggestion_buf, rl_line_buffer, (size_t)rl_point) != 0))
			clear_suggestion(CS_FREEBUF);

		printed = PARTIAL_MATCH;
		zero_offset = 1;
		goto SUCCESS;
	}

		/* ######################################
		 * #	    Search for suggestions		#
		 * ######################################*/

	/* Fastback */
//	if (word && *word == '.' && word[1] == '.' && word[2] == '.'
	if (word && *word == '.' && word[1] == '.' && (!word[2] || word[2] == '.')
//	if (word && *word == '.' && (!word[1] || (word[1] == '.' && (!word[2] || word[2] == '.')))
	&& (printed = check_fastback(word)) != NO_MATCH)
		goto SUCCESS;

	/* 3.a) Internal command description */
	char *cdesc = (char *)NULL;
	if (conf.cmd_desc_sug == 1 && c != ' ' && words_num == 1
	&& (!rl_line_buffer || (rl_end > 0 && rl_line_buffer[rl_end - 1] != ' '))
	&& (cdesc = check_int_cmd_desc(word, wlen)) != NULL) {
		suggestion.type = CMD_DESC_SUG;
		print_suggestion(cdesc, 0, sd_c);
		printed = PARTIAL_MATCH;
		goto SUCCESS;
	}

	/* 3.b) Check already suggested string */
	if (suggestion_buf && suggestion.printed
	&& !(flags & BAEJ_SUGGESTION) && !IS_DIGIT(c)) {
		if (suggestion.type == HIST_SUG || suggestion.type == INT_CMD) {
			/* Skip the j cmd: we always want the BAEJ suggestion here */
			if (full_line && *full_line == 'j' && full_line[1] == ' ') {
				;
			} else if (full_line && *full_line == *suggestion_buf
			&& strncmp(full_line, suggestion_buf, (size_t)rl_end) == 0) {
				printed = PARTIAL_MATCH;
				zero_offset = 1;
				goto SUCCESS;
			}

		/* An alias name could be the same as the beginning of the alias
		 * defintion, so that this test must always be skipped in case
		 * of aliases. */
		} else if (suggestion.type != ALIAS_SUG && c != ' ' && word
		&& (conf.case_sens_path_comp ? (*word == *suggestion_buf
		&& strncmp(word, suggestion_buf, wlen) == 0)
		: (TOUPPER(*word) == TOUPPER(*suggestion_buf)
		&& strncasecmp(word, suggestion_buf, wlen) == 0) ) ) {
			printed = PARTIAL_MATCH;
			goto SUCCESS;
		}
	}

	/* 3.c) Internal commands fixed parameters */
	if (words_num > 1) {
		/* 3.c.1) Suggest the sel keyword only if not first word */
		if (sel_n > 0 && *word == 's' && strncmp(word, "sel", wlen) == 0) {
			suggestion.type = SEL_SUG;
			printed = 1;
			print_suggestion("sel", wlen, sx_c);
			goto SUCCESS;
		}

		/* 3.c.2) Check commands fixed parameters */
		printed = check_int_params(full_line, (size_t)rl_end);
		if (printed != NO_MATCH) {
			zero_offset = 1;
			goto SUCCESS;
		}

		/* 3.c.3) Let's suggest --help for internal commands */
		if (*word == '-') {
			if ((printed = check_help(full_line, word)) != NO_MATCH)
				goto SUCCESS;
		}
	}

	char *lb = rl_line_buffer;
	/* 3.d) Let's suggest non-fixed parameters for internal commands
	 * Only if second word or more (first word is the command name). */

	switch (words_num > 1 ? *lb : '\0') {
	case 'b': /* Bookmark names */
		if (bm_n > 0 && lb[1] == 'm' && lb[2] == ' ') {
			if (!(*(lb + 3) == 'a' && *(lb + 4) == ' ')
			&& strncmp(lb + 3, "add", 3) != 0) {
				if ((printed = check_bookmark_names(word, wlen)) != NO_MATCH) {
					goto SUCCESS;
				} else {
					if (suggestion.printed)
						clear_suggestion(CS_FREEBUF);
					if (*(lb + 3) != '-') /* Might be --help, let it continue */
						goto FAIL;
				}
			} else if (words_num > 5) {
				/* 5 == 'bm add dir/ name shortcut'*/
				goto FAIL;
			}
		}

		/* Backdir function (bd) */
		else if (lb[1] == 'd' && lb[2] == ' ' && lb[3]) {
				if (*(lb + 3) == '/' && !*(lb + 4)) {
					/* The query string is a single slash: do nothing */
					goto FAIL;
				}
				if ((printed = check_backdir()) != NO_MATCH)
					goto SUCCESS;
		}

		/* REMOVE AS SOON AS BH IS REPLACED BY DH!! */
		else if (words_num == 2 && old_pwd && dirhist_total_index > 0 && wlen > 0
		&& lb[1] == 'h' && lb[2] == ' ' && !strchr(word, '/')) {
			if (lb[1] == 'h' && lb[2] == ' ' && (lb[3] == '-'
			|| strncmp(lb + 3, "--help", strlen(lb + 3)) == 0))
				break;
			if ((printed = check_dirhist(word, wlen)) != NO_MATCH)
				goto SUCCESS;
			else
				goto FAIL;
		}
		break;

	case 'c': /* Color schemes */
		if (conf.colorize == 1 && color_schemes && lb[1] == 's'
		&& lb[2] == ' ') {
			if ((printed = check_color_schemes(word, wlen)) != NO_MATCH)
				goto SUCCESS;
		}
		break;

	case 'f': /* fallthrough */ /* REMOVE AS SOON AS FH CMD IS REMOVED! */
	case 'd': /* Dirhist command (dh) */
		if (lb[1] == 'h' && lb[2] == ' ' && (lb[3] == '-'
		|| strncmp(lb + 3, "--help", strlen(lb + 3)) == 0))
			break;
		if (words_num == 2 && old_pwd && dirhist_total_index > 0 && wlen > 0
		&& lb[1] == 'h' && lb[2] == ' ' && !strchr(word, '/')) {
			if ((printed = check_dirhist(word, wlen)) != NO_MATCH)
				goto SUCCESS;
			else
				goto FAIL;
		}
		break;

	case 'j': /* j command */
		if (lb[1] == ' ' && lb[2] == '-' && (lb[3] == 'h'
		|| strncmp(lb + 2, "--help", strlen(lb + 2)) == 0))
			break;

		if (lb[1] == ' '  || ((lb[1] == 'c'	|| lb[1] == 'p') && lb[2] == ' ')) {
			if ((printed = check_jcmd(full_line)) != NO_MATCH) {
				zero_offset = 1;
				goto SUCCESS;
			} else {
				goto FAIL;
			}
		}
		break;

	case 'n': /* 'net' command: remotes */
		if (remotes && lb[1] == 'e' && lb[2] == 't' && lb[3] == ' ') {
			if ((printed = check_remotes(word, wlen)) != NO_MATCH)
				goto SUCCESS;
		}
		break;

	case 'p': /* Profiles */
#ifndef _NO_PROFILES
		if (profile_names && words_num == 3 && lb[1] == 'f' && lb[2] == ' '
		&& (strncmp(lb + 3, "set ", 4) == 0 || strncmp(lb + 3, "del ", 4) == 0
		|| strncmp(lb + 3, "rename ", 7) == 0)) {
			if ((printed = check_profiles(word, wlen)) != NO_MATCH)
				goto SUCCESS;
			else
				goto FAIL;
		}
#endif /* !_NO_PROFILES */

		if (lb[1] == 'r' && strncmp(lb, "prompt set ", 11) == 0) {
			printed = check_prompts(word, wlen);
			if (prompts_n > 0 && printed != NO_MATCH)
				goto SUCCESS;
		}
		break;

	case 's': /* Sort */
		if (((lb[1] == 't' && lb[2] == ' ') || strncmp(lb, "sort ", 5) == 0)
		&& is_number(word)) {
			if (words_num > 2)
				goto FAIL;
			if ((printed = check_sort_methods(word, wlen)) != NO_MATCH)
				goto SUCCESS;
			goto FAIL;
		}
		break;

#ifndef _NO_TAGS
	case 't': /* Tag command */
		if ((lb[1] == 'a' || lb[1] == 'u') && lb[2] == ' ') {
			if (*word == ':' && *(word + 1)
			&& (printed = check_tags(word + 1, wlen - 1, TAGC_SUG)) != NO_MATCH)
				goto SUCCESS;
		} else if ((lb[1] == 'l' || lb[1] == 'm' || lb[1] == 'n'
		|| lb[1] == 'r' || lb[1] == 'y') && lb[2] == ' ') {
			printed = check_tags(word, wlen, TAGS_SUG);
			if (*word && printed != NO_MATCH)
				goto SUCCESS;
		}
		break;
#endif /* _NO_TAGS */

	case 'w': /* Workspaces */
		if (lb[1] == 's' && lb[2] == ' ') {
			if (words_num > 2)
				goto FAIL;
			if ((printed = check_workspaces(word, wlen)) != NO_MATCH)
				goto SUCCESS;
		}
	break;

	default: break;
	}

	/* 3.d.1) Variable names, both environment and internal. */
	if (*word == '$') {
		if ((printed = check_variables(word + 1, wlen - 1)) != NO_MATCH)
			goto SUCCESS;
	}

	/* 3.d.2) ~usernames */
	if (*word == '~' && *(word + 1) != '/') {
		if ((printed = check_users(word + 1, wlen - 1)) != NO_MATCH)
			goto SUCCESS;
	}

	/* 3.d.3) Bookmark names (b:) */
	if (*word == 'b' && *(word + 1) == ':' && *(word + 2)) {
		if ((printed = check_bookmark_names(word, wlen)) != NO_MATCH)
			goto SUCCESS;
	}

#ifndef _NO_TAGS
	/* 3.d.4) Tag names (t:) */
	if (*lb != ';' && *lb != ':' && *word == 't' && *(word + 1) == ':'
	&& *(word + 2)) {
		if ((printed = check_tags(word + 2, wlen - 2, TAGT_SUG)) != NO_MATCH)
			goto SUCCESS;
	}
#endif /* _NO_TAGS */

	/* 3.e) Execute the following checks in the order specified by
	 * suggestion_strategy (the value is taken from the configuration file). */
	size_t st;
	int flag = 0;

	/* Let's find out whether the last entered character is escaped. */
	int escaped = (wlen > 1 && word[wlen - 2] == '\\') ? 1 : 0;

	for (st = 0; st < SUG_STRATS; st++) {
		switch (conf.suggestion_strategy[st]) {

		case 'a': /* 3.e.1) Aliases */
			flag = c == ' ' ? CHECK_MATCH : PRINT_MATCH;
			if (flag == CHECK_MATCH && suggestion.printed)
				clear_suggestion(CS_FREEBUF);

			if ((printed = check_aliases(word, wlen, flag)) != NO_MATCH)
				goto SUCCESS;
			break;

		case 'c': /* 3.e.2) Path completion */
			if (rl_point < rl_end && c == '/') goto NO_SUGGESTION;

			/* First word and neither autocd nor auto-open */
			if (!last_space && conf.autocd == 0 && conf.auto_open == 0)
				break;

			/* Skip internal commands not dealing with file names */
			if (first_word) {
				flags |= STATE_COMPLETING;
				if (is_internal_c(first_word) && !is_internal_f(first_word)) {
					flags &= ~STATE_COMPLETING;
					goto NO_SUGGESTION;
				}
				flags &= ~STATE_COMPLETING;
			}

			if (words_num == 1) {
				word = first_word ? first_word : last_word;
				wlen = strlen(word);
			}

			if (wlen > 0 && word[wlen - 1] == ' ' && escaped == 0)
				word[wlen - 1] = '\0';

			flag = (c == ' ' && escaped == 0) ? CHECK_MATCH : PRINT_MATCH;

			char *d = word;
			if (wlen > FILE_URI_PREFIX_LEN && IS_FILE_URI(word)) {
				d += FILE_URI_PREFIX_LEN;
				wlen -= FILE_URI_PREFIX_LEN;
				last_word_offset += FILE_URI_PREFIX_LEN;
			}

			printed = check_completions(d, wlen, flag);
			if (printed != NO_MATCH) {
				if (flag == CHECK_MATCH) {
					if (printed == FULL_MATCH)
						goto SUCCESS;
				} else {
					goto SUCCESS;
				}
			}

			break;

		case 'e': /* 3.e.3) ELN's */
			if (words_num == 1 && first_word) {
				word = first_word;
				wlen = strlen(word);
			}

			if (wlen == 0)
				break;

			int nlen = (int)wlen;
			while (nlen > 0 && word[nlen - 1] == ' ' && escaped == 0) {
				nlen--;
				word[nlen] = '\0';
			}

			/* If ELN&, remove ending '&' to check the ELN */
			if (nlen > 0 && word[nlen - 1] == '&') {
				nlen--;
				word[nlen] = '\0';
			}

			flag = c == ' ' ? CHECK_MATCH : PRINT_MATCH;

			if (flag == CHECK_MATCH && suggestion.printed)
				clear_suggestion(CS_FREEBUF);

			if (*lb != ';' && *lb != ':' && *word >= '1' && *word <= '9') {
				if (should_expand_eln(word) == 1
				&& (printed = check_eln(word, flag)) > 0)
					goto SUCCESS;
			}
			break;

		case 'f': /* 3.e.4) File names in CWD */
			/* Do not check dirs and filenames if first word and
			 * neither autocd nor auto-open are enabled */
			if (!last_space && conf.autocd == 0 && conf.auto_open == 0)
				break;

			if (words_num == 1) {
				word = (first_word && *first_word) ? first_word : last_word;
				wlen = strlen(word);
			}

			/* If "./FILE" check only "FILE" */
			if (wlen > 2 && *word == '.' && *(word + 1) == '/') {
				word += 2;
				wlen -= 2;
				last_word_offset += 2;
			}

			if (*word == '/') /* Absolute path: nothing to do here */
				break;

			/* If we have a slash, we're not looking for files in the CWD */
			char *p = strchr(word, '/');
			if (p && *(p + 1))
				break;

			/* Skip internal commands not dealing with file names */
			if (first_word) {
				flags |= STATE_COMPLETING;
				if (is_internal_c(first_word) && !is_internal_f(first_word)) {
					flags &= ~STATE_COMPLETING;
					goto NO_SUGGESTION;
				}
				flags &= ~STATE_COMPLETING;
			}

			if (wlen > 0 && word[wlen - 1] == ' ' && escaped == 0)
				word[wlen - 1] = '\0';

			if (c == ' ' && escaped == 0 && suggestion.printed)
				clear_suggestion(CS_FREEBUF);

			printed = check_filenames(word, wlen,
				last_space ? 0 : 1, c == ' ' ? 1 : 0);
			if (printed != NO_MATCH)
				goto SUCCESS;

			break;

		case 'h': /* 3.e.5) Commands history */
			printed = check_history(full_line, (size_t)rl_end);
			if (printed != NO_MATCH) {
				zero_offset = 1;
				goto SUCCESS;
			}
			break;

		case 'j': /* 3.e.6) Jump database */
			/* We don't care about auto-open here: the jump function
			 * deals with directories only */
			if (!last_space && conf.autocd == 0)
				break;

			if (words_num == 1) {
				word = (first_word && *first_word) ? first_word : last_word;
				wlen = strlen(word);
			}

			if (wlen > 0 && word[wlen - 1] == ' ' && escaped == 0)
				word[wlen - 1] = '\0';

			flag = (c == ' ' || full_word) ? CHECK_MATCH : PRINT_MATCH;

			if (flag == CHECK_MATCH && suggestion.printed)
				clear_suggestion(CS_FREEBUF);

			if ((printed = check_jumpdb(word, wlen, flag)) != NO_MATCH)
				goto SUCCESS;

			break;

		case '-': break; /* Ignore check */
		default: break;
		}
	}

	/* 3.f) Cmds in PATH and CliFM internals cmds, but only for the first word. */
	if (words_num > 1)
		goto NO_SUGGESTION;

CHECK_FIRST_WORD:

	word = first_word ? first_word : last_word;

	/* Skip 'b:' (bookmarks), 's:' (sel files) and 't:' (tags) constructs. */
	if ((*word == 'b' || *word == 's' || *word == 't') && *(word + 1) == ':')
		goto NO_SUGGESTION;

	if (!word || !*word

	|| (c == ' ' && (*word == '\''
	|| *word == '"' || *word == '$' || *word == '#'))
	|| *word == '<' || *word == '>' || *word == '!' || *word == '{'
	|| *word == '[' || *word == '(' || strchr(word, '=')
	|| *rl_line_buffer == ' ' || *word == '|' || *word == ';'
	|| *word == '&') {
		if (suggestion.printed && suggestion_buf)
			clear_suggestion(CS_FREEBUF);
		goto SUCCESS;
	}

	wlen = strlen(word);
	/* If absolute path */
	if (point_is_first_word && *word == '/' && access(word, X_OK) == 0) {
		printed = 1;

	} else if (point_is_first_word && rl_point < rl_end
	&& *word >= '1' && *word <= '9' && is_number(word)) {
		const filesn_t a = xatof(word);
		if (a > 0 && a <= files)
			printed = PARTIAL_MATCH;

	} else if (point_is_first_word && rl_point < rl_end
	&& (printed = check_completions(word, wlen, CHECK_MATCH)) != NO_MATCH) {
		if (c == ' ' && printed != FULL_MATCH)
			/* We have a partial match for a file name. If not a command
			 * name, let's return NO_MATCH. */
			printed = check_cmds(word, wlen, CHECK_MATCH);

	} else {
		if (wlen > 0 && word[wlen - 1] == ' ')
			word[wlen - 1] = '\0';

		flag = (c == ' ' || full_word) ? CHECK_MATCH : PRINT_MATCH;
		printed = check_cmds(word, wlen, flag);
	}

	if (printed != NO_MATCH) {
		if (wrong_cmd && (words_num == 1 || point_is_first_word)) {
			rl_dispatching = 1;
			recover_from_wrong_cmd();
			rl_dispatching = 0;
		}
		goto SUCCESS;

	/* Let's assume that two slashes do not constitue a search expression */
	} else {
	/* There's no suggestion nor any command name matching the first entered
	 * word. So, we assume we have an invalid command name. Switch to the
	 * warning prompt to warn the user. */
		if (*word != '/' || strchr(word + 1, '/'))
			print_warning_prompt(*word, c);
	}

NO_SUGGESTION:
	/* Clear current suggestion, if any, only if no escape char is contained
	 * in the current input sequence. This is mainly to avoid erasing
	 * the suggestion if moving thought the text via the arrow keys. */
	if (suggestion.printed) {
		if (!strchr(word, KEY_ESC)) {
			goto FAIL;
		} else {
			/* Go to SUCCESS to avoid removing the suggestion buffer. */
			printed = PARTIAL_MATCH;
			goto SUCCESS;
		}
	}

SUCCESS:
	if (printed != NO_MATCH) {
		suggestion.offset = zero_offset ? 0 : last_word_offset;

		if (printed == FULL_MATCH && suggestion_buf)
			clear_suggestion(CS_FREEBUF);

		suggestion.printed = rl_point < rl_end ? 0 : 1;

		if (wrong_cmd == 1 && words_num == 1) {
			rl_dispatching = 1;
			recover_from_wrong_cmd();
			rl_dispatching = 0;
			/* recover_from_wrong_cmd() removes the suggestion.
			 * Let's reprint it */
			if (rl_point < rl_end && suggestion_buf && rl_line_buffer
			&& *rl_line_buffer) {
				print_suggestion(suggestion_buf, wc_xstrlen(rl_line_buffer),
					suggestion.color);
				suggestion.printed = 1;
			}
		}

		fputs(NC, stdout);

		/* Restore color */
		if (wrong_cmd == 0) {
			fputs(cur_color ? cur_color : tx_c, stdout);
		} else {
			fputs(wp_c, stdout);
		}
	} else {
		if (wrong_cmd == 1) {
			fputs(NC, stdout);
			fputs(wp_c, stdout);
		}
		suggestion.printed = 0;
	}

	free(first_word);
	free(last_word);
	last_word = (char *)NULL;

	return EXIT_SUCCESS;

FAIL:
	if (suggestion.printed == 1)
		clear_suggestion(CS_FREEBUF);

	free(first_word);
	free(last_word);
	last_word = (char *)NULL;

	return EXIT_FAILURE;
}
#else
void *_skip_me_suggestions;
#endif /* _NO_SUGGESTIONS */
