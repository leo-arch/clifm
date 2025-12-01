/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* jump.h */

#ifndef JUMP_H
#define JUMP_H

__BEGIN_DECLS

int  add_to_jumpdb(char *dir);
void save_jumpdb(void);
int  dirjump(char **args, int mode);

__END_DECLS

#endif /* JUMP_H */
