/* autocmds.h
 *
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
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

#ifndef AUTOCMDS_H
#define AUTOCMDS_H

__BEGIN_DECLS

int  add_autocmd(char **args);
int  check_autocmds(void);
void parse_autocmd_line(char *cmd, const size_t buflen);
void print_autocmd_msg(void);
void reset_opts(void);
void revert_autocmd_opts(void);

__END_DECLS

#endif /* AUTOCMDS_H */
