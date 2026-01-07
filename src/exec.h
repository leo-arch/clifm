/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* exec.h */

#ifndef EXEC_H
#define EXEC_H

__BEGIN_DECLS

int  exec_cmd_tm(char **cmd);
void exec_chained_cmds(char *cmd);
void exec_profile(void);

__END_DECLS

#endif /* EXEC_H */
