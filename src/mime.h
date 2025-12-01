/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* mime.h */

#ifndef MIME_H
#define MIME_H

__BEGIN_DECLS

int  mime_open(char **args);
int  mime_open_url(char *url);
int  mime_open_with(char *filename, char **args);
char **mime_open_with_tab(char *filename, const char *prefix,
	const int only_names);
char *xmagic(const char *file, const int query_mime);

__END_DECLS

#endif /* MIME_H */
