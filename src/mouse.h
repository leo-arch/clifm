/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* mouse.h */

#ifndef CLIFM_MOUSE_H
#define CLIFM_MOUSE_H

enum mouse_seq_ret {
	MOUSE_SEQ_NOT_MOUSE = 0,
	MOUSE_SEQ_CONSUMED,
	MOUSE_SEQ_ACCEPT_LINE
};

__BEGIN_DECLS

void enable_mouse_if_interactive(void);
int  mouse_handle_escape(FILE *stream);
int  mouse_pop_pending_byte(unsigned char *c);
int  mouse_pending_single_wait_ms(void);
int  mouse_commit_pending_single_click(void);
void mouse_reset_layout_state(void);
void mouse_set_post_listing_lines(int lines);
void mouse_add_post_listing_lines(int lines);

__END_DECLS

#endif /* CLIFM_MOUSE_H */
