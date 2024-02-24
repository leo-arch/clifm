/* dothidden.h */

/*
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
