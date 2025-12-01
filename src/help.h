/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* help.h */

#ifndef HELP_H
#define HELP_H

__BEGIN_DECLS

int  list_commands(void);
void print_tips(const int all);
int  quick_help(const char *topic);
void help_function(void);

__END_DECLS

#endif /* HELP_H */
