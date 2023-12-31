/* exec.h */

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

#ifndef EXEC_H
#define EXEC_H

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
# include <signal.h>
#endif /* BSD || __sun */

__BEGIN_DECLS

int  exec_cmd(char **comm);
void exec_chained_cmds(char *cmd);
void exec_profile(void);
int  get_exit_code(const int status, const int exec_flag);
int  launch_execv(char **cmd, const int bg, const int xflags);
int  launch_execl(const char *cmd);

__END_DECLS

#endif /* EXEC_H */
