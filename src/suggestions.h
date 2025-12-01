/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* suggestions.h */

#ifndef SUGGESTIONS_H
#define SUGGESTIONS_H

__BEGIN_DECLS

void clear_suggestion(const int sflag);
void free_suggestion(void);
void print_suggestion(char *str, size_t offset, char *color);
int  recover_from_wrong_cmd(void);
int  rl_suggestions(const unsigned char c);

__END_DECLS

#endif /* SUGGESTIONS_H */
