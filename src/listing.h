/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* listing.h */

#ifndef LISTING_H
#define LISTING_H

__BEGIN_DECLS

void free_dirlist(void);
int  list_dir(void);
void reload_dirlist(void);
void refresh_screen(void);

#ifndef _NO_ICONS
void init_icons_hashes(void);
#endif /* !_NO_ICONS */

__END_DECLS

#endif /* LISTING_H */
