/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* mem.h */

/* This file is included by aux.h */

#ifndef CLIFM_MEM_H
#define CLIFM_MEM_H

__BEGIN_DECLS

void *xnrealloc(void *ptr, const size_t nmemb, const size_t size);
void *xcalloc(const size_t nmemb, const size_t size);
void *xnmalloc(const size_t nmemb, const size_t size);

__END_DECLS

#endif /* CLIFM_MEM_H */
