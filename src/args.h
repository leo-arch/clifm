/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* args.h */

#ifndef ARGS_H
#define ARGS_H

__BEGIN_DECLS

void parse_cmdline_args(const int argc, char **argv);
void get_data_dir(void);
int  set_start_path(void);

__END_DECLS

#endif /* ARGS_H */
