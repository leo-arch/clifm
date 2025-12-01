/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* remotes.h */

#ifndef REMOTES_H
#define REMOTES_H

__BEGIN_DECLS

int remotes_function(char **args);
int automount_remotes(void);
int autounmount_remotes(void);

__END_DECLS

#endif /* REMOTES_H */
