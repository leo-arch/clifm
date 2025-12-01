/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* tabcomp.h */

#ifndef TABCOMP_H
#define TABCOMP_H

__BEGIN_DECLS

int  tab_complete(const int what_to_do);
void reinsert_slashes(char *str);

__END_DECLS

#endif /* TABCOMP_H */
