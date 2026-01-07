/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* highlight.h */

#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

__BEGIN_DECLS

char *rl_highlight(char *str, const size_t pos, const int flag);
void recolorize_line(void);

__END_DECLS

#endif /* HIGHLIGHT_H */
