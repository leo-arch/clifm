/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* xbrace.h -- perform csh-like brace expansion */

#ifndef CLIFM_XBRACE_H
#define CLIFM_XBRACE_H

char **brace_expand(const char *expression, size_t *count);

#endif /* CLIFM_XBRACE_H */
