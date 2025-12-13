/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* term.h */

#ifndef CLIFM_TERM_H
#define CLIFM_TERM_H

/* TERMINAL ESCAPE CODES */
#define CLEAR \
	if (term_caps.home == 1 && term_caps.clear == 1) { \
		if (term_caps.del_scrollback == 1)             \
			fputs("\x1b[H\x1b[2J\x1b[3J", stdout);     \
		else if (term_caps.del_scrollback == 2)        \
			fputs("\033c", stdout);                    \
		else                                           \
			fputs("\x1b[H\x1b[J", stdout);             \
	}

#define MOVE_CURSOR_DOWN(n)      printf("\x1b[%dB", (n))  /* CUD */

/* ######## Escape sequences used by the suggestions system */
#define MOVE_CURSOR_UP(n)        printf("\x1b[%dA", (n))  /* CUU */
#define MOVE_CURSOR_RIGHT(n)     printf("\x1b[%dC", (n))  /* CUF */
#define MOVE_CURSOR_LEFT(n)      printf("\x1b[%dD", (n))  /* CUB */
#define ERASE_TO_RIGHT           fputs("\x1b[0K", stdout) /* EL0 */
#define ERASE_TO_LEFT            fputs("\x1b[1K", stdout) /* EL1 */
#define ERASE_TO_RIGHT_AND_BELOW fputs("\x1b[J", stdout)  /* ED0 */

#define	SUGGEST_BAEJ(offset, color) printf("\x1b[%dC%s%c\x1b[0m ", \
	(offset), (color), SUG_POINTER)
/* ######## */

/* Sequences used by the pad_filename function (listing.c):
 * MOVE_CURSOR_RIGHT() */

/* Sequences used by the pager (listing.c):
 * MOVE_CURSOR_DOWN(n)
 * ERASE_TO_RIGHT */

#define META_SENDS_ESC  fputs("\x1b[?1036h", stdout)
#define HIDE_CURSOR     fputs(term_caps.hide_cursor == 1 ? "\x1b[?25l" : "", stdout) /* DECTCEM */
#define UNHIDE_CURSOR   fputs(term_caps.hide_cursor == 1 ? "\x1b[?25h" : "", stdout)

#define RESTORE_COLOR   fputs("\x1b[0;39;49m", stdout)
#define SET_RVIDEO      fputs("\x1b[?5h", stderr) /* DECSCNM: Enable reverse video */
#define UNSET_RVIDEO    fputs("\x1b[?5l", stderr)
#define SET_LINE_WRAP   fputs("\x1b[?7h", stderr) /* DECAWM */
#define UNSET_LINE_WRAP fputs("\x1b[?7l", stderr)
#define RING_BELL       fputs("\007", stderr)

#define CPR_CODE "\x1b[6n" /* Cursor position report */

/* Kitty keyboard protocol */
#define SET_KITTY_KEYS   fputs("\x1b[>1u", stdout)
#define UNSET_KITTY_KEYS fputs("\x1b[<u", stdout)

/* Time in millisenconds to wait for terminal responses. */
#define DEF_READ_TIMEOUT_MS_LOCAL  100
#define DEF_READ_TIMEOUT_MS_REMOTE 500

__BEGIN_DECLS

void check_term(void);
int  disable_raw_mode(const int fd);
int  enable_raw_mode(const int fd);
int  get_cursor_position(int *c, int *l);
void init_shell(void);
void report_cwd(const char *dir);
void set_term_title(char *str);

__END_DECLS

#endif /* !CLIFM_TERM_H */
