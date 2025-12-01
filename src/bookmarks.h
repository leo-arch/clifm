/*
 * This file is part of Clifm
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2016-2025 L. Abramovich <leo.clifm@outlook.com>
*/

/* bookmarks.h */

#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#define NO_BOOKMARKS "bookmarks: No bookmarks\nUse 'bm add dir/ name' \
to create a bookmark\nTry 'bm --help' for more information"

#define BM_ADD_NO_PARAM "bookmarks: A file and a name are required\n\
Example: 'bm add dir/ name'\nTry 'bm --help' for more information"

#define BM_DEL_NO_PARAM "bookmarks: A name is required\n\
Example: 'bm del name'\nTry 'bm --help' for more information"

#define PRINT_BM_HEADER 1
#define NO_BM_HEADER    0
#define BM_SCREEN       1 /* The edit function is called from the bookmarks screen */
#define NO_BM_SCREEN    0

__BEGIN_DECLS

int  bookmarks_function(char **cmd);
int  open_bookmark(void);
void free_bookmarks(void);

__END_DECLS

#endif /* BOOKMARKS_H */
