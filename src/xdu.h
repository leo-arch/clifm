/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2026 L. Abramovich <leo.clifm@outlook.com>
*/

/* xdu.h */

#ifndef CLIFM_XDU_H
#define CLIFM_XDU_H

__BEGIN_DECLS

void dir_info(const char *dir, const int first_level, struct dir_info_t *info);
#ifdef USE_DU1
off_t dir_size(char *dir, const int first_level, int *status);
#else
off_t dir_size(const char *dir, const int first_level, int *status);
#endif /* USE_DU1 */

__END_DECLS

#endif /* CLIFM_XDU_H */
