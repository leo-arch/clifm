/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* dothidden.h */

#ifndef DOTHIDDEN_H
#define DOTHIDDEN_H

/* File containing the list of files to be hidden */
#define DOTHIDDEN_FILE ".hidden"

struct dothidden_t {
	char  *name;
	size_t len;
};

__BEGIN_DECLS

struct dothidden_t *load_dothidden(void);
int check_dothidden(const char *restrict name, struct dothidden_t **h);
void free_dothidden(struct dothidden_t **h);

__END_DECLS

#endif /* DOTHIDDEN_H */
