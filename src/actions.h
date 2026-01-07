/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* actions.h */

#ifndef ACTIONS_H
#define ACTIONS_H

__BEGIN_DECLS

int actions_function(char **args);
int run_action(char *action, char **args);

__END_DECLS

#endif /* ACTIONS_H */
