/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* spawn.h */

#ifndef SPAWN_H
#define SPAWN_H

__BEGIN_DECLS

int get_exit_code(const int status, const int exec_flag);
int launch_execl(const char *cmd);
int launch_execv(char **cmd, const int bg, const int xflags);

__END_DECLS

#endif /* SPAWN_H */
