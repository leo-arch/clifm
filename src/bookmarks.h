/* bookmarks.h */

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
