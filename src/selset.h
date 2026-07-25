/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* selset.h */

#ifndef SELSET_H
#define SELSET_H

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

int  devino_set_init(devino_set_t *s, size_t initial_cap);
void devino_set_destroy(devino_set_t *s);
int  devino_set_insert(devino_set_t *s, dev_t dev, ino_t ino); // returns 1 if inserted, 0 if already present
int  devino_set_contains(const devino_set_t *s, dev_t dev, ino_t ino);

__END_DECLS

#endif /* SORT_H */
