/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* archives.h */

#ifndef ARCHIVES_H
#define ARCHIVES_H

__BEGIN_DECLS

int is_compressed(char *file, const int test_iso);
int archiver(char **args, char mode);

__END_DECLS

#endif /* ARCHIVES_H */
